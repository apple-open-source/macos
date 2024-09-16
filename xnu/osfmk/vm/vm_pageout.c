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
/*
 * @OSF_COPYRIGHT@
 */
/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 */
/*
 *	File:	vm/vm_pageout.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *	Date:	1985
 *
 *	The proverbial page-out daemon.
 */

#include "mach/kern_return.h"
#include <stdint.h>
#include <ptrauth.h>

#include <debug.h>

#include <mach/mach_types.h>
#include <mach/memory_object.h>
#include <mach/mach_host_server.h>
#include <mach/upl.h>
#include <mach/vm_map.h>
#include <mach/vm_param.h>
#include <mach/vm_statistics.h>
#include <mach/sdt.h>

#include <kern/kern_types.h>
#include <kern/counter.h>
#include <kern/host_statistics.h>
#include <kern/machine.h>
#include <kern/misc_protos.h>
#include <kern/sched.h>
#include <kern/thread.h>
#include <kern/kalloc.h>
#include <kern/zalloc_internal.h>
#include <kern/policy_internal.h>
#include <kern/thread_group.h>

#include <os/log.h>

#include <sys/kdebug_triage.h>

#include <machine/vm_tuning.h>
#include <machine/commpage.h>

#include <vm/pmap.h>
#include <vm/vm_compressor_pager_internal.h>
#include <vm/vm_fault_internal.h>
#include <vm/vm_map_internal.h>
#include <vm/vm_object_internal.h>
#include <vm/vm_page_internal.h>
#include <vm/vm_pageout_internal.h>
#include <vm/vm_protos_internal.h> /* must be last */
#include <vm/memory_object.h>
#include <vm/vm_purgeable_internal.h>
#include <vm/vm_shared_region.h>
#include <vm/vm_compressor_internal.h>
#include <vm/vm_kern_xnu.h>
#include <vm/vm_iokit.h>
#include <vm/vm_ubc.h>

#include <san/kasan.h>

#if CONFIG_PHANTOM_CACHE
#include <vm/vm_phantom_cache_internal.h>
#endif

#if UPL_DEBUG
#include <libkern/OSDebug.h>
#endif

extern int cs_debug;

#if CONFIG_MBUF_MCACHE
extern void mbuf_drain(boolean_t);
#endif /* CONFIG_MBUF_MCACHE */

#if VM_PRESSURE_EVENTS
#if CONFIG_JETSAM
extern unsigned int memorystatus_available_pages;
extern unsigned int memorystatus_available_pages_pressure;
extern unsigned int memorystatus_available_pages_critical;
#else /* CONFIG_JETSAM */
extern uint64_t memorystatus_available_pages;
extern uint64_t memorystatus_available_pages_pressure;
extern uint64_t memorystatus_available_pages_critical;
#endif /* CONFIG_JETSAM */
#if CONFIG_FREEZE
extern unsigned int memorystatus_frozen_count;
extern unsigned int memorystatus_suspended_count;
#endif /* CONFIG_FREEZE */
extern vm_pressure_level_t memorystatus_vm_pressure_level;

extern lck_mtx_t memorystatus_jetsam_broadcast_lock;
extern uint32_t memorystatus_jetsam_fg_band_waiters;
extern uint32_t memorystatus_jetsam_bg_band_waiters;

void vm_pressure_response(void);
extern void consider_vm_pressure_events(void);

#define MEMORYSTATUS_SUSPENDED_THRESHOLD  4
#endif /* VM_PRESSURE_EVENTS */

SECURITY_READ_ONLY_LATE(thread_t) vm_pageout_scan_thread;
SECURITY_READ_ONLY_LATE(thread_t) vm_pageout_gc_thread;
#if CONFIG_VPS_DYNAMIC_PRIO
TUNABLE(bool, vps_dynamic_priority_enabled, "vps_dynamic_priority_enabled", false);
#else
const bool vps_dynamic_priority_enabled = false;
#endif
boolean_t vps_yield_for_pgqlockwaiters = TRUE;

#ifndef VM_PAGEOUT_BURST_INACTIVE_THROTTLE  /* maximum iterations of the inactive queue w/o stealing/cleaning a page */
#if !XNU_TARGET_OS_OSX
#define VM_PAGEOUT_BURST_INACTIVE_THROTTLE 1024
#else /* !XNU_TARGET_OS_OSX */
#define VM_PAGEOUT_BURST_INACTIVE_THROTTLE 4096
#endif /* !XNU_TARGET_OS_OSX */
#endif

#ifndef VM_PAGEOUT_DEADLOCK_RELIEF
#define VM_PAGEOUT_DEADLOCK_RELIEF 100  /* number of pages to move to break deadlock */
#endif

#ifndef VM_PAGE_LAUNDRY_MAX
#define VM_PAGE_LAUNDRY_MAX     128UL   /* maximum pageouts on a given pageout queue */
#endif  /* VM_PAGEOUT_LAUNDRY_MAX */

#ifndef VM_PAGEOUT_BURST_WAIT
#define VM_PAGEOUT_BURST_WAIT   1       /* milliseconds */
#endif  /* VM_PAGEOUT_BURST_WAIT */

#ifndef VM_PAGEOUT_EMPTY_WAIT
#define VM_PAGEOUT_EMPTY_WAIT   50      /* milliseconds */
#endif  /* VM_PAGEOUT_EMPTY_WAIT */

#ifndef VM_PAGEOUT_DEADLOCK_WAIT
#define VM_PAGEOUT_DEADLOCK_WAIT 100    /* milliseconds */
#endif  /* VM_PAGEOUT_DEADLOCK_WAIT */

#ifndef VM_PAGEOUT_IDLE_WAIT
#define VM_PAGEOUT_IDLE_WAIT    10      /* milliseconds */
#endif  /* VM_PAGEOUT_IDLE_WAIT */

#ifndef VM_PAGEOUT_SWAP_WAIT
#define VM_PAGEOUT_SWAP_WAIT    10      /* milliseconds */
#endif  /* VM_PAGEOUT_SWAP_WAIT */

/*
 * vm_page_max_speculative_age_q should be less than or equal to
 * VM_PAGE_RESERVED_SPECULATIVE_AGE_Q which is number of allocated
 * vm_page_queue_speculative entries.
 */

TUNABLE_DEV_WRITEABLE(unsigned int, vm_page_max_speculative_age_q, "vm_page_max_speculative_age_q", VM_PAGE_DEFAULT_MAX_SPECULATIVE_AGE_Q);
#ifndef VM_PAGE_SPECULATIVE_TARGET
#define VM_PAGE_SPECULATIVE_TARGET(total) ((total) * 1 / (100 / vm_pageout_state.vm_page_speculative_percentage))
#endif /* VM_PAGE_SPECULATIVE_TARGET */


/*
 *	To obtain a reasonable LRU approximation, the inactive queue
 *	needs to be large enough to give pages on it a chance to be
 *	referenced a second time.  This macro defines the fraction
 *	of active+inactive pages that should be inactive.
 *	The pageout daemon uses it to update vm_page_inactive_target.
 *
 *	If vm_page_free_count falls below vm_page_free_target and
 *	vm_page_inactive_count is below vm_page_inactive_target,
 *	then the pageout daemon starts running.
 */

#ifndef VM_PAGE_INACTIVE_TARGET
#define VM_PAGE_INACTIVE_TARGET(avail)  ((avail) * 1 / 2)
#endif  /* VM_PAGE_INACTIVE_TARGET */

/*
 *	Once the pageout daemon starts running, it keeps going
 *	until vm_page_free_count meets or exceeds vm_page_free_target.
 */

#ifndef VM_PAGE_FREE_TARGET
#if !XNU_TARGET_OS_OSX
#define VM_PAGE_FREE_TARGET(free)       (15 + (free) / 100)
#else /* !XNU_TARGET_OS_OSX */
#define VM_PAGE_FREE_TARGET(free)       (15 + (free) / 80)
#endif /* !XNU_TARGET_OS_OSX */
#endif  /* VM_PAGE_FREE_TARGET */


/*
 *	The pageout daemon always starts running once vm_page_free_count
 *	falls below vm_page_free_min.
 */

#ifndef VM_PAGE_FREE_MIN
#if !XNU_TARGET_OS_OSX
#define VM_PAGE_FREE_MIN(free)          (10 + (free) / 200)
#else /* !XNU_TARGET_OS_OSX */
#define VM_PAGE_FREE_MIN(free)          (10 + (free) / 100)
#endif /* !XNU_TARGET_OS_OSX */
#endif  /* VM_PAGE_FREE_MIN */

#if !XNU_TARGET_OS_OSX
#define VM_PAGE_FREE_RESERVED_LIMIT     100
#define VM_PAGE_FREE_MIN_LIMIT          1500
#define VM_PAGE_FREE_TARGET_LIMIT       2000
#else /* !XNU_TARGET_OS_OSX */
#define VM_PAGE_FREE_RESERVED_LIMIT     1700
#define VM_PAGE_FREE_MIN_LIMIT          3500
#define VM_PAGE_FREE_TARGET_LIMIT       4000
#endif /* !XNU_TARGET_OS_OSX */

/*
 *	When vm_page_free_count falls below vm_page_free_reserved,
 *	only vm-privileged threads can allocate pages.  vm-privilege
 *	allows the pageout daemon and default pager (and any other
 *	associated threads needed for default pageout) to continue
 *	operation by dipping into the reserved pool of pages.
 */

#ifndef VM_PAGE_FREE_RESERVED
#define VM_PAGE_FREE_RESERVED(n)        \
	((unsigned) (6 * VM_PAGE_LAUNDRY_MAX) + (n))
#endif  /* VM_PAGE_FREE_RESERVED */

/*
 *	When we dequeue pages from the inactive list, they are
 *	reactivated (ie, put back on the active queue) if referenced.
 *	However, it is possible to starve the free list if other
 *	processors are referencing pages faster than we can turn off
 *	the referenced bit.  So we limit the number of reactivations
 *	we will make per call of vm_pageout_scan().
 */
#define VM_PAGE_REACTIVATE_LIMIT_MAX 20000

#ifndef VM_PAGE_REACTIVATE_LIMIT
#if !XNU_TARGET_OS_OSX
#define VM_PAGE_REACTIVATE_LIMIT(avail) (VM_PAGE_INACTIVE_TARGET(avail) / 2)
#else /* !XNU_TARGET_OS_OSX */
#define VM_PAGE_REACTIVATE_LIMIT(avail) (MAX((avail) * 1 / 20,VM_PAGE_REACTIVATE_LIMIT_MAX))
#endif /* !XNU_TARGET_OS_OSX */
#endif  /* VM_PAGE_REACTIVATE_LIMIT */
#define VM_PAGEOUT_INACTIVE_FORCE_RECLAIM       1000

int vm_pageout_protect_realtime = true;

extern boolean_t hibernate_cleaning_in_progress;

struct pgo_iothread_state pgo_iothread_internal_state[MAX_COMPRESSOR_THREAD_COUNT];
struct pgo_iothread_state pgo_iothread_external_state;

#if VM_PRESSURE_EVENTS
void vm_pressure_thread(void);

boolean_t VM_PRESSURE_NORMAL_TO_WARNING(void);
boolean_t VM_PRESSURE_WARNING_TO_CRITICAL(void);

boolean_t VM_PRESSURE_WARNING_TO_NORMAL(void);
boolean_t VM_PRESSURE_CRITICAL_TO_WARNING(void);
#endif

static void vm_pageout_iothread_external(struct pgo_iothread_state *, wait_result_t);
static void vm_pageout_iothread_internal(struct pgo_iothread_state *, wait_result_t);
static void vm_pageout_adjust_eq_iothrottle(struct pgo_iothread_state *, boolean_t);

extern void vm_pageout_continue(void);
extern void vm_pageout_scan(void);

boolean_t vm_pageout_running = FALSE;

uint32_t vm_page_upl_tainted = 0;
uint32_t vm_page_iopl_tainted = 0;

#if XNU_TARGET_OS_OSX
static boolean_t vm_pageout_waiter  = FALSE;
#endif /* XNU_TARGET_OS_OSX */


#if DEVELOPMENT || DEBUG
struct vm_pageout_debug vm_pageout_debug;
#endif
struct vm_pageout_vminfo vm_pageout_vminfo;
struct vm_pageout_state  vm_pageout_state;
struct vm_config         vm_config;

struct  vm_pageout_queue vm_pageout_queue_internal VM_PAGE_PACKED_ALIGNED;
struct  vm_pageout_queue vm_pageout_queue_external VM_PAGE_PACKED_ALIGNED;
#if DEVELOPMENT || DEBUG
struct vm_pageout_queue vm_pageout_queue_benchmark VM_PAGE_PACKED_ALIGNED;
#endif /* DEVELOPMENT || DEBUG */

int         vm_upl_wait_for_pages = 0;
vm_object_t vm_pageout_scan_wants_object = VM_OBJECT_NULL;

boolean_t(*volatile consider_buffer_cache_collect)(int) = NULL;

int     vm_debug_events = 0;

LCK_GRP_DECLARE(vm_pageout_lck_grp, "vm_pageout");

#if CONFIG_MEMORYSTATUS
extern void memorystatus_kill_on_vps_starvation(void);

uint32_t vm_pageout_memorystatus_fb_factor_nr = 5;
uint32_t vm_pageout_memorystatus_fb_factor_dr = 2;

#endif

#if __AMP__


/*
 * Bind compressor threads to e-cores unless there are multiple non-e clusters
 */
#if (MAX_CPU_CLUSTERS > 2)
#define VM_COMPRESSOR_EBOUND_DEFAULT false
#elif defined(XNU_TARGET_OS_XR)
#define VM_COMPRESSOR_EBOUND_DEFAULT false
#else
#define VM_COMPRESSOR_EBOUND_DEFAULT true
#endif

TUNABLE(bool, vm_compressor_ebound, "vmcomp_ecluster", VM_COMPRESSOR_EBOUND_DEFAULT);
int vm_pgo_pbound = 0;
extern void thread_bind_cluster_type(thread_t, char, bool);

#endif /* __AMP__ */


/*
 *	Routine:	vm_pageout_object_terminate
 *	Purpose:
 *		Destroy the pageout_object, and perform all of the
 *		required cleanup actions.
 *
 *	In/Out conditions:
 *		The object must be locked, and will be returned locked.
 */
void
vm_pageout_object_terminate(
	vm_object_t     object)
{
	vm_object_t     shadow_object;

	/*
	 * Deal with the deallocation (last reference) of a pageout object
	 * (used for cleaning-in-place) by dropping the paging references/
	 * freeing pages in the original object.
	 */

	assert(object->pageout);
	shadow_object = object->shadow;
	vm_object_lock(shadow_object);

	while (!vm_page_queue_empty(&object->memq)) {
		vm_page_t               p, m;
		vm_object_offset_t      offset;

		p = (vm_page_t) vm_page_queue_first(&object->memq);

		assert(p->vmp_private);
		assert(p->vmp_free_when_done);
		p->vmp_free_when_done = FALSE;
		assert(!p->vmp_cleaning);
		assert(!p->vmp_laundry);

		offset = p->vmp_offset;
		VM_PAGE_FREE(p);
		p = VM_PAGE_NULL;

		m = vm_page_lookup(shadow_object,
		    offset + object->vo_shadow_offset);

		if (m == VM_PAGE_NULL) {
			continue;
		}

		assert((m->vmp_dirty) || (m->vmp_precious) ||
		    (m->vmp_busy && m->vmp_cleaning));

		/*
		 * Handle the trusted pager throttle.
		 * Also decrement the burst throttle (if external).
		 */
		vm_page_lock_queues();
		if (m->vmp_q_state == VM_PAGE_ON_PAGEOUT_Q) {
			vm_pageout_throttle_up(m);
		}

		/*
		 * Handle the "target" page(s). These pages are to be freed if
		 * successfully cleaned. Target pages are always busy, and are
		 * wired exactly once. The initial target pages are not mapped,
		 * (so cannot be referenced or modified) but converted target
		 * pages may have been modified between the selection as an
		 * adjacent page and conversion to a target.
		 */
		if (m->vmp_free_when_done) {
			assert(m->vmp_busy);
			assert(m->vmp_q_state == VM_PAGE_IS_WIRED);
			assert(m->vmp_wire_count == 1);
			m->vmp_cleaning = FALSE;
			m->vmp_free_when_done = FALSE;
			/*
			 * Revoke all access to the page. Since the object is
			 * locked, and the page is busy, this prevents the page
			 * from being dirtied after the pmap_disconnect() call
			 * returns.
			 *
			 * Since the page is left "dirty" but "not modifed", we
			 * can detect whether the page was redirtied during
			 * pageout by checking the modify state.
			 */
			if (pmap_disconnect(VM_PAGE_GET_PHYS_PAGE(m)) & VM_MEM_MODIFIED) {
				SET_PAGE_DIRTY(m, FALSE);
			} else {
				m->vmp_dirty = FALSE;
			}

			if (m->vmp_dirty) {
				vm_page_unwire(m, TRUE);        /* reactivates */
				counter_inc(&vm_statistics_reactivations);
				vm_page_wakeup_done(object, m);
			} else {
				vm_page_free(m);  /* clears busy, etc. */
			}
			vm_page_unlock_queues();
			continue;
		}
		/*
		 * Handle the "adjacent" pages. These pages were cleaned in
		 * place, and should be left alone.
		 * If prep_pin_count is nonzero, then someone is using the
		 * page, so make it active.
		 */
		if ((m->vmp_q_state == VM_PAGE_NOT_ON_Q) && !m->vmp_private) {
			if (m->vmp_reference) {
				vm_page_activate(m);
			} else {
				vm_page_deactivate(m);
			}
		}
		if (m->vmp_overwriting) {
			/*
			 * the (COPY_OUT_FROM == FALSE) request_page_list case
			 */
			if (m->vmp_busy) {
				/*
				 * We do not re-set m->vmp_dirty !
				 * The page was busy so no extraneous activity
				 * could have occurred. COPY_INTO is a read into the
				 * new pages. CLEAN_IN_PLACE does actually write
				 * out the pages but handling outside of this code
				 * will take care of resetting dirty. We clear the
				 * modify however for the Programmed I/O case.
				 */
				pmap_clear_modify(VM_PAGE_GET_PHYS_PAGE(m));

				m->vmp_busy = FALSE;
				m->vmp_absent = FALSE;
			} else {
				/*
				 * alternate (COPY_OUT_FROM == FALSE) request_page_list case
				 * Occurs when the original page was wired
				 * at the time of the list request
				 */
				assert(VM_PAGE_WIRED(m));
				vm_page_unwire(m, TRUE);        /* reactivates */
			}
			m->vmp_overwriting = FALSE;
		} else {
			m->vmp_dirty = FALSE;
		}
		m->vmp_cleaning = FALSE;

		/*
		 * Wakeup any thread waiting for the page to be un-cleaning.
		 */
		vm_page_wakeup(object, m);
		vm_page_unlock_queues();
	}
	/*
	 * Account for the paging reference taken in vm_paging_object_allocate.
	 */
	vm_object_activity_end(shadow_object);
	vm_object_unlock(shadow_object);

	assert(object->ref_count == 0);
	assert(object->paging_in_progress == 0);
	assert(object->activity_in_progress == 0);
	assert(object->resident_page_count == 0);
	return;
}

/*
 * Routine:	vm_pageclean_setup
 *
 * Purpose:	setup a page to be cleaned (made non-dirty), but not
 *		necessarily flushed from the VM page cache.
 *		This is accomplished by cleaning in place.
 *
 *		The page must not be busy, and new_object
 *		must be locked.
 *
 */
static void
vm_pageclean_setup(
	vm_page_t               m,
	vm_page_t               new_m,
	vm_object_t             new_object,
	vm_object_offset_t      new_offset)
{
	assert(!m->vmp_busy);
#if 0
	assert(!m->vmp_cleaning);
#endif

	pmap_clear_modify(VM_PAGE_GET_PHYS_PAGE(m));

	/*
	 * Mark original page as cleaning in place.
	 */
	m->vmp_cleaning = TRUE;
	SET_PAGE_DIRTY(m, FALSE);
	m->vmp_precious = FALSE;

	/*
	 * Convert the fictitious page to a private shadow of
	 * the real page.
	 */
	assert(new_m->vmp_fictitious);
	assert(VM_PAGE_GET_PHYS_PAGE(new_m) == vm_page_fictitious_addr);
	new_m->vmp_fictitious = FALSE;
	new_m->vmp_private = TRUE;
	new_m->vmp_free_when_done = TRUE;
	VM_PAGE_SET_PHYS_PAGE(new_m, VM_PAGE_GET_PHYS_PAGE(m));

	vm_page_lockspin_queues();
	vm_page_wire(new_m, VM_KERN_MEMORY_NONE, TRUE);
	vm_page_unlock_queues();

	vm_page_insert_wired(new_m, new_object, new_offset, VM_KERN_MEMORY_NONE);
	assert(!new_m->vmp_wanted);
	new_m->vmp_busy = FALSE;
}

/*
 *	Routine:	vm_pageout_initialize_page
 *	Purpose:
 *		Causes the specified page to be initialized in
 *		the appropriate memory object. This routine is used to push
 *		pages into a copy-object when they are modified in the
 *		permanent object.
 *
 *		The page is moved to a temporary object and paged out.
 *
 *	In/out conditions:
 *		The page in question must not be on any pageout queues.
 *		The object to which it belongs must be locked.
 *		The page must be busy, but not hold a paging reference.
 *
 *	Implementation:
 *		Move this page to a completely new object.
 */
void
vm_pageout_initialize_page(
	vm_page_t       m)
{
	vm_object_t             object;
	vm_object_offset_t      paging_offset;
	memory_object_t         pager;

	assert(VM_CONFIG_COMPRESSOR_IS_PRESENT);

	object = VM_PAGE_OBJECT(m);

	assert(m->vmp_busy);
	assert(object->internal);

	/*
	 *	Verify that we really want to clean this page
	 */
	assert(!m->vmp_absent);
	assert(m->vmp_dirty);

	/*
	 *	Create a paging reference to let us play with the object.
	 */
	paging_offset = m->vmp_offset + object->paging_offset;

	if (m->vmp_absent || VMP_ERROR_GET(m) || m->vmp_restart || (!m->vmp_dirty && !m->vmp_precious)) {
		panic("reservation without pageout?"); /* alan */

		VM_PAGE_FREE(m);
		vm_object_unlock(object);

		return;
	}

	/*
	 * If there's no pager, then we can't clean the page.  This should
	 * never happen since this should be a copy object and therefore not
	 * an external object, so the pager should always be there.
	 */

	pager = object->pager;

	if (pager == MEMORY_OBJECT_NULL) {
		panic("missing pager for copy object");

		VM_PAGE_FREE(m);
		return;
	}

	/*
	 * set the page for future call to vm_fault_list_request
	 */
	pmap_clear_modify(VM_PAGE_GET_PHYS_PAGE(m));
	SET_PAGE_DIRTY(m, FALSE);

	/*
	 * keep the object from collapsing or terminating
	 */
	vm_object_paging_begin(object);
	vm_object_unlock(object);

	/*
	 *	Write the data to its pager.
	 *	Note that the data is passed by naming the new object,
	 *	not a virtual address; the pager interface has been
	 *	manipulated to use the "internal memory" data type.
	 *	[The object reference from its allocation is donated
	 *	to the eventual recipient.]
	 */
	memory_object_data_initialize(pager, paging_offset, PAGE_SIZE);

	vm_object_lock(object);
	vm_object_paging_end(object);
}


/*
 * vm_pageout_cluster:
 *
 * Given a page, queue it to the appropriate I/O thread,
 * which will page it out and attempt to clean adjacent pages
 * in the same operation.
 *
 * The object and queues must be locked. We will take a
 * paging reference to prevent deallocation or collapse when we
 * release the object lock back at the call site.  The I/O thread
 * is responsible for consuming this reference
 *
 * The page must not be on any pageout queue.
 */
#if DEVELOPMENT || DEBUG
vmct_stats_t vmct_stats;

int32_t vmct_active = 0;
uint64_t vm_compressor_epoch_start = 0;
uint64_t vm_compressor_epoch_stop = 0;

typedef enum vmct_state_t {
	VMCT_IDLE,
	VMCT_AWAKENED,
	VMCT_ACTIVE,
} vmct_state_t;
vmct_state_t vmct_state[MAX_COMPRESSOR_THREAD_COUNT];
#endif



static void
vm_pageout_cluster_to_queue(vm_page_t m, struct vm_pageout_queue *q)
{
	vm_object_t object = VM_PAGE_OBJECT(m);

	VM_PAGE_CHECK(m);
	LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);
	vm_object_lock_assert_exclusive(object);

	/*
	 * Make sure it's OK to page this out.
	 */
	assert((m->vmp_dirty || m->vmp_precious) && (!VM_PAGE_WIRED(m)));
	assert(!m->vmp_cleaning && !m->vmp_laundry);
	assert(m->vmp_q_state == VM_PAGE_NOT_ON_Q);

	/*
	 * protect the object from collapse or termination
	 */
	vm_object_activity_begin(object);


	/*
	 * pgo_laundry count is tied to the laundry bit
	 */
	m->vmp_laundry = TRUE;
	q->pgo_laundry++;

	m->vmp_q_state = VM_PAGE_ON_PAGEOUT_Q;
	vm_page_queue_enter(&q->pgo_pending, m, vmp_pageq);

	if (object->internal == TRUE) {
		assert(VM_CONFIG_COMPRESSOR_IS_PRESENT);
		m->vmp_busy = TRUE;
#if DEVELOPMENT || DEBUG
		/*
		 * The benchmark queue will be woken up independently by the benchmark
		 * itself.
		 */
		if (q != &vm_pageout_queue_benchmark) {
#else /* DEVELOPMENT || DEBUG */
		if (true) {
#endif /* DEVELOPMENT || DEBUG */
			/*
			 * Wake up the first compressor thread. It will wake subsequent
			 * threads if necessary.
			 */
			sched_cond_signal(&pgo_iothread_internal_state[0].pgo_wakeup,
			    pgo_iothread_internal_state[0].pgo_iothread);
		}
	} else {
		sched_cond_signal(&pgo_iothread_external_state.pgo_wakeup, pgo_iothread_external_state.pgo_iothread);
	}
	VM_PAGE_CHECK(m);
}

void
vm_pageout_cluster(vm_page_t m)
{
	struct          vm_pageout_queue *q;
	vm_object_t     object = VM_PAGE_OBJECT(m);
	if (object->internal) {
		q = &vm_pageout_queue_internal;
	} else {
		q = &vm_pageout_queue_external;
	}
	vm_pageout_cluster_to_queue(m, q);
}


/*
 * A page is back from laundry or we are stealing it back from
 * the laundering state.  See if there are some pages waiting to
 * go to laundry and if we can let some of them go now.
 *
 * Object and page queues must be locked.
 */
void
vm_pageout_throttle_up(
	vm_page_t       m)
{
	struct vm_pageout_queue *q;
	vm_object_t      m_object;

	m_object = VM_PAGE_OBJECT(m);

	assert(m_object != VM_OBJECT_NULL);
	assert(!is_kernel_object(m_object));

	LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);
	vm_object_lock_assert_exclusive(m_object);

	if (m_object->internal == TRUE) {
		q = &vm_pageout_queue_internal;
	} else {
		q = &vm_pageout_queue_external;
	}

	if (m->vmp_q_state == VM_PAGE_ON_PAGEOUT_Q) {
		vm_page_queue_remove(&q->pgo_pending, m, vmp_pageq);
		m->vmp_q_state = VM_PAGE_NOT_ON_Q;

		VM_PAGE_ZERO_PAGEQ_ENTRY(m);

		vm_object_activity_end(m_object);

		VM_PAGEOUT_DEBUG(vm_page_steal_pageout_page, 1);
	}
	if (m->vmp_laundry == TRUE) {
		m->vmp_laundry = FALSE;
		q->pgo_laundry--;

		if (q->pgo_throttled == TRUE) {
			q->pgo_throttled = FALSE;
			thread_wakeup((event_t) &q->pgo_laundry);
		}
		if (q->pgo_draining == TRUE && q->pgo_laundry == 0) {
			q->pgo_draining = FALSE;
			thread_wakeup((event_t) (&q->pgo_laundry + 1));
		}
		VM_PAGEOUT_DEBUG(vm_pageout_throttle_up_count, 1);
	}
}


static void
vm_pageout_throttle_up_batch(
	struct vm_pageout_queue *q,
	int             batch_cnt)
{
	LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);

	VM_PAGEOUT_DEBUG(vm_pageout_throttle_up_count, batch_cnt);

	q->pgo_laundry -= batch_cnt;

	if (q->pgo_throttled == TRUE) {
		q->pgo_throttled = FALSE;
		thread_wakeup((event_t) &q->pgo_laundry);
	}
	if (q->pgo_draining == TRUE && q->pgo_laundry == 0) {
		q->pgo_draining = FALSE;
		thread_wakeup((event_t) (&q->pgo_laundry + 1));
	}
}



/*
 * VM memory pressure monitoring.
 *
 * vm_pageout_scan() keeps track of the number of pages it considers and
 * reclaims, in the currently active vm_pageout_stat[vm_pageout_stat_now].
 *
 * compute_memory_pressure() is called every second from compute_averages()
 * and moves "vm_pageout_stat_now" forward, to start accumulating the number
 * of recalimed pages in a new vm_pageout_stat[] bucket.
 *
 * mach_vm_pressure_monitor() collects past statistics about memory pressure.
 * The caller provides the number of seconds ("nsecs") worth of statistics
 * it wants, up to 30 seconds.
 * It computes the number of pages reclaimed in the past "nsecs" seconds and
 * also returns the number of pages the system still needs to reclaim at this
 * moment in time.
 */
#if DEVELOPMENT || DEBUG
#define VM_PAGEOUT_STAT_SIZE    (30 * 8) + 1
#else
#define VM_PAGEOUT_STAT_SIZE    (1 * 8) + 1
#endif
struct vm_pageout_stat {
	unsigned long vm_page_active_count;
	unsigned long vm_page_speculative_count;
	unsigned long vm_page_inactive_count;
	unsigned long vm_page_anonymous_count;

	unsigned long vm_page_free_count;
	unsigned long vm_page_wire_count;
	unsigned long vm_page_compressor_count;

	unsigned long vm_page_pages_compressed;
	unsigned long vm_page_pageable_internal_count;
	unsigned long vm_page_pageable_external_count;
	unsigned long vm_page_xpmapped_external_count;

	unsigned int pages_grabbed;
	unsigned int pages_freed;

	unsigned int pages_compressed;
	unsigned int pages_grabbed_by_compressor;
	unsigned int failed_compressions;

	unsigned int pages_evicted;
	unsigned int pages_purged;

	unsigned int considered;
	unsigned int considered_bq_internal;
	unsigned int considered_bq_external;

	unsigned int skipped_external;
	unsigned int skipped_internal;
	unsigned int filecache_min_reactivations;

	unsigned int freed_speculative;
	unsigned int freed_cleaned;
	unsigned int freed_internal;
	unsigned int freed_external;

	unsigned int cleaned_dirty_external;
	unsigned int cleaned_dirty_internal;

	unsigned int inactive_referenced;
	unsigned int inactive_nolock;
	unsigned int reactivation_limit_exceeded;
	unsigned int forced_inactive_reclaim;

	unsigned int throttled_internal_q;
	unsigned int throttled_external_q;

	unsigned int phantom_ghosts_found;
	unsigned int phantom_ghosts_added;

	unsigned int vm_page_realtime_count;
	unsigned int forcereclaimed_sharedcache;
	unsigned int forcereclaimed_realtime;
	unsigned int protected_sharedcache;
	unsigned int protected_realtime;
} vm_pageout_stats[VM_PAGEOUT_STAT_SIZE];

unsigned int vm_pageout_stat_now = 0;

#define VM_PAGEOUT_STAT_BEFORE(i) \
	(((i) == 0) ? VM_PAGEOUT_STAT_SIZE - 1 : (i) - 1)
#define VM_PAGEOUT_STAT_AFTER(i) \
	(((i) == VM_PAGEOUT_STAT_SIZE - 1) ? 0 : (i) + 1)

#if VM_PAGE_BUCKETS_CHECK
int vm_page_buckets_check_interval = 80; /* in eighths of a second */
#endif /* VM_PAGE_BUCKETS_CHECK */


void
record_memory_pressure(void);
void
record_memory_pressure(void)
{
	unsigned int vm_pageout_next;

#if VM_PAGE_BUCKETS_CHECK
	/* check the consistency of VM page buckets at regular interval */
	static int counter = 0;
	if ((++counter % vm_page_buckets_check_interval) == 0) {
		vm_page_buckets_check();
	}
#endif /* VM_PAGE_BUCKETS_CHECK */

	vm_pageout_state.vm_memory_pressure =
	    vm_pageout_stats[VM_PAGEOUT_STAT_BEFORE(vm_pageout_stat_now)].freed_speculative +
	    vm_pageout_stats[VM_PAGEOUT_STAT_BEFORE(vm_pageout_stat_now)].freed_cleaned +
	    vm_pageout_stats[VM_PAGEOUT_STAT_BEFORE(vm_pageout_stat_now)].freed_internal +
	    vm_pageout_stats[VM_PAGEOUT_STAT_BEFORE(vm_pageout_stat_now)].freed_external;

	commpage_set_memory_pressure((unsigned int)vm_pageout_state.vm_memory_pressure );

	/* move "now" forward */
	vm_pageout_next = VM_PAGEOUT_STAT_AFTER(vm_pageout_stat_now);

	bzero(&vm_pageout_stats[vm_pageout_next], sizeof(struct vm_pageout_stat));

	vm_pageout_stat_now = vm_pageout_next;
}


/*
 * IMPORTANT
 * mach_vm_ctl_page_free_wanted() is called indirectly, via
 * mach_vm_pressure_monitor(), when taking a stackshot. Therefore,
 * it must be safe in the restricted stackshot context. Locks and/or
 * blocking are not allowable.
 */
unsigned int
mach_vm_ctl_page_free_wanted(void)
{
	unsigned int page_free_target, page_free_count, page_free_wanted;

	page_free_target = vm_page_free_target;
	page_free_count = vm_page_free_count;
	if (page_free_target > page_free_count) {
		page_free_wanted = page_free_target - page_free_count;
	} else {
		page_free_wanted = 0;
	}

	return page_free_wanted;
}


/*
 * IMPORTANT:
 * mach_vm_pressure_monitor() is called when taking a stackshot, with
 * wait_for_pressure FALSE, so that code path must remain safe in the
 * restricted stackshot context. No blocking or locks are allowable.
 * on that code path.
 */

kern_return_t
mach_vm_pressure_monitor(
	boolean_t       wait_for_pressure,
	unsigned int    nsecs_monitored,
	unsigned int    *pages_reclaimed_p,
	unsigned int    *pages_wanted_p)
{
	wait_result_t   wr;
	unsigned int    vm_pageout_then, vm_pageout_now;
	unsigned int    pages_reclaimed;
	unsigned int    units_of_monitor;

	units_of_monitor = 8 * nsecs_monitored;
	/*
	 * We don't take the vm_page_queue_lock here because we don't want
	 * vm_pressure_monitor() to get in the way of the vm_pageout_scan()
	 * thread when it's trying to reclaim memory.  We don't need fully
	 * accurate monitoring anyway...
	 */

	if (wait_for_pressure) {
		/* wait until there's memory pressure */
		while (vm_page_free_count >= vm_page_free_target) {
			wr = assert_wait((event_t) &vm_page_free_wanted,
			    THREAD_INTERRUPTIBLE);
			if (wr == THREAD_WAITING) {
				wr = thread_block(THREAD_CONTINUE_NULL);
			}
			if (wr == THREAD_INTERRUPTED) {
				return KERN_ABORTED;
			}
			if (wr == THREAD_AWAKENED) {
				/*
				 * The memory pressure might have already
				 * been relieved but let's not block again
				 * and let's report that there was memory
				 * pressure at some point.
				 */
				break;
			}
		}
	}

	/* provide the number of pages the system wants to reclaim */
	if (pages_wanted_p != NULL) {
		*pages_wanted_p = mach_vm_ctl_page_free_wanted();
	}

	if (pages_reclaimed_p == NULL) {
		return KERN_SUCCESS;
	}

	/* provide number of pages reclaimed in the last "nsecs_monitored" */
	vm_pageout_now = vm_pageout_stat_now;
	pages_reclaimed = 0;
	for (vm_pageout_then =
	    VM_PAGEOUT_STAT_BEFORE(vm_pageout_now);
	    vm_pageout_then != vm_pageout_now &&
	    units_of_monitor-- != 0;
	    vm_pageout_then =
	    VM_PAGEOUT_STAT_BEFORE(vm_pageout_then)) {
		pages_reclaimed += vm_pageout_stats[vm_pageout_then].freed_speculative;
		pages_reclaimed += vm_pageout_stats[vm_pageout_then].freed_cleaned;
		pages_reclaimed += vm_pageout_stats[vm_pageout_then].freed_internal;
		pages_reclaimed += vm_pageout_stats[vm_pageout_then].freed_external;
	}
	*pages_reclaimed_p = pages_reclaimed;

	return KERN_SUCCESS;
}



#if DEVELOPMENT || DEBUG

static void
vm_pageout_disconnect_all_pages_in_queue(vm_page_queue_head_t *, int);

/*
 * condition variable used to make sure there is
 * only a single sweep going on at a time
 */
bool vm_pageout_disconnect_all_pages_active = false;

void
vm_pageout_disconnect_all_pages()
{
	vm_page_lock_queues();

	if (vm_pageout_disconnect_all_pages_active) {
		vm_page_unlock_queues();
		return;
	}
	vm_pageout_disconnect_all_pages_active = true;

	vm_pageout_disconnect_all_pages_in_queue(&vm_page_queue_throttled,
	    vm_page_throttled_count);
	vm_pageout_disconnect_all_pages_in_queue(&vm_page_queue_anonymous,
	    vm_page_anonymous_count);
	vm_pageout_disconnect_all_pages_in_queue(&vm_page_queue_inactive,
	    (vm_page_inactive_count - vm_page_anonymous_count));
	vm_pageout_disconnect_all_pages_in_queue(&vm_page_queue_active,
	    vm_page_active_count);
#ifdef CONFIG_SECLUDED_MEMORY
	vm_pageout_disconnect_all_pages_in_queue(&vm_page_queue_secluded,
	    vm_page_secluded_count);
#endif /* CONFIG_SECLUDED_MEMORY */
	vm_page_unlock_queues();

	vm_pageout_disconnect_all_pages_active = false;
}

/* NB: assumes the page_queues lock is held on entry, returns with page queue lock held */
void
vm_pageout_disconnect_all_pages_in_queue(vm_page_queue_head_t *q, int qcount)
{
	vm_page_t       m;
	vm_object_t     t_object = NULL;
	vm_object_t     l_object = NULL;
	vm_object_t     m_object = NULL;
	int             delayed_unlock = 0;
	int             try_failed_count = 0;
	int             disconnected_count = 0;
	int             paused_count = 0;
	int             object_locked_count = 0;

	KDBG((MACHDBG_CODE(DBG_MACH_WORKINGSET, VM_DISCONNECT_ALL_PAGE_MAPPINGS) |
	    DBG_FUNC_START),
	    q, qcount);

	while (qcount && !vm_page_queue_empty(q)) {
		LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);

		m = (vm_page_t) vm_page_queue_first(q);
		m_object = VM_PAGE_OBJECT(m);

		if (m_object == VM_OBJECT_NULL) {
			/*
			 * Bumped into a free page. This should only happen on the
			 * secluded queue
			 */
#if CONFIG_SECLUDED_MEMORY
			assert(q == &vm_page_queue_secluded);
#endif /* CONFIG_SECLUDED_MEMORY */
			goto reenter_pg_on_q;
		}

		/*
		 * check to see if we currently are working
		 * with the same object... if so, we've
		 * already got the lock
		 */
		if (m_object != l_object) {
			/*
			 * the object associated with candidate page is
			 * different from the one we were just working
			 * with... dump the lock if we still own it
			 */
			if (l_object != NULL) {
				vm_object_unlock(l_object);
				l_object = NULL;
			}
			if (m_object != t_object) {
				try_failed_count = 0;
			}

			/*
			 * Try to lock object; since we've alread got the
			 * page queues lock, we can only 'try' for this one.
			 * if the 'try' fails, we need to do a mutex_pause
			 * to allow the owner of the object lock a chance to
			 * run...
			 */
			if (!vm_object_lock_try_scan(m_object)) {
				if (try_failed_count > 20) {
					goto reenter_pg_on_q;
				}
				vm_page_unlock_queues();
				mutex_pause(try_failed_count++);
				vm_page_lock_queues();
				delayed_unlock = 0;

				paused_count++;

				t_object = m_object;
				continue;
			}
			object_locked_count++;

			l_object = m_object;
		}
		if (!m_object->alive || m->vmp_cleaning || m->vmp_laundry ||
		    m->vmp_busy || m->vmp_absent || VMP_ERROR_GET(m) ||
		    m->vmp_free_when_done) {
			/*
			 * put it back on the head of its queue
			 */
			goto reenter_pg_on_q;
		}
		if (m->vmp_pmapped == TRUE) {
			pmap_disconnect(VM_PAGE_GET_PHYS_PAGE(m));

			disconnected_count++;
		}
reenter_pg_on_q:
		vm_page_queue_remove(q, m, vmp_pageq);
		vm_page_queue_enter(q, m, vmp_pageq);

		qcount--;
		try_failed_count = 0;

		if (delayed_unlock++ > 128) {
			if (l_object != NULL) {
				vm_object_unlock(l_object);
				l_object = NULL;
			}
			lck_mtx_yield(&vm_page_queue_lock);
			delayed_unlock = 0;
		}
	}
	if (l_object != NULL) {
		vm_object_unlock(l_object);
		l_object = NULL;
	}

	KDBG((MACHDBG_CODE(DBG_MACH_WORKINGSET, VM_DISCONNECT_ALL_PAGE_MAPPINGS) |
	    DBG_FUNC_END),
	    q, disconnected_count, object_locked_count, paused_count);
}

extern const char *proc_best_name(struct proc* proc);

int
vm_toggle_task_selfdonate_pages(task_t task)
{
	int state = 0;
	if (vm_page_donate_mode == VM_PAGE_DONATE_DISABLED) {
		printf("VM Donation mode is OFF on the system\n");
		return state;
	}
	if (task != kernel_task) {
		task_lock(task);
		if (!task->donates_own_pages) {
			printf("SELF DONATE for %s ON\n", proc_best_name(get_bsdtask_info(task)));
			task->donates_own_pages = true;
			state = 1;
		} else if (task->donates_own_pages) {
			printf("SELF DONATE for %s OFF\n", proc_best_name(get_bsdtask_info(task)));
			task->donates_own_pages = false;
			state = 0;
		}
		task_unlock(task);
	}
	return state;
}
#endif /* DEVELOPMENT || DEBUG */

void
vm_task_set_selfdonate_pages(task_t task, bool donate)
{
	assert(vm_page_donate_mode != VM_PAGE_DONATE_DISABLED);
	assert(task != kernel_task);

	task_lock(task);
	task->donates_own_pages = donate;
	task_unlock(task);
}



static size_t
vm_pageout_page_queue(vm_page_queue_head_t *, size_t, bool);

/*
 * condition variable used to make sure there is
 * only a single sweep going on at a time
 */
boolean_t       vm_pageout_anonymous_pages_active = FALSE;


kern_return_t
vm_pageout_anonymous_pages()
{
	if (VM_CONFIG_COMPRESSOR_IS_PRESENT) {
		size_t throttled_pages_moved, anonymous_pages_moved, active_pages_moved;
		vm_page_lock_queues();

		if (vm_pageout_anonymous_pages_active == TRUE) {
			vm_page_unlock_queues();
			return KERN_RESOURCE_SHORTAGE;
		}
		vm_pageout_anonymous_pages_active = TRUE;
		vm_page_unlock_queues();

		throttled_pages_moved = vm_pageout_page_queue(&vm_page_queue_throttled, vm_page_throttled_count, false);
		anonymous_pages_moved = vm_pageout_page_queue(&vm_page_queue_anonymous, vm_page_anonymous_count, false);
		active_pages_moved = vm_pageout_page_queue(&vm_page_queue_active, vm_page_active_count, false);

		os_log(OS_LOG_DEFAULT,
		    "%s: throttled pages moved: %zu, anonymous pages moved: %zu, active pages moved: %zu",
		    __func__, throttled_pages_moved, anonymous_pages_moved, active_pages_moved);

		if (VM_CONFIG_SWAP_IS_PRESENT) {
			vm_consider_swapping();
		}

		vm_page_lock_queues();
		vm_pageout_anonymous_pages_active = FALSE;
		vm_page_unlock_queues();
		return KERN_SUCCESS;
	} else {
		return KERN_NOT_SUPPORTED;
	}
}


size_t
vm_pageout_page_queue(vm_page_queue_head_t *q, size_t qcount, bool perf_test)
{
	vm_page_t       m;
	vm_object_t     t_object = NULL;
	vm_object_t     l_object = NULL;
	vm_object_t     m_object = NULL;
	int             delayed_unlock = 0;
	int             try_failed_count = 0;
	int             refmod_state;
	int             pmap_options;
	struct          vm_pageout_queue *iq;
	ppnum_t         phys_page;
	size_t          pages_moved = 0;


	iq = &vm_pageout_queue_internal;

	vm_page_lock_queues();

#if DEVELOPMENT || DEBUG
	if (perf_test) {
		iq = &vm_pageout_queue_benchmark;
		// ensure the benchmark queue isn't throttled
		iq->pgo_maxlaundry = (unsigned int) qcount;
	}
#endif /* DEVELOPMENT ||DEBUG */

	while (qcount && !vm_page_queue_empty(q)) {
		LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);

		if (VM_PAGE_Q_THROTTLED(iq)) {
			if (l_object != NULL) {
				vm_object_unlock(l_object);
				l_object = NULL;
			}
			iq->pgo_draining = TRUE;

			assert_wait((event_t) (&iq->pgo_laundry + 1), THREAD_INTERRUPTIBLE);
			vm_page_unlock_queues();

			thread_block(THREAD_CONTINUE_NULL);

			vm_page_lock_queues();
			delayed_unlock = 0;
			continue;
		}
		m = (vm_page_t) vm_page_queue_first(q);
		m_object = VM_PAGE_OBJECT(m);

		/*
		 * check to see if we currently are working
		 * with the same object... if so, we've
		 * already got the lock
		 */
		if (m_object != l_object) {
			if (!m_object->internal) {
				goto reenter_pg_on_q;
			}

			/*
			 * the object associated with candidate page is
			 * different from the one we were just working
			 * with... dump the lock if we still own it
			 */
			if (l_object != NULL) {
				vm_object_unlock(l_object);
				l_object = NULL;
			}
			if (m_object != t_object) {
				try_failed_count = 0;
			}

			/*
			 * Try to lock object; since we've alread got the
			 * page queues lock, we can only 'try' for this one.
			 * if the 'try' fails, we need to do a mutex_pause
			 * to allow the owner of the object lock a chance to
			 * run...
			 */
			if (!vm_object_lock_try_scan(m_object)) {
				if (try_failed_count > 20) {
					goto reenter_pg_on_q;
				}
				vm_page_unlock_queues();
				mutex_pause(try_failed_count++);
				vm_page_lock_queues();
				delayed_unlock = 0;

				t_object = m_object;
				continue;
			}
			l_object = m_object;
		}
		if (!m_object->alive || m->vmp_cleaning || m->vmp_laundry || m->vmp_busy || m->vmp_absent || VMP_ERROR_GET(m) || m->vmp_free_when_done) {
			/*
			 * page is not to be cleaned
			 * put it back on the head of its queue
			 */
			goto reenter_pg_on_q;
		}
		phys_page = VM_PAGE_GET_PHYS_PAGE(m);

		if (m->vmp_reference == FALSE && m->vmp_pmapped == TRUE) {
			refmod_state = pmap_get_refmod(phys_page);

			if (refmod_state & VM_MEM_REFERENCED) {
				m->vmp_reference = TRUE;
			}
			if (refmod_state & VM_MEM_MODIFIED) {
				SET_PAGE_DIRTY(m, FALSE);
			}
		}
		if (m->vmp_reference == TRUE) {
			m->vmp_reference = FALSE;
			pmap_clear_refmod_options(phys_page, VM_MEM_REFERENCED, PMAP_OPTIONS_NOFLUSH, (void *)NULL);
			goto reenter_pg_on_q;
		}
		if (m->vmp_pmapped == TRUE) {
			if (m->vmp_dirty || m->vmp_precious) {
				pmap_options = PMAP_OPTIONS_COMPRESSOR;
			} else {
				pmap_options = PMAP_OPTIONS_COMPRESSOR_IFF_MODIFIED;
			}
			refmod_state = pmap_disconnect_options(phys_page, pmap_options, NULL);
			if (refmod_state & VM_MEM_MODIFIED) {
				SET_PAGE_DIRTY(m, FALSE);
			}
		}

		if (!m->vmp_dirty && !m->vmp_precious) {
			vm_page_unlock_queues();
			VM_PAGE_FREE(m);
			vm_page_lock_queues();
			delayed_unlock = 0;

			goto next_pg;
		}
		if (!m_object->pager_initialized || m_object->pager == MEMORY_OBJECT_NULL) {
			if (!m_object->pager_initialized) {
				vm_page_unlock_queues();

				vm_object_collapse(m_object, (vm_object_offset_t) 0, TRUE);

				if (!m_object->pager_initialized) {
					vm_object_compressor_pager_create(m_object);
				}

				vm_page_lock_queues();
				delayed_unlock = 0;
			}
			if (!m_object->pager_initialized || m_object->pager == MEMORY_OBJECT_NULL) {
				goto reenter_pg_on_q;
			}
			/*
			 * vm_object_compressor_pager_create will drop the object lock
			 * which means 'm' may no longer be valid to use
			 */
			continue;
		}

		if (!perf_test) {
			/*
			 * we've already factored out pages in the laundry which
			 * means this page can't be on the pageout queue so it's
			 * safe to do the vm_page_queues_remove
			 */
			bool donate = (m->vmp_on_specialq == VM_PAGE_SPECIAL_Q_DONATE);
			vm_page_queues_remove(m, TRUE);
			if (donate) {
				/*
				 * The compressor needs to see this bit to know
				 * where this page needs to land. Also if stolen,
				 * this bit helps put the page back in the right
				 * special queue where it belongs.
				 */
				m->vmp_on_specialq = VM_PAGE_SPECIAL_Q_DONATE;
			}
		} else {
			vm_page_queue_remove(q, m, vmp_pageq);
		}

		LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);

		vm_pageout_cluster_to_queue(m, iq);

		pages_moved++;
		goto next_pg;

reenter_pg_on_q:
		vm_page_queue_remove(q, m, vmp_pageq);
		vm_page_queue_enter(q, m, vmp_pageq);
next_pg:
		qcount--;
		try_failed_count = 0;

		if (delayed_unlock++ > 128) {
			if (l_object != NULL) {
				vm_object_unlock(l_object);
				l_object = NULL;
			}
			lck_mtx_yield(&vm_page_queue_lock);
			delayed_unlock = 0;
		}
	}
	if (l_object != NULL) {
		vm_object_unlock(l_object);
		l_object = NULL;
	}
	vm_page_unlock_queues();
	return pages_moved;
}



/*
 * function in BSD to apply I/O throttle to the pageout thread
 */
extern void vm_pageout_io_throttle(void);

#define VM_PAGEOUT_SCAN_HANDLE_REUSABLE_PAGE(m, obj)                    \
	MACRO_BEGIN                                                     \
	/* \
	 * If a "reusable" page somehow made it back into \
	 * the active queue, it's been re-used and is not \
	 * quite re-usable. \
	 * If the VM object was "all_reusable", consider it \
	 * as "all re-used" instead of converting it to \
	 * "partially re-used", which could be expensive. \
	 */                                                             \
	assert(VM_PAGE_OBJECT((m)) == (obj));                           \
	if ((m)->vmp_reusable ||                                        \
	    (obj)->all_reusable) {                                      \
	        vm_object_reuse_pages((obj),                            \
	                              (m)->vmp_offset,                  \
	                              (m)->vmp_offset + PAGE_SIZE_64,   \
	                              FALSE);                           \
	}                                                               \
	MACRO_END


#define VM_PAGEOUT_DELAYED_UNLOCK_LIMIT         64
#define VM_PAGEOUT_DELAYED_UNLOCK_LIMIT_MAX     1024

#define FCS_IDLE                0
#define FCS_DELAYED             1
#define FCS_DEADLOCK_DETECTED   2

struct flow_control {
	int             state;
	mach_timespec_t ts;
};


uint64_t vm_pageout_rejected_bq_internal = 0;
uint64_t vm_pageout_rejected_bq_external = 0;
uint64_t vm_pageout_skipped_bq_internal = 0;
uint64_t vm_pageout_skipped_bq_external = 0;

#define ANONS_GRABBED_LIMIT     2


#if 0
static void vm_pageout_delayed_unlock(int *, int *, vm_page_t *);
#endif
static void vm_pageout_prepare_to_block(vm_object_t *, int *, vm_page_t *, int *, int);

#define VM_PAGEOUT_PB_NO_ACTION                         0
#define VM_PAGEOUT_PB_CONSIDER_WAKING_COMPACTOR_SWAPPER 1
#define VM_PAGEOUT_PB_THREAD_YIELD                      2


#if 0
static void
vm_pageout_delayed_unlock(int *delayed_unlock, int *local_freed, vm_page_t *local_freeq)
{
	if (*local_freeq) {
		vm_page_unlock_queues();

		VM_DEBUG_CONSTANT_EVENT(
			vm_pageout_freelist, DBG_VM_PAGEOUT_FREELIST, DBG_FUNC_START,
			vm_page_free_count, 0, 0, 1);

		vm_page_free_list(*local_freeq, TRUE);

		VM_DEBUG_CONSTANT_EVENT(vm_pageout_freelist, DBG_VM_PAGEOUT_FREELIST, DBG_FUNC_END,
		    vm_page_free_count, *local_freed, 0, 1);

		*local_freeq = NULL;
		*local_freed = 0;

		vm_page_lock_queues();
	} else {
		lck_mtx_yield(&vm_page_queue_lock);
	}
	*delayed_unlock = 1;
}
#endif


static void
vm_pageout_prepare_to_block(vm_object_t *object, int *delayed_unlock,
    vm_page_t *local_freeq, int *local_freed, int action)
{
	vm_page_unlock_queues();

	if (*object != NULL) {
		vm_object_unlock(*object);
		*object = NULL;
	}
	if (*local_freeq) {
		vm_page_free_list(*local_freeq, TRUE);

		*local_freeq = NULL;
		*local_freed = 0;
	}
	*delayed_unlock = 1;

	switch (action) {
	case VM_PAGEOUT_PB_CONSIDER_WAKING_COMPACTOR_SWAPPER:
		vm_consider_waking_compactor_swapper();
		break;
	case VM_PAGEOUT_PB_THREAD_YIELD:
		thread_yield_internal(1);
		break;
	case VM_PAGEOUT_PB_NO_ACTION:
	default:
		break;
	}
	vm_page_lock_queues();
}


static struct vm_pageout_vminfo last;

uint64_t last_vm_page_pages_grabbed = 0;

extern  uint32_t c_segment_pages_compressed;

extern uint64_t shared_region_pager_reclaimed;
extern struct memory_object_pager_ops shared_region_pager_ops;

void
update_vm_info(void)
{
	unsigned long tmp;
	uint64_t tmp64;

	vm_pageout_stats[vm_pageout_stat_now].vm_page_active_count = vm_page_active_count;
	vm_pageout_stats[vm_pageout_stat_now].vm_page_speculative_count = vm_page_speculative_count;
	vm_pageout_stats[vm_pageout_stat_now].vm_page_inactive_count = vm_page_inactive_count;
	vm_pageout_stats[vm_pageout_stat_now].vm_page_anonymous_count = vm_page_anonymous_count;

	vm_pageout_stats[vm_pageout_stat_now].vm_page_free_count = vm_page_free_count;
	vm_pageout_stats[vm_pageout_stat_now].vm_page_wire_count = vm_page_wire_count;
	vm_pageout_stats[vm_pageout_stat_now].vm_page_compressor_count = VM_PAGE_COMPRESSOR_COUNT;

	vm_pageout_stats[vm_pageout_stat_now].vm_page_pages_compressed = c_segment_pages_compressed;
	vm_pageout_stats[vm_pageout_stat_now].vm_page_pageable_internal_count = vm_page_pageable_internal_count;
	vm_pageout_stats[vm_pageout_stat_now].vm_page_pageable_external_count = vm_page_pageable_external_count;
	vm_pageout_stats[vm_pageout_stat_now].vm_page_xpmapped_external_count = vm_page_xpmapped_external_count;
	vm_pageout_stats[vm_pageout_stat_now].vm_page_realtime_count = vm_page_realtime_count;

	tmp = vm_pageout_vminfo.vm_pageout_considered_page;
	vm_pageout_stats[vm_pageout_stat_now].considered = (unsigned int)(tmp - last.vm_pageout_considered_page);
	last.vm_pageout_considered_page = tmp;

	tmp64 = vm_pageout_vminfo.vm_pageout_compressions;
	vm_pageout_stats[vm_pageout_stat_now].pages_compressed = (unsigned int)(tmp64 - last.vm_pageout_compressions);
	last.vm_pageout_compressions = tmp64;

	tmp = vm_pageout_vminfo.vm_compressor_failed;
	vm_pageout_stats[vm_pageout_stat_now].failed_compressions = (unsigned int)(tmp - last.vm_compressor_failed);
	last.vm_compressor_failed = tmp;

	tmp64 = vm_pageout_vminfo.vm_compressor_pages_grabbed;
	vm_pageout_stats[vm_pageout_stat_now].pages_grabbed_by_compressor = (unsigned int)(tmp64 - last.vm_compressor_pages_grabbed);
	last.vm_compressor_pages_grabbed = tmp64;

	tmp = vm_pageout_vminfo.vm_phantom_cache_found_ghost;
	vm_pageout_stats[vm_pageout_stat_now].phantom_ghosts_found = (unsigned int)(tmp - last.vm_phantom_cache_found_ghost);
	last.vm_phantom_cache_found_ghost = tmp;

	tmp = vm_pageout_vminfo.vm_phantom_cache_added_ghost;
	vm_pageout_stats[vm_pageout_stat_now].phantom_ghosts_added = (unsigned int)(tmp - last.vm_phantom_cache_added_ghost);
	last.vm_phantom_cache_added_ghost = tmp;

	tmp64 = counter_load(&vm_page_grab_count);
	vm_pageout_stats[vm_pageout_stat_now].pages_grabbed = (unsigned int)(tmp64 - last_vm_page_pages_grabbed);
	last_vm_page_pages_grabbed = tmp64;

	tmp = vm_pageout_vminfo.vm_page_pages_freed;
	vm_pageout_stats[vm_pageout_stat_now].pages_freed = (unsigned int)(tmp - last.vm_page_pages_freed);
	last.vm_page_pages_freed = tmp;

	if (vm_pageout_stats[vm_pageout_stat_now].considered) {
		tmp = vm_pageout_vminfo.vm_pageout_pages_evicted;
		vm_pageout_stats[vm_pageout_stat_now].pages_evicted = (unsigned int)(tmp - last.vm_pageout_pages_evicted);
		last.vm_pageout_pages_evicted = tmp;

		tmp = vm_pageout_vminfo.vm_pageout_pages_purged;
		vm_pageout_stats[vm_pageout_stat_now].pages_purged = (unsigned int)(tmp - last.vm_pageout_pages_purged);
		last.vm_pageout_pages_purged = tmp;

		tmp = vm_pageout_vminfo.vm_pageout_freed_speculative;
		vm_pageout_stats[vm_pageout_stat_now].freed_speculative = (unsigned int)(tmp - last.vm_pageout_freed_speculative);
		last.vm_pageout_freed_speculative = tmp;

		tmp = vm_pageout_vminfo.vm_pageout_freed_external;
		vm_pageout_stats[vm_pageout_stat_now].freed_external = (unsigned int)(tmp - last.vm_pageout_freed_external);
		last.vm_pageout_freed_external = tmp;

		tmp = vm_pageout_vminfo.vm_pageout_inactive_referenced;
		vm_pageout_stats[vm_pageout_stat_now].inactive_referenced = (unsigned int)(tmp - last.vm_pageout_inactive_referenced);
		last.vm_pageout_inactive_referenced = tmp;

		tmp = vm_pageout_vminfo.vm_pageout_scan_inactive_throttled_external;
		vm_pageout_stats[vm_pageout_stat_now].throttled_external_q = (unsigned int)(tmp - last.vm_pageout_scan_inactive_throttled_external);
		last.vm_pageout_scan_inactive_throttled_external = tmp;

		tmp = vm_pageout_vminfo.vm_pageout_inactive_dirty_external;
		vm_pageout_stats[vm_pageout_stat_now].cleaned_dirty_external = (unsigned int)(tmp - last.vm_pageout_inactive_dirty_external);
		last.vm_pageout_inactive_dirty_external = tmp;

		tmp = vm_pageout_vminfo.vm_pageout_freed_cleaned;
		vm_pageout_stats[vm_pageout_stat_now].freed_cleaned = (unsigned int)(tmp - last.vm_pageout_freed_cleaned);
		last.vm_pageout_freed_cleaned = tmp;

		tmp = vm_pageout_vminfo.vm_pageout_inactive_nolock;
		vm_pageout_stats[vm_pageout_stat_now].inactive_nolock = (unsigned int)(tmp - last.vm_pageout_inactive_nolock);
		last.vm_pageout_inactive_nolock = tmp;

		tmp = vm_pageout_vminfo.vm_pageout_scan_inactive_throttled_internal;
		vm_pageout_stats[vm_pageout_stat_now].throttled_internal_q = (unsigned int)(tmp - last.vm_pageout_scan_inactive_throttled_internal);
		last.vm_pageout_scan_inactive_throttled_internal = tmp;

		tmp = vm_pageout_vminfo.vm_pageout_skipped_external;
		vm_pageout_stats[vm_pageout_stat_now].skipped_external = (unsigned int)(tmp - last.vm_pageout_skipped_external);
		last.vm_pageout_skipped_external = tmp;

		tmp = vm_pageout_vminfo.vm_pageout_skipped_internal;
		vm_pageout_stats[vm_pageout_stat_now].skipped_internal = (unsigned int)(tmp - last.vm_pageout_skipped_internal);
		last.vm_pageout_skipped_internal = tmp;

		tmp = vm_pageout_vminfo.vm_pageout_reactivation_limit_exceeded;
		vm_pageout_stats[vm_pageout_stat_now].reactivation_limit_exceeded = (unsigned int)(tmp - last.vm_pageout_reactivation_limit_exceeded);
		last.vm_pageout_reactivation_limit_exceeded = tmp;

		tmp = vm_pageout_vminfo.vm_pageout_inactive_force_reclaim;
		vm_pageout_stats[vm_pageout_stat_now].forced_inactive_reclaim = (unsigned int)(tmp - last.vm_pageout_inactive_force_reclaim);
		last.vm_pageout_inactive_force_reclaim = tmp;

		tmp = vm_pageout_vminfo.vm_pageout_freed_internal;
		vm_pageout_stats[vm_pageout_stat_now].freed_internal = (unsigned int)(tmp - last.vm_pageout_freed_internal);
		last.vm_pageout_freed_internal = tmp;

		tmp = vm_pageout_vminfo.vm_pageout_considered_bq_internal;
		vm_pageout_stats[vm_pageout_stat_now].considered_bq_internal = (unsigned int)(tmp - last.vm_pageout_considered_bq_internal);
		last.vm_pageout_considered_bq_internal = tmp;

		tmp = vm_pageout_vminfo.vm_pageout_considered_bq_external;
		vm_pageout_stats[vm_pageout_stat_now].considered_bq_external = (unsigned int)(tmp - last.vm_pageout_considered_bq_external);
		last.vm_pageout_considered_bq_external = tmp;

		tmp = vm_pageout_vminfo.vm_pageout_filecache_min_reactivated;
		vm_pageout_stats[vm_pageout_stat_now].filecache_min_reactivations = (unsigned int)(tmp - last.vm_pageout_filecache_min_reactivated);
		last.vm_pageout_filecache_min_reactivated = tmp;

		tmp = vm_pageout_vminfo.vm_pageout_inactive_dirty_internal;
		vm_pageout_stats[vm_pageout_stat_now].cleaned_dirty_internal = (unsigned int)(tmp - last.vm_pageout_inactive_dirty_internal);
		last.vm_pageout_inactive_dirty_internal = tmp;

		tmp = vm_pageout_vminfo.vm_pageout_forcereclaimed_sharedcache;
		vm_pageout_stats[vm_pageout_stat_now].forcereclaimed_sharedcache = (unsigned int)(tmp - last.vm_pageout_forcereclaimed_sharedcache);
		last.vm_pageout_forcereclaimed_sharedcache = tmp;

		tmp = vm_pageout_vminfo.vm_pageout_forcereclaimed_realtime;
		vm_pageout_stats[vm_pageout_stat_now].forcereclaimed_realtime = (unsigned int)(tmp - last.vm_pageout_forcereclaimed_realtime);
		last.vm_pageout_forcereclaimed_realtime = tmp;

		tmp = vm_pageout_vminfo.vm_pageout_protected_sharedcache;
		vm_pageout_stats[vm_pageout_stat_now].protected_sharedcache = (unsigned int)(tmp - last.vm_pageout_protected_sharedcache);
		last.vm_pageout_protected_sharedcache = tmp;

		tmp = vm_pageout_vminfo.vm_pageout_protected_realtime;
		vm_pageout_stats[vm_pageout_stat_now].protected_realtime = (unsigned int)(tmp - last.vm_pageout_protected_realtime);
		last.vm_pageout_protected_realtime = tmp;
	}

	KDBG((VMDBG_CODE(DBG_VM_INFO1)) | DBG_FUNC_NONE,
	    vm_pageout_stats[vm_pageout_stat_now].vm_page_active_count,
	    vm_pageout_stats[vm_pageout_stat_now].vm_page_speculative_count,
	    vm_pageout_stats[vm_pageout_stat_now].vm_page_inactive_count,
	    vm_pageout_stats[vm_pageout_stat_now].vm_page_anonymous_count);

	KDBG((VMDBG_CODE(DBG_VM_INFO2)) | DBG_FUNC_NONE,
	    vm_pageout_stats[vm_pageout_stat_now].vm_page_free_count,
	    vm_pageout_stats[vm_pageout_stat_now].vm_page_wire_count,
	    vm_pageout_stats[vm_pageout_stat_now].vm_page_compressor_count);

	KDBG((VMDBG_CODE(DBG_VM_INFO3)) | DBG_FUNC_NONE,
	    vm_pageout_stats[vm_pageout_stat_now].vm_page_pages_compressed,
	    vm_pageout_stats[vm_pageout_stat_now].vm_page_pageable_internal_count,
	    vm_pageout_stats[vm_pageout_stat_now].vm_page_pageable_external_count,
	    vm_pageout_stats[vm_pageout_stat_now].vm_page_xpmapped_external_count);

	if (vm_pageout_stats[vm_pageout_stat_now].considered ||
	    vm_pageout_stats[vm_pageout_stat_now].pages_compressed ||
	    vm_pageout_stats[vm_pageout_stat_now].failed_compressions) {
		KDBG((VMDBG_CODE(DBG_VM_INFO4)) | DBG_FUNC_NONE,
		    vm_pageout_stats[vm_pageout_stat_now].considered,
		    vm_pageout_stats[vm_pageout_stat_now].freed_speculative,
		    vm_pageout_stats[vm_pageout_stat_now].freed_external,
		    vm_pageout_stats[vm_pageout_stat_now].inactive_referenced);

		KDBG((VMDBG_CODE(DBG_VM_INFO5)) | DBG_FUNC_NONE,
		    vm_pageout_stats[vm_pageout_stat_now].throttled_external_q,
		    vm_pageout_stats[vm_pageout_stat_now].cleaned_dirty_external,
		    vm_pageout_stats[vm_pageout_stat_now].freed_cleaned,
		    vm_pageout_stats[vm_pageout_stat_now].inactive_nolock);

		KDBG((VMDBG_CODE(DBG_VM_INFO6)) | DBG_FUNC_NONE,
		    vm_pageout_stats[vm_pageout_stat_now].throttled_internal_q,
		    vm_pageout_stats[vm_pageout_stat_now].pages_compressed,
		    vm_pageout_stats[vm_pageout_stat_now].pages_grabbed_by_compressor,
		    vm_pageout_stats[vm_pageout_stat_now].skipped_external);

		KDBG((VMDBG_CODE(DBG_VM_INFO7)) | DBG_FUNC_NONE,
		    vm_pageout_stats[vm_pageout_stat_now].reactivation_limit_exceeded,
		    vm_pageout_stats[vm_pageout_stat_now].forced_inactive_reclaim,
		    vm_pageout_stats[vm_pageout_stat_now].failed_compressions,
		    vm_pageout_stats[vm_pageout_stat_now].freed_internal);

		KDBG((VMDBG_CODE(DBG_VM_INFO8)) | DBG_FUNC_NONE,
		    vm_pageout_stats[vm_pageout_stat_now].considered_bq_internal,
		    vm_pageout_stats[vm_pageout_stat_now].considered_bq_external,
		    vm_pageout_stats[vm_pageout_stat_now].filecache_min_reactivations,
		    vm_pageout_stats[vm_pageout_stat_now].cleaned_dirty_internal);

		KDBG((VMDBG_CODE(DBG_VM_INFO10)) | DBG_FUNC_NONE,
		    vm_pageout_stats[vm_pageout_stat_now].forcereclaimed_sharedcache,
		    vm_pageout_stats[vm_pageout_stat_now].forcereclaimed_realtime,
		    vm_pageout_stats[vm_pageout_stat_now].protected_sharedcache,
		    vm_pageout_stats[vm_pageout_stat_now].protected_realtime);
	}
	KDBG((VMDBG_CODE(DBG_VM_INFO9)) | DBG_FUNC_NONE,
	    vm_pageout_stats[vm_pageout_stat_now].pages_grabbed,
	    vm_pageout_stats[vm_pageout_stat_now].pages_freed,
	    vm_pageout_stats[vm_pageout_stat_now].phantom_ghosts_found,
	    vm_pageout_stats[vm_pageout_stat_now].phantom_ghosts_added);

	record_memory_pressure();
}

extern boolean_t hibernation_vmqueues_inspection;

/*
 * Return values for functions called by vm_pageout_scan
 * that control its flow.
 *
 * PROCEED -- vm_pageout_scan will keep making forward progress.
 * DONE_RETURN -- page demand satisfied, work is done -> vm_pageout_scan returns.
 * NEXT_ITERATION -- restart the 'for' loop in vm_pageout_scan aka continue.
 */

#define VM_PAGEOUT_SCAN_PROCEED                 (0)
#define VM_PAGEOUT_SCAN_DONE_RETURN             (1)
#define VM_PAGEOUT_SCAN_NEXT_ITERATION          (2)

/*
 * This function is called only from vm_pageout_scan and
 * it moves overflow secluded pages (one-at-a-time) to the
 * batched 'local' free Q or active Q.
 */
static void
vps_deal_with_secluded_page_overflow(vm_page_t *local_freeq, int *local_freed)
{
#if CONFIG_SECLUDED_MEMORY
	/*
	 * Deal with secluded_q overflow.
	 */
	if (vm_page_secluded_count > vm_page_secluded_target) {
		vm_page_t secluded_page;

		/*
		 * SECLUDED_AGING_BEFORE_ACTIVE:
		 * Excess secluded pages go to the active queue and
		 * will later go to the inactive queue.
		 */
		assert((vm_page_secluded_count_free +
		    vm_page_secluded_count_inuse) ==
		    vm_page_secluded_count);
		secluded_page = (vm_page_t)vm_page_queue_first(&vm_page_queue_secluded);
		assert(secluded_page->vmp_q_state == VM_PAGE_ON_SECLUDED_Q);

		vm_page_queues_remove(secluded_page, FALSE);
		assert(!secluded_page->vmp_fictitious);
		assert(!VM_PAGE_WIRED(secluded_page));

		if (secluded_page->vmp_object == 0) {
			/* transfer to free queue */
			assert(secluded_page->vmp_busy);
			secluded_page->vmp_snext = *local_freeq;
			*local_freeq = secluded_page;
			*local_freed += 1;
		} else {
			/* transfer to head of active queue */
			vm_page_enqueue_active(secluded_page, FALSE);
			secluded_page = VM_PAGE_NULL;
		}
	}
#else /* CONFIG_SECLUDED_MEMORY */

#pragma unused(local_freeq)
#pragma unused(local_freed)

	return;

#endif /* CONFIG_SECLUDED_MEMORY */
}

/*
 * This function is called only from vm_pageout_scan and
 * it initializes the loop targets for vm_pageout_scan().
 */
static void
vps_init_page_targets(void)
{
	/*
	 * LD TODO: Other page targets should be calculated here too.
	 */
	vm_page_anonymous_min = vm_page_inactive_target / 20;

	if (vm_pageout_state.vm_page_speculative_percentage > 50) {
		vm_pageout_state.vm_page_speculative_percentage = 50;
	} else if (vm_pageout_state.vm_page_speculative_percentage <= 0) {
		vm_pageout_state.vm_page_speculative_percentage = 1;
	}

	vm_pageout_state.vm_page_speculative_target = VM_PAGE_SPECULATIVE_TARGET(vm_page_active_count +
	    vm_page_inactive_count);
}

/*
 * This function is called only from vm_pageout_scan and
 * it purges a single VM object at-a-time and will either
 * make vm_pageout_scan() restart the loop or keeping moving forward.
 */
static int
vps_purge_object()
{
	int             force_purge;

	assert(available_for_purge >= 0);
	force_purge = 0; /* no force-purging */

#if VM_PRESSURE_EVENTS
	vm_pressure_level_t pressure_level;

	pressure_level = memorystatus_vm_pressure_level;

	if (pressure_level > kVMPressureNormal) {
		if (pressure_level >= kVMPressureCritical) {
			force_purge = vm_pageout_state.memorystatus_purge_on_critical;
		} else if (pressure_level >= kVMPressureUrgent) {
			force_purge = vm_pageout_state.memorystatus_purge_on_urgent;
		} else if (pressure_level >= kVMPressureWarning) {
			force_purge = vm_pageout_state.memorystatus_purge_on_warning;
		}
	}
#endif /* VM_PRESSURE_EVENTS */

	if (available_for_purge || force_purge) {
		memoryshot(DBG_VM_PAGEOUT_PURGEONE, DBG_FUNC_START);

		VM_DEBUG_EVENT(vm_pageout_purgeone, DBG_VM_PAGEOUT_PURGEONE, DBG_FUNC_START, vm_page_free_count, 0, 0, 0);
		if (vm_purgeable_object_purge_one(force_purge, C_DONT_BLOCK)) {
			VM_PAGEOUT_DEBUG(vm_pageout_purged_objects, 1);
			VM_DEBUG_EVENT(vm_pageout_purgeone, DBG_VM_PAGEOUT_PURGEONE, DBG_FUNC_END, vm_page_free_count, 0, 0, 0);
			memoryshot(DBG_VM_PAGEOUT_PURGEONE, DBG_FUNC_END);

			return VM_PAGEOUT_SCAN_NEXT_ITERATION;
		}
		VM_DEBUG_EVENT(vm_pageout_purgeone, DBG_VM_PAGEOUT_PURGEONE, DBG_FUNC_END, 0, 0, 0, -1);
		memoryshot(DBG_VM_PAGEOUT_PURGEONE, DBG_FUNC_END);
	}

	return VM_PAGEOUT_SCAN_PROCEED;
}

/*
 * This function is called only from vm_pageout_scan and
 * it will try to age the next speculative Q if the oldest
 * one is empty.
 */
static int
vps_age_speculative_queue(boolean_t force_speculative_aging)
{
#define DELAY_SPECULATIVE_AGE   1000

	/*
	 * try to pull pages from the aging bins...
	 * see vm_page_internal.h for an explanation of how
	 * this mechanism works
	 */
	boolean_t                       can_steal = FALSE;
	int                             num_scanned_queues;
	static int                      delay_speculative_age = 0; /* depends the # of times we go through the main pageout_scan loop.*/
	mach_timespec_t                 ts;
	struct vm_speculative_age_q     *aq;
	struct vm_speculative_age_q     *sq;

	sq = &vm_page_queue_speculative[VM_PAGE_SPECULATIVE_AGED_Q];

	aq = &vm_page_queue_speculative[speculative_steal_index];

	num_scanned_queues = 0;
	while (vm_page_queue_empty(&aq->age_q) &&
	    num_scanned_queues++ != vm_page_max_speculative_age_q) {
		speculative_steal_index++;

		if (speculative_steal_index > vm_page_max_speculative_age_q) {
			speculative_steal_index = VM_PAGE_MIN_SPECULATIVE_AGE_Q;
		}

		aq = &vm_page_queue_speculative[speculative_steal_index];
	}

	if (num_scanned_queues == vm_page_max_speculative_age_q + 1) {
		/*
		 * XXX We've scanned all the speculative
		 * queues but still haven't found one
		 * that is not empty, even though
		 * vm_page_speculative_count is not 0.
		 */
		if (!vm_page_queue_empty(&sq->age_q)) {
			return VM_PAGEOUT_SCAN_NEXT_ITERATION;
		}
#if DEVELOPMENT || DEBUG
		panic("vm_pageout_scan: vm_page_speculative_count=%d but queues are empty", vm_page_speculative_count);
#endif
		/* readjust... */
		vm_page_speculative_count = 0;
		/* ... and continue */
		return VM_PAGEOUT_SCAN_NEXT_ITERATION;
	}

	if (vm_page_speculative_count > vm_pageout_state.vm_page_speculative_target || force_speculative_aging == TRUE) {
		can_steal = TRUE;
	} else {
		if (!delay_speculative_age) {
			mach_timespec_t ts_fully_aged;

			ts_fully_aged.tv_sec = (vm_page_max_speculative_age_q * vm_pageout_state.vm_page_speculative_q_age_ms) / 1000;
			ts_fully_aged.tv_nsec = ((vm_page_max_speculative_age_q * vm_pageout_state.vm_page_speculative_q_age_ms) % 1000)
			    * 1000 * NSEC_PER_USEC;

			ADD_MACH_TIMESPEC(&ts_fully_aged, &aq->age_ts);

			clock_sec_t sec;
			clock_nsec_t nsec;
			clock_get_system_nanotime(&sec, &nsec);
			ts.tv_sec = (unsigned int) sec;
			ts.tv_nsec = nsec;

			if (CMP_MACH_TIMESPEC(&ts, &ts_fully_aged) >= 0) {
				can_steal = TRUE;
			} else {
				delay_speculative_age++;
			}
		} else {
			delay_speculative_age++;
			if (delay_speculative_age == DELAY_SPECULATIVE_AGE) {
				delay_speculative_age = 0;
			}
		}
	}
	if (can_steal == TRUE) {
		vm_page_speculate_ageit(aq);
	}

	return VM_PAGEOUT_SCAN_PROCEED;
}

/*
 * This function is called only from vm_pageout_scan and
 * it evicts a single VM object from the cache.
 */
static int inline
vps_object_cache_evict(vm_object_t *object_to_unlock)
{
	static int                      cache_evict_throttle = 0;
	struct vm_speculative_age_q     *sq;

	sq = &vm_page_queue_speculative[VM_PAGE_SPECULATIVE_AGED_Q];

	if (vm_page_queue_empty(&sq->age_q) && cache_evict_throttle == 0) {
		int     pages_evicted;

		if (*object_to_unlock != NULL) {
			vm_object_unlock(*object_to_unlock);
			*object_to_unlock = NULL;
		}
		KDBG(0x13001ec | DBG_FUNC_START);

		pages_evicted = vm_object_cache_evict(100, 10);

		KDBG(0x13001ec | DBG_FUNC_END, pages_evicted);

		if (pages_evicted) {
			vm_pageout_vminfo.vm_pageout_pages_evicted += pages_evicted;

			VM_DEBUG_EVENT(vm_pageout_cache_evict, DBG_VM_PAGEOUT_CACHE_EVICT, DBG_FUNC_NONE,
			    vm_page_free_count, pages_evicted, vm_pageout_vminfo.vm_pageout_pages_evicted, 0);
			memoryshot(DBG_VM_PAGEOUT_CACHE_EVICT, DBG_FUNC_NONE);

			/*
			 * we just freed up to 100 pages,
			 * so go back to the top of the main loop
			 * and re-evaulate the memory situation
			 */
			return VM_PAGEOUT_SCAN_NEXT_ITERATION;
		} else {
			cache_evict_throttle = 1000;
		}
	}
	if (cache_evict_throttle) {
		cache_evict_throttle--;
	}

	return VM_PAGEOUT_SCAN_PROCEED;
}


/*
 * This function is called only from vm_pageout_scan and
 * it calculates the filecache min. that needs to be maintained
 * as we start to steal pages.
 */
static void
vps_calculate_filecache_min(void)
{
	int divisor = vm_pageout_state.vm_page_filecache_min_divisor;

#if CONFIG_JETSAM
	/*
	 * don't let the filecache_min fall below 15% of available memory
	 * on systems with an active compressor that isn't nearing its
	 * limits w/r to accepting new data
	 *
	 * on systems w/o the compressor/swapper, the filecache is always
	 * a very large percentage of the AVAILABLE_NON_COMPRESSED_MEMORY
	 * since most (if not all) of the anonymous pages are in the
	 * throttled queue (which isn't counted as available) which
	 * effectively disables this filter
	 */
	if (vm_compressor_low_on_space() || divisor == 0) {
		vm_pageout_state.vm_page_filecache_min = 0;
	} else {
		vm_pageout_state.vm_page_filecache_min =
		    ((AVAILABLE_NON_COMPRESSED_MEMORY) * 10) / divisor;
	}
#else
	if (vm_compressor_out_of_space() || divisor == 0) {
		vm_pageout_state.vm_page_filecache_min = 0;
	} else {
		/*
		 * don't let the filecache_min fall below the specified critical level
		 */
		vm_pageout_state.vm_page_filecache_min =
		    ((AVAILABLE_NON_COMPRESSED_MEMORY) * 10) / divisor;
	}
#endif
	if (vm_page_free_count < (vm_page_free_reserved / 4)) {
		vm_pageout_state.vm_page_filecache_min = 0;
	}
}

/*
 * This function is called only from vm_pageout_scan and
 * it updates the flow control time to detect if VM pageoutscan
 * isn't making progress.
 */
static void
vps_flow_control_reset_deadlock_timer(struct flow_control *flow_control)
{
	mach_timespec_t ts;
	clock_sec_t sec;
	clock_nsec_t nsec;

	ts.tv_sec = vm_pageout_state.vm_pageout_deadlock_wait / 1000;
	ts.tv_nsec = (vm_pageout_state.vm_pageout_deadlock_wait % 1000) * 1000 * NSEC_PER_USEC;
	clock_get_system_nanotime(&sec, &nsec);
	flow_control->ts.tv_sec = (unsigned int) sec;
	flow_control->ts.tv_nsec = nsec;
	ADD_MACH_TIMESPEC(&flow_control->ts, &ts);

	flow_control->state = FCS_DELAYED;

	vm_pageout_vminfo.vm_pageout_scan_inactive_throttled_internal++;
}

/*
 * This function is called only from vm_pageout_scan and
 * it is the flow control logic of VM pageout scan which
 * controls if it should block and for how long.
 * Any blocking of vm_pageout_scan happens ONLY in this function.
 */
static int
vps_flow_control(struct flow_control *flow_control, int *anons_grabbed, vm_object_t *object, int *delayed_unlock,
    vm_page_t *local_freeq, int *local_freed, int *vm_pageout_deadlock_target, unsigned int inactive_burst_count)
{
	boolean_t       exceeded_burst_throttle = FALSE;
	unsigned int    msecs = 0;
	uint32_t        inactive_external_count;
	mach_timespec_t ts;
	struct  vm_pageout_queue *iq;
	struct  vm_pageout_queue *eq;
	struct  vm_speculative_age_q *sq;

	iq = &vm_pageout_queue_internal;
	eq = &vm_pageout_queue_external;
	sq = &vm_page_queue_speculative[VM_PAGE_SPECULATIVE_AGED_Q];

	/*
	 * Sometimes we have to pause:
	 *	1) No inactive pages - nothing to do.
	 *	2) Loop control - no acceptable pages found on the inactive queue
	 *         within the last vm_pageout_burst_inactive_throttle iterations
	 *	3) Flow control - default pageout queue is full
	 */
	if (vm_page_queue_empty(&vm_page_queue_inactive) &&
	    vm_page_queue_empty(&vm_page_queue_anonymous) &&
	    vm_page_queue_empty(&vm_page_queue_cleaned) &&
	    vm_page_queue_empty(&sq->age_q)) {
		VM_PAGEOUT_DEBUG(vm_pageout_scan_empty_throttle, 1);
		msecs = vm_pageout_state.vm_pageout_empty_wait;
	} else if (inactive_burst_count >=
	    MIN(vm_pageout_state.vm_pageout_burst_inactive_throttle,
	    (vm_page_inactive_count +
	    vm_page_speculative_count))) {
		VM_PAGEOUT_DEBUG(vm_pageout_scan_burst_throttle, 1);
		msecs = vm_pageout_state.vm_pageout_burst_wait;

		exceeded_burst_throttle = TRUE;
	} else if (VM_PAGE_Q_THROTTLED(iq) &&
	    VM_DYNAMIC_PAGING_ENABLED()) {
		clock_sec_t sec;
		clock_nsec_t nsec;

		switch (flow_control->state) {
		case FCS_IDLE:
			if ((vm_page_free_count + *local_freed) < vm_page_free_target &&
			    vm_pageout_state.vm_restricted_to_single_processor == FALSE) {
				/*
				 * since the compressor is running independently of vm_pageout_scan
				 * let's not wait for it just yet... as long as we have a healthy supply
				 * of filecache pages to work with, let's keep stealing those.
				 */
				inactive_external_count = vm_page_inactive_count - vm_page_anonymous_count;

				if (vm_page_pageable_external_count > vm_pageout_state.vm_page_filecache_min &&
				    (inactive_external_count >= VM_PAGE_INACTIVE_TARGET(vm_page_pageable_external_count))) {
					*anons_grabbed = ANONS_GRABBED_LIMIT;
					VM_PAGEOUT_DEBUG(vm_pageout_scan_throttle_deferred, 1);
					return VM_PAGEOUT_SCAN_PROCEED;
				}
			}

			vps_flow_control_reset_deadlock_timer(flow_control);
			msecs = vm_pageout_state.vm_pageout_deadlock_wait;

			break;

		case FCS_DELAYED:
			clock_get_system_nanotime(&sec, &nsec);
			ts.tv_sec = (unsigned int) sec;
			ts.tv_nsec = nsec;

			if (CMP_MACH_TIMESPEC(&ts, &flow_control->ts) >= 0) {
				/*
				 * the pageout thread for the default pager is potentially
				 * deadlocked since the
				 * default pager queue has been throttled for more than the
				 * allowable time... we need to move some clean pages or dirty
				 * pages belonging to the external pagers if they aren't throttled
				 * vm_page_free_wanted represents the number of threads currently
				 * blocked waiting for pages... we'll move one page for each of
				 * these plus a fixed amount to break the logjam... once we're done
				 * moving this number of pages, we'll re-enter the FSC_DELAYED state
				 * with a new timeout target since we have no way of knowing
				 * whether we've broken the deadlock except through observation
				 * of the queue associated with the default pager... we need to
				 * stop moving pages and allow the system to run to see what
				 * state it settles into.
				 */

				*vm_pageout_deadlock_target = vm_pageout_state.vm_pageout_deadlock_relief +
				    vm_page_free_wanted + vm_page_free_wanted_privileged;
				VM_PAGEOUT_DEBUG(vm_pageout_scan_deadlock_detected, 1);
				flow_control->state = FCS_DEADLOCK_DETECTED;
				thread_wakeup(VM_PAGEOUT_GC_EVENT);
				return VM_PAGEOUT_SCAN_PROCEED;
			}
			/*
			 * just resniff instead of trying
			 * to compute a new delay time... we're going to be
			 * awakened immediately upon a laundry completion,
			 * so we won't wait any longer than necessary
			 */
			msecs = vm_pageout_state.vm_pageout_idle_wait;
			break;

		case FCS_DEADLOCK_DETECTED:
			if (*vm_pageout_deadlock_target) {
				return VM_PAGEOUT_SCAN_PROCEED;
			}

			vps_flow_control_reset_deadlock_timer(flow_control);
			msecs = vm_pageout_state.vm_pageout_deadlock_wait;

			break;
		}
	} else {
		/*
		 * No need to pause...
		 */
		return VM_PAGEOUT_SCAN_PROCEED;
	}

	vm_pageout_scan_wants_object = VM_OBJECT_NULL;

	vm_pageout_prepare_to_block(object, delayed_unlock, local_freeq, local_freed,
	    VM_PAGEOUT_PB_CONSIDER_WAKING_COMPACTOR_SWAPPER);

	if (vm_page_free_count >= vm_page_free_target) {
		/*
		 * we're here because
		 *  1) someone else freed up some pages while we had
		 *     the queues unlocked above
		 * and we've hit one of the 3 conditions that
		 * cause us to pause the pageout scan thread
		 *
		 * since we already have enough free pages,
		 * let's avoid stalling and return normally
		 *
		 * before we return, make sure the pageout I/O threads
		 * are running throttled in case there are still requests
		 * in the laundry... since we have enough free pages
		 * we don't need the laundry to be cleaned in a timely
		 * fashion... so let's avoid interfering with foreground
		 * activity
		 *
		 * we don't want to hold vm_page_queue_free_lock when
		 * calling vm_pageout_adjust_eq_iothrottle (since it
		 * may cause other locks to be taken), we do the intitial
		 * check outside of the lock.  Once we take the lock,
		 * we recheck the condition since it may have changed.
		 * if it has, no problem, we will make the threads
		 * non-throttled before actually blocking
		 */
		vm_pageout_adjust_eq_iothrottle(&pgo_iothread_external_state, TRUE);
	}
	vm_free_page_lock();

	if (vm_page_free_count >= vm_page_free_target &&
	    (vm_page_free_wanted == 0) && (vm_page_free_wanted_privileged == 0)) {
		return VM_PAGEOUT_SCAN_DONE_RETURN;
	}
	vm_free_page_unlock();

	if ((vm_page_free_count + vm_page_cleaned_count) < vm_page_free_target) {
		/*
		 * we're most likely about to block due to one of
		 * the 3 conditions that cause vm_pageout_scan to
		 * not be able to make forward progress w/r
		 * to providing new pages to the free queue,
		 * so unthrottle the I/O threads in case we
		 * have laundry to be cleaned... it needs
		 * to be completed ASAP.
		 *
		 * even if we don't block, we want the io threads
		 * running unthrottled since the sum of free +
		 * clean pages is still under our free target
		 */
		vm_pageout_adjust_eq_iothrottle(&pgo_iothread_external_state, FALSE);
	}
	if (vm_page_cleaned_count > 0 && exceeded_burst_throttle == FALSE) {
		/*
		 * if we get here we're below our free target and
		 * we're stalling due to a full laundry queue or
		 * we don't have any inactive pages other then
		 * those in the clean queue...
		 * however, we have pages on the clean queue that
		 * can be moved to the free queue, so let's not
		 * stall the pageout scan
		 */
		flow_control->state = FCS_IDLE;
		return VM_PAGEOUT_SCAN_PROCEED;
	}
	if (flow_control->state == FCS_DELAYED && !VM_PAGE_Q_THROTTLED(iq)) {
		flow_control->state = FCS_IDLE;
		return VM_PAGEOUT_SCAN_PROCEED;
	}

	VM_CHECK_MEMORYSTATUS;

	if (flow_control->state != FCS_IDLE) {
		VM_PAGEOUT_DEBUG(vm_pageout_scan_throttle, 1);
	}

	iq->pgo_throttled = TRUE;
	assert_wait_timeout((event_t) &iq->pgo_laundry, THREAD_INTERRUPTIBLE, msecs, 1000 * NSEC_PER_USEC);

	vm_page_unlock_queues();

	assert(vm_pageout_scan_wants_object == VM_OBJECT_NULL);

	VM_DEBUG_EVENT(vm_pageout_thread_block, DBG_VM_PAGEOUT_THREAD_BLOCK, DBG_FUNC_START,
	    iq->pgo_laundry, iq->pgo_maxlaundry, msecs, 0);
	memoryshot(DBG_VM_PAGEOUT_THREAD_BLOCK, DBG_FUNC_START);

	thread_block(THREAD_CONTINUE_NULL);

	VM_DEBUG_EVENT(vm_pageout_thread_block, DBG_VM_PAGEOUT_THREAD_BLOCK, DBG_FUNC_END,
	    iq->pgo_laundry, iq->pgo_maxlaundry, msecs, 0);
	memoryshot(DBG_VM_PAGEOUT_THREAD_BLOCK, DBG_FUNC_END);

	vm_page_lock_queues();

	iq->pgo_throttled = FALSE;

	vps_init_page_targets();

	return VM_PAGEOUT_SCAN_NEXT_ITERATION;
}

extern boolean_t vm_darkwake_mode;
/*
 * This function is called only from vm_pageout_scan and
 * it will find and return the most appropriate page to be
 * reclaimed.
 */
static int
vps_choose_victim_page(vm_page_t *victim_page, int *anons_grabbed, boolean_t *grab_anonymous, boolean_t force_anonymous,
    boolean_t *is_page_from_bg_q, unsigned int *reactivated_this_call)
{
	vm_page_t                       m = NULL;
	vm_object_t                     m_object = VM_OBJECT_NULL;
	uint32_t                        inactive_external_count;
	struct vm_speculative_age_q     *sq;
	struct vm_pageout_queue         *iq;
	int                             retval = VM_PAGEOUT_SCAN_PROCEED;

	sq = &vm_page_queue_speculative[VM_PAGE_SPECULATIVE_AGED_Q];
	iq = &vm_pageout_queue_internal;

	*is_page_from_bg_q = FALSE;

	m = NULL;
	m_object = VM_OBJECT_NULL;

	if (VM_DYNAMIC_PAGING_ENABLED()) {
		assert(vm_page_throttled_count == 0);
		assert(vm_page_queue_empty(&vm_page_queue_throttled));
	}

	/*
	 * Try for a clean-queue inactive page.
	 * These are pages that vm_pageout_scan tried to steal earlier, but
	 * were dirty and had to be cleaned.  Pick them up now that they are clean.
	 */
	if (!vm_page_queue_empty(&vm_page_queue_cleaned)) {
		m = (vm_page_t) vm_page_queue_first(&vm_page_queue_cleaned);

		assert(m->vmp_q_state == VM_PAGE_ON_INACTIVE_CLEANED_Q);

		goto found_page;
	}

	/*
	 * The next most eligible pages are ones we paged in speculatively,
	 * but which have not yet been touched and have been aged out.
	 */
	if (!vm_page_queue_empty(&sq->age_q)) {
		m = (vm_page_t) vm_page_queue_first(&sq->age_q);

		assert(m->vmp_q_state == VM_PAGE_ON_SPECULATIVE_Q);

		if (!m->vmp_dirty || force_anonymous == FALSE) {
			goto found_page;
		} else {
			m = NULL;
		}
	}

#if !CONFIG_JETSAM
	if (vm_page_donate_mode != VM_PAGE_DONATE_DISABLED) {
		if (vm_page_donate_queue_ripe && !vm_page_queue_empty(&vm_page_queue_donate)) {
			m = (vm_page_t) vm_page_queue_first(&vm_page_queue_donate);
			assert(m->vmp_on_specialq == VM_PAGE_SPECIAL_Q_DONATE);
			goto found_page;
		}
	}
#endif /* !CONFIG_JETSAM */

	if (vm_page_background_mode != VM_PAGE_BG_DISABLED && (vm_page_background_count > vm_page_background_target)) {
		vm_object_t     bg_m_object = NULL;

		m = (vm_page_t) vm_page_queue_first(&vm_page_queue_background);

		bg_m_object = VM_PAGE_OBJECT(m);

		if (!VM_PAGE_PAGEABLE(m) || (vm_darkwake_mode && m->vmp_busy)) {
			/*
			 * This page is on the background queue
			 * but not on a pageable queue OR is busy during
			 * darkwake mode when the target is artificially lowered.
			 * If it is busy during darkwake mode, and we don't skip it,
			 * we will just swing back around and try again with the same
			 * queue and might hit the same page or its neighbor in a
			 * similar state. Both of these are transient states and will
			 * get resolved, but, at this point let's ignore this page.
			 */
			if (vm_darkwake_mode && m->vmp_busy) {
				if (bg_m_object->internal) {
					vm_pageout_skipped_bq_internal++;
				} else {
					vm_pageout_skipped_bq_external++;
				}
			}
		} else if (force_anonymous == FALSE || bg_m_object->internal) {
			if (bg_m_object->internal &&
			    (VM_PAGE_Q_THROTTLED(iq) ||
			    vm_compressor_out_of_space() == TRUE ||
			    vm_page_free_count < (vm_page_free_reserved / 4))) {
				vm_pageout_skipped_bq_internal++;
			} else {
				*is_page_from_bg_q = TRUE;

				if (bg_m_object->internal) {
					vm_pageout_vminfo.vm_pageout_considered_bq_internal++;
				} else {
					vm_pageout_vminfo.vm_pageout_considered_bq_external++;
				}
				goto found_page;
			}
		}
	}

	inactive_external_count = vm_page_inactive_count - vm_page_anonymous_count;

	if ((vm_page_pageable_external_count < vm_pageout_state.vm_page_filecache_min || force_anonymous == TRUE) ||
	    (inactive_external_count < VM_PAGE_INACTIVE_TARGET(vm_page_pageable_external_count))) {
		*grab_anonymous = TRUE;
		*anons_grabbed = 0;

		if (VM_CONFIG_SWAP_IS_ACTIVE) {
			vm_pageout_vminfo.vm_pageout_skipped_external++;
		} else {
			if (vm_page_free_count < (COMPRESSOR_FREE_RESERVED_LIMIT * 2)) {
				/*
				 * No swap and we are in dangerously low levels of free memory.
				 * If we keep going ahead with anonymous pages, we are going to run into a situation
				 * where the compressor will be stuck waiting for free pages (if it isn't already).
				 *
				 * So, pick a file backed page...
				 */
				*grab_anonymous = FALSE;
				*anons_grabbed = ANONS_GRABBED_LIMIT;
				vm_pageout_vminfo.vm_pageout_skipped_internal++;
			}
		}
		goto want_anonymous;
	}
	*grab_anonymous = (vm_page_anonymous_count > vm_page_anonymous_min);

#if CONFIG_JETSAM
	/* If the file-backed pool has accumulated
	 * significantly more pages than the jetsam
	 * threshold, prefer to reclaim those
	 * inline to minimise compute overhead of reclaiming
	 * anonymous pages.
	 * This calculation does not account for the CPU local
	 * external page queues, as those are expected to be
	 * much smaller relative to the global pools.
	 */

	struct vm_pageout_queue *eq = &vm_pageout_queue_external;

	if (*grab_anonymous == TRUE && !VM_PAGE_Q_THROTTLED(eq)) {
		if (vm_page_pageable_external_count >
		    vm_pageout_state.vm_page_filecache_min) {
			if ((vm_page_pageable_external_count *
			    vm_pageout_memorystatus_fb_factor_dr) >
			    (memorystatus_available_pages_critical *
			    vm_pageout_memorystatus_fb_factor_nr)) {
				*grab_anonymous = FALSE;

				VM_PAGEOUT_DEBUG(vm_grab_anon_overrides, 1);
			}
		}
		if (*grab_anonymous) {
			VM_PAGEOUT_DEBUG(vm_grab_anon_nops, 1);
		}
	}
#endif /* CONFIG_JETSAM */

want_anonymous:
	if (*grab_anonymous == FALSE || *anons_grabbed >= ANONS_GRABBED_LIMIT || vm_page_queue_empty(&vm_page_queue_anonymous)) {
		if (!vm_page_queue_empty(&vm_page_queue_inactive)) {
			m = (vm_page_t) vm_page_queue_first(&vm_page_queue_inactive);

			assert(m->vmp_q_state == VM_PAGE_ON_INACTIVE_EXTERNAL_Q);
			*anons_grabbed = 0;

			if (vm_page_pageable_external_count < vm_pageout_state.vm_page_filecache_min) {
				if (!vm_page_queue_empty(&vm_page_queue_anonymous)) {
					if ((++(*reactivated_this_call) % 100)) {
						vm_pageout_vminfo.vm_pageout_filecache_min_reactivated++;

						vm_page_activate(m);
						counter_inc(&vm_statistics_reactivations);
#if DEVELOPMENT || DEBUG
						if (*is_page_from_bg_q == TRUE) {
							if (m_object->internal) {
								vm_pageout_rejected_bq_internal++;
							} else {
								vm_pageout_rejected_bq_external++;
							}
						}
#endif /* DEVELOPMENT || DEBUG */
						vm_pageout_state.vm_pageout_inactive_used++;

						m = NULL;
						retval = VM_PAGEOUT_SCAN_NEXT_ITERATION;

						goto found_page;
					}

					/*
					 * steal 1 of the file backed pages even if
					 * we are under the limit that has been set
					 * for a healthy filecache
					 */
				}
			}
			goto found_page;
		}
	}
	if (!vm_page_queue_empty(&vm_page_queue_anonymous)) {
		m = (vm_page_t) vm_page_queue_first(&vm_page_queue_anonymous);

		assert(m->vmp_q_state == VM_PAGE_ON_INACTIVE_INTERNAL_Q);
		*anons_grabbed += 1;

		goto found_page;
	}

	m = NULL;

found_page:
	*victim_page = m;

	return retval;
}

/*
 * This function is called only from vm_pageout_scan and
 * it will put a page back on the active/inactive queue
 * if we can't reclaim it for some reason.
 */
static void
vps_requeue_page(vm_page_t m, int page_prev_q_state, __unused boolean_t page_from_bg_q)
{
	if (page_prev_q_state == VM_PAGE_ON_SPECULATIVE_Q) {
		vm_page_enqueue_inactive(m, FALSE);
	} else {
		vm_page_activate(m);
	}

#if DEVELOPMENT || DEBUG
	vm_object_t m_object = VM_PAGE_OBJECT(m);

	if (page_from_bg_q == TRUE) {
		if (m_object->internal) {
			vm_pageout_rejected_bq_internal++;
		} else {
			vm_pageout_rejected_bq_external++;
		}
	}
#endif /* DEVELOPMENT || DEBUG */
}

/*
 * This function is called only from vm_pageout_scan and
 * it will try to grab the victim page's VM object (m_object)
 * which differs from the previous victim page's object (object).
 */
static int
vps_switch_object(vm_page_t m, vm_object_t m_object, vm_object_t *object, int page_prev_q_state, boolean_t avoid_anon_pages, boolean_t page_from_bg_q)
{
	struct vm_speculative_age_q *sq;

	sq = &vm_page_queue_speculative[VM_PAGE_SPECULATIVE_AGED_Q];

	/*
	 * the object associated with candidate page is
	 * different from the one we were just working
	 * with... dump the lock if we still own it
	 */
	if (*object != NULL) {
		vm_object_unlock(*object);
		*object = NULL;
	}
	/*
	 * Try to lock object; since we've alread got the
	 * page queues lock, we can only 'try' for this one.
	 * if the 'try' fails, we need to do a mutex_pause
	 * to allow the owner of the object lock a chance to
	 * run... otherwise, we're likely to trip over this
	 * object in the same state as we work our way through
	 * the queue... clumps of pages associated with the same
	 * object are fairly typical on the inactive and active queues
	 */
	if (!vm_object_lock_try_scan(m_object)) {
		vm_page_t m_want = NULL;

		vm_pageout_vminfo.vm_pageout_inactive_nolock++;

		if (page_prev_q_state == VM_PAGE_ON_INACTIVE_CLEANED_Q) {
			VM_PAGEOUT_DEBUG(vm_pageout_cleaned_nolock, 1);
		}

		pmap_clear_reference(VM_PAGE_GET_PHYS_PAGE(m));

		m->vmp_reference = FALSE;

		if (!m_object->object_is_shared_cache) {
			/*
			 * don't apply this optimization if this is the shared cache
			 * object, it's too easy to get rid of very hot and important
			 * pages...
			 * m->vmp_object must be stable since we hold the page queues lock...
			 * we can update the scan_collisions field sans the object lock
			 * since it is a separate field and this is the only spot that does
			 * a read-modify-write operation and it is never executed concurrently...
			 * we can asynchronously set this field to 0 when creating a UPL, so it
			 * is possible for the value to be a bit non-determistic, but that's ok
			 * since it's only used as a hint
			 */
			m_object->scan_collisions = 1;
		}
		if (page_from_bg_q) {
			m_want = (vm_page_t) vm_page_queue_first(&vm_page_queue_background);
		} else if (!vm_page_queue_empty(&vm_page_queue_cleaned)) {
			m_want = (vm_page_t) vm_page_queue_first(&vm_page_queue_cleaned);
		} else if (!vm_page_queue_empty(&sq->age_q)) {
			m_want = (vm_page_t) vm_page_queue_first(&sq->age_q);
		} else if ((avoid_anon_pages || vm_page_queue_empty(&vm_page_queue_anonymous)) &&
		    !vm_page_queue_empty(&vm_page_queue_inactive)) {
			m_want = (vm_page_t) vm_page_queue_first(&vm_page_queue_inactive);
		} else if (!vm_page_queue_empty(&vm_page_queue_anonymous)) {
			m_want = (vm_page_t) vm_page_queue_first(&vm_page_queue_anonymous);
		}

		/*
		 * this is the next object we're going to be interested in
		 * try to make sure its available after the mutex_pause
		 * returns control
		 */
		if (m_want) {
			vm_pageout_scan_wants_object = VM_PAGE_OBJECT(m_want);
		}

		vps_requeue_page(m, page_prev_q_state, page_from_bg_q);

		return VM_PAGEOUT_SCAN_NEXT_ITERATION;
	} else {
		*object = m_object;
		vm_pageout_scan_wants_object = VM_OBJECT_NULL;
	}

	return VM_PAGEOUT_SCAN_PROCEED;
}

/*
 * This function is called only from vm_pageout_scan and
 * it notices that pageout scan may be rendered ineffective
 * due to a FS deadlock and will jetsam a process if possible.
 * If jetsam isn't supported, it'll move the page to the active
 * queue to try and get some different pages pushed onwards so
 * we can try to get out of this scenario.
 */
static void
vps_deal_with_throttled_queues(vm_page_t m, vm_object_t *object, uint32_t *vm_pageout_inactive_external_forced_reactivate_limit,
    boolean_t *force_anonymous, __unused boolean_t is_page_from_bg_q)
{
	struct  vm_pageout_queue *eq;
	vm_object_t cur_object = VM_OBJECT_NULL;

	cur_object = *object;

	eq = &vm_pageout_queue_external;

	if (cur_object->internal == FALSE) {
		/*
		 * we need to break up the following potential deadlock case...
		 *  a) The external pageout thread is stuck on the truncate lock for a file that is being extended i.e. written.
		 *  b) The thread doing the writing is waiting for pages while holding the truncate lock
		 *  c) Most of the pages in the inactive queue belong to this file.
		 *
		 * we are potentially in this deadlock because...
		 *  a) the external pageout queue is throttled
		 *  b) we're done with the active queue and moved on to the inactive queue
		 *  c) we've got a dirty external page
		 *
		 * since we don't know the reason for the external pageout queue being throttled we
		 * must suspect that we are deadlocked, so move the current page onto the active queue
		 * in an effort to cause a page from the active queue to 'age' to the inactive queue
		 *
		 * if we don't have jetsam configured (i.e. we have a dynamic pager), set
		 * 'force_anonymous' to TRUE to cause us to grab a page from the cleaned/anonymous
		 * pool the next time we select a victim page... if we can make enough new free pages,
		 * the deadlock will break, the external pageout queue will empty and it will no longer
		 * be throttled
		 *
		 * if we have jetsam configured, keep a count of the pages reactivated this way so
		 * that we can try to find clean pages in the active/inactive queues before
		 * deciding to jetsam a process
		 */
		vm_pageout_vminfo.vm_pageout_scan_inactive_throttled_external++;

		vm_page_check_pageable_safe(m);
		assert(m->vmp_q_state == VM_PAGE_NOT_ON_Q);
		vm_page_queue_enter(&vm_page_queue_active, m, vmp_pageq);
		m->vmp_q_state = VM_PAGE_ON_ACTIVE_Q;
		vm_page_active_count++;
		vm_page_pageable_external_count++;

		vm_pageout_adjust_eq_iothrottle(&pgo_iothread_external_state, FALSE);

#if CONFIG_MEMORYSTATUS && CONFIG_JETSAM

#pragma unused(force_anonymous)

		*vm_pageout_inactive_external_forced_reactivate_limit -= 1;

		if (*vm_pageout_inactive_external_forced_reactivate_limit <= 0) {
			*vm_pageout_inactive_external_forced_reactivate_limit = vm_page_active_count + vm_page_inactive_count;
			/*
			 * Possible deadlock scenario so request jetsam action
			 */
			memorystatus_kill_on_vps_starvation();
			VM_DEBUG_CONSTANT_EVENT(vm_pageout_jetsam, DBG_VM_PAGEOUT_JETSAM, DBG_FUNC_NONE,
			    vm_page_active_count, vm_page_inactive_count, vm_page_free_count, vm_page_free_count);
		}
#else /* CONFIG_MEMORYSTATUS && CONFIG_JETSAM */

#pragma unused(vm_pageout_inactive_external_forced_reactivate_limit)

		*force_anonymous = TRUE;
#endif /* CONFIG_MEMORYSTATUS && CONFIG_JETSAM */
	} else {
		vm_page_activate(m);
		counter_inc(&vm_statistics_reactivations);

#if DEVELOPMENT || DEBUG
		if (is_page_from_bg_q == TRUE) {
			if (cur_object->internal) {
				vm_pageout_rejected_bq_internal++;
			} else {
				vm_pageout_rejected_bq_external++;
			}
		}
#endif /* DEVELOPMENT || DEBUG */

		vm_pageout_state.vm_pageout_inactive_used++;
	}
}


void
vm_page_balance_inactive(int max_to_move)
{
	vm_page_t m;

	LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);

	if (hibernation_vmqueues_inspection || hibernate_cleaning_in_progress) {
		/*
		 * It is likely that the hibernation code path is
		 * dealing with these very queues as we are about
		 * to move pages around in/from them and completely
		 * change the linkage of the pages.
		 *
		 * And so we skip the rebalancing of these queues.
		 */
		return;
	}
	vm_page_inactive_target = VM_PAGE_INACTIVE_TARGET(vm_page_active_count +
	    vm_page_inactive_count +
	    vm_page_speculative_count);

	while (max_to_move-- && (vm_page_inactive_count + vm_page_speculative_count) < vm_page_inactive_target) {
		VM_PAGEOUT_DEBUG(vm_pageout_balanced, 1);

		m = (vm_page_t) vm_page_queue_first(&vm_page_queue_active);

		assert(m->vmp_q_state == VM_PAGE_ON_ACTIVE_Q);
		assert(!m->vmp_laundry);
		assert(!is_kernel_object(VM_PAGE_OBJECT(m)));
		assert(VM_PAGE_GET_PHYS_PAGE(m) != vm_page_guard_addr);

		DTRACE_VM2(scan, int, 1, (uint64_t *), NULL);

		/*
		 * by not passing in a pmap_flush_context we will forgo any TLB flushing, local or otherwise...
		 *
		 * a TLB flush isn't really needed here since at worst we'll miss the reference bit being
		 * updated in the PTE if a remote processor still has this mapping cached in its TLB when the
		 * new reference happens. If no futher references happen on the page after that remote TLB flushes
		 * we'll see a clean, non-referenced page when it eventually gets pulled out of the inactive queue
		 * by pageout_scan, which is just fine since the last reference would have happened quite far
		 * in the past (TLB caches don't hang around for very long), and of course could just as easily
		 * have happened before we moved the page
		 */
		if (m->vmp_pmapped == TRUE) {
			/*
			 * We might be holding the page queue lock as a
			 * spin lock and clearing the "referenced" bit could
			 * take a while if there are lots of mappings of
			 * that page, so make sure we acquire the lock as
			 * as mutex to avoid a spinlock timeout.
			 */
			vm_page_lockconvert_queues();
			pmap_clear_refmod_options(VM_PAGE_GET_PHYS_PAGE(m), VM_MEM_REFERENCED, PMAP_OPTIONS_NOFLUSH, (void *)NULL);
		}

		/*
		 * The page might be absent or busy,
		 * but vm_page_deactivate can handle that.
		 * FALSE indicates that we don't want a H/W clear reference
		 */
		vm_page_deactivate_internal(m, FALSE);
	}
}

/*
 *	vm_pageout_scan does the dirty work for the pageout daemon.
 *	It returns with both vm_page_queue_free_lock and vm_page_queue_lock
 *	held and vm_page_free_wanted == 0.
 */
void
vm_pageout_scan(void)
{
	unsigned int loop_count = 0;
	unsigned int inactive_burst_count = 0;
	unsigned int reactivated_this_call;
	unsigned int reactivate_limit;
	vm_page_t   local_freeq = NULL;
	int         local_freed = 0;
	int         delayed_unlock;
	int         delayed_unlock_limit = 0;
	int         refmod_state = 0;
	int     vm_pageout_deadlock_target = 0;
	struct  vm_pageout_queue *iq;
	struct  vm_pageout_queue *eq;
	struct  vm_speculative_age_q *sq;
	struct  flow_control    flow_control = { .state = 0, .ts = { .tv_sec = 0, .tv_nsec = 0 } };
	boolean_t inactive_throttled = FALSE;
	vm_object_t     object = NULL;
	uint32_t        inactive_reclaim_run;
	boolean_t       grab_anonymous = FALSE;
	boolean_t       force_anonymous = FALSE;
	boolean_t       force_speculative_aging = FALSE;
	int             anons_grabbed = 0;
	int             page_prev_q_state = 0;
	boolean_t       page_from_bg_q = FALSE;
	uint32_t        vm_pageout_inactive_external_forced_reactivate_limit = 0;
	vm_object_t     m_object = VM_OBJECT_NULL;
	int             retval = 0;
	boolean_t       lock_yield_check = FALSE;


	VM_DEBUG_CONSTANT_EVENT(vm_pageout_scan, DBG_VM_PAGEOUT_SCAN, DBG_FUNC_START,
	    vm_pageout_vminfo.vm_pageout_freed_speculative,
	    vm_pageout_state.vm_pageout_inactive_clean,
	    vm_pageout_vminfo.vm_pageout_inactive_dirty_internal,
	    vm_pageout_vminfo.vm_pageout_inactive_dirty_external);

	flow_control.state = FCS_IDLE;
	iq = &vm_pageout_queue_internal;
	eq = &vm_pageout_queue_external;
	sq = &vm_page_queue_speculative[VM_PAGE_SPECULATIVE_AGED_Q];

	/* Ask the pmap layer to return any pages it no longer needs. */
	pmap_release_pages_fast();

	vm_page_lock_queues();

	delayed_unlock = 1;

	/*
	 *	Calculate the max number of referenced pages on the inactive
	 *	queue that we will reactivate.
	 */
	reactivated_this_call = 0;
	reactivate_limit = VM_PAGE_REACTIVATE_LIMIT(vm_page_active_count +
	    vm_page_inactive_count);
	inactive_reclaim_run = 0;

	vm_pageout_inactive_external_forced_reactivate_limit = vm_page_active_count + vm_page_inactive_count;

	/*
	 *	We must limit the rate at which we send pages to the pagers
	 *	so that we don't tie up too many pages in the I/O queues.
	 *	We implement a throttling mechanism using the laundry count
	 *      to limit the number of pages outstanding to the default
	 *	and external pagers.  We can bypass the throttles and look
	 *	for clean pages if the pageout queues don't drain in a timely
	 *	fashion since this may indicate that the pageout paths are
	 *	stalled waiting for memory, which only we can provide.
	 */

	vps_init_page_targets();
	assert(object == NULL);
	assert(delayed_unlock != 0);

	for (;;) {
		vm_page_t m;

		DTRACE_VM2(rev, int, 1, (uint64_t *), NULL);

		if (lock_yield_check) {
			lock_yield_check = FALSE;

			if (delayed_unlock++ > delayed_unlock_limit) {
				vm_pageout_prepare_to_block(&object, &delayed_unlock, &local_freeq, &local_freed,
				    VM_PAGEOUT_PB_CONSIDER_WAKING_COMPACTOR_SWAPPER);
			} else if (vm_pageout_scan_wants_object) {
				vm_page_unlock_queues();
				mutex_pause(0);
				vm_page_lock_queues();
			} else if (vps_yield_for_pgqlockwaiters && lck_mtx_yield(&vm_page_queue_lock)) {
				VM_PAGEOUT_DEBUG(vm_pageout_yield_for_free_pages, 1);
			}
		}

		if (vm_upl_wait_for_pages < 0) {
			vm_upl_wait_for_pages = 0;
		}

		delayed_unlock_limit = VM_PAGEOUT_DELAYED_UNLOCK_LIMIT + vm_upl_wait_for_pages;

		if (delayed_unlock_limit > VM_PAGEOUT_DELAYED_UNLOCK_LIMIT_MAX) {
			delayed_unlock_limit = VM_PAGEOUT_DELAYED_UNLOCK_LIMIT_MAX;
		}

		vps_deal_with_secluded_page_overflow(&local_freeq, &local_freed);

		assert(delayed_unlock);

		/*
		 * maintain our balance
		 */
		vm_page_balance_inactive(1);


		/**********************************************************************
		* above this point we're playing with the active and secluded queues
		* below this point we're playing with the throttling mechanisms
		* and the inactive queue
		**********************************************************************/

		if (vm_page_free_count + local_freed >= vm_page_free_target) {
			vm_pageout_scan_wants_object = VM_OBJECT_NULL;

			vm_pageout_prepare_to_block(&object, &delayed_unlock, &local_freeq, &local_freed,
			    VM_PAGEOUT_PB_CONSIDER_WAKING_COMPACTOR_SWAPPER);
			/*
			 * make sure the pageout I/O threads are running
			 * throttled in case there are still requests
			 * in the laundry... since we have met our targets
			 * we don't need the laundry to be cleaned in a timely
			 * fashion... so let's avoid interfering with foreground
			 * activity
			 */
			vm_pageout_adjust_eq_iothrottle(&pgo_iothread_external_state, TRUE);

			vm_free_page_lock();

			if ((vm_page_free_count >= vm_page_free_target) &&
			    (vm_page_free_wanted == 0) && (vm_page_free_wanted_privileged == 0)) {
				/*
				 * done - we have met our target *and*
				 * there is no one waiting for a page.
				 */
return_from_scan:
				assert(vm_pageout_scan_wants_object == VM_OBJECT_NULL);

				VM_DEBUG_CONSTANT_EVENT(vm_pageout_scan, DBG_VM_PAGEOUT_SCAN, DBG_FUNC_NONE,
				    vm_pageout_state.vm_pageout_inactive,
				    vm_pageout_state.vm_pageout_inactive_used, 0, 0);
				VM_DEBUG_CONSTANT_EVENT(vm_pageout_scan, DBG_VM_PAGEOUT_SCAN, DBG_FUNC_END,
				    vm_pageout_vminfo.vm_pageout_freed_speculative,
				    vm_pageout_state.vm_pageout_inactive_clean,
				    vm_pageout_vminfo.vm_pageout_inactive_dirty_internal,
				    vm_pageout_vminfo.vm_pageout_inactive_dirty_external);

				return;
			}
			vm_free_page_unlock();
		}

		/*
		 * Before anything, we check if we have any ripe volatile
		 * objects around. If so, try to purge the first object.
		 * If the purge fails, fall through to reclaim a page instead.
		 * If the purge succeeds, go back to the top and reevalute
		 * the new memory situation.
		 */
		retval = vps_purge_object();

		if (retval == VM_PAGEOUT_SCAN_NEXT_ITERATION) {
			/*
			 * Success
			 */
			if (object != NULL) {
				vm_object_unlock(object);
				object = NULL;
			}

			lock_yield_check = FALSE;
			continue;
		}

		/*
		 * If our 'aged' queue is empty and we have some speculative pages
		 * in the other queues, let's go through and see if we need to age
		 * them.
		 *
		 * If we succeeded in aging a speculative Q or just that everything
		 * looks normal w.r.t queue age and queue counts, we keep going onward.
		 *
		 * If, for some reason, we seem to have a mismatch between the spec.
		 * page count and the page queues, we reset those variables and
		 * restart the loop (LD TODO: Track this better?).
		 */
		if (vm_page_queue_empty(&sq->age_q) && vm_page_speculative_count) {
			retval = vps_age_speculative_queue(force_speculative_aging);

			if (retval == VM_PAGEOUT_SCAN_NEXT_ITERATION) {
				lock_yield_check = FALSE;
				continue;
			}
		}
		force_speculative_aging = FALSE;

		/*
		 * Check to see if we need to evict objects from the cache.
		 *
		 * Note: 'object' here doesn't have anything to do with
		 * the eviction part. We just need to make sure we have dropped
		 * any object lock we might be holding if we need to go down
		 * into the eviction logic.
		 */
		retval = vps_object_cache_evict(&object);

		if (retval == VM_PAGEOUT_SCAN_NEXT_ITERATION) {
			lock_yield_check = FALSE;
			continue;
		}


		/*
		 * Calculate our filecache_min that will affect the loop
		 * going forward.
		 */
		vps_calculate_filecache_min();

		/*
		 * LD TODO: Use a structure to hold all state variables for a single
		 * vm_pageout_scan iteration and pass that structure to this function instead.
		 */
		retval = vps_flow_control(&flow_control, &anons_grabbed, &object,
		    &delayed_unlock, &local_freeq, &local_freed,
		    &vm_pageout_deadlock_target, inactive_burst_count);

		if (retval == VM_PAGEOUT_SCAN_NEXT_ITERATION) {
			if (loop_count >= vm_page_inactive_count) {
				loop_count = 0;
			}

			inactive_burst_count = 0;

			assert(object == NULL);
			assert(delayed_unlock != 0);

			lock_yield_check = FALSE;
			continue;
		} else if (retval == VM_PAGEOUT_SCAN_DONE_RETURN) {
			goto return_from_scan;
		}

		flow_control.state = FCS_IDLE;

		vm_pageout_inactive_external_forced_reactivate_limit = MIN((vm_page_active_count + vm_page_inactive_count),
		    vm_pageout_inactive_external_forced_reactivate_limit);
		loop_count++;
		inactive_burst_count++;
		vm_pageout_state.vm_pageout_inactive++;

		/*
		 * Choose a victim.
		 */

		m = NULL;
		retval = vps_choose_victim_page(&m, &anons_grabbed, &grab_anonymous, force_anonymous, &page_from_bg_q, &reactivated_this_call);

		if (m == NULL) {
			if (retval == VM_PAGEOUT_SCAN_NEXT_ITERATION) {
				inactive_burst_count = 0;

				if (page_prev_q_state == VM_PAGE_ON_INACTIVE_CLEANED_Q) {
					VM_PAGEOUT_DEBUG(vm_pageout_cleaned_reactivated, 1);
				}

				lock_yield_check = TRUE;
				continue;
			}

			/*
			 * if we've gotten here, we have no victim page.
			 * check to see if we've not finished balancing the queues
			 * or we have a page on the aged speculative queue that we
			 * skipped due to force_anonymous == TRUE.. or we have
			 * speculative  pages that we can prematurely age... if
			 * one of these cases we'll keep going, else panic
			 */
			force_anonymous = FALSE;
			VM_PAGEOUT_DEBUG(vm_pageout_no_victim, 1);

			if (!vm_page_queue_empty(&sq->age_q)) {
				lock_yield_check = TRUE;
				continue;
			}

			if (vm_page_speculative_count) {
				force_speculative_aging = TRUE;
				lock_yield_check = TRUE;
				continue;
			}
			panic("vm_pageout: no victim");

			/* NOTREACHED */
		}

		assert(VM_PAGE_PAGEABLE(m));
		m_object = VM_PAGE_OBJECT(m);
		force_anonymous = FALSE;

		page_prev_q_state = m->vmp_q_state;
		/*
		 * we just found this page on one of our queues...
		 * it can't also be on the pageout queue, so safe
		 * to call vm_page_queues_remove
		 */
		bool donate = (m->vmp_on_specialq == VM_PAGE_SPECIAL_Q_DONATE);
		vm_page_queues_remove(m, TRUE);
		if (donate) {
			/*
			 * The compressor needs to see this bit to know
			 * where this page needs to land. Also if stolen,
			 * this bit helps put the page back in the right
			 * special queue where it belongs.
			 */
			m->vmp_on_specialq = VM_PAGE_SPECIAL_Q_DONATE;
		}

		assert(!m->vmp_laundry);
		assert(!m->vmp_private);
		assert(!m->vmp_fictitious);
		assert(!is_kernel_object(m_object));
		assert(VM_PAGE_GET_PHYS_PAGE(m) != vm_page_guard_addr);

		vm_pageout_vminfo.vm_pageout_considered_page++;

		DTRACE_VM2(scan, int, 1, (uint64_t *), NULL);

		/*
		 * check to see if we currently are working
		 * with the same object... if so, we've
		 * already got the lock
		 */
		if (m_object != object) {
			boolean_t avoid_anon_pages = (grab_anonymous == FALSE || anons_grabbed >= ANONS_GRABBED_LIMIT);

			/*
			 * vps_switch_object() will always drop the 'object' lock first
			 * and then try to acquire the 'm_object' lock. So 'object' has to point to
			 * either 'm_object' or NULL.
			 */
			retval = vps_switch_object(m, m_object, &object, page_prev_q_state, avoid_anon_pages, page_from_bg_q);

			if (retval == VM_PAGEOUT_SCAN_NEXT_ITERATION) {
				lock_yield_check = TRUE;
				continue;
			}
		}
		assert(m_object == object);
		assert(VM_PAGE_OBJECT(m) == m_object);

		if (m->vmp_busy) {
			/*
			 *	Somebody is already playing with this page.
			 *	Put it back on the appropriate queue
			 *
			 */
			VM_PAGEOUT_DEBUG(vm_pageout_inactive_busy, 1);

			if (page_prev_q_state == VM_PAGE_ON_INACTIVE_CLEANED_Q) {
				VM_PAGEOUT_DEBUG(vm_pageout_cleaned_busy, 1);
			}

			vps_requeue_page(m, page_prev_q_state, page_from_bg_q);

			lock_yield_check = TRUE;
			continue;
		}

		/*
		 *   if (m->vmp_cleaning && !m->vmp_free_when_done)
		 *	If already cleaning this page in place
		 *	just leave if off the paging queues.
		 *	We can leave the page mapped, and upl_commit_range
		 *	will put it on the clean queue.
		 *
		 *   if (m->vmp_free_when_done && !m->vmp_cleaning)
		 *	an msync INVALIDATE is in progress...
		 *	this page has been marked for destruction
		 *      after it has been cleaned,
		 *      but not yet gathered into a UPL
		 *	where 'cleaning' will be set...
		 *	just leave it off the paging queues
		 *
		 *   if (m->vmp_free_when_done && m->vmp_clenaing)
		 *	an msync INVALIDATE is in progress
		 *	and the UPL has already gathered this page...
		 *	just leave it off the paging queues
		 */
		if (m->vmp_free_when_done || m->vmp_cleaning) {
			lock_yield_check = TRUE;
			continue;
		}


		/*
		 *	If it's absent, in error or the object is no longer alive,
		 *	we can reclaim the page... in the no longer alive case,
		 *	there are 2 states the page can be in that preclude us
		 *	from reclaiming it - busy or cleaning - that we've already
		 *	dealt with
		 */
		if (m->vmp_absent || VMP_ERROR_GET(m) || !object->alive ||
		    (!object->internal && object->pager == MEMORY_OBJECT_NULL)) {
			if (m->vmp_absent) {
				VM_PAGEOUT_DEBUG(vm_pageout_inactive_absent, 1);
			} else if (!object->alive ||
			    (!object->internal &&
			    object->pager == MEMORY_OBJECT_NULL)) {
				VM_PAGEOUT_DEBUG(vm_pageout_inactive_notalive, 1);
			} else {
				VM_PAGEOUT_DEBUG(vm_pageout_inactive_error, 1);
			}
reclaim_page:
			if (vm_pageout_deadlock_target) {
				VM_PAGEOUT_DEBUG(vm_pageout_scan_inactive_throttle_success, 1);
				vm_pageout_deadlock_target--;
			}

			DTRACE_VM2(dfree, int, 1, (uint64_t *), NULL);

			if (object->internal) {
				DTRACE_VM2(anonfree, int, 1, (uint64_t *), NULL);
			} else {
				DTRACE_VM2(fsfree, int, 1, (uint64_t *), NULL);
			}
			assert(!m->vmp_cleaning);
			assert(!m->vmp_laundry);

			if (!object->internal &&
			    object->pager != NULL &&
			    object->pager->mo_pager_ops == &shared_region_pager_ops) {
				shared_region_pager_reclaimed++;
			}

			m->vmp_busy = TRUE;

			/*
			 * remove page from object here since we're already
			 * behind the object lock... defer the rest of the work
			 * we'd normally do in vm_page_free_prepare_object
			 * until 'vm_page_free_list' is called
			 */
			if (m->vmp_tabled) {
				vm_page_remove(m, TRUE);
			}

			assert(m->vmp_pageq.next == 0 && m->vmp_pageq.prev == 0);
			m->vmp_snext = local_freeq;
			local_freeq = m;
			local_freed++;

			if (page_prev_q_state == VM_PAGE_ON_SPECULATIVE_Q) {
				vm_pageout_vminfo.vm_pageout_freed_speculative++;
			} else if (page_prev_q_state == VM_PAGE_ON_INACTIVE_CLEANED_Q) {
				vm_pageout_vminfo.vm_pageout_freed_cleaned++;
			} else if (page_prev_q_state == VM_PAGE_ON_INACTIVE_INTERNAL_Q) {
				vm_pageout_vminfo.vm_pageout_freed_internal++;
			} else {
				vm_pageout_vminfo.vm_pageout_freed_external++;
			}

			inactive_burst_count = 0;

			lock_yield_check = TRUE;
			continue;
		}
		if (object->vo_copy == VM_OBJECT_NULL) {
			/*
			 * No one else can have any interest in this page.
			 * If this is an empty purgable object, the page can be
			 * reclaimed even if dirty.
			 * If the page belongs to a volatile purgable object, we
			 * reactivate it if the compressor isn't active.
			 */
			if (object->purgable == VM_PURGABLE_EMPTY) {
				if (m->vmp_pmapped == TRUE) {
					/* unmap the page */
					refmod_state = pmap_disconnect(VM_PAGE_GET_PHYS_PAGE(m));
					if (refmod_state & VM_MEM_MODIFIED) {
						SET_PAGE_DIRTY(m, FALSE);
					}
				}
				if (m->vmp_dirty || m->vmp_precious) {
					/* we saved the cost of cleaning this page ! */
					vm_page_purged_count++;
				}
				goto reclaim_page;
			}

			if (VM_CONFIG_COMPRESSOR_IS_ACTIVE) {
				/*
				 * With the VM compressor, the cost of
				 * reclaiming a page is much lower (no I/O),
				 * so if we find a "volatile" page, it's better
				 * to let it get compressed rather than letting
				 * it occupy a full page until it gets purged.
				 * So no need to check for "volatile" here.
				 */
			} else if (object->purgable == VM_PURGABLE_VOLATILE) {
				/*
				 * Avoid cleaning a "volatile" page which might
				 * be purged soon.
				 */

				/* if it's wired, we can't put it on our queue */
				assert(!VM_PAGE_WIRED(m));

				/* just stick it back on! */
				reactivated_this_call++;

				if (page_prev_q_state == VM_PAGE_ON_INACTIVE_CLEANED_Q) {
					VM_PAGEOUT_DEBUG(vm_pageout_cleaned_volatile_reactivated, 1);
				}

				goto reactivate_page;
			}
		} /* vo_copy NULL */
		/*
		 *	If it's being used, reactivate.
		 *	(Fictitious pages are either busy or absent.)
		 *	First, update the reference and dirty bits
		 *	to make sure the page is unreferenced.
		 */
		refmod_state = -1;

		if (m->vmp_reference == FALSE && m->vmp_pmapped == TRUE) {
			refmod_state = pmap_get_refmod(VM_PAGE_GET_PHYS_PAGE(m));

			if (refmod_state & VM_MEM_REFERENCED) {
				m->vmp_reference = TRUE;
			}
			if (refmod_state & VM_MEM_MODIFIED) {
				SET_PAGE_DIRTY(m, FALSE);
			}
		}

		if (m->vmp_reference || m->vmp_dirty) {
			/* deal with a rogue "reusable" page */
			VM_PAGEOUT_SCAN_HANDLE_REUSABLE_PAGE(m, m_object);
		}

		if (vm_pageout_state.vm_page_xpmapped_min_divisor == 0) {
			vm_pageout_state.vm_page_xpmapped_min = 0;
		} else {
			vm_pageout_state.vm_page_xpmapped_min = (vm_page_pageable_external_count * 10) /
			    vm_pageout_state.vm_page_xpmapped_min_divisor;
		}

		if (!m->vmp_no_cache &&
		    page_from_bg_q == FALSE &&
		    (m->vmp_reference || (m->vmp_xpmapped && !object->internal &&
		    (vm_page_xpmapped_external_count < vm_pageout_state.vm_page_xpmapped_min)))) {
			/*
			 * The page we pulled off the inactive list has
			 * been referenced.  It is possible for other
			 * processors to be touching pages faster than we
			 * can clear the referenced bit and traverse the
			 * inactive queue, so we limit the number of
			 * reactivations.
			 */
			if (++reactivated_this_call >= reactivate_limit &&
			    !object->object_is_shared_cache &&
			    !((m->vmp_realtime ||
			    object->for_realtime) &&
			    vm_pageout_protect_realtime)) {
				vm_pageout_vminfo.vm_pageout_reactivation_limit_exceeded++;
			} else if (++inactive_reclaim_run >= VM_PAGEOUT_INACTIVE_FORCE_RECLAIM) {
				vm_pageout_vminfo.vm_pageout_inactive_force_reclaim++;
				if (object->object_is_shared_cache) {
					vm_pageout_vminfo.vm_pageout_forcereclaimed_sharedcache++;
				} else if (m->vmp_realtime ||
				    object->for_realtime) {
					vm_pageout_vminfo.vm_pageout_forcereclaimed_realtime++;
				}
			} else {
				uint32_t isinuse;

				if (reactivated_this_call >= reactivate_limit) {
					if (object->object_is_shared_cache) {
						vm_pageout_vminfo.vm_pageout_protected_sharedcache++;
					} else if ((m->vmp_realtime ||
					    object->for_realtime) &&
					    vm_pageout_protect_realtime) {
						vm_pageout_vminfo.vm_pageout_protected_realtime++;
					}
				}
				if (page_prev_q_state == VM_PAGE_ON_INACTIVE_CLEANED_Q) {
					VM_PAGEOUT_DEBUG(vm_pageout_cleaned_reference_reactivated, 1);
				}

				vm_pageout_vminfo.vm_pageout_inactive_referenced++;
reactivate_page:
				if (!object->internal && object->pager != MEMORY_OBJECT_NULL &&
				    vnode_pager_get_isinuse(object->pager, &isinuse) == KERN_SUCCESS && !isinuse) {
					/*
					 * no explict mappings of this object exist
					 * and it's not open via the filesystem
					 */
					vm_page_deactivate(m);
					VM_PAGEOUT_DEBUG(vm_pageout_inactive_deactivated, 1);
				} else {
					/*
					 * The page was/is being used, so put back on active list.
					 */
					vm_page_activate(m);
					counter_inc(&vm_statistics_reactivations);
					inactive_burst_count = 0;
				}
#if DEVELOPMENT || DEBUG
				if (page_from_bg_q == TRUE) {
					if (m_object->internal) {
						vm_pageout_rejected_bq_internal++;
					} else {
						vm_pageout_rejected_bq_external++;
					}
				}
#endif /* DEVELOPMENT || DEBUG */

				if (page_prev_q_state == VM_PAGE_ON_INACTIVE_CLEANED_Q) {
					VM_PAGEOUT_DEBUG(vm_pageout_cleaned_reactivated, 1);
				}
				vm_pageout_state.vm_pageout_inactive_used++;

				lock_yield_check = TRUE;
				continue;
			}
			/*
			 * Make sure we call pmap_get_refmod() if it
			 * wasn't already called just above, to update
			 * the dirty bit.
			 */
			if ((refmod_state == -1) && !m->vmp_dirty && m->vmp_pmapped) {
				refmod_state = pmap_get_refmod(VM_PAGE_GET_PHYS_PAGE(m));
				if (refmod_state & VM_MEM_MODIFIED) {
					SET_PAGE_DIRTY(m, FALSE);
				}
			}
		}

		/*
		 * we've got a candidate page to steal...
		 *
		 * m->vmp_dirty is up to date courtesy of the
		 * preceding check for m->vmp_reference... if
		 * we get here, then m->vmp_reference had to be
		 * FALSE (or possibly "reactivate_limit" was
		 * exceeded), but in either case we called
		 * pmap_get_refmod() and updated both
		 * m->vmp_reference and m->vmp_dirty
		 *
		 * if it's dirty or precious we need to
		 * see if the target queue is throtttled
		 * it if is, we need to skip over it by moving it back
		 * to the end of the inactive queue
		 */

		inactive_throttled = FALSE;

		if (m->vmp_dirty || m->vmp_precious) {
			if (object->internal) {
				if (VM_PAGE_Q_THROTTLED(iq)) {
					inactive_throttled = TRUE;
				}
			} else if (VM_PAGE_Q_THROTTLED(eq)) {
				inactive_throttled = TRUE;
			}
		}
throttle_inactive:
		if (!VM_DYNAMIC_PAGING_ENABLED() &&
		    object->internal && m->vmp_dirty &&
		    (object->purgable == VM_PURGABLE_DENY ||
		    object->purgable == VM_PURGABLE_NONVOLATILE ||
		    object->purgable == VM_PURGABLE_VOLATILE)) {
			vm_page_check_pageable_safe(m);
			assert(m->vmp_q_state == VM_PAGE_NOT_ON_Q);
			vm_page_queue_enter(&vm_page_queue_throttled, m, vmp_pageq);
			m->vmp_q_state = VM_PAGE_ON_THROTTLED_Q;
			vm_page_throttled_count++;

			VM_PAGEOUT_DEBUG(vm_pageout_scan_reclaimed_throttled, 1);

			inactive_burst_count = 0;

			lock_yield_check = TRUE;
			continue;
		}
		if (inactive_throttled == TRUE) {
			vps_deal_with_throttled_queues(m, &object, &vm_pageout_inactive_external_forced_reactivate_limit,
			    &force_anonymous, page_from_bg_q);

			inactive_burst_count = 0;

			if (page_prev_q_state == VM_PAGE_ON_INACTIVE_CLEANED_Q) {
				VM_PAGEOUT_DEBUG(vm_pageout_cleaned_reactivated, 1);
			}

			lock_yield_check = TRUE;
			continue;
		}

		/*
		 * we've got a page that we can steal...
		 * eliminate all mappings and make sure
		 * we have the up-to-date modified state
		 *
		 * if we need to do a pmap_disconnect then we
		 * need to re-evaluate m->vmp_dirty since the pmap_disconnect
		 * provides the true state atomically... the
		 * page was still mapped up to the pmap_disconnect
		 * and may have been dirtied at the last microsecond
		 *
		 * Note that if 'pmapped' is FALSE then the page is not
		 * and has not been in any map, so there is no point calling
		 * pmap_disconnect().  m->vmp_dirty could have been set in anticipation
		 * of likely usage of the page.
		 */
		if (m->vmp_pmapped == TRUE) {
			int pmap_options;

			/*
			 * Don't count this page as going into the compressor
			 * if any of these are true:
			 * 1) compressed pager isn't enabled
			 * 2) Freezer enabled device with compressed pager
			 *    backend (exclusive use) i.e. most of the VM system
			 *    (including vm_pageout_scan) has no knowledge of
			 *    the compressor
			 * 3) This page belongs to a file and hence will not be
			 *    sent into the compressor
			 */
			if (!VM_CONFIG_COMPRESSOR_IS_ACTIVE ||
			    object->internal == FALSE) {
				pmap_options = 0;
			} else if (m->vmp_dirty || m->vmp_precious) {
				/*
				 * VM knows that this page is dirty (or
				 * precious) and needs to be compressed
				 * rather than freed.
				 * Tell the pmap layer to count this page
				 * as "compressed".
				 */
				pmap_options = PMAP_OPTIONS_COMPRESSOR;
			} else {
				/*
				 * VM does not know if the page needs to
				 * be preserved but the pmap layer might tell
				 * us if any mapping has "modified" it.
				 * Let's the pmap layer to count this page
				 * as compressed if and only if it has been
				 * modified.
				 */
				pmap_options =
				    PMAP_OPTIONS_COMPRESSOR_IFF_MODIFIED;
			}
			refmod_state = pmap_disconnect_options(VM_PAGE_GET_PHYS_PAGE(m),
			    pmap_options,
			    NULL);
			if (refmod_state & VM_MEM_MODIFIED) {
				SET_PAGE_DIRTY(m, FALSE);
			}
		}

		/*
		 * reset our count of pages that have been reclaimed
		 * since the last page was 'stolen'
		 */
		inactive_reclaim_run = 0;

		/*
		 *	If it's clean and not precious, we can free the page.
		 */
		if (!m->vmp_dirty && !m->vmp_precious) {
			vm_pageout_state.vm_pageout_inactive_clean++;

			/*
			 * OK, at this point we have found a page we are going to free.
			 */
#if CONFIG_PHANTOM_CACHE
			if (!object->internal) {
				vm_phantom_cache_add_ghost(m);
			}
#endif
			goto reclaim_page;
		}

		/*
		 * The page may have been dirtied since the last check
		 * for a throttled target queue (which may have been skipped
		 * if the page was clean then).  With the dirty page
		 * disconnected here, we can make one final check.
		 */
		if (object->internal) {
			if (VM_PAGE_Q_THROTTLED(iq)) {
				inactive_throttled = TRUE;
			}
		} else if (VM_PAGE_Q_THROTTLED(eq)) {
			inactive_throttled = TRUE;
		}

		if (inactive_throttled == TRUE) {
			goto throttle_inactive;
		}

#if VM_PRESSURE_EVENTS
#if CONFIG_JETSAM

		/*
		 * If Jetsam is enabled, then the sending
		 * of memory pressure notifications is handled
		 * from the same thread that takes care of high-water
		 * and other jetsams i.e. the memorystatus_thread.
		 */

#else /* CONFIG_JETSAM */

		vm_pressure_response();

#endif /* CONFIG_JETSAM */
#endif /* VM_PRESSURE_EVENTS */

		if (page_prev_q_state == VM_PAGE_ON_SPECULATIVE_Q) {
			VM_PAGEOUT_DEBUG(vm_pageout_speculative_dirty, 1);
		}

		if (object->internal) {
			vm_pageout_vminfo.vm_pageout_inactive_dirty_internal++;
		} else {
			vm_pageout_vminfo.vm_pageout_inactive_dirty_external++;
		}

		/*
		 * internal pages will go to the compressor...
		 * external pages will go to the appropriate pager to be cleaned
		 * and upon completion will end up on 'vm_page_queue_cleaned' which
		 * is a preferred queue to steal from
		 */
		vm_pageout_cluster(m);
		inactive_burst_count = 0;

		/*
		 * back to top of pageout scan loop
		 */
	}
}


void
vm_page_free_reserve(
	int pages)
{
	int             free_after_reserve;

	if (VM_CONFIG_COMPRESSOR_IS_PRESENT) {
		if ((vm_page_free_reserved + pages + COMPRESSOR_FREE_RESERVED_LIMIT) >= (VM_PAGE_FREE_RESERVED_LIMIT + COMPRESSOR_FREE_RESERVED_LIMIT)) {
			vm_page_free_reserved = VM_PAGE_FREE_RESERVED_LIMIT + COMPRESSOR_FREE_RESERVED_LIMIT;
		} else {
			vm_page_free_reserved += (pages + COMPRESSOR_FREE_RESERVED_LIMIT);
		}
	} else {
		if ((vm_page_free_reserved + pages) >= VM_PAGE_FREE_RESERVED_LIMIT) {
			vm_page_free_reserved = VM_PAGE_FREE_RESERVED_LIMIT;
		} else {
			vm_page_free_reserved += pages;
		}
	}
	free_after_reserve = vm_pageout_state.vm_page_free_count_init - vm_page_free_reserved;

	vm_page_free_min = vm_page_free_reserved +
	    VM_PAGE_FREE_MIN(free_after_reserve);

	if (vm_page_free_min > VM_PAGE_FREE_MIN_LIMIT) {
		vm_page_free_min = VM_PAGE_FREE_MIN_LIMIT;
	}

	vm_page_free_target = vm_page_free_reserved +
	    VM_PAGE_FREE_TARGET(free_after_reserve);

	if (vm_page_free_target > VM_PAGE_FREE_TARGET_LIMIT) {
		vm_page_free_target = VM_PAGE_FREE_TARGET_LIMIT;
	}

	if (vm_page_free_target < vm_page_free_min + 5) {
		vm_page_free_target = vm_page_free_min + 5;
	}

	vm_page_throttle_limit = vm_page_free_target - (vm_page_free_target / 2);
}

/*
 *	vm_pageout is the high level pageout daemon.
 */

void
vm_pageout_continue(void)
{
	DTRACE_VM2(pgrrun, int, 1, (uint64_t *), NULL);
	VM_PAGEOUT_DEBUG(vm_pageout_scan_event_counter, 1);

	vm_free_page_lock();
	vm_pageout_running = TRUE;
	vm_free_page_unlock();

	vm_pageout_scan();
	/*
	 * we hold both the vm_page_queue_free_lock
	 * and the vm_page_queues_lock at this point
	 */
	assert(vm_page_free_wanted == 0);
	assert(vm_page_free_wanted_privileged == 0);
	assert_wait((event_t) &vm_page_free_wanted, THREAD_UNINT);

	vm_pageout_running = FALSE;
#if XNU_TARGET_OS_OSX
	if (vm_pageout_waiter) {
		vm_pageout_waiter = FALSE;
		thread_wakeup((event_t)&vm_pageout_waiter);
	}
#endif /* XNU_TARGET_OS_OSX */

	vm_free_page_unlock();
	vm_page_unlock_queues();

	thread_block((thread_continue_t)vm_pageout_continue);
	/*NOTREACHED*/
}

#if XNU_TARGET_OS_OSX
kern_return_t
vm_pageout_wait(uint64_t deadline)
{
	kern_return_t kr;

	vm_free_page_lock();
	for (kr = KERN_SUCCESS; vm_pageout_running && (KERN_SUCCESS == kr);) {
		vm_pageout_waiter = TRUE;
		if (THREAD_AWAKENED != lck_mtx_sleep_deadline(
			    &vm_page_queue_free_lock, LCK_SLEEP_DEFAULT,
			    (event_t) &vm_pageout_waiter, THREAD_UNINT, deadline)) {
			kr = KERN_OPERATION_TIMED_OUT;
		}
	}
	vm_free_page_unlock();

	return kr;
}
#endif /* XNU_TARGET_OS_OSX */

OS_NORETURN
static void
vm_pageout_iothread_external_continue(struct pgo_iothread_state *ethr, __unused wait_result_t w)
{
	vm_page_t       m = NULL;
	vm_object_t     object;
	vm_object_offset_t offset;
	memory_object_t pager;
	struct vm_pageout_queue *q = ethr->q;

	/* On systems with a compressor, the external IO thread clears its
	 * VM privileged bit to accommodate large allocations (e.g. bulk UPL
	 * creation)
	 */
	if (VM_CONFIG_COMPRESSOR_IS_PRESENT) {
		current_thread()->options &= ~TH_OPT_VMPRIV;
	}

	sched_cond_ack(&(ethr->pgo_wakeup));

	while (true) {
		vm_page_lockspin_queues();

		while (!vm_page_queue_empty(&q->pgo_pending)) {
			q->pgo_busy = TRUE;
			vm_page_queue_remove_first(&q->pgo_pending, m, vmp_pageq);

			assert(m->vmp_q_state == VM_PAGE_ON_PAGEOUT_Q);
			VM_PAGE_CHECK(m);
			/*
			 * grab a snapshot of the object and offset this
			 * page is tabled in so that we can relookup this
			 * page after we've taken the object lock - these
			 * fields are stable while we hold the page queues lock
			 * but as soon as we drop it, there is nothing to keep
			 * this page in this object... we hold an activity_in_progress
			 * on this object which will keep it from terminating
			 */
			object = VM_PAGE_OBJECT(m);
			offset = m->vmp_offset;

			m->vmp_q_state = VM_PAGE_NOT_ON_Q;
			VM_PAGE_ZERO_PAGEQ_ENTRY(m);

			vm_page_unlock_queues();

			vm_object_lock(object);

			m = vm_page_lookup(object, offset);

			if (m == NULL || m->vmp_busy || m->vmp_cleaning ||
			    !m->vmp_laundry || (m->vmp_q_state != VM_PAGE_NOT_ON_Q)) {
				/*
				 * it's either the same page that someone else has
				 * started cleaning (or it's finished cleaning or
				 * been put back on the pageout queue), or
				 * the page has been freed or we have found a
				 * new page at this offset... in all of these cases
				 * we merely need to release the activity_in_progress
				 * we took when we put the page on the pageout queue
				 */
				vm_object_activity_end(object);
				vm_object_unlock(object);

				vm_page_lockspin_queues();
				continue;
			}
			pager = object->pager;

			if (pager == MEMORY_OBJECT_NULL) {
				/*
				 * This pager has been destroyed by either
				 * memory_object_destroy or vm_object_destroy, and
				 * so there is nowhere for the page to go.
				 */
				if (m->vmp_free_when_done) {
					/*
					 * Just free the page... VM_PAGE_FREE takes
					 * care of cleaning up all the state...
					 * including doing the vm_pageout_throttle_up
					 */
					VM_PAGE_FREE(m);
				} else {
					vm_page_lockspin_queues();

					vm_pageout_throttle_up(m);
					vm_page_activate(m);

					vm_page_unlock_queues();

					/*
					 *	And we are done with it.
					 */
				}
				vm_object_activity_end(object);
				vm_object_unlock(object);

				vm_page_lockspin_queues();
				continue;
			}
	#if 0
			/*
			 * we don't hold the page queue lock
			 * so this check isn't safe to make
			 */
			VM_PAGE_CHECK(m);
	#endif
			/*
			 * give back the activity_in_progress reference we
			 * took when we queued up this page and replace it
			 * it with a paging_in_progress reference that will
			 * also hold the paging offset from changing and
			 * prevent the object from terminating
			 */
			vm_object_activity_end(object);
			vm_object_paging_begin(object);
			vm_object_unlock(object);

			/*
			 * Send the data to the pager.
			 * any pageout clustering happens there
			 */
			memory_object_data_return(pager,
			    m->vmp_offset + object->paging_offset,
			    PAGE_SIZE,
			    NULL,
			    NULL,
			    FALSE,
			    FALSE,
			    0);

			vm_object_lock(object);
			vm_object_paging_end(object);
			vm_object_unlock(object);

			vm_pageout_io_throttle();

			vm_page_lockspin_queues();
		}
		q->pgo_busy = FALSE;

		vm_page_unlock_queues();
		sched_cond_wait_parameter(&(ethr->pgo_wakeup), THREAD_UNINT, (thread_continue_t)vm_pageout_iothread_external_continue, ethr);
	}
	/*NOTREACHED*/
}

uint32_t vm_compressor_time_thread; /* Set via sysctl 'vm.compressor_timing_enabled' to record time accrued by this thread. */

#if DEVELOPMENT || DEBUG
static void
vm_pageout_record_thread_time(int cqid, int ncomps)
{
	if (__improbable(vm_compressor_time_thread)) {
		vmct_stats.vmct_runtimes[cqid] = thread_get_runtime_self();
		vmct_stats.vmct_pages[cqid] += ncomps;
		vmct_stats.vmct_iterations[cqid]++;
		if (ncomps > vmct_stats.vmct_maxpages[cqid]) {
			vmct_stats.vmct_maxpages[cqid] = ncomps;
		}
		if (ncomps < vmct_stats.vmct_minpages[cqid]) {
			vmct_stats.vmct_minpages[cqid] = ncomps;
		}
	}
}
#endif

static void *
vm_pageout_select_filling_chead(struct pgo_iothread_state *cq, vm_page_t m)
{
	/*
	 * Technically we need the pageq locks to manipulate the vmp_on_specialq field.
	 * However, this page has been removed from all queues and is only
	 * known to this compressor thread dealing with this local queue.
	 *
	 * TODO: Add a second localq that is the early localq and
	 * put special pages like this one on that queue in the block above
	 * under the pageq lock to avoid this 'works but not clean' logic.
	 */
	void *donate_queue_head;
#if XNU_TARGET_OS_OSX /* tag:DONATE */
	donate_queue_head = &cq->current_early_swapout_chead;
#else /* XNU_TARGET_OS_OSX */
	donate_queue_head = &cq->current_late_swapout_chead;
#endif /* XNU_TARGET_OS_OSX */
	if (m->vmp_on_specialq == VM_PAGE_SPECIAL_Q_DONATE) {
		m->vmp_on_specialq = VM_PAGE_SPECIAL_Q_EMPTY;
		return donate_queue_head;
	} else {
		return &cq->current_regular_swapout_chead;
	}
}

#define         MAX_FREE_BATCH          32

OS_NORETURN
static void
vm_pageout_iothread_internal_continue(struct pgo_iothread_state *cq, __unused wait_result_t w)
{
	struct vm_pageout_queue *q;
	vm_page_t       m = NULL;
	boolean_t       pgo_draining;
	vm_page_t   local_q;
	int         local_cnt;
	vm_page_t   local_freeq = NULL;
	int         local_freed = 0;
	int         local_batch_size;
#if DEVELOPMENT || DEBUG
	int       ncomps = 0;
	boolean_t marked_active = FALSE;
	int       num_pages_processed = 0;
#endif
	void *chead = NULL;

	KDBG_FILTERED(0xe040000c | DBG_FUNC_END);

	sched_cond_ack(&(cq->pgo_wakeup));

	q = cq->q;

	while (true) { /* this top loop is for the compressor_running_perf_test running a full speed without blocking */
#if DEVELOPMENT || DEBUG
		bool benchmark_accounting = false;
		/* If we're running the compressor perf test, only process the benchmark pages.
		 * We'll get back to our regular queue once the benchmark is done */
		if (compressor_running_perf_test) {
			q = cq->benchmark_q;
			if (!vm_page_queue_empty(&q->pgo_pending)) {
				benchmark_accounting = true;
			} else {
				q = cq->q;
				benchmark_accounting = false;
			}
		}
#endif /* DEVELOPMENT || DEBUG */

#if __AMP__
		if (vm_compressor_ebound && (vm_pageout_state.vm_compressor_thread_count > 1)) {
			local_batch_size = (q->pgo_maxlaundry >> 3);
			local_batch_size = MAX(local_batch_size, 16);
		} else {
			local_batch_size = q->pgo_maxlaundry / (vm_pageout_state.vm_compressor_thread_count * 2);
		}
#else
		local_batch_size = q->pgo_maxlaundry / (vm_pageout_state.vm_compressor_thread_count * 2);
#endif

#if RECORD_THE_COMPRESSED_DATA
		if (q->pgo_laundry) {
			c_compressed_record_init();
		}
#endif
		while (true) { /* this loop is for working though all the pages in the pending queue */
			int     pages_left_on_q = 0;

			local_cnt = 0;
			local_q = NULL;

			KDBG_FILTERED(0xe0400014 | DBG_FUNC_START);

			vm_page_lock_queues();
#if DEVELOPMENT || DEBUG
			if (marked_active == FALSE) {
				vmct_active++;
				vmct_state[cq->id] = VMCT_ACTIVE;
				marked_active = TRUE;
				if (vmct_active == 1) {
					vm_compressor_epoch_start = mach_absolute_time();
				}
			}
#endif
			KDBG_FILTERED(0xe0400014 | DBG_FUNC_END);

			KDBG_FILTERED(0xe0400018 | DBG_FUNC_START, q->pgo_laundry);

			/* empty the entire content of the thread input q to local_q, but not more than local_batch_size pages */
			while (!vm_page_queue_empty(&q->pgo_pending) && local_cnt < local_batch_size) {
				vm_page_queue_remove_first(&q->pgo_pending, m, vmp_pageq);
				assert(m->vmp_q_state == VM_PAGE_ON_PAGEOUT_Q);
				VM_PAGE_CHECK(m);

				m->vmp_q_state = VM_PAGE_NOT_ON_Q;
				VM_PAGE_ZERO_PAGEQ_ENTRY(m);
				m->vmp_laundry = FALSE;

				m->vmp_snext = local_q;
				local_q = m;
				local_cnt++;
			}
			if (local_q == NULL) {
				break;
			}

			q->pgo_busy = TRUE;

			if ((pgo_draining = q->pgo_draining) == FALSE) {
				vm_pageout_throttle_up_batch(q, local_cnt);
				pages_left_on_q = q->pgo_laundry;
			} else {
				pages_left_on_q = q->pgo_laundry - local_cnt;
			}

			vm_page_unlock_queues();

#if !RECORD_THE_COMPRESSED_DATA
			/* if we have lots to compress, wake up the other thread to help.
			 * disabled when recording data since record data is not protected with a mutex so this may cause races */
			if (pages_left_on_q >= local_batch_size && cq->id < (vm_pageout_state.vm_compressor_thread_count - 1)) {
				// wake up the next compressor thread
				sched_cond_signal(&pgo_iothread_internal_state[cq->id + 1].pgo_wakeup,
				    pgo_iothread_internal_state[cq->id + 1].pgo_iothread);
			}
#endif
			KDBG_FILTERED(0xe0400018 | DBG_FUNC_END, q->pgo_laundry);

			while (local_q) {
				KDBG_FILTERED(0xe0400024 | DBG_FUNC_START, local_cnt);

				m = local_q;
				local_q = m->vmp_snext;
				m->vmp_snext = NULL;


				chead = vm_pageout_select_filling_chead(cq, m);

				if (vm_pageout_compress_page(chead, cq->scratch_buf, m) == KERN_SUCCESS) {
#if DEVELOPMENT || DEBUG
					ncomps++;
#endif
					KDBG_FILTERED(0xe0400024 | DBG_FUNC_END, local_cnt);

					m->vmp_snext = local_freeq;
					local_freeq = m;
					local_freed++;

					/* if we gathered enough free pages, free them now */
					if (local_freed >= MAX_FREE_BATCH) {
						OSAddAtomic64(local_freed, &vm_pageout_vminfo.vm_pageout_compressions);

						vm_page_free_list(local_freeq, TRUE);

						local_freeq = NULL;
						local_freed = 0;
					}
				}
#if DEVELOPMENT || DEBUG
				num_pages_processed++;
#endif /* DEVELOPMENT || DEBUG */
#if !CONFIG_JETSAM /* Maybe: if there's no JETSAM, be more proactive in waking up anybody that needs free pages */
				while (vm_page_free_count < COMPRESSOR_FREE_RESERVED_LIMIT) {
					kern_return_t   wait_result;
					int             need_wakeup = 0;

					if (local_freeq) {
						OSAddAtomic64(local_freed, &vm_pageout_vminfo.vm_pageout_compressions);

						vm_page_free_list(local_freeq, TRUE);
						local_freeq = NULL;
						local_freed = 0;

						continue;
					}
					vm_free_page_lock_spin();

					if (vm_page_free_count < COMPRESSOR_FREE_RESERVED_LIMIT) {
						if (vm_page_free_wanted_privileged++ == 0) {
							need_wakeup = 1;
						}
						wait_result = assert_wait((event_t)&vm_page_free_wanted_privileged, THREAD_UNINT);

						vm_free_page_unlock();

						if (need_wakeup) {
							thread_wakeup((event_t)&vm_page_free_wanted);
						}

						if (wait_result == THREAD_WAITING) {
							thread_block(THREAD_CONTINUE_NULL);
						}
					} else {
						vm_free_page_unlock();
					}
				}
#endif
			}  /* while (local_q) */
			/* free any leftovers in the freeq */
			if (local_freeq) {
				OSAddAtomic64(local_freed, &vm_pageout_vminfo.vm_pageout_compressions);

				vm_page_free_list(local_freeq, TRUE);
				local_freeq = NULL;
				local_freed = 0;
			}
			if (pgo_draining == TRUE) {
				vm_page_lockspin_queues();
				vm_pageout_throttle_up_batch(q, local_cnt);
				vm_page_unlock_queues();
			}
		}
		KDBG_FILTERED(0xe040000c | DBG_FUNC_START);

		/*
		 * queue lock is held and our q is empty
		 */
		q->pgo_busy = FALSE;
#if DEVELOPMENT || DEBUG
		if (marked_active == TRUE) {
			vmct_active--;
			vmct_state[cq->id] = VMCT_IDLE;

			if (vmct_active == 0) {
				vm_compressor_epoch_stop = mach_absolute_time();
				assertf(vm_compressor_epoch_stop >= vm_compressor_epoch_start,
				    "Compressor epoch non-monotonic: 0x%llx -> 0x%llx",
				    vm_compressor_epoch_start, vm_compressor_epoch_stop);
				/* This interval includes intervals where one or more
				 * compressor threads were pre-empted
				 */
				vmct_stats.vmct_cthreads_total += vm_compressor_epoch_stop - vm_compressor_epoch_start;
			}
		}
		if (compressor_running_perf_test && benchmark_accounting) {
			/*
			 * We could turn ON compressor_running_perf_test while still processing
			 * regular non-benchmark pages. We shouldn't count them here else we
			 * could overshoot. We might also still be populating that benchmark Q
			 * and be under pressure. So we will go back to the regular queues. And
			 * benchmark accounting will be off for that case too.
			 */
			compressor_perf_test_pages_processed += num_pages_processed;
			thread_wakeup(&compressor_perf_test_pages_processed);
		}
#endif
		vm_page_unlock_queues();
#if DEVELOPMENT || DEBUG
		vm_pageout_record_thread_time(cq->id, ncomps);
#endif

		KDBG_FILTERED(0xe0400018 | DBG_FUNC_END);
#if DEVELOPMENT || DEBUG
		if (compressor_running_perf_test && benchmark_accounting) {
			/*
			 * We've been exclusively compressing pages from the benchmark queue,
			 * do 1 pass over the internal queue before blocking.
			 */
			continue;
		}
#endif

		sched_cond_wait_parameter(&(cq->pgo_wakeup), THREAD_UNINT, (thread_continue_t)vm_pageout_iothread_internal_continue, (void *) cq);
	}
	/*NOTREACHED*/
}

/* resolves the pager and maintain stats in the pager and in the vm_object */
kern_return_t
vm_pageout_compress_page(void **current_chead, char *scratch_buf, vm_page_t m)
{
	vm_object_t     object;
	memory_object_t pager;
	int             compressed_count_delta;
	kern_return_t   retval;

	object = VM_PAGE_OBJECT(m);

	assert(!m->vmp_free_when_done);
	assert(!m->vmp_laundry);

	pager = object->pager;

	if (!object->pager_initialized || pager == MEMORY_OBJECT_NULL) {
		KDBG_FILTERED(0xe0400010 | DBG_FUNC_START, object, pager);

		vm_object_lock(object);

		/*
		 * If there is no memory object for the page, create
		 * one and hand it to the compression pager.
		 */

		if (!object->pager_initialized) {
			vm_object_collapse(object, (vm_object_offset_t) 0, TRUE);
		}
		if (!object->pager_initialized) {
			vm_object_compressor_pager_create(object);
		}

		pager = object->pager;

		if (!object->pager_initialized || pager == MEMORY_OBJECT_NULL) {
			/*
			 * Still no pager for the object,
			 * or the pager has been destroyed.
			 * Reactivate the page.
			 *
			 * Should only happen if there is no
			 * compression pager
			 */
			vm_page_wakeup_done(object, m);

			vm_page_lockspin_queues();
			vm_page_activate(m);
			VM_PAGEOUT_DEBUG(vm_pageout_dirty_no_pager, 1);
			vm_page_unlock_queues();

			/*
			 *	And we are done with it.
			 */
			vm_object_activity_end(object);
			vm_object_unlock(object);

			return KERN_FAILURE;
		}
		vm_object_unlock(object);

		KDBG_FILTERED(0xe0400010 | DBG_FUNC_END, object, pager);
	}
	assert(object->pager_initialized && pager != MEMORY_OBJECT_NULL);
	assert(object->activity_in_progress > 0);

#if CONFIG_TRACK_UNMODIFIED_ANON_PAGES
	if (m->vmp_unmodified_ro == true) {
		os_atomic_inc(&compressor_ro_uncompressed_total_returned, relaxed);
	}
#endif /* CONFIG_TRACK_UNMODIFIED_ANON_PAGES */

	vm_compressor_options_t flags = 0;

#if CONFIG_TRACK_UNMODIFIED_ANON_PAGES
	if (m->vmp_unmodified_ro) {
		flags |= C_PAGE_UNMODIFIED;
	}
#endif /* CONFIG_TRACK_UNMODIFIED_ANON_PAGES */


	retval = vm_compressor_pager_put(
		pager,
		m->vmp_offset + object->paging_offset,
		VM_PAGE_GET_PHYS_PAGE(m),
		current_chead,
		scratch_buf,
		&compressed_count_delta,
		flags);

	vm_object_lock(object);

	assert(object->activity_in_progress > 0);
	assert(VM_PAGE_OBJECT(m) == object);
	assert( !VM_PAGE_WIRED(m));

	vm_compressor_pager_count(pager,
	    compressed_count_delta,
	    FALSE,                       /* shared_lock */
	    object);

	if (retval == KERN_SUCCESS) {
		/*
		 * If the object is purgeable, its owner's
		 * purgeable ledgers will be updated in
		 * vm_page_remove() but the page still
		 * contributes to the owner's memory footprint,
		 * so account for it as such.
		 */
		if (m->vmp_tabled) {
			vm_page_remove(m, TRUE);
		}
		if ((object->purgable != VM_PURGABLE_DENY ||
		    object->vo_ledger_tag) &&
		    object->vo_owner != NULL) {
			/* one more compressed purgeable/tagged page */
			vm_object_owner_compressed_update(object,
			    compressed_count_delta);
		}
		counter_inc(&vm_statistics_compressions);
	} else {
		vm_page_wakeup_done(object, m);

		vm_page_lockspin_queues();

		vm_page_activate(m);
		vm_pageout_vminfo.vm_compressor_failed++;

		vm_page_unlock_queues();
	}
	vm_object_activity_end(object);
	vm_object_unlock(object);

	return retval;
}


static void
vm_pageout_adjust_eq_iothrottle(struct pgo_iothread_state *ethr, boolean_t req_lowpriority)
{
	uint32_t        policy;

	if (hibernate_cleaning_in_progress == TRUE) {
		req_lowpriority = FALSE;
	}

	if (ethr->q->pgo_inited == TRUE && ethr->q->pgo_lowpriority != req_lowpriority) {
		vm_page_unlock_queues();

		if (req_lowpriority == TRUE) {
			policy = THROTTLE_LEVEL_PAGEOUT_THROTTLED;
			DTRACE_VM(laundrythrottle);
		} else {
			policy = THROTTLE_LEVEL_PAGEOUT_UNTHROTTLED;
			DTRACE_VM(laundryunthrottle);
		}
		proc_set_thread_policy(ethr->pgo_iothread,
		    TASK_POLICY_EXTERNAL, TASK_POLICY_IO, policy);

		vm_page_lock_queues();
		ethr->q->pgo_lowpriority = req_lowpriority;
	}
}

OS_NORETURN
static void
vm_pageout_iothread_external(struct pgo_iothread_state *ethr, __unused wait_result_t w)
{
	thread_t        self = current_thread();

	self->options |= TH_OPT_VMPRIV;

	DTRACE_VM2(laundrythrottle, int, 1, (uint64_t *), NULL);

	proc_set_thread_policy(self, TASK_POLICY_EXTERNAL,
	    TASK_POLICY_IO, THROTTLE_LEVEL_PAGEOUT_THROTTLED);

	vm_page_lock_queues();

	vm_pageout_queue_external.pgo_lowpriority = TRUE;
	vm_pageout_queue_external.pgo_inited = TRUE;

	vm_page_unlock_queues();

#if CONFIG_THREAD_GROUPS
	thread_group_vm_add();
#endif /* CONFIG_THREAD_GROUPS */

	vm_pageout_iothread_external_continue(ethr, 0);
	/*NOTREACHED*/
}


OS_NORETURN
static void
vm_pageout_iothread_internal(struct pgo_iothread_state *cthr, __unused wait_result_t w)
{
	thread_t        self = current_thread();

	self->options |= TH_OPT_VMPRIV;

	vm_page_lock_queues();

	vm_pageout_queue_internal.pgo_lowpriority = TRUE;
	vm_pageout_queue_internal.pgo_inited = TRUE;

#if DEVELOPMENT || DEBUG
	vm_pageout_queue_benchmark.pgo_lowpriority = vm_pageout_queue_internal.pgo_lowpriority;
	vm_pageout_queue_benchmark.pgo_inited = vm_pageout_queue_internal.pgo_inited;
	vm_pageout_queue_benchmark.pgo_busy = FALSE;
#endif /* DEVELOPMENT || DEBUG */

	vm_page_unlock_queues();

	if (vm_pageout_state.vm_restricted_to_single_processor == TRUE) {
		thread_vm_bind_group_add();
	}

#if CONFIG_THREAD_GROUPS
	thread_group_vm_add();
#endif /* CONFIG_THREAD_GROUPS */

#if __AMP__
	if (vm_compressor_ebound) {
		/*
		 * Use the soft bound option for vm_compressor to allow it to run on
		 * P-cores if E-cluster is unavailable.
		 */
		thread_bind_cluster_type(self, 'E', true);
	}
#endif /* __AMP__ */

	thread_set_thread_name(current_thread(), "VM_compressor");
#if DEVELOPMENT || DEBUG
	vmct_stats.vmct_minpages[cthr->id] = INT32_MAX;
#endif
	vm_pageout_iothread_internal_continue(cthr, 0);

	/*NOTREACHED*/
}

kern_return_t
vm_set_buffer_cleanup_callout(boolean_t (*func)(int))
{
	if (OSCompareAndSwapPtr(NULL, ptrauth_nop_cast(void *, func), (void * volatile *) &consider_buffer_cache_collect)) {
		return KERN_SUCCESS;
	} else {
		return KERN_FAILURE; /* Already set */
	}
}

extern boolean_t        memorystatus_manual_testing_on;
extern unsigned int     memorystatus_level;


#if VM_PRESSURE_EVENTS

boolean_t vm_pressure_events_enabled = FALSE;

extern uint64_t next_warning_notification_sent_at_ts;
extern uint64_t next_critical_notification_sent_at_ts;

#define PRESSURE_LEVEL_STUCK_THRESHOLD_MINS    (30)    /* 30 minutes. */

/*
 * The last time there was change in pressure level OR we forced a check
 * because the system is stuck in a non-normal pressure level.
 */
uint64_t  vm_pressure_last_level_transition_abs = 0;

/*
 *  This is how the long the system waits 'stuck' in an unchanged non-normal pressure
 * level before resending out notifications for that level again.
 */
int  vm_pressure_level_transition_threshold = PRESSURE_LEVEL_STUCK_THRESHOLD_MINS;

void
vm_pressure_response(void)
{
	vm_pressure_level_t     old_level = kVMPressureNormal;
	int                     new_level = -1;
	unsigned int            total_pages;
	uint64_t                available_memory = 0;
	uint64_t                curr_ts, abs_time_since_level_transition, time_in_ns;
	bool                    force_check = false;
	int                     time_in_mins;


	if (vm_pressure_events_enabled == FALSE) {
		return;
	}

#if !XNU_TARGET_OS_OSX

	available_memory = (uint64_t) memorystatus_available_pages;

#else /* !XNU_TARGET_OS_OSX */

	available_memory = (uint64_t) AVAILABLE_NON_COMPRESSED_MEMORY;
	memorystatus_available_pages = (uint64_t) AVAILABLE_NON_COMPRESSED_MEMORY;

#endif /* !XNU_TARGET_OS_OSX */

	total_pages = (unsigned int) atop_64(max_mem);
#if CONFIG_SECLUDED_MEMORY
	total_pages -= vm_page_secluded_count;
#endif /* CONFIG_SECLUDED_MEMORY */
	memorystatus_level = (unsigned int) ((available_memory * 100) / total_pages);

	if (memorystatus_manual_testing_on) {
		return;
	}

	curr_ts = mach_absolute_time();
	abs_time_since_level_transition = curr_ts - vm_pressure_last_level_transition_abs;

	absolutetime_to_nanoseconds(abs_time_since_level_transition, &time_in_ns);
	time_in_mins = (int) ((time_in_ns / NSEC_PER_SEC) / 60);
	force_check = (time_in_mins >= vm_pressure_level_transition_threshold);

	old_level = memorystatus_vm_pressure_level;

	switch (memorystatus_vm_pressure_level) {
	case kVMPressureNormal:
	{
		if (VM_PRESSURE_WARNING_TO_CRITICAL()) {
			new_level = kVMPressureCritical;
		} else if (VM_PRESSURE_NORMAL_TO_WARNING()) {
			new_level = kVMPressureWarning;
		}
		break;
	}

	case kVMPressureWarning:
	case kVMPressureUrgent:
	{
		if (VM_PRESSURE_WARNING_TO_NORMAL()) {
			new_level = kVMPressureNormal;
		} else if (VM_PRESSURE_WARNING_TO_CRITICAL()) {
			new_level = kVMPressureCritical;
		} else if (force_check) {
			new_level = kVMPressureWarning;
			next_warning_notification_sent_at_ts = curr_ts;
		}
		break;
	}

	case kVMPressureCritical:
	{
		if (VM_PRESSURE_WARNING_TO_NORMAL()) {
			new_level = kVMPressureNormal;
		} else if (VM_PRESSURE_CRITICAL_TO_WARNING()) {
			new_level = kVMPressureWarning;
		} else if (force_check) {
			new_level = kVMPressureCritical;
			next_critical_notification_sent_at_ts = curr_ts;
		}
		break;
	}

	default:
		return;
	}

	if (new_level != -1 || force_check) {
		if (new_level != -1) {
			memorystatus_vm_pressure_level = (vm_pressure_level_t) new_level;

			if (new_level != (int) old_level) {
				VM_DEBUG_CONSTANT_EVENT(vm_pressure_level_change, DBG_VM_PRESSURE_LEVEL_CHANGE, DBG_FUNC_NONE,
				    new_level, old_level, 0, 0);
			}
		} else {
			VM_DEBUG_CONSTANT_EVENT(vm_pressure_level_change, DBG_VM_PRESSURE_LEVEL_CHANGE, DBG_FUNC_NONE,
			    new_level, old_level, force_check, 0);
		}

		if (hibernation_vmqueues_inspection || hibernate_cleaning_in_progress) {
			/*
			 * We don't want to schedule a wakeup while hibernation is in progress
			 * because that could collide with checks for non-monotonicity in the scheduler.
			 * We do however do all the updates to memorystatus_vm_pressure_level because
			 * we _might_ want to use that for decisions regarding which pages or how
			 * many pages we want to dump in hibernation.
			 */
			return;
		}

		if ((memorystatus_vm_pressure_level != kVMPressureNormal) || (old_level != memorystatus_vm_pressure_level) || force_check) {
			if (vm_pageout_state.vm_pressure_thread_running == FALSE) {
				thread_wakeup(&vm_pressure_thread);
			}

			if (old_level != memorystatus_vm_pressure_level) {
				thread_wakeup(&vm_pageout_state.vm_pressure_changed);
			}
			vm_pressure_last_level_transition_abs = curr_ts; /* renew the window of observation for a stuck pressure level */
		}
	}
}
#endif /* VM_PRESSURE_EVENTS */


/**
 * Called by a kernel thread to ask if a number of pages may be wired.
 */
kern_return_t
mach_vm_wire_level_monitor(int64_t requested_pages)
{
	if (requested_pages <= 0) {
		return KERN_INVALID_ARGUMENT;
	}

	const int64_t max_wire_pages = atop_64(vm_global_user_wire_limit);
	/**
	 * Available pages can be negative in the case where more system memory is
	 * wired than the threshold, so we must use a signed integer.
	 */
	const int64_t available_pages = max_wire_pages - vm_page_wire_count;

	if (requested_pages > available_pages) {
		return KERN_RESOURCE_SHORTAGE;
	}
	return KERN_SUCCESS;
}

/*
 * Function called by a kernel thread to either get the current pressure level or
 * wait until memory pressure changes from a given level.
 */
kern_return_t
mach_vm_pressure_level_monitor(boolean_t wait_for_pressure, unsigned int *pressure_level)
{
#if !VM_PRESSURE_EVENTS
	(void)wait_for_pressure;
	(void)pressure_level;
	return KERN_NOT_SUPPORTED;
#else /* VM_PRESSURE_EVENTS */

	uint32_t *waiters = NULL;
	wait_result_t wr = 0;
	vm_pressure_level_t old_level = memorystatus_vm_pressure_level;

	if (pressure_level == NULL) {
		return KERN_INVALID_ARGUMENT;
	}
	if (!wait_for_pressure && (*pressure_level == kVMPressureBackgroundJetsam ||
	    *pressure_level == kVMPressureForegroundJetsam)) {
		return KERN_INVALID_ARGUMENT;
	}

	if (wait_for_pressure) {
		switch (*pressure_level) {
		case kVMPressureForegroundJetsam:
		case kVMPressureBackgroundJetsam:

			if (*pressure_level == kVMPressureForegroundJetsam) {
				waiters = &memorystatus_jetsam_fg_band_waiters;
			} else {
				/* kVMPressureBackgroundJetsam */
				waiters = &memorystatus_jetsam_bg_band_waiters;
			}

			lck_mtx_lock(&memorystatus_jetsam_broadcast_lock);
			wr = assert_wait((event_t)waiters, THREAD_INTERRUPTIBLE);
			if (wr == THREAD_WAITING) {
				*waiters += 1;
				lck_mtx_unlock(&memorystatus_jetsam_broadcast_lock);
				wr = thread_block(THREAD_CONTINUE_NULL);
			} else {
				lck_mtx_unlock(&memorystatus_jetsam_broadcast_lock);
			}

			if (wr != THREAD_AWAKENED) {
				return KERN_ABORTED;
			}

			return KERN_SUCCESS;
		case kVMPressureNormal:
		case kVMPressureWarning:
		case kVMPressureUrgent:
		case kVMPressureCritical:
			while (old_level == *pressure_level) {
				wr = assert_wait((event_t) &vm_pageout_state.vm_pressure_changed,
				    THREAD_INTERRUPTIBLE);
				if (wr == THREAD_WAITING) {
					wr = thread_block(THREAD_CONTINUE_NULL);
				}
				if (wr == THREAD_INTERRUPTED) {
					return KERN_ABORTED;
				}

				if (wr == THREAD_AWAKENED) {
					old_level = memorystatus_vm_pressure_level;
				}
			}
			break;
		default:
			return KERN_INVALID_ARGUMENT;
		}
	}

	*pressure_level = old_level;
	return KERN_SUCCESS;
#endif /* VM_PRESSURE_EVENTS */
}

#if VM_PRESSURE_EVENTS
void
vm_pressure_thread(void)
{
	static boolean_t thread_initialized = FALSE;

	if (thread_initialized == TRUE) {
		vm_pageout_state.vm_pressure_thread_running = TRUE;
		consider_vm_pressure_events();
		vm_pageout_state.vm_pressure_thread_running = FALSE;
	}

#if CONFIG_THREAD_GROUPS
	thread_group_vm_add();
#endif /* CONFIG_THREAD_GROUPS */

	thread_set_thread_name(current_thread(), "VM_pressure");
	thread_initialized = TRUE;
	assert_wait((event_t) &vm_pressure_thread, THREAD_UNINT);
	thread_block((thread_continue_t)vm_pressure_thread);
}
#endif /* VM_PRESSURE_EVENTS */


/*
 * called once per-second via "compute_averages"
 */
void
compute_pageout_gc_throttle(__unused void *arg)
{
	if (vm_pageout_vminfo.vm_pageout_considered_page != vm_pageout_state.vm_pageout_considered_page_last) {
		vm_pageout_state.vm_pageout_considered_page_last = vm_pageout_vminfo.vm_pageout_considered_page;

		thread_wakeup(VM_PAGEOUT_GC_EVENT);
	}
}

/*
 * vm_pageout_garbage_collect can also be called when the zone allocator needs
 * to call zone_gc on a different thread in order to trigger zone-map-exhaustion
 * jetsams. We need to check if the zone map size is above its jetsam limit to
 * decide if this was indeed the case.
 *
 * We need to do this on a different thread because of the following reasons:
 *
 * 1. In the case of synchronous jetsams, the leaking process can try to jetsam
 * itself causing the system to hang. We perform synchronous jetsams if we're
 * leaking in the VM map entries zone, so the leaking process could be doing a
 * zalloc for a VM map entry while holding its vm_map lock, when it decides to
 * jetsam itself. We also need the vm_map lock on the process termination path,
 * which would now lead the dying process to deadlock against itself.
 *
 * 2. The jetsam path might need to allocate zone memory itself. We could try
 * using the non-blocking variant of zalloc for this path, but we can still
 * end up trying to do a kmem_alloc when the zone maps are almost full.
 */
__dead2
void
vm_pageout_garbage_collect(void *step, wait_result_t wr __unused)
{
	assert(step == VM_PAGEOUT_GC_INIT || step == VM_PAGEOUT_GC_COLLECT);

	if (step == VM_PAGEOUT_GC_INIT) {
		/* first time being called is not about GC */
#if CONFIG_THREAD_GROUPS
		thread_group_vm_add();
#endif /* CONFIG_THREAD_GROUPS */
	} else if (zone_map_nearing_exhaustion()) {
		/*
		 * Woken up by the zone allocator for zone-map-exhaustion jetsams.
		 *
		 * Bail out after calling zone_gc (which triggers the
		 * zone-map-exhaustion jetsams). If we fall through, the subsequent
		 * operations that clear out a bunch of caches might allocate zone
		 * memory themselves (for eg. vm_map operations would need VM map
		 * entries). Since the zone map is almost full at this point, we
		 * could end up with a panic. We just need to quickly jetsam a
		 * process and exit here.
		 *
		 * It could so happen that we were woken up to relieve memory
		 * pressure and the zone map also happened to be near its limit at
		 * the time, in which case we'll skip out early. But that should be
		 * ok; if memory pressure persists, the thread will simply be woken
		 * up again.
		 */
		zone_gc(ZONE_GC_JETSAM);
	} else {
		/* Woken up by vm_pageout_scan or compute_pageout_gc_throttle. */
		boolean_t buf_large_zfree = FALSE;
		boolean_t first_try = TRUE;

		stack_collect();

		consider_machine_collect();
#if CONFIG_MBUF_MCACHE
		mbuf_drain(FALSE);
#endif /* CONFIG_MBUF_MCACHE */

		do {
			if (consider_buffer_cache_collect != NULL) {
				buf_large_zfree = (*consider_buffer_cache_collect)(0);
			}
			if (first_try == TRUE || buf_large_zfree == TRUE) {
				/*
				 * zone_gc should be last, because the other operations
				 * might return memory to zones.
				 */
				zone_gc(ZONE_GC_TRIM);
			}
			first_try = FALSE;
		} while (buf_large_zfree == TRUE && vm_page_free_count < vm_page_free_target);

		consider_machine_adjust();
	}

	assert_wait(VM_PAGEOUT_GC_EVENT, THREAD_UNINT);

	thread_block_parameter(vm_pageout_garbage_collect, VM_PAGEOUT_GC_COLLECT);
	__builtin_unreachable();
}


#if VM_PAGE_BUCKETS_CHECK
#if VM_PAGE_FAKE_BUCKETS
extern vm_map_offset_t vm_page_fake_buckets_start, vm_page_fake_buckets_end;
#endif /* VM_PAGE_FAKE_BUCKETS */
#endif /* VM_PAGE_BUCKETS_CHECK */



void
vm_set_restrictions(unsigned int num_cpus)
{
	int vm_restricted_to_single_processor = 0;

	if (PE_parse_boot_argn("vm_restricted_to_single_processor", &vm_restricted_to_single_processor, sizeof(vm_restricted_to_single_processor))) {
		kprintf("Overriding vm_restricted_to_single_processor to %d\n", vm_restricted_to_single_processor);
		vm_pageout_state.vm_restricted_to_single_processor = (vm_restricted_to_single_processor ? TRUE : FALSE);
	} else {
		assert(num_cpus > 0);

		if (num_cpus <= 3) {
			/*
			 * on systems with a limited number of CPUS, bind the
			 * 4 major threads that can free memory and that tend to use
			 * a fair bit of CPU under pressured conditions to a single processor.
			 * This insures that these threads don't hog all of the available CPUs
			 * (important for camera launch), while allowing them to run independently
			 * w/r to locks... the 4 threads are
			 * vm_pageout_scan,  vm_pageout_iothread_internal (compressor),
			 * vm_compressor_swap_trigger_thread (minor and major compactions),
			 * memorystatus_thread (jetsams).
			 *
			 * the first time the thread is run, it is responsible for checking the
			 * state of vm_restricted_to_single_processor, and if TRUE it calls
			 * thread_bind_master...  someday this should be replaced with a group
			 * scheduling mechanism and KPI.
			 */
			vm_pageout_state.vm_restricted_to_single_processor = TRUE;
		} else {
			vm_pageout_state.vm_restricted_to_single_processor = FALSE;
		}
	}
}

/*
 * Set up vm_config based on the vm_compressor_mode.
 * Must run BEFORE the pageout thread starts up.
 */
__startup_func
void
vm_config_init(void)
{
	bzero(&vm_config, sizeof(vm_config));

	switch (vm_compressor_mode) {
	case VM_PAGER_DEFAULT:
		printf("mapping deprecated VM_PAGER_DEFAULT to VM_PAGER_COMPRESSOR_WITH_SWAP\n");
		OS_FALLTHROUGH;

	case VM_PAGER_COMPRESSOR_WITH_SWAP:
		vm_config.compressor_is_present = TRUE;
		vm_config.swap_is_present = TRUE;
		vm_config.compressor_is_active = TRUE;
		vm_config.swap_is_active = TRUE;
		break;

	case VM_PAGER_COMPRESSOR_NO_SWAP:
		vm_config.compressor_is_present = TRUE;
		vm_config.swap_is_present = TRUE;
		vm_config.compressor_is_active = TRUE;
		break;

	case VM_PAGER_FREEZER_DEFAULT:
		printf("mapping deprecated VM_PAGER_FREEZER_DEFAULT to VM_PAGER_FREEZER_COMPRESSOR_NO_SWAP\n");
		OS_FALLTHROUGH;

	case VM_PAGER_FREEZER_COMPRESSOR_NO_SWAP:
		vm_config.compressor_is_present = TRUE;
		vm_config.swap_is_present = TRUE;
		break;

	case VM_PAGER_COMPRESSOR_NO_SWAP_PLUS_FREEZER_COMPRESSOR_WITH_SWAP:
		vm_config.compressor_is_present = TRUE;
		vm_config.swap_is_present = TRUE;
		vm_config.compressor_is_active = TRUE;
		vm_config.freezer_swap_is_active = TRUE;
		break;

	case VM_PAGER_NOT_CONFIGURED:
		break;

	default:
		printf("unknown compressor mode - %x\n", vm_compressor_mode);
		break;
	}
}

__startup_func
static void
vm_pageout_create_gc_thread(void)
{
	thread_t thread;

	if (kernel_thread_create(vm_pageout_garbage_collect,
	    VM_PAGEOUT_GC_INIT, BASEPRI_DEFAULT, &thread) != KERN_SUCCESS) {
		panic("vm_pageout_garbage_collect: create failed");
	}
	thread_set_thread_name(thread, "VM_pageout_garbage_collect");
	if (thread->reserved_stack == 0) {
		assert(thread->kernel_stack);
		thread->reserved_stack = thread->kernel_stack;
	}

	/* thread is started in vm_pageout() */
	vm_pageout_gc_thread = thread;
}
STARTUP(EARLY_BOOT, STARTUP_RANK_MIDDLE, vm_pageout_create_gc_thread);

void
vm_pageout(void)
{
	thread_t        self = current_thread();
	thread_t        thread;
	kern_return_t   result;
	spl_t           s;

	/*
	 * Set thread privileges.
	 */
	s = splsched();

#if CONFIG_VPS_DYNAMIC_PRIO
	if (vps_dynamic_priority_enabled) {
		sched_set_kernel_thread_priority(self, MAXPRI_THROTTLE);
		thread_set_eager_preempt(self);
	} else {
		sched_set_kernel_thread_priority(self, BASEPRI_VM);
	}
#else /* CONFIG_VPS_DYNAMIC_PRIO */
	sched_set_kernel_thread_priority(self, BASEPRI_VM);
#endif /* CONFIG_VPS_DYNAMIC_PRIO */

	thread_lock(self);
	self->options |= TH_OPT_VMPRIV;
	thread_unlock(self);

	if (!self->reserved_stack) {
		self->reserved_stack = self->kernel_stack;
	}

	if (vm_pageout_state.vm_restricted_to_single_processor == TRUE &&
	    !vps_dynamic_priority_enabled) {
		thread_vm_bind_group_add();
	}


#if CONFIG_THREAD_GROUPS
	thread_group_vm_add();
#endif /* CONFIG_THREAD_GROUPS */

#if __AMP__
	PE_parse_boot_argn("vmpgo_pcluster", &vm_pgo_pbound, sizeof(vm_pgo_pbound));
	if (vm_pgo_pbound) {
		/*
		 * Use the soft bound option for vm pageout to allow it to run on
		 * E-cores if P-cluster is unavailable.
		 */
		thread_bind_cluster_type(self, 'P', true);
	}
#endif /* __AMP__ */

	PE_parse_boot_argn("vmpgo_protect_realtime",
	    &vm_pageout_protect_realtime,
	    sizeof(vm_pageout_protect_realtime));
	splx(s);

	thread_set_thread_name(current_thread(), "VM_pageout_scan");

	/*
	 *	Initialize some paging parameters.
	 */

	vm_pageout_state.vm_pressure_thread_running = FALSE;
	vm_pageout_state.vm_pressure_changed = FALSE;
	vm_pageout_state.memorystatus_purge_on_warning = 2;
	vm_pageout_state.memorystatus_purge_on_urgent = 5;
	vm_pageout_state.memorystatus_purge_on_critical = 8;
	vm_pageout_state.vm_page_speculative_q_age_ms = VM_PAGE_SPECULATIVE_Q_AGE_MS;
	vm_pageout_state.vm_page_speculative_percentage = 5;
	vm_pageout_state.vm_page_speculative_target = 0;

	vm_pageout_state.vm_pageout_swap_wait = 0;
	vm_pageout_state.vm_pageout_idle_wait = 0;
	vm_pageout_state.vm_pageout_empty_wait = 0;
	vm_pageout_state.vm_pageout_burst_wait = 0;
	vm_pageout_state.vm_pageout_deadlock_wait = 0;
	vm_pageout_state.vm_pageout_deadlock_relief = 0;
	vm_pageout_state.vm_pageout_burst_inactive_throttle = 0;

	vm_pageout_state.vm_pageout_inactive = 0;
	vm_pageout_state.vm_pageout_inactive_used = 0;
	vm_pageout_state.vm_pageout_inactive_clean = 0;

	vm_pageout_state.vm_memory_pressure = 0;
	vm_pageout_state.vm_page_filecache_min = 0;
#if CONFIG_JETSAM
	vm_pageout_state.vm_page_filecache_min_divisor = 70;
	vm_pageout_state.vm_page_xpmapped_min_divisor = 40;
#else
	vm_pageout_state.vm_page_filecache_min_divisor = 27;
	vm_pageout_state.vm_page_xpmapped_min_divisor = 36;
#endif
	vm_pageout_state.vm_page_free_count_init = vm_page_free_count;

	vm_pageout_state.vm_pageout_considered_page_last = 0;

	if (vm_pageout_state.vm_pageout_swap_wait == 0) {
		vm_pageout_state.vm_pageout_swap_wait = VM_PAGEOUT_SWAP_WAIT;
	}

	if (vm_pageout_state.vm_pageout_idle_wait == 0) {
		vm_pageout_state.vm_pageout_idle_wait = VM_PAGEOUT_IDLE_WAIT;
	}

	if (vm_pageout_state.vm_pageout_burst_wait == 0) {
		vm_pageout_state.vm_pageout_burst_wait = VM_PAGEOUT_BURST_WAIT;
	}

	if (vm_pageout_state.vm_pageout_empty_wait == 0) {
		vm_pageout_state.vm_pageout_empty_wait = VM_PAGEOUT_EMPTY_WAIT;
	}

	if (vm_pageout_state.vm_pageout_deadlock_wait == 0) {
		vm_pageout_state.vm_pageout_deadlock_wait = VM_PAGEOUT_DEADLOCK_WAIT;
	}

	if (vm_pageout_state.vm_pageout_deadlock_relief == 0) {
		vm_pageout_state.vm_pageout_deadlock_relief = VM_PAGEOUT_DEADLOCK_RELIEF;
	}

	if (vm_pageout_state.vm_pageout_burst_inactive_throttle == 0) {
		vm_pageout_state.vm_pageout_burst_inactive_throttle = VM_PAGEOUT_BURST_INACTIVE_THROTTLE;
	}
	/*
	 * even if we've already called vm_page_free_reserve
	 * call it again here to insure that the targets are
	 * accurately calculated (it uses vm_page_free_count_init)
	 * calling it with an arg of 0 will not change the reserve
	 * but will re-calculate free_min and free_target
	 */
	if (vm_page_free_reserved < VM_PAGE_FREE_RESERVED(processor_count)) {
		vm_page_free_reserve((VM_PAGE_FREE_RESERVED(processor_count)) - vm_page_free_reserved);
	} else {
		vm_page_free_reserve(0);
	}

	bzero(&vm_pageout_queue_external, sizeof(struct vm_pageout_queue));
	bzero(&vm_pageout_queue_internal, sizeof(struct vm_pageout_queue));

	vm_page_queue_init(&vm_pageout_queue_external.pgo_pending);
	vm_pageout_queue_external.pgo_maxlaundry = VM_PAGE_LAUNDRY_MAX;

	vm_page_queue_init(&vm_pageout_queue_internal.pgo_pending);

#if DEVELOPMENT || DEBUG
	bzero(&vm_pageout_queue_benchmark, sizeof(struct vm_pageout_queue));
	vm_page_queue_init(&vm_pageout_queue_benchmark.pgo_pending);
#endif /* DEVELOPMENT || DEBUG */


	/* internal pageout thread started when default pager registered first time */
	/* external pageout and garbage collection threads started here */
	struct pgo_iothread_state *ethr = &pgo_iothread_external_state;
	ethr->id = 0;
	ethr->q = &vm_pageout_queue_external;
	/* in external_state these cheads are never used, they are used only in internal_state for te compressor */
	ethr->current_early_swapout_chead = NULL;
	ethr->current_regular_swapout_chead = NULL;
	ethr->current_late_swapout_chead = NULL;
	ethr->scratch_buf = NULL;
#if DEVELOPMENT || DEBUG
	ethr->benchmark_q = NULL;
#endif /* DEVELOPMENT || DEBUG */
	sched_cond_init(&(ethr->pgo_wakeup));

	result = kernel_thread_start_priority((thread_continue_t)vm_pageout_iothread_external,
	    (void *)ethr, BASEPRI_VM,
	    &(ethr->pgo_iothread));
	if (result != KERN_SUCCESS) {
		panic("vm_pageout: Unable to create external thread (%d)\n", result);
	}
	thread_set_thread_name(ethr->pgo_iothread, "VM_pageout_external_iothread");

	thread_mtx_lock(vm_pageout_gc_thread );
	thread_start(vm_pageout_gc_thread );
	thread_mtx_unlock(vm_pageout_gc_thread);

#if VM_PRESSURE_EVENTS
	result = kernel_thread_start_priority((thread_continue_t)vm_pressure_thread, NULL,
	    BASEPRI_DEFAULT,
	    &thread);

	if (result != KERN_SUCCESS) {
		panic("vm_pressure_thread: create failed");
	}

	thread_deallocate(thread);
#endif

	vm_object_reaper_init();


	if (VM_CONFIG_COMPRESSOR_IS_PRESENT) {
		vm_compressor_init();
	}

#if VM_PRESSURE_EVENTS
	vm_pressure_events_enabled = TRUE;
#endif /* VM_PRESSURE_EVENTS */

#if CONFIG_PHANTOM_CACHE
	vm_phantom_cache_init();
#endif
#if VM_PAGE_BUCKETS_CHECK
#if VM_PAGE_FAKE_BUCKETS
	printf("**** DEBUG: protecting fake buckets [0x%llx:0x%llx]\n",
	    (uint64_t) vm_page_fake_buckets_start,
	    (uint64_t) vm_page_fake_buckets_end);
	pmap_protect(kernel_pmap,
	    vm_page_fake_buckets_start,
	    vm_page_fake_buckets_end,
	    VM_PROT_READ);
//	*(char *) vm_page_fake_buckets_start = 'x';	/* panic! */
#endif /* VM_PAGE_FAKE_BUCKETS */
#endif /* VM_PAGE_BUCKETS_CHECK */

#if VM_OBJECT_TRACKING
	vm_object_tracking_init();
#endif /* VM_OBJECT_TRACKING */

#if __arm64__
//	vm_tests();
#endif /* __arm64__ */

	vm_pageout_continue();

	/*
	 * Unreached code!
	 *
	 * The vm_pageout_continue() call above never returns, so the code below is never
	 * executed.  We take advantage of this to declare several DTrace VM related probe
	 * points that our kernel doesn't have an analog for.  These are probe points that
	 * exist in Solaris and are in the DTrace documentation, so people may have written
	 * scripts that use them.  Declaring the probe points here means their scripts will
	 * compile and execute which we want for portability of the scripts, but since this
	 * section of code is never reached, the probe points will simply never fire.  Yes,
	 * this is basically a hack.  The problem is the DTrace probe points were chosen with
	 * Solaris specific VM events in mind, not portability to different VM implementations.
	 */

	DTRACE_VM2(execfree, int, 1, (uint64_t *), NULL);
	DTRACE_VM2(execpgin, int, 1, (uint64_t *), NULL);
	DTRACE_VM2(execpgout, int, 1, (uint64_t *), NULL);
	DTRACE_VM2(pgswapin, int, 1, (uint64_t *), NULL);
	DTRACE_VM2(pgswapout, int, 1, (uint64_t *), NULL);
	DTRACE_VM2(swapin, int, 1, (uint64_t *), NULL);
	DTRACE_VM2(swapout, int, 1, (uint64_t *), NULL);
	/*NOTREACHED*/
}



kern_return_t
vm_pageout_internal_start(void)
{
	kern_return_t   result = KERN_SUCCESS;
	host_basic_info_data_t hinfo;
	vm_offset_t     buf, bufsize;

	assert(VM_CONFIG_COMPRESSOR_IS_PRESENT);

	mach_msg_type_number_t count = HOST_BASIC_INFO_COUNT;
#define BSD_HOST 1
	host_info((host_t)BSD_HOST, HOST_BASIC_INFO, (host_info_t)&hinfo, &count);

	assert(hinfo.max_cpus > 0);

#if !XNU_TARGET_OS_OSX
	vm_pageout_state.vm_compressor_thread_count = 1;
#else /* !XNU_TARGET_OS_OSX */
	if (hinfo.max_cpus > 4) {
		vm_pageout_state.vm_compressor_thread_count = 2;
	} else {
		vm_pageout_state.vm_compressor_thread_count = 1;
	}
#endif /* !XNU_TARGET_OS_OSX */
#if     __AMP__
	if (vm_compressor_ebound) {
		vm_pageout_state.vm_compressor_thread_count = 2;
	}
#endif
	PE_parse_boot_argn("vmcomp_threads", &vm_pageout_state.vm_compressor_thread_count,
	    sizeof(vm_pageout_state.vm_compressor_thread_count));

	/* did we get from the bootargs an unreasonable number? */
	if (vm_pageout_state.vm_compressor_thread_count >= hinfo.max_cpus) {
		vm_pageout_state.vm_compressor_thread_count = hinfo.max_cpus - 1;
	}
	if (vm_pageout_state.vm_compressor_thread_count <= 0) {
		vm_pageout_state.vm_compressor_thread_count = 1;
	} else if (vm_pageout_state.vm_compressor_thread_count > MAX_COMPRESSOR_THREAD_COUNT) {
		vm_pageout_state.vm_compressor_thread_count = MAX_COMPRESSOR_THREAD_COUNT;
	}

	vm_pageout_queue_internal.pgo_maxlaundry =
	    (vm_pageout_state.vm_compressor_thread_count * 4) * VM_PAGE_LAUNDRY_MAX;

	PE_parse_boot_argn("vmpgoi_maxlaundry",
	    &vm_pageout_queue_internal.pgo_maxlaundry,
	    sizeof(vm_pageout_queue_internal.pgo_maxlaundry));

#if DEVELOPMENT || DEBUG
	// Note: this will be modified at enqueue-time such that the benchmark queue is never throttled
	vm_pageout_queue_benchmark.pgo_maxlaundry = vm_pageout_queue_internal.pgo_maxlaundry;
#endif /* DEVELOPMENT || DEBUG */

	bufsize = COMPRESSOR_SCRATCH_BUF_SIZE;

	kmem_alloc(kernel_map, &buf,
	    bufsize * vm_pageout_state.vm_compressor_thread_count,
	    KMA_DATA | KMA_NOFAIL | KMA_KOBJECT | KMA_PERMANENT,
	    VM_KERN_MEMORY_COMPRESSOR);

	for (int i = 0; i < vm_pageout_state.vm_compressor_thread_count; i++) {
		struct pgo_iothread_state *iq = &pgo_iothread_internal_state[i];
		iq->id = i;
		iq->q = &vm_pageout_queue_internal;
		iq->current_early_swapout_chead = NULL;
		iq->current_regular_swapout_chead = NULL;
		iq->current_late_swapout_chead = NULL;
		iq->scratch_buf = (char *)(buf + i * bufsize);
#if DEVELOPMENT || DEBUG
		iq->benchmark_q = &vm_pageout_queue_benchmark;
#endif /* DEVELOPMENT || DEBUG */
		sched_cond_init(&(iq->pgo_wakeup));
		result = kernel_thread_start_priority((thread_continue_t)vm_pageout_iothread_internal,
		    (void *)iq, BASEPRI_VM,
		    &(iq->pgo_iothread));

		if (result != KERN_SUCCESS) {
			panic("vm_pageout: Unable to create compressor thread no. %d (%d)\n", i, result);
		}
	}
	return result;
}

#if CONFIG_IOSCHED
/*
 * To support I/O Expedite for compressed files we mark the upls with special flags.
 * The way decmpfs works is that we create a big upl which marks all the pages needed to
 * represent the compressed file as busy. We tag this upl with the flag UPL_DECMP_REQ. Decmpfs
 * then issues smaller I/Os for compressed I/Os, deflates them and puts the data into the pages
 * being held in the big original UPL. We mark each of these smaller UPLs with the flag
 * UPL_DECMP_REAL_IO. Any outstanding real I/O UPL is tracked by the big req upl using the
 * decmp_io_upl field (in the upl structure). This link is protected in the forward direction
 * by the req upl lock (the reverse link doesnt need synch. since we never inspect this link
 * unless the real I/O upl is being destroyed).
 */


static void
upl_set_decmp_info(upl_t upl, upl_t src_upl)
{
	assert((src_upl->flags & UPL_DECMP_REQ) != 0);

	upl_lock(src_upl);
	if (src_upl->decmp_io_upl) {
		/*
		 * If there is already an alive real I/O UPL, ignore this new UPL.
		 * This case should rarely happen and even if it does, it just means
		 * that we might issue a spurious expedite which the driver is expected
		 * to handle.
		 */
		upl_unlock(src_upl);
		return;
	}
	src_upl->decmp_io_upl = (void *)upl;
	src_upl->ref_count++;

	upl->flags |= UPL_DECMP_REAL_IO;
	upl->decmp_io_upl = (void *)src_upl;
	upl_unlock(src_upl);
}
#endif /* CONFIG_IOSCHED */

#if UPL_DEBUG
int     upl_debug_enabled = 1;
#else
int     upl_debug_enabled = 0;
#endif

static upl_t
upl_create(int type, int flags, upl_size_t size)
{
	uint32_t pages = (uint32_t)atop(round_page_32(size));
	upl_t    upl;

	assert(page_aligned(size));

	/*
	 * FIXME: this code assumes the allocation always succeeds,
	 *        however `pages` can be up to MAX_UPL_SIZE.
	 *
	 *        The allocation size is above 32k (resp. 128k)
	 *        on 16k pages (resp. 4k), which kalloc might fail
	 *        to allocate.
	 */
	upl = kalloc_type(struct upl, struct upl_page_info,
	    (type & UPL_CREATE_INTERNAL) ? pages : 0, Z_WAITOK | Z_ZERO);
	if (type & UPL_CREATE_INTERNAL) {
		flags |= UPL_INTERNAL;
	}

	if (type & UPL_CREATE_LITE) {
		flags |= UPL_LITE;
		if (pages) {
			upl->lite_list = bitmap_alloc(pages);
		}
	}

	upl->flags = flags;
	upl->ref_count = 1;
	upl_lock_init(upl);
#if CONFIG_IOSCHED
	if (type & UPL_CREATE_IO_TRACKING) {
		upl->upl_priority = proc_get_effective_thread_policy(current_thread(), TASK_POLICY_IO);
	}

	if ((type & UPL_CREATE_INTERNAL) && (type & UPL_CREATE_EXPEDITE_SUP)) {
		/* Only support expedite on internal UPLs */
		thread_t        curthread = current_thread();
		upl->upl_reprio_info = kalloc_data(sizeof(uint64_t) * pages,
		    Z_WAITOK | Z_ZERO);
		upl->flags |= UPL_EXPEDITE_SUPPORTED;
		if (curthread->decmp_upl != NULL) {
			upl_set_decmp_info(upl, curthread->decmp_upl);
		}
	}
#endif
#if CONFIG_IOSCHED || UPL_DEBUG
	if ((type & UPL_CREATE_IO_TRACKING) || upl_debug_enabled) {
		upl->upl_creator = current_thread();
		upl->flags |= UPL_TRACKED_BY_OBJECT;
	}
#endif

#if UPL_DEBUG
	upl->uple_create_btref = btref_get(__builtin_frame_address(0), 0);
#endif /* UPL_DEBUG */

	return upl;
}

static void
upl_destroy(upl_t upl)
{
	uint32_t pages;

//	DEBUG4K_UPL("upl %p (u_offset 0x%llx u_size 0x%llx) object %p\n", upl, (uint64_t)upl->u_offset, (uint64_t)upl->u_size, upl->map_object);

	if (upl->ext_ref_count) {
		panic("upl(%p) ext_ref_count", upl);
	}

#if CONFIG_IOSCHED
	if ((upl->flags & UPL_DECMP_REAL_IO) && upl->decmp_io_upl) {
		upl_t src_upl;
		src_upl = upl->decmp_io_upl;
		assert((src_upl->flags & UPL_DECMP_REQ) != 0);
		upl_lock(src_upl);
		src_upl->decmp_io_upl = NULL;
		upl_unlock(src_upl);
		upl_deallocate(src_upl);
	}
#endif /* CONFIG_IOSCHED */

#if CONFIG_IOSCHED || UPL_DEBUG
	if (((upl->flags & UPL_TRACKED_BY_OBJECT) || upl_debug_enabled) &&
	    !(upl->flags & UPL_VECTOR)) {
		vm_object_t     object;

		if (upl->flags & UPL_SHADOWED) {
			object = upl->map_object->shadow;
		} else {
			object = upl->map_object;
		}

		vm_object_lock(object);
		queue_remove(&object->uplq, upl, upl_t, uplq);
		vm_object_activity_end(object);
		vm_object_collapse(object, 0, TRUE);
		vm_object_unlock(object);
	}
#endif
	/*
	 * drop a reference on the map_object whether or
	 * not a pageout object is inserted
	 */
	if (upl->flags & UPL_SHADOWED) {
		vm_object_deallocate(upl->map_object);
	}

	if (upl->flags & UPL_DEVICE_MEMORY) {
		pages = 1;
	} else {
		pages = (uint32_t)atop(upl_adjusted_size(upl, PAGE_MASK));
	}

	upl_lock_destroy(upl);

#if CONFIG_IOSCHED
	if (upl->flags & UPL_EXPEDITE_SUPPORTED) {
		kfree_data(upl->upl_reprio_info, sizeof(uint64_t) * pages);
	}
#endif

#if UPL_DEBUG
	for (int i = 0; i < upl->upl_commit_index; i++) {
		btref_put(upl->upl_commit_records[i].c_btref);
	}
	btref_put(upl->uple_create_btref);
#endif /* UPL_DEBUG */

	if ((upl->flags & UPL_LITE) && pages) {
		bitmap_free(upl->lite_list, pages);
	}
	kfree_type(struct upl, struct upl_page_info,
	    (upl->flags & UPL_INTERNAL) ? pages : 0, upl);
}

void
upl_deallocate(upl_t upl)
{
	upl_lock(upl);

	if (--upl->ref_count == 0) {
		if (vector_upl_is_valid(upl)) {
			vector_upl_deallocate(upl);
		}
		upl_unlock(upl);

		if (upl->upl_iodone) {
			upl_callout_iodone(upl);
		}

		upl_destroy(upl);
	} else {
		upl_unlock(upl);
	}
}

#if CONFIG_IOSCHED
void
upl_mark_decmp(upl_t upl)
{
	if (upl->flags & UPL_TRACKED_BY_OBJECT) {
		upl->flags |= UPL_DECMP_REQ;
		upl->upl_creator->decmp_upl = (void *)upl;
	}
}

void
upl_unmark_decmp(upl_t upl)
{
	if (upl && (upl->flags & UPL_DECMP_REQ)) {
		upl->upl_creator->decmp_upl = NULL;
	}
}

#endif /* CONFIG_IOSCHED */

#define VM_PAGE_Q_BACKING_UP(q)         \
	((q)->pgo_laundry >= (((q)->pgo_maxlaundry * 8) / 10))

boolean_t must_throttle_writes(void);

boolean_t
must_throttle_writes()
{
	if (VM_PAGE_Q_BACKING_UP(&vm_pageout_queue_external) &&
	    vm_page_pageable_external_count > (AVAILABLE_NON_COMPRESSED_MEMORY * 6) / 10) {
		return TRUE;
	}

	return FALSE;
}

int vm_page_delayed_work_ctx_needed = 0;
KALLOC_TYPE_DEFINE(dw_ctx_zone, struct vm_page_delayed_work_ctx, KT_PRIV_ACCT);

__startup_func
static void
vm_page_delayed_work_init_ctx(void)
{
	uint16_t min_delayed_work_ctx_allocated = 16;

	/*
	 * try really hard to always keep NCPU elements around in the zone
	 * in order for the UPL code to almost always get an element.
	 */
	if (min_delayed_work_ctx_allocated < zpercpu_count()) {
		min_delayed_work_ctx_allocated = (uint16_t)zpercpu_count();
	}

	zone_raise_reserve(dw_ctx_zone, min_delayed_work_ctx_allocated);
}
STARTUP(ZALLOC, STARTUP_RANK_LAST, vm_page_delayed_work_init_ctx);

struct vm_page_delayed_work*
vm_page_delayed_work_get_ctx(void)
{
	struct vm_page_delayed_work_ctx * dw_ctx = NULL;

	dw_ctx = zalloc_flags(dw_ctx_zone, Z_ZERO | Z_NOWAIT);

	if (__probable(dw_ctx)) {
		dw_ctx->delayed_owner = current_thread();
	} else {
		vm_page_delayed_work_ctx_needed++;
	}
	return dw_ctx ? dw_ctx->dwp : NULL;
}

void
vm_page_delayed_work_finish_ctx(struct vm_page_delayed_work* dwp)
{
	struct  vm_page_delayed_work_ctx *ldw_ctx;

	ldw_ctx = (struct vm_page_delayed_work_ctx *)dwp;
	ldw_ctx->delayed_owner = NULL;

	zfree(dw_ctx_zone, ldw_ctx);
}

/*
 *	Routine:	vm_object_upl_request
 *	Purpose:
 *		Cause the population of a portion of a vm_object.
 *		Depending on the nature of the request, the pages
 *		returned may be contain valid data or be uninitialized.
 *		A page list structure, listing the physical pages
 *		will be returned upon request.
 *		This function is called by the file system or any other
 *		supplier of backing store to a pager.
 *		IMPORTANT NOTE: The caller must still respect the relationship
 *		between the vm_object and its backing memory object.  The
 *		caller MUST NOT substitute changes in the backing file
 *		without first doing a memory_object_lock_request on the
 *		target range unless it is know that the pages are not
 *		shared with another entity at the pager level.
 *		Copy_in_to:
 *			if a page list structure is present
 *			return the mapped physical pages, where a
 *			page is not present, return a non-initialized
 *			one.  If the no_sync bit is turned on, don't
 *			call the pager unlock to synchronize with other
 *			possible copies of the page. Leave pages busy
 *			in the original object, if a page list structure
 *			was specified.  When a commit of the page list
 *			pages is done, the dirty bit will be set for each one.
 *		Copy_out_from:
 *			If a page list structure is present, return
 *			all mapped pages.  Where a page does not exist
 *			map a zero filled one. Leave pages busy in
 *			the original object.  If a page list structure
 *			is not specified, this call is a no-op.
 *
 *		Note:  access of default pager objects has a rather interesting
 *		twist.  The caller of this routine, presumably the file system
 *		page cache handling code, will never actually make a request
 *		against a default pager backed object.  Only the default
 *		pager will make requests on backing store related vm_objects
 *		In this way the default pager can maintain the relationship
 *		between backing store files (abstract memory objects) and
 *		the vm_objects (cache objects), they support.
 *
 */

__private_extern__ kern_return_t
vm_object_upl_request(
	vm_object_t             object,
	vm_object_offset_t      offset,
	upl_size_t              size,
	upl_t                   *upl_ptr,
	upl_page_info_array_t   user_page_list,
	unsigned int            *page_list_count,
	upl_control_flags_t     cntrl_flags,
	vm_tag_t                tag)
{
	vm_page_t               dst_page = VM_PAGE_NULL;
	vm_object_offset_t      dst_offset;
	upl_size_t              xfer_size;
	unsigned int            size_in_pages;
	boolean_t               dirty;
	boolean_t               hw_dirty;
	upl_t                   upl = NULL;
	unsigned int            entry;
	vm_page_t               alias_page = NULL;
	int                     refmod_state = 0;
	vm_object_t             last_copy_object;
	uint32_t                last_copy_version;
	struct  vm_page_delayed_work    dw_array;
	struct  vm_page_delayed_work    *dwp, *dwp_start;
	bool                    dwp_finish_ctx = TRUE;
	int                     dw_count;
	int                     dw_limit;
	int                     io_tracking_flag = 0;
	int                     grab_options;
	int                     page_grab_count = 0;
	ppnum_t                 phys_page;
	pmap_flush_context      pmap_flush_context_storage;
	boolean_t               pmap_flushes_delayed = FALSE;
#if DEVELOPMENT || DEBUG
	task_t                  task = current_task();
#endif /* DEVELOPMENT || DEBUG */

	dwp_start = dwp = NULL;

	if (cntrl_flags & ~UPL_VALID_FLAGS) {
		/*
		 * For forward compatibility's sake,
		 * reject any unknown flag.
		 */
		return KERN_INVALID_VALUE;
	}
	if ((!object->internal) && (object->paging_offset != 0)) {
		panic("vm_object_upl_request: external object with non-zero paging offset");
	}
	if (object->phys_contiguous) {
		panic("vm_object_upl_request: contiguous object specified");
	}

	assertf(page_aligned(offset) && page_aligned(size),
	    "offset 0x%llx size 0x%x",
	    offset, size);

	VM_DEBUG_CONSTANT_EVENT(vm_object_upl_request, DBG_VM_UPL_REQUEST, DBG_FUNC_START, size, cntrl_flags, 0, 0);

	dw_count = 0;
	dw_limit = DELAYED_WORK_LIMIT(DEFAULT_DELAYED_WORK_LIMIT);
	dwp_start = vm_page_delayed_work_get_ctx();
	if (dwp_start == NULL) {
		dwp_start = &dw_array;
		dw_limit = 1;
		dwp_finish_ctx = FALSE;
	}

	dwp = dwp_start;

	if (size > MAX_UPL_SIZE_BYTES) {
		size = MAX_UPL_SIZE_BYTES;
	}

	if ((cntrl_flags & UPL_SET_INTERNAL) && page_list_count != NULL) {
		*page_list_count = MAX_UPL_SIZE_BYTES >> PAGE_SHIFT;
	}

#if CONFIG_IOSCHED || UPL_DEBUG
	if (object->io_tracking || upl_debug_enabled) {
		io_tracking_flag |= UPL_CREATE_IO_TRACKING;
	}
#endif
#if CONFIG_IOSCHED
	if (object->io_tracking) {
		io_tracking_flag |= UPL_CREATE_EXPEDITE_SUP;
	}
#endif

	if (cntrl_flags & UPL_SET_INTERNAL) {
		if (cntrl_flags & UPL_SET_LITE) {
			upl = upl_create(UPL_CREATE_INTERNAL | UPL_CREATE_LITE | io_tracking_flag, 0, size);
		} else {
			upl = upl_create(UPL_CREATE_INTERNAL | io_tracking_flag, 0, size);
		}
		user_page_list = size ? upl->page_list : NULL;
	} else {
		if (cntrl_flags & UPL_SET_LITE) {
			upl = upl_create(UPL_CREATE_EXTERNAL | UPL_CREATE_LITE | io_tracking_flag, 0, size);
		} else {
			upl = upl_create(UPL_CREATE_EXTERNAL | io_tracking_flag, 0, size);
		}
	}
	*upl_ptr = upl;

	if (user_page_list) {
		user_page_list[0].device = FALSE;
	}

	if (cntrl_flags & UPL_SET_LITE) {
		upl->map_object = object;
	} else {
		upl->map_object = vm_object_allocate(size);
		vm_object_lock(upl->map_object);
		/*
		 * No neeed to lock the new object: nobody else knows
		 * about it yet, so it's all ours so far.
		 */
		upl->map_object->shadow = object;
		VM_OBJECT_SET_PAGEOUT(upl->map_object, TRUE);
		VM_OBJECT_SET_CAN_PERSIST(upl->map_object, FALSE);
		upl->map_object->copy_strategy = MEMORY_OBJECT_COPY_NONE;
		upl->map_object->vo_shadow_offset = offset;
		upl->map_object->wimg_bits = object->wimg_bits;
		assertf(page_aligned(upl->map_object->vo_shadow_offset),
		    "object %p shadow_offset 0x%llx",
		    upl->map_object, upl->map_object->vo_shadow_offset);
		vm_object_unlock(upl->map_object);

		alias_page = vm_page_grab_fictitious(TRUE);

		upl->flags |= UPL_SHADOWED;
	}
	if (cntrl_flags & UPL_FOR_PAGEOUT) {
		upl->flags |= UPL_PAGEOUT;
	}

	vm_object_lock(object);
	vm_object_activity_begin(object);

	grab_options = 0;
#if CONFIG_SECLUDED_MEMORY
	if (object->can_grab_secluded) {
		grab_options |= VM_PAGE_GRAB_SECLUDED;
	}
#endif /* CONFIG_SECLUDED_MEMORY */

	/*
	 * we can lock in the paging_offset once paging_in_progress is set
	 */
	upl->u_size = size;
	upl->u_offset = offset + object->paging_offset;

#if CONFIG_IOSCHED || UPL_DEBUG
	if (object->io_tracking || upl_debug_enabled) {
		vm_object_activity_begin(object);
		queue_enter(&object->uplq, upl, upl_t, uplq);
	}
#endif
	if ((cntrl_flags & UPL_WILL_MODIFY) && object->vo_copy != VM_OBJECT_NULL) {
		/*
		 * Honor copy-on-write obligations
		 *
		 * The caller is gathering these pages and
		 * might modify their contents.  We need to
		 * make sure that the copy object has its own
		 * private copies of these pages before we let
		 * the caller modify them.
		 */
		vm_object_update(object,
		    offset,
		    size,
		    NULL,
		    NULL,
		    FALSE,              /* should_return */
		    MEMORY_OBJECT_COPY_SYNC,
		    VM_PROT_NO_CHANGE);

		VM_PAGEOUT_DEBUG(upl_cow, 1);
		VM_PAGEOUT_DEBUG(upl_cow_pages, (size >> PAGE_SHIFT));
	}
	/*
	 * remember which copy object we synchronized with
	 */
	last_copy_object = object->vo_copy;
	last_copy_version = object->vo_copy_version;
	entry = 0;

	xfer_size = size;
	dst_offset = offset;
	size_in_pages = size / PAGE_SIZE;

	if (vm_page_free_count > (vm_page_free_target + size_in_pages) ||
	    object->resident_page_count < ((MAX_UPL_SIZE_BYTES * 2) >> PAGE_SHIFT)) {
		object->scan_collisions = 0;
	}

	if ((cntrl_flags & UPL_WILL_MODIFY) && must_throttle_writes() == TRUE) {
		boolean_t       isSSD = FALSE;

#if !XNU_TARGET_OS_OSX
		isSSD = TRUE;
#else /* !XNU_TARGET_OS_OSX */
		vnode_pager_get_isSSD(object->pager, &isSSD);
#endif /* !XNU_TARGET_OS_OSX */
		vm_object_unlock(object);

		OSAddAtomic(size_in_pages, &vm_upl_wait_for_pages);

		if (isSSD == TRUE) {
			delay(1000 * size_in_pages);
		} else {
			delay(5000 * size_in_pages);
		}
		OSAddAtomic(-size_in_pages, &vm_upl_wait_for_pages);

		vm_object_lock(object);
	}

	while (xfer_size) {
		dwp->dw_mask = 0;

		if ((alias_page == NULL) && !(cntrl_flags & UPL_SET_LITE)) {
			vm_object_unlock(object);
			alias_page = vm_page_grab_fictitious(TRUE);
			vm_object_lock(object);
		}
		if (cntrl_flags & UPL_COPYOUT_FROM) {
			upl->flags |= UPL_PAGE_SYNC_DONE;

			if (((dst_page = vm_page_lookup(object, dst_offset)) == VM_PAGE_NULL) ||
			    dst_page->vmp_fictitious ||
			    dst_page->vmp_absent ||
			    VMP_ERROR_GET(dst_page) ||
			    dst_page->vmp_cleaning ||
			    (VM_PAGE_WIRED(dst_page))) {
				if (user_page_list) {
					user_page_list[entry].phys_addr = 0;
				}

				goto try_next_page;
			}
			phys_page = VM_PAGE_GET_PHYS_PAGE(dst_page);

			/*
			 * grab this up front...
			 * a high percentange of the time we're going to
			 * need the hardware modification state a bit later
			 * anyway... so we can eliminate an extra call into
			 * the pmap layer by grabbing it here and recording it
			 */
			if (dst_page->vmp_pmapped) {
				refmod_state = pmap_get_refmod(phys_page);
			} else {
				refmod_state = 0;
			}

			if ((refmod_state & VM_MEM_REFERENCED) && VM_PAGE_INACTIVE(dst_page)) {
				/*
				 * page is on inactive list and referenced...
				 * reactivate it now... this gets it out of the
				 * way of vm_pageout_scan which would have to
				 * reactivate it upon tripping over it
				 */
				dwp->dw_mask |= DW_vm_page_activate;
			}
			if (cntrl_flags & UPL_RET_ONLY_DIRTY) {
				/*
				 * we're only asking for DIRTY pages to be returned
				 */
				if (dst_page->vmp_laundry || !(cntrl_flags & UPL_FOR_PAGEOUT)) {
					/*
					 * if we were the page stolen by vm_pageout_scan to be
					 * cleaned (as opposed to a buddy being clustered in
					 * or this request is not being driven by a PAGEOUT cluster
					 * then we only need to check for the page being dirty or
					 * precious to decide whether to return it
					 */
					if (dst_page->vmp_dirty || dst_page->vmp_precious || (refmod_state & VM_MEM_MODIFIED)) {
						goto check_busy;
					}
					goto dont_return;
				}
				/*
				 * this is a request for a PAGEOUT cluster and this page
				 * is merely along for the ride as a 'buddy'... not only
				 * does it have to be dirty to be returned, but it also
				 * can't have been referenced recently...
				 */
				if ((hibernate_cleaning_in_progress == TRUE ||
				    (!((refmod_state & VM_MEM_REFERENCED) || dst_page->vmp_reference) ||
				    (dst_page->vmp_q_state == VM_PAGE_ON_THROTTLED_Q))) &&
				    ((refmod_state & VM_MEM_MODIFIED) || dst_page->vmp_dirty || dst_page->vmp_precious)) {
					goto check_busy;
				}
dont_return:
				/*
				 * if we reach here, we're not to return
				 * the page... go on to the next one
				 */
				if (dst_page->vmp_laundry == TRUE) {
					/*
					 * if we get here, the page is not 'cleaning' (filtered out above).
					 * since it has been referenced, remove it from the laundry
					 * so we don't pay the cost of an I/O to clean a page
					 * we're just going to take back
					 */
					vm_page_lockspin_queues();

					vm_pageout_steal_laundry(dst_page, TRUE);
					vm_page_activate(dst_page);

					vm_page_unlock_queues();
				}
				if (user_page_list) {
					user_page_list[entry].phys_addr = 0;
				}

				goto try_next_page;
			}
check_busy:
			if (dst_page->vmp_busy) {
				if (cntrl_flags & UPL_NOBLOCK) {
					if (user_page_list) {
						user_page_list[entry].phys_addr = 0;
					}
					dwp->dw_mask = 0;

					goto try_next_page;
				}
				/*
				 * someone else is playing with the
				 * page.  We will have to wait.
				 */
				vm_page_sleep(object, dst_page, THREAD_UNINT, LCK_SLEEP_EXCLUSIVE);

				continue;
			}
			if (dst_page->vmp_q_state == VM_PAGE_ON_PAGEOUT_Q) {
				vm_page_lockspin_queues();

				if (dst_page->vmp_q_state == VM_PAGE_ON_PAGEOUT_Q) {
					/*
					 * we've buddied up a page for a clustered pageout
					 * that has already been moved to the pageout
					 * queue by pageout_scan... we need to remove
					 * it from the queue and drop the laundry count
					 * on that queue
					 */
					vm_pageout_throttle_up(dst_page);
				}
				vm_page_unlock_queues();
			}
			hw_dirty = refmod_state & VM_MEM_MODIFIED;
			dirty = hw_dirty ? TRUE : dst_page->vmp_dirty;

			if (phys_page > upl->highest_page) {
				upl->highest_page = phys_page;
			}

			assert(!pmap_is_noencrypt(phys_page));

			if (cntrl_flags & UPL_SET_LITE) {
				unsigned int    pg_num;

				pg_num = (unsigned int) ((dst_offset - offset) / PAGE_SIZE);
				assert(pg_num == (dst_offset - offset) / PAGE_SIZE);
				bitmap_set(upl->lite_list, pg_num);

				if (hw_dirty) {
					if (pmap_flushes_delayed == FALSE) {
						pmap_flush_context_init(&pmap_flush_context_storage);
						pmap_flushes_delayed = TRUE;
					}
					pmap_clear_refmod_options(phys_page,
					    VM_MEM_MODIFIED,
					    PMAP_OPTIONS_NOFLUSH | PMAP_OPTIONS_CLEAR_WRITE,
					    &pmap_flush_context_storage);
				}

				/*
				 * Mark original page as cleaning
				 * in place.
				 */
				dst_page->vmp_cleaning = TRUE;
				dst_page->vmp_precious = FALSE;
			} else {
				/*
				 * use pageclean setup, it is more
				 * convenient even for the pageout
				 * cases here
				 */
				vm_object_lock(upl->map_object);
				vm_pageclean_setup(dst_page, alias_page, upl->map_object, size - xfer_size);
				vm_object_unlock(upl->map_object);

				alias_page->vmp_absent = FALSE;
				alias_page = NULL;
			}
			if (dirty) {
				SET_PAGE_DIRTY(dst_page, FALSE);
			} else {
				dst_page->vmp_dirty = FALSE;
			}

			if (!dirty) {
				dst_page->vmp_precious = TRUE;
			}

			if (!(cntrl_flags & UPL_CLEAN_IN_PLACE)) {
				if (!VM_PAGE_WIRED(dst_page)) {
					dst_page->vmp_free_when_done = TRUE;
				}
			}
		} else {
			if ((cntrl_flags & UPL_WILL_MODIFY) &&
			    (object->vo_copy != last_copy_object ||
			    object->vo_copy_version != last_copy_version)) {
				/*
				 * Honor copy-on-write obligations
				 *
				 * The copy object has changed since we
				 * last synchronized for copy-on-write.
				 * Another copy object might have been
				 * inserted while we released the object's
				 * lock.  Since someone could have seen the
				 * original contents of the remaining pages
				 * through that new object, we have to
				 * synchronize with it again for the remaining
				 * pages only.  The previous pages are "busy"
				 * so they can not be seen through the new
				 * mapping.  The new mapping will see our
				 * upcoming changes for those previous pages,
				 * but that's OK since they couldn't see what
				 * was there before.  It's just a race anyway
				 * and there's no guarantee of consistency or
				 * atomicity.  We just don't want new mappings
				 * to see both the *before* and *after* pages.
				 */
				if (object->vo_copy != VM_OBJECT_NULL) {
					vm_object_update(
						object,
						dst_offset,/* current offset */
						xfer_size, /* remaining size */
						NULL,
						NULL,
						FALSE,     /* should_return */
						MEMORY_OBJECT_COPY_SYNC,
						VM_PROT_NO_CHANGE);

					VM_PAGEOUT_DEBUG(upl_cow_again, 1);
					VM_PAGEOUT_DEBUG(upl_cow_again_pages, (xfer_size >> PAGE_SHIFT));
				}
				/*
				 * remember the copy object we synced with
				 */
				last_copy_object = object->vo_copy;
				last_copy_version = object->vo_copy_version;
			}
			dst_page = vm_page_lookup(object, dst_offset);

			if (dst_page != VM_PAGE_NULL) {
				if ((cntrl_flags & UPL_RET_ONLY_ABSENT)) {
					/*
					 * skip over pages already present in the cache
					 */
					if (user_page_list) {
						user_page_list[entry].phys_addr = 0;
					}

					goto try_next_page;
				}
				if (dst_page->vmp_fictitious) {
					panic("need corner case for fictitious page");
				}

				if (dst_page->vmp_busy || dst_page->vmp_cleaning) {
					/*
					 * someone else is playing with the
					 * page.  We will have to wait.
					 */
					vm_page_sleep(object, dst_page, THREAD_UNINT, LCK_SLEEP_EXCLUSIVE);

					continue;
				}
				if (dst_page->vmp_laundry) {
					vm_pageout_steal_laundry(dst_page, FALSE);
				}
			} else {
				if (object->private) {
					/*
					 * This is a nasty wrinkle for users
					 * of upl who encounter device or
					 * private memory however, it is
					 * unavoidable, only a fault can
					 * resolve the actual backing
					 * physical page by asking the
					 * backing device.
					 */
					if (user_page_list) {
						user_page_list[entry].phys_addr = 0;
					}

					goto try_next_page;
				}
				if (object->scan_collisions) {
					/*
					 * the pageout_scan thread is trying to steal
					 * pages from this object, but has run into our
					 * lock... grab 2 pages from the head of the object...
					 * the first is freed on behalf of pageout_scan, the
					 * 2nd is for our own use... we use vm_object_page_grab
					 * in both cases to avoid taking pages from the free
					 * list since we are under memory pressure and our
					 * lock on this object is getting in the way of
					 * relieving it
					 */
					dst_page = vm_object_page_grab(object);

					if (dst_page != VM_PAGE_NULL) {
						vm_page_release(dst_page,
						    FALSE);
					}

					dst_page = vm_object_page_grab(object);
				}
				if (dst_page == VM_PAGE_NULL) {
					/*
					 * need to allocate a page
					 */
					dst_page = vm_page_grab_options(grab_options);
					if (dst_page != VM_PAGE_NULL) {
						page_grab_count++;
					}
				}
				if (dst_page == VM_PAGE_NULL) {
					if ((cntrl_flags & (UPL_RET_ONLY_ABSENT | UPL_NOBLOCK)) == (UPL_RET_ONLY_ABSENT | UPL_NOBLOCK)) {
						/*
						 * we don't want to stall waiting for pages to come onto the free list
						 * while we're already holding absent pages in this UPL
						 * the caller will deal with the empty slots
						 */
						if (user_page_list) {
							user_page_list[entry].phys_addr = 0;
						}

						goto try_next_page;
					}
					/*
					 * no pages available... wait
					 * then try again for the same
					 * offset...
					 */
					vm_object_unlock(object);

					OSAddAtomic(size_in_pages, &vm_upl_wait_for_pages);

					VM_DEBUG_EVENT(vm_upl_page_wait, DBG_VM_UPL_PAGE_WAIT, DBG_FUNC_START, vm_upl_wait_for_pages, 0, 0, 0);

					VM_PAGE_WAIT();
					OSAddAtomic(-size_in_pages, &vm_upl_wait_for_pages);

					VM_DEBUG_EVENT(vm_upl_page_wait, DBG_VM_UPL_PAGE_WAIT, DBG_FUNC_END, vm_upl_wait_for_pages, 0, 0, 0);

					vm_object_lock(object);

					continue;
				}
				vm_page_insert(dst_page, object, dst_offset);

				dst_page->vmp_absent = TRUE;
				dst_page->vmp_busy = FALSE;

				if (cntrl_flags & UPL_RET_ONLY_ABSENT) {
					/*
					 * if UPL_RET_ONLY_ABSENT was specified,
					 * than we're definitely setting up a
					 * upl for a clustered read/pagein
					 * operation... mark the pages as clustered
					 * so upl_commit_range can put them on the
					 * speculative list
					 */
					dst_page->vmp_clustered = TRUE;

					if (!(cntrl_flags & UPL_FILE_IO)) {
						counter_inc(&vm_statistics_pageins);
					}
				}
			}
			phys_page = VM_PAGE_GET_PHYS_PAGE(dst_page);

			dst_page->vmp_overwriting = TRUE;

			if (dst_page->vmp_pmapped) {
				if (!(cntrl_flags & UPL_FILE_IO)) {
					/*
					 * eliminate all mappings from the
					 * original object and its prodigy
					 */
					refmod_state = pmap_disconnect(phys_page);
				} else {
					refmod_state = pmap_get_refmod(phys_page);
				}
			} else {
				refmod_state = 0;
			}

			hw_dirty = refmod_state & VM_MEM_MODIFIED;
			dirty = hw_dirty ? TRUE : dst_page->vmp_dirty;

			if (cntrl_flags & UPL_SET_LITE) {
				unsigned int    pg_num;

				pg_num = (unsigned int) ((dst_offset - offset) / PAGE_SIZE);
				assert(pg_num == (dst_offset - offset) / PAGE_SIZE);
				bitmap_set(upl->lite_list, pg_num);

				if (hw_dirty) {
					pmap_clear_modify(phys_page);
				}

				/*
				 * Mark original page as cleaning
				 * in place.
				 */
				dst_page->vmp_cleaning = TRUE;
				dst_page->vmp_precious = FALSE;
			} else {
				/*
				 * use pageclean setup, it is more
				 * convenient even for the pageout
				 * cases here
				 */
				vm_object_lock(upl->map_object);
				vm_pageclean_setup(dst_page, alias_page, upl->map_object, size - xfer_size);
				vm_object_unlock(upl->map_object);

				alias_page->vmp_absent = FALSE;
				alias_page = NULL;
			}

			if (cntrl_flags & UPL_REQUEST_SET_DIRTY) {
				upl->flags &= ~UPL_CLEAR_DIRTY;
				upl->flags |= UPL_SET_DIRTY;
				dirty = TRUE;
				/*
				 * Page belonging to a code-signed object is about to
				 * be written. Mark it tainted and disconnect it from
				 * all pmaps so processes have to fault it back in and
				 * deal with the tainted bit.
				 */
				if (object->code_signed && dst_page->vmp_cs_tainted != VMP_CS_ALL_TRUE) {
					dst_page->vmp_cs_tainted = VMP_CS_ALL_TRUE;
					vm_page_upl_tainted++;
					if (dst_page->vmp_pmapped) {
						refmod_state = pmap_disconnect(VM_PAGE_GET_PHYS_PAGE(dst_page));
						if (refmod_state & VM_MEM_REFERENCED) {
							dst_page->vmp_reference = TRUE;
						}
					}
				}
			} else if (cntrl_flags & UPL_CLEAN_IN_PLACE) {
				/*
				 * clean in place for read implies
				 * that a write will be done on all
				 * the pages that are dirty before
				 * a upl commit is done.  The caller
				 * is obligated to preserve the
				 * contents of all pages marked dirty
				 */
				upl->flags |= UPL_CLEAR_DIRTY;
			}
			dst_page->vmp_dirty = dirty;

			if (!dirty) {
				dst_page->vmp_precious = TRUE;
			}

			if (!VM_PAGE_WIRED(dst_page)) {
				/*
				 * deny access to the target page while
				 * it is being worked on
				 */
				dst_page->vmp_busy = TRUE;
			} else {
				dwp->dw_mask |= DW_vm_page_wire;
			}

			/*
			 * We might be about to satisfy a fault which has been
			 * requested. So no need for the "restart" bit.
			 */
			dst_page->vmp_restart = FALSE;
			if (!dst_page->vmp_absent && !(cntrl_flags & UPL_WILL_MODIFY)) {
				/*
				 * expect the page to be used
				 */
				dwp->dw_mask |= DW_set_reference;
			}
			if (cntrl_flags & UPL_PRECIOUS) {
				if (object->internal) {
					SET_PAGE_DIRTY(dst_page, FALSE);
					dst_page->vmp_precious = FALSE;
				} else {
					dst_page->vmp_precious = TRUE;
				}
			} else {
				dst_page->vmp_precious = FALSE;
			}
		}
		if (dst_page->vmp_busy) {
			upl->flags |= UPL_HAS_BUSY;
		}

		if (phys_page > upl->highest_page) {
			upl->highest_page = phys_page;
		}
		assert(!pmap_is_noencrypt(phys_page));
		if (user_page_list) {
			user_page_list[entry].phys_addr = phys_page;
			user_page_list[entry].free_when_done    = dst_page->vmp_free_when_done;
			user_page_list[entry].absent    = dst_page->vmp_absent;
			user_page_list[entry].dirty     = dst_page->vmp_dirty;
			user_page_list[entry].precious  = dst_page->vmp_precious;
			user_page_list[entry].device    = FALSE;
			user_page_list[entry].needed    = FALSE;
			if (dst_page->vmp_clustered == TRUE) {
				user_page_list[entry].speculative = (dst_page->vmp_q_state == VM_PAGE_ON_SPECULATIVE_Q) ? TRUE : FALSE;
			} else {
				user_page_list[entry].speculative = FALSE;
			}
			user_page_list[entry].cs_validated = dst_page->vmp_cs_validated;
			user_page_list[entry].cs_tainted = dst_page->vmp_cs_tainted;
			user_page_list[entry].cs_nx = dst_page->vmp_cs_nx;
			user_page_list[entry].mark      = FALSE;
		}
		/*
		 * if UPL_RET_ONLY_ABSENT is set, then
		 * we are working with a fresh page and we've
		 * just set the clustered flag on it to
		 * indicate that it was drug in as part of a
		 * speculative cluster... so leave it alone
		 */
		if (!(cntrl_flags & UPL_RET_ONLY_ABSENT)) {
			/*
			 * someone is explicitly grabbing this page...
			 * update clustered and speculative state
			 *
			 */
			if (dst_page->vmp_clustered) {
				VM_PAGE_CONSUME_CLUSTERED(dst_page);
			}
		}
try_next_page:
		if (dwp->dw_mask) {
			if (dwp->dw_mask & DW_vm_page_activate) {
				counter_inc(&vm_statistics_reactivations);
			}

			VM_PAGE_ADD_DELAYED_WORK(dwp, dst_page, dw_count);

			if (dw_count >= dw_limit) {
				vm_page_do_delayed_work(object, tag, dwp_start, dw_count);

				dwp = dwp_start;
				dw_count = 0;
			}
		}
		entry++;
		dst_offset += PAGE_SIZE_64;
		xfer_size -= PAGE_SIZE;
	}
	if (dw_count) {
		vm_page_do_delayed_work(object, tag, dwp_start, dw_count);
		dwp = dwp_start;
		dw_count = 0;
	}

	if (alias_page != NULL) {
		VM_PAGE_FREE(alias_page);
	}
	if (pmap_flushes_delayed == TRUE) {
		pmap_flush(&pmap_flush_context_storage);
	}

	if (page_list_count != NULL) {
		if (upl->flags & UPL_INTERNAL) {
			*page_list_count = 0;
		} else if (*page_list_count > entry) {
			*page_list_count = entry;
		}
	}
#if UPL_DEBUG
	upl->upl_state = 1;
#endif
	vm_object_unlock(object);

	VM_DEBUG_CONSTANT_EVENT(vm_object_upl_request, DBG_VM_UPL_REQUEST, DBG_FUNC_END, page_grab_count, 0, 0, 0);
#if DEVELOPMENT || DEBUG
	if (task != NULL) {
		ledger_credit(task->ledger, task_ledgers.pages_grabbed_upl, page_grab_count);
	}
#endif /* DEVELOPMENT || DEBUG */

	if (dwp_start && dwp_finish_ctx) {
		vm_page_delayed_work_finish_ctx(dwp_start);
		dwp_start = dwp = NULL;
	}

	return KERN_SUCCESS;
}

/*
 *	Routine:	vm_object_super_upl_request
 *	Purpose:
 *		Cause the population of a portion of a vm_object
 *		in much the same way as memory_object_upl_request.
 *		Depending on the nature of the request, the pages
 *		returned may be contain valid data or be uninitialized.
 *		However, the region may be expanded up to the super
 *		cluster size provided.
 */

__private_extern__ kern_return_t
vm_object_super_upl_request(
	vm_object_t object,
	vm_object_offset_t      offset,
	upl_size_t              size,
	upl_size_t              super_cluster,
	upl_t                   *upl,
	upl_page_info_t         *user_page_list,
	unsigned int            *page_list_count,
	upl_control_flags_t     cntrl_flags,
	vm_tag_t                tag)
{
	if (object->paging_offset > offset || ((cntrl_flags & UPL_VECTOR) == UPL_VECTOR)) {
		return KERN_FAILURE;
	}

	assert(object->paging_in_progress);
	offset = offset - object->paging_offset;

	if (super_cluster > size) {
		vm_object_offset_t      base_offset;
		upl_size_t              super_size;
		vm_object_size_t        super_size_64;

		base_offset = (offset & ~((vm_object_offset_t) super_cluster - 1));
		super_size = (offset + size) > (base_offset + super_cluster) ? super_cluster << 1 : super_cluster;
		super_size_64 = ((base_offset + super_size) > object->vo_size) ? (object->vo_size - base_offset) : super_size;
		super_size = (upl_size_t) super_size_64;
		assert(super_size == super_size_64);

		if (offset > (base_offset + super_size)) {
			panic("vm_object_super_upl_request: Missed target pageout"
			    " %#llx,%#llx, %#x, %#x, %#x, %#llx\n",
			    offset, base_offset, super_size, super_cluster,
			    size, object->paging_offset);
		}
		/*
		 * apparently there is a case where the vm requests a
		 * page to be written out who's offset is beyond the
		 * object size
		 */
		if ((offset + size) > (base_offset + super_size)) {
			super_size_64 = (offset + size) - base_offset;
			super_size = (upl_size_t) super_size_64;
			assert(super_size == super_size_64);
		}

		offset = base_offset;
		size = super_size;
	}
	return vm_object_upl_request(object, offset, size, upl, user_page_list, page_list_count, cntrl_flags, tag);
}

int cs_executable_create_upl = 0;
extern int proc_selfpid(void);
extern char *proc_name_address(void *p);

kern_return_t
vm_map_create_upl(
	vm_map_t                map,
	vm_map_address_t        offset,
	upl_size_t              *upl_size,
	upl_t                   *upl,
	upl_page_info_array_t   page_list,
	unsigned int            *count,
	upl_control_flags_t     *flags,
	vm_tag_t                tag)
{
	vm_map_entry_t          entry;
	upl_control_flags_t     caller_flags;
	int                     force_data_sync;
	int                     sync_cow_data;
	vm_object_t             local_object;
	vm_map_offset_t         local_offset;
	vm_map_offset_t         local_start;
	kern_return_t           ret;
	vm_map_address_t        original_offset;
	vm_map_size_t           original_size, adjusted_size;
	vm_map_offset_t         local_entry_start;
	vm_object_offset_t      local_entry_offset;
	vm_object_offset_t      offset_in_mapped_page;
	boolean_t               release_map = FALSE;


start_with_map:

	original_offset = offset;
	original_size = *upl_size;
	adjusted_size = original_size;

	caller_flags = *flags;

	if (caller_flags & ~UPL_VALID_FLAGS) {
		/*
		 * For forward compatibility's sake,
		 * reject any unknown flag.
		 */
		ret = KERN_INVALID_VALUE;
		goto done;
	}
	force_data_sync = (caller_flags & UPL_FORCE_DATA_SYNC);
	sync_cow_data = !(caller_flags & UPL_COPYOUT_FROM);

	if (upl == NULL) {
		ret = KERN_INVALID_ARGUMENT;
		goto done;
	}

REDISCOVER_ENTRY:
	vm_map_lock_read(map);

	if (!vm_map_lookup_entry(map, offset, &entry)) {
		vm_map_unlock_read(map);
		ret = KERN_FAILURE;
		goto done;
	}

	local_entry_start = entry->vme_start;
	local_entry_offset = VME_OFFSET(entry);

	if (VM_MAP_PAGE_SHIFT(map) < PAGE_SHIFT) {
		DEBUG4K_UPL("map %p (%d) offset 0x%llx size 0x%x flags 0x%llx\n", map, VM_MAP_PAGE_SHIFT(map), (uint64_t)offset, *upl_size, *flags);
	}

	if (entry->vme_end - original_offset < adjusted_size) {
		adjusted_size = entry->vme_end - original_offset;
		assert(adjusted_size > 0);
		*upl_size = (upl_size_t) adjusted_size;
		assert(*upl_size == adjusted_size);
	}

	if (caller_flags & UPL_QUERY_OBJECT_TYPE) {
		*flags = 0;

		if (!entry->is_sub_map &&
		    VME_OBJECT(entry) != VM_OBJECT_NULL) {
			if (VME_OBJECT(entry)->private) {
				*flags = UPL_DEV_MEMORY;
			}

			if (VME_OBJECT(entry)->phys_contiguous) {
				*flags |= UPL_PHYS_CONTIG;
			}
		}
		vm_map_unlock_read(map);
		ret = KERN_SUCCESS;
		goto done;
	}

	offset_in_mapped_page = 0;
	if (VM_MAP_PAGE_SIZE(map) < PAGE_SIZE) {
		offset = vm_map_trunc_page(original_offset, VM_MAP_PAGE_MASK(map));
		*upl_size = (upl_size_t)
		    (vm_map_round_page(original_offset + adjusted_size,
		    VM_MAP_PAGE_MASK(map))
		    - offset);

		offset_in_mapped_page = original_offset - offset;
		assert(offset_in_mapped_page < VM_MAP_PAGE_SIZE(map));

		DEBUG4K_UPL("map %p (%d) offset 0x%llx size 0x%llx flags 0x%llx -> offset 0x%llx adjusted_size 0x%llx *upl_size 0x%x offset_in_mapped_page 0x%llx\n", map, VM_MAP_PAGE_SHIFT(map), (uint64_t)original_offset, (uint64_t)original_size, *flags, (uint64_t)offset, (uint64_t)adjusted_size, *upl_size, offset_in_mapped_page);
	}

	if (!entry->is_sub_map) {
		if (VME_OBJECT(entry) == VM_OBJECT_NULL ||
		    !VME_OBJECT(entry)->phys_contiguous) {
			if (*upl_size > MAX_UPL_SIZE_BYTES) {
				*upl_size = MAX_UPL_SIZE_BYTES;
			}
		}

		/*
		 *      Create an object if necessary.
		 */
		if (VME_OBJECT(entry) == VM_OBJECT_NULL) {
			if (vm_map_lock_read_to_write(map)) {
				goto REDISCOVER_ENTRY;
			}

			VME_OBJECT_SET(entry,
			    vm_object_allocate((vm_size_t)
			    vm_object_round_page((entry->vme_end - entry->vme_start))),
			    false, 0);
			VME_OFFSET_SET(entry, 0);
			assert(entry->use_pmap);

			vm_map_lock_write_to_read(map);
		}

		if (!(caller_flags & UPL_COPYOUT_FROM) &&
		    !(entry->protection & VM_PROT_WRITE)) {
			vm_map_unlock_read(map);
			ret = KERN_PROTECTION_FAILURE;
			goto done;
		}
	}

#if !XNU_TARGET_OS_OSX
	if (map->pmap != kernel_pmap &&
	    (caller_flags & UPL_COPYOUT_FROM) &&
	    (entry->protection & VM_PROT_EXECUTE) &&
	    !(entry->protection & VM_PROT_WRITE)) {
		vm_offset_t     kaddr;
		vm_size_t       ksize;

		/*
		 * We're about to create a read-only UPL backed by
		 * memory from an executable mapping.
		 * Wiring the pages would result in the pages being copied
		 * (due to the "MAP_PRIVATE" mapping) and no longer
		 * code-signed, so no longer eligible for execution.
		 * Instead, let's copy the data into a kernel buffer and
		 * create the UPL from this kernel buffer.
		 * The kernel buffer is then freed, leaving the UPL holding
		 * the last reference on the VM object, so the memory will
		 * be released when the UPL is committed.
		 */

		vm_map_unlock_read(map);
		entry = VM_MAP_ENTRY_NULL;
		/* allocate kernel buffer */
		ksize = round_page(*upl_size);
		kaddr = 0;
		ret = kmem_alloc(kernel_map, &kaddr, ksize,
		    KMA_PAGEABLE | KMA_DATA, tag);
		if (ret == KERN_SUCCESS) {
			/* copyin the user data */
			ret = copyinmap(map, offset, (void *)kaddr, *upl_size);
		}
		if (ret == KERN_SUCCESS) {
			if (ksize > *upl_size) {
				/* zero out the extra space in kernel buffer */
				memset((void *)(kaddr + *upl_size),
				    0,
				    ksize - *upl_size);
			}
			/* create the UPL from the kernel buffer */
			vm_object_offset_t      offset_in_object;
			vm_object_offset_t      offset_in_object_page;

			offset_in_object = offset - local_entry_start + local_entry_offset;
			offset_in_object_page = offset_in_object - vm_object_trunc_page(offset_in_object);
			assert(offset_in_object_page < PAGE_SIZE);
			assert(offset_in_object_page + offset_in_mapped_page < PAGE_SIZE);
			*upl_size -= offset_in_object_page + offset_in_mapped_page;
			ret = vm_map_create_upl(kernel_map,
			    (vm_map_address_t)(kaddr + offset_in_object_page + offset_in_mapped_page),
			    upl_size, upl, page_list, count, flags, tag);
		}
		if (kaddr != 0) {
			/* free the kernel buffer */
			kmem_free(kernel_map, kaddr, ksize);
			kaddr = 0;
			ksize = 0;
		}
#if DEVELOPMENT || DEBUG
		DTRACE_VM4(create_upl_from_executable,
		    vm_map_t, map,
		    vm_map_address_t, offset,
		    upl_size_t, *upl_size,
		    kern_return_t, ret);
#endif /* DEVELOPMENT || DEBUG */
		goto done;
	}
#endif /* !XNU_TARGET_OS_OSX */

	if (!entry->is_sub_map) {
		local_object = VME_OBJECT(entry);
		assert(local_object != VM_OBJECT_NULL);
	}

	if (!entry->is_sub_map &&
	    !entry->needs_copy &&
	    *upl_size != 0 &&
	    local_object->vo_size > *upl_size && /* partial UPL */
	    entry->wired_count == 0 && /* No COW for entries that are wired */
	    (map->pmap != kernel_pmap) && /* alias checks */
	    (vm_map_entry_should_cow_for_true_share(entry) /* case 1 */
	    ||
	    ( /* case 2 */
		    local_object->internal &&
		    (local_object->copy_strategy == MEMORY_OBJECT_COPY_SYMMETRIC) &&
		    local_object->ref_count > 1))) {
		vm_prot_t       prot;

		/*
		 * Case 1:
		 * Set up the targeted range for copy-on-write to avoid
		 * applying true_share/copy_delay to the entire object.
		 *
		 * Case 2:
		 * This map entry covers only part of an internal
		 * object.  There could be other map entries covering
		 * other areas of this object and some of these map
		 * entries could be marked as "needs_copy", which
		 * assumes that the object is COPY_SYMMETRIC.
		 * To avoid marking this object as COPY_DELAY and
		 * "true_share", let's shadow it and mark the new
		 * (smaller) object as "true_share" and COPY_DELAY.
		 */

		if (vm_map_lock_read_to_write(map)) {
			goto REDISCOVER_ENTRY;
		}
		vm_map_lock_assert_exclusive(map);
		assert(VME_OBJECT(entry) == local_object);

		vm_map_clip_start(map,
		    entry,
		    vm_map_trunc_page(offset,
		    VM_MAP_PAGE_MASK(map)));
		vm_map_clip_end(map,
		    entry,
		    vm_map_round_page(offset + *upl_size,
		    VM_MAP_PAGE_MASK(map)));
		if ((entry->vme_end - offset) < *upl_size) {
			*upl_size = (upl_size_t) (entry->vme_end - offset);
			assert(*upl_size == entry->vme_end - offset);
		}

		prot = entry->protection & ~VM_PROT_WRITE;
		if (override_nx(map, VME_ALIAS(entry)) && prot) {
			prot |= VM_PROT_EXECUTE;
		}
		vm_object_pmap_protect(local_object,
		    VME_OFFSET(entry),
		    entry->vme_end - entry->vme_start,
		    ((entry->is_shared ||
		    map->mapped_in_other_pmaps)
		    ? PMAP_NULL
		    : map->pmap),
		    VM_MAP_PAGE_SIZE(map),
		    entry->vme_start,
		    prot);

		assert(entry->wired_count == 0);

		/*
		 * Lock the VM object and re-check its status: if it's mapped
		 * in another address space, we could still be racing with
		 * another thread holding that other VM map exclusively.
		 */
		vm_object_lock(local_object);
		if (local_object->true_share) {
			/* object is already in proper state: no COW needed */
			assert(local_object->copy_strategy !=
			    MEMORY_OBJECT_COPY_SYMMETRIC);
		} else {
			/* not true_share: ask for copy-on-write below */
			assert(local_object->copy_strategy ==
			    MEMORY_OBJECT_COPY_SYMMETRIC);
			entry->needs_copy = TRUE;
		}
		vm_object_unlock(local_object);

		vm_map_lock_write_to_read(map);
	}

	if (entry->needs_copy) {
		/*
		 * Honor copy-on-write for COPY_SYMMETRIC
		 * strategy.
		 */
		vm_map_t                local_map;
		vm_object_t             object;
		vm_object_offset_t      new_offset;
		vm_prot_t               prot;
		boolean_t               wired;
		vm_map_version_t        version;
		vm_map_t                real_map;
		vm_prot_t               fault_type;

		local_map = map;

		if (caller_flags & UPL_COPYOUT_FROM) {
			fault_type = VM_PROT_READ | VM_PROT_COPY;
			vm_counters.create_upl_extra_cow++;
			vm_counters.create_upl_extra_cow_pages +=
			    (entry->vme_end - entry->vme_start) / PAGE_SIZE;
		} else {
			fault_type = VM_PROT_WRITE;
		}
		if (vm_map_lookup_and_lock_object(&local_map,
		    offset, fault_type,
		    OBJECT_LOCK_EXCLUSIVE,
		    &version, &object,
		    &new_offset, &prot, &wired,
		    NULL,
		    &real_map, NULL) != KERN_SUCCESS) {
			if (fault_type == VM_PROT_WRITE) {
				vm_counters.create_upl_lookup_failure_write++;
			} else {
				vm_counters.create_upl_lookup_failure_copy++;
			}
			vm_map_unlock_read(local_map);
			ret = KERN_FAILURE;
			goto done;
		}
		if (real_map != local_map) {
			vm_map_unlock(real_map);
		}
		vm_map_unlock_read(local_map);

		vm_object_unlock(object);

		goto REDISCOVER_ENTRY;
	}

	if (entry->is_sub_map) {
		vm_map_t        submap;

		submap = VME_SUBMAP(entry);
		local_start = entry->vme_start;
		local_offset = (vm_map_offset_t)VME_OFFSET(entry);

		vm_map_reference(submap);
		vm_map_unlock_read(map);

		DEBUG4K_UPL("map %p offset 0x%llx (0x%llx) size 0x%x (adjusted 0x%llx original 0x%llx) offset_in_mapped_page 0x%llx submap %p\n", map, (uint64_t)offset, (uint64_t)original_offset, *upl_size, (uint64_t)adjusted_size, (uint64_t)original_size, offset_in_mapped_page, submap);
		offset += offset_in_mapped_page;
		*upl_size -= offset_in_mapped_page;

		if (release_map) {
			vm_map_deallocate(map);
		}
		map = submap;
		release_map = TRUE;
		offset = local_offset + (offset - local_start);
		goto start_with_map;
	}

	if (sync_cow_data &&
	    (VME_OBJECT(entry)->shadow ||
	    VME_OBJECT(entry)->vo_copy)) {
		local_object = VME_OBJECT(entry);
		local_start = entry->vme_start;
		local_offset = (vm_map_offset_t)VME_OFFSET(entry);

		vm_object_reference(local_object);
		vm_map_unlock_read(map);

		if (local_object->shadow && local_object->vo_copy) {
			vm_object_lock_request(local_object->shadow,
			    ((vm_object_offset_t)
			    ((offset - local_start) +
			    local_offset) +
			    local_object->vo_shadow_offset),
			    *upl_size, FALSE,
			    MEMORY_OBJECT_DATA_SYNC,
			    VM_PROT_NO_CHANGE);
		}
		sync_cow_data = FALSE;
		vm_object_deallocate(local_object);

		goto REDISCOVER_ENTRY;
	}
	if (force_data_sync) {
		local_object = VME_OBJECT(entry);
		local_start = entry->vme_start;
		local_offset = (vm_map_offset_t)VME_OFFSET(entry);

		vm_object_reference(local_object);
		vm_map_unlock_read(map);

		vm_object_lock_request(local_object,
		    ((vm_object_offset_t)
		    ((offset - local_start) +
		    local_offset)),
		    (vm_object_size_t)*upl_size,
		    FALSE,
		    MEMORY_OBJECT_DATA_SYNC,
		    VM_PROT_NO_CHANGE);

		force_data_sync = FALSE;
		vm_object_deallocate(local_object);

		goto REDISCOVER_ENTRY;
	}
	if (VME_OBJECT(entry)->private) {
		*flags = UPL_DEV_MEMORY;
	} else {
		*flags = 0;
	}

	if (VME_OBJECT(entry)->phys_contiguous) {
		*flags |= UPL_PHYS_CONTIG;
	}

	local_object = VME_OBJECT(entry);
	local_offset = (vm_map_offset_t)VME_OFFSET(entry);
	local_start = entry->vme_start;

	/*
	 * Wiring will copy the pages to the shadow object.
	 * The shadow object will not be code-signed so
	 * attempting to execute code from these copied pages
	 * would trigger a code-signing violation.
	 */
	if (entry->protection & VM_PROT_EXECUTE) {
#if MACH_ASSERT
		printf("pid %d[%s] create_upl out of executable range from "
		    "0x%llx to 0x%llx: side effects may include "
		    "code-signing violations later on\n",
		    proc_selfpid(),
		    (get_bsdtask_info(current_task())
		    ? proc_name_address(get_bsdtask_info(current_task()))
		    : "?"),
		    (uint64_t) entry->vme_start,
		    (uint64_t) entry->vme_end);
#endif /* MACH_ASSERT */
		DTRACE_VM2(cs_executable_create_upl,
		    uint64_t, (uint64_t)entry->vme_start,
		    uint64_t, (uint64_t)entry->vme_end);
		cs_executable_create_upl++;
	}

	vm_object_lock(local_object);

	/*
	 * Ensure that this object is "true_share" and "copy_delay" now,
	 * while we're still holding the VM map lock.  After we unlock the map,
	 * anything could happen to that mapping, including some copy-on-write
	 * activity.  We need to make sure that the IOPL will point at the
	 * same memory as the mapping.
	 */
	if (local_object->true_share) {
		assert(local_object->copy_strategy !=
		    MEMORY_OBJECT_COPY_SYMMETRIC);
	} else if (!is_kernel_object(local_object) &&
	    local_object != compressor_object &&
	    !local_object->phys_contiguous) {
#if VM_OBJECT_TRACKING_OP_TRUESHARE
		if (!local_object->true_share &&
		    vm_object_tracking_btlog) {
			btlog_record(vm_object_tracking_btlog, local_object,
			    VM_OBJECT_TRACKING_OP_TRUESHARE,
			    btref_get(__builtin_frame_address(0), 0));
		}
#endif /* VM_OBJECT_TRACKING_OP_TRUESHARE */
		VM_OBJECT_SET_TRUE_SHARE(local_object, TRUE);
		if (local_object->copy_strategy ==
		    MEMORY_OBJECT_COPY_SYMMETRIC) {
			local_object->copy_strategy = MEMORY_OBJECT_COPY_DELAY;
		}
	}

	vm_object_reference_locked(local_object);
	vm_object_unlock(local_object);

	vm_map_unlock_read(map);

	offset += offset_in_mapped_page;
	assert(*upl_size > offset_in_mapped_page);
	*upl_size -= offset_in_mapped_page;

	ret = vm_object_iopl_request(local_object,
	    ((vm_object_offset_t)
	    ((offset - local_start) + local_offset)),
	    *upl_size,
	    upl,
	    page_list,
	    count,
	    caller_flags,
	    tag);
	vm_object_deallocate(local_object);

done:
	if (release_map) {
		vm_map_deallocate(map);
	}

	return ret;
}

/*
 * Internal routine to enter a UPL into a VM map.
 *
 * JMM - This should just be doable through the standard
 * vm_map_enter() API.
 */
kern_return_t
vm_map_enter_upl_range(
	vm_map_t                map,
	upl_t                   upl,
	vm_object_offset_t      offset_to_map,
	vm_size_t               size_to_map,
	vm_prot_t               prot_to_map,
	vm_map_offset_t         *dst_addr)
{
	vm_map_size_t           size;
	vm_object_offset_t      offset;
	vm_map_offset_t         addr;
	vm_page_t               m;
	kern_return_t           kr;
	int                     isVectorUPL = 0, curr_upl = 0;
	upl_t                   vector_upl = NULL;
	mach_vm_offset_t        vector_upl_dst_addr = 0;
	vm_map_t                vector_upl_submap = NULL;
	upl_offset_t            subupl_offset = 0;
	upl_size_t              subupl_size = 0;

	if (upl == UPL_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	DEBUG4K_UPL("map %p upl %p flags 0x%x object %p offset 0x%llx (uploff: 0x%llx) size 0x%lx (uplsz: 0x%x) \n", map, upl, upl->flags, upl->map_object, offset_to_map, upl->u_offset, size_to_map, upl->u_size);
	assert(map == kernel_map);

	if ((isVectorUPL = vector_upl_is_valid(upl))) {
		int mapped = 0, valid_upls = 0;
		vector_upl = upl;

		upl_lock(vector_upl);
		for (curr_upl = 0; curr_upl < vector_upl_max_upls(vector_upl); curr_upl++) {
			upl =  vector_upl_subupl_byindex(vector_upl, curr_upl );
			if (upl == NULL) {
				continue;
			}
			valid_upls++;
			if (UPL_PAGE_LIST_MAPPED & upl->flags) {
				mapped++;
			}
		}

		if (mapped) {
			if (mapped != valid_upls) {
				panic("Only %d of the %d sub-upls within the Vector UPL are alread mapped", mapped, valid_upls);
			} else {
				upl_unlock(vector_upl);
				return KERN_FAILURE;
			}
		}

		if (VM_MAP_PAGE_MASK(map) < PAGE_MASK) {
			panic("TODO4K: vector UPL not implemented");
		}

		vector_upl_submap = kmem_suballoc(map, &vector_upl_dst_addr,
		    vector_upl->u_size, VM_MAP_CREATE_DEFAULT,
		    VM_FLAGS_ANYWHERE, KMS_NOFAIL | KMS_DATA,
		    VM_KERN_MEMORY_NONE).kmr_submap;
		map = vector_upl_submap;
		vector_upl_set_submap(vector_upl, vector_upl_submap, vector_upl_dst_addr);
		curr_upl = 0;
	} else {
		upl_lock(upl);
	}

process_upl_to_enter:
	if (isVectorUPL) {
		if (curr_upl == vector_upl_max_upls(vector_upl)) {
			*dst_addr = vector_upl_dst_addr;
			upl_unlock(vector_upl);
			return KERN_SUCCESS;
		}
		upl =  vector_upl_subupl_byindex(vector_upl, curr_upl++ );
		if (upl == NULL) {
			goto process_upl_to_enter;
		}

		vector_upl_get_iostate(vector_upl, upl, &subupl_offset, &subupl_size);
		*dst_addr = (vm_map_offset_t)(vector_upl_dst_addr + (vm_map_offset_t)subupl_offset);
	} else {
		/*
		 * check to see if already mapped
		 */
		if (UPL_PAGE_LIST_MAPPED & upl->flags) {
			upl_unlock(upl);
			return KERN_FAILURE;
		}
	}

	if ((!(upl->flags & UPL_SHADOWED)) &&
	    ((upl->flags & UPL_HAS_BUSY) ||
	    !((upl->flags & (UPL_DEVICE_MEMORY | UPL_IO_WIRE)) || (upl->map_object->phys_contiguous)))) {
		vm_object_t             object;
		vm_page_t               alias_page;
		vm_object_offset_t      new_offset;
		unsigned int            pg_num;

		size = upl_adjusted_size(upl, VM_MAP_PAGE_MASK(map));
		object = upl->map_object;
		upl->map_object = vm_object_allocate(vm_object_round_page(size));

		vm_object_lock(upl->map_object);

		upl->map_object->shadow = object;
		VM_OBJECT_SET_PAGEOUT(upl->map_object, TRUE);
		VM_OBJECT_SET_CAN_PERSIST(upl->map_object, FALSE);
		upl->map_object->copy_strategy = MEMORY_OBJECT_COPY_NONE;
		upl->map_object->vo_shadow_offset = upl_adjusted_offset(upl, PAGE_MASK) - object->paging_offset;
		assertf(page_aligned(upl->map_object->vo_shadow_offset),
		    "object %p shadow_offset 0x%llx",
		    upl->map_object,
		    (uint64_t)upl->map_object->vo_shadow_offset);
		upl->map_object->wimg_bits = object->wimg_bits;
		offset = upl->map_object->vo_shadow_offset;
		new_offset = 0;

		upl->flags |= UPL_SHADOWED;

		while (size) {
			pg_num = (unsigned int) (new_offset / PAGE_SIZE);
			assert(pg_num == new_offset / PAGE_SIZE);

			if (bitmap_test(upl->lite_list, pg_num)) {
				alias_page = vm_page_grab_fictitious(TRUE);

				vm_object_lock(object);

				m = vm_page_lookup(object, offset);
				if (m == VM_PAGE_NULL) {
					panic("vm_upl_map: page missing");
				}

				/*
				 * Convert the fictitious page to a private
				 * shadow of the real page.
				 */
				assert(alias_page->vmp_fictitious);
				alias_page->vmp_fictitious = FALSE;
				alias_page->vmp_private = TRUE;
				alias_page->vmp_free_when_done = TRUE;
				/*
				 * since m is a page in the upl it must
				 * already be wired or BUSY, so it's
				 * safe to assign the underlying physical
				 * page to the alias
				 */
				VM_PAGE_SET_PHYS_PAGE(alias_page, VM_PAGE_GET_PHYS_PAGE(m));

				vm_object_unlock(object);

				vm_page_lockspin_queues();
				vm_page_wire(alias_page, VM_KERN_MEMORY_NONE, TRUE);
				vm_page_unlock_queues();

				vm_page_insert_wired(alias_page, upl->map_object, new_offset, VM_KERN_MEMORY_NONE);

				assert(!alias_page->vmp_wanted);
				alias_page->vmp_busy = FALSE;
				alias_page->vmp_absent = FALSE;
			}
			size -= PAGE_SIZE;
			offset += PAGE_SIZE_64;
			new_offset += PAGE_SIZE_64;
		}
		vm_object_unlock(upl->map_object);
	}
	if (upl->flags & UPL_SHADOWED) {
		if (isVectorUPL) {
			offset = 0;
		} else {
			offset = offset_to_map;
		}
	} else {
		offset = upl_adjusted_offset(upl, VM_MAP_PAGE_MASK(map)) - upl->map_object->paging_offset;
		if (!isVectorUPL) {
			offset += offset_to_map;
		}
	}

	if (isVectorUPL) {
		size = upl_adjusted_size(upl, VM_MAP_PAGE_MASK(map));
	} else {
		size = MIN(upl_adjusted_size(upl, VM_MAP_PAGE_MASK(map)), size_to_map);
	}

	vm_object_reference(upl->map_object);

	if (!isVectorUPL) {
		*dst_addr = 0;
		/*
		 * NEED A UPL_MAP ALIAS
		 */
		kr = vm_map_enter(map, dst_addr, (vm_map_size_t)size, (vm_map_offset_t) 0,
		    VM_MAP_KERNEL_FLAGS_DATA_ANYWHERE(.vm_tag = VM_KERN_MEMORY_OSFMK),
		    upl->map_object, offset, FALSE,
		    prot_to_map, VM_PROT_ALL, VM_INHERIT_DEFAULT);

		if (kr != KERN_SUCCESS) {
			vm_object_deallocate(upl->map_object);
			upl_unlock(upl);
			return kr;
		}
	} else {
		kr = vm_map_enter(map, dst_addr, (vm_map_size_t)size, (vm_map_offset_t) 0,
		    VM_MAP_KERNEL_FLAGS_FIXED(.vm_tag = VM_KERN_MEMORY_OSFMK),
		    upl->map_object, offset, FALSE,
		    prot_to_map, VM_PROT_ALL, VM_INHERIT_DEFAULT);
		if (kr) {
			panic("vm_map_enter failed for a Vector UPL");
		}
	}
	upl->u_mapped_size = (upl_size_t) size; /* When we allow multiple submappings of the UPL */
	                                        /* this will have to be an increment rather than */
	                                        /* an assignment. */
	vm_object_lock(upl->map_object);

	for (addr = *dst_addr; size > 0; size -= PAGE_SIZE, addr += PAGE_SIZE) {
		m = vm_page_lookup(upl->map_object, offset);

		if (m) {
			m->vmp_pmapped = TRUE;

			/*
			 * CODE SIGNING ENFORCEMENT: page has been wpmapped,
			 * but only in kernel space. If this was on a user map,
			 * we'd have to set the wpmapped bit.
			 */
			/* m->vmp_wpmapped = TRUE; */
			assert(map->pmap == kernel_pmap);

			kr = pmap_enter_check(map->pmap, addr, m, prot_to_map, VM_PROT_NONE, 0, TRUE);

			assert(kr == KERN_SUCCESS);
#if KASAN
			kasan_notify_address(addr, PAGE_SIZE_64);
#endif
		}
		offset += PAGE_SIZE_64;
	}
	vm_object_unlock(upl->map_object);

	/*
	 * hold a reference for the mapping
	 */
	upl->ref_count++;
	upl->flags |= UPL_PAGE_LIST_MAPPED;
	upl->kaddr = (vm_offset_t) *dst_addr;
	assert(upl->kaddr == *dst_addr);

	if (isVectorUPL) {
		goto process_upl_to_enter;
	}

	if (!isVectorUPL) {
		vm_map_offset_t addr_adjustment;

		addr_adjustment = (vm_map_offset_t)(upl->u_offset - upl_adjusted_offset(upl, VM_MAP_PAGE_MASK(map)));
		if (addr_adjustment) {
			assert(VM_MAP_PAGE_MASK(map) != PAGE_MASK);
			DEBUG4K_UPL("dst_addr 0x%llx (+ 0x%llx) -> 0x%llx\n", (uint64_t)*dst_addr, (uint64_t)addr_adjustment, (uint64_t)(*dst_addr + addr_adjustment));
			*dst_addr += addr_adjustment;
		}
	}

	upl_unlock(upl);

	return KERN_SUCCESS;
}

kern_return_t
vm_map_enter_upl(
	vm_map_t                map,
	upl_t                   upl,
	vm_map_offset_t         *dst_addr)
{
	upl_size_t upl_size = upl_adjusted_size(upl, VM_MAP_PAGE_MASK(map));
	return vm_map_enter_upl_range(map, upl, 0, upl_size, VM_PROT_DEFAULT, dst_addr);
}

/*
 * Internal routine to remove a UPL mapping from a VM map.
 *
 * XXX - This should just be doable through a standard
 * vm_map_remove() operation.  Otherwise, implicit clean-up
 * of the target map won't be able to correctly remove
 * these (and release the reference on the UPL).  Having
 * to do this means we can't map these into user-space
 * maps yet.
 */
kern_return_t
vm_map_remove_upl_range(
	vm_map_t        map,
	upl_t           upl,
	__unused vm_object_offset_t    offset_to_unmap,
	__unused vm_size_t             size_to_unmap)
{
	vm_address_t    addr;
	upl_size_t      size;
	int             isVectorUPL = 0, curr_upl = 0;
	upl_t           vector_upl = NULL;

	if (upl == UPL_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	if ((isVectorUPL = vector_upl_is_valid(upl))) {
		int     unmapped = 0, valid_upls = 0;
		vector_upl = upl;
		upl_lock(vector_upl);
		for (curr_upl = 0; curr_upl < vector_upl_max_upls(vector_upl); curr_upl++) {
			upl =  vector_upl_subupl_byindex(vector_upl, curr_upl );
			if (upl == NULL) {
				continue;
			}
			valid_upls++;
			if (!(UPL_PAGE_LIST_MAPPED & upl->flags)) {
				unmapped++;
			}
		}

		if (unmapped) {
			if (unmapped != valid_upls) {
				panic("%d of the %d sub-upls within the Vector UPL is/are not mapped", unmapped, valid_upls);
			} else {
				upl_unlock(vector_upl);
				return KERN_FAILURE;
			}
		}
		curr_upl = 0;
	} else {
		upl_lock(upl);
	}

process_upl_to_remove:
	if (isVectorUPL) {
		if (curr_upl == vector_upl_max_upls(vector_upl)) {
			vm_map_t v_upl_submap;
			vm_offset_t v_upl_submap_dst_addr;
			vector_upl_get_submap(vector_upl, &v_upl_submap, &v_upl_submap_dst_addr);

			kmem_free_guard(map, v_upl_submap_dst_addr,
			    vector_upl->u_size, KMF_NONE, KMEM_GUARD_SUBMAP);
			vm_map_deallocate(v_upl_submap);
			upl_unlock(vector_upl);
			return KERN_SUCCESS;
		}

		upl =  vector_upl_subupl_byindex(vector_upl, curr_upl++ );
		if (upl == NULL) {
			goto process_upl_to_remove;
		}
	}

	if (upl->flags & UPL_PAGE_LIST_MAPPED) {
		addr = upl->kaddr;
		size = upl->u_mapped_size;

		assert(upl->ref_count > 1);
		upl->ref_count--;               /* removing mapping ref */

		upl->flags &= ~UPL_PAGE_LIST_MAPPED;
		upl->kaddr = (vm_offset_t) 0;
		upl->u_mapped_size = 0;

		if (isVectorUPL) {
			/*
			 * If it's a Vectored UPL, we'll be removing the entire
			 * submap anyways, so no need to remove individual UPL
			 * element mappings from within the submap
			 */
			goto process_upl_to_remove;
		}

		upl_unlock(upl);

		vm_map_remove(map,
		    vm_map_trunc_page(addr, VM_MAP_PAGE_MASK(map)),
		    vm_map_round_page(addr + size, VM_MAP_PAGE_MASK(map)));
		return KERN_SUCCESS;
	}
	upl_unlock(upl);

	return KERN_FAILURE;
}

kern_return_t
vm_map_remove_upl(
	vm_map_t        map,
	upl_t           upl)
{
	upl_size_t upl_size = upl_adjusted_size(upl, VM_MAP_PAGE_MASK(map));
	return vm_map_remove_upl_range(map, upl, 0, upl_size);
}

void
iopl_valid_data(
	upl_t    upl,
	vm_tag_t tag)
{
	vm_object_t     object;
	vm_offset_t     offset;
	vm_page_t       m, nxt_page = VM_PAGE_NULL;
	upl_size_t      size;
	int             wired_count = 0;

	if (upl == NULL) {
		panic("iopl_valid_data: NULL upl");
	}
	if (vector_upl_is_valid(upl)) {
		panic("iopl_valid_data: vector upl");
	}
	if ((upl->flags & (UPL_DEVICE_MEMORY | UPL_SHADOWED | UPL_ACCESS_BLOCKED | UPL_IO_WIRE | UPL_INTERNAL)) != UPL_IO_WIRE) {
		panic("iopl_valid_data: unsupported upl, flags = %x", upl->flags);
	}

	object = upl->map_object;

	if (is_kernel_object(object) || object == compressor_object) {
		panic("iopl_valid_data: object == kernel or compressor");
	}

	if (object->purgable == VM_PURGABLE_VOLATILE ||
	    object->purgable == VM_PURGABLE_EMPTY) {
		panic("iopl_valid_data: object %p purgable %d",
		    object, object->purgable);
	}

	size = upl_adjusted_size(upl, PAGE_MASK);

	vm_object_lock(object);
	VM_OBJECT_WIRED_PAGE_UPDATE_START(object);

	bool whole_object;

	if (object->vo_size == size && object->resident_page_count == (size / PAGE_SIZE)) {
		nxt_page = (vm_page_t)vm_page_queue_first(&object->memq);
		whole_object = true;
	} else {
		offset = (vm_offset_t)(upl_adjusted_offset(upl, PAGE_MASK) - object->paging_offset);
		whole_object = false;
	}

	while (size) {
		if (whole_object) {
			if (nxt_page != VM_PAGE_NULL) {
				m = nxt_page;
				nxt_page = (vm_page_t)vm_page_queue_next(&nxt_page->vmp_listq);
			}
		} else {
			m = vm_page_lookup(object, offset);
			offset += PAGE_SIZE;

			if (m == VM_PAGE_NULL) {
				panic("iopl_valid_data: missing expected page at offset %lx", (long)offset);
			}
		}
		if (m->vmp_busy) {
			if (!m->vmp_absent) {
				panic("iopl_valid_data: busy page w/o absent");
			}

			if (m->vmp_pageq.next || m->vmp_pageq.prev) {
				panic("iopl_valid_data: busy+absent page on page queue");
			}
			if (m->vmp_reusable) {
				panic("iopl_valid_data: %p is reusable", m);
			}

			m->vmp_absent = FALSE;
			m->vmp_dirty = TRUE;
			assert(m->vmp_q_state == VM_PAGE_NOT_ON_Q);
			assert(m->vmp_wire_count == 0);
			m->vmp_wire_count++;
			assert(m->vmp_wire_count);
			if (m->vmp_wire_count == 1) {
				m->vmp_q_state = VM_PAGE_IS_WIRED;
				wired_count++;
			} else {
				panic("iopl_valid_data: %p already wired", m);
			}

			vm_page_wakeup_done(object, m);
		}
		size -= PAGE_SIZE;
	}
	if (wired_count) {
		VM_OBJECT_WIRED_PAGE_COUNT(object, wired_count);
		assert(object->resident_page_count >= object->wired_page_count);

		/* no need to adjust purgeable accounting for this object: */
		assert(object->purgable != VM_PURGABLE_VOLATILE);
		assert(object->purgable != VM_PURGABLE_EMPTY);

		vm_page_lockspin_queues();
		vm_page_wire_count += wired_count;
		vm_page_unlock_queues();
	}
	VM_OBJECT_WIRED_PAGE_UPDATE_END(object, tag);
	vm_object_unlock(object);
}


void
vm_object_set_pmap_cache_attr(
	vm_object_t             object,
	upl_page_info_array_t   user_page_list,
	unsigned int            num_pages,
	boolean_t               batch_pmap_op)
{
	unsigned int    cache_attr = 0;

	cache_attr = object->wimg_bits & VM_WIMG_MASK;
	assert(user_page_list);
	if (cache_attr != VM_WIMG_USE_DEFAULT) {
		PMAP_BATCH_SET_CACHE_ATTR(object, user_page_list, cache_attr, num_pages, batch_pmap_op);
	}
}


static bool
vm_object_iopl_wire_full(
	vm_object_t             object,
	upl_t                   upl,
	upl_page_info_array_t   user_page_list,
	upl_control_flags_t     cntrl_flags,
	vm_tag_t                tag)
{
	vm_page_t       dst_page;
	unsigned int    entry;
	int             page_count;
	int             delayed_unlock = 0;
	boolean_t       retval = TRUE;
	ppnum_t         phys_page;

	vm_object_lock_assert_exclusive(object);
	assert(object->purgable != VM_PURGABLE_VOLATILE);
	assert(object->purgable != VM_PURGABLE_EMPTY);
	assert(object->pager == NULL);
	assert(object->vo_copy == NULL);
	assert(object->shadow == NULL);

	page_count = object->resident_page_count;
	dst_page = (vm_page_t)vm_page_queue_first(&object->memq);

	vm_page_lock_queues();

	while (page_count--) {
		if (dst_page->vmp_busy ||
		    dst_page->vmp_fictitious ||
		    dst_page->vmp_absent ||
		    VMP_ERROR_GET(dst_page) ||
		    dst_page->vmp_cleaning ||
		    dst_page->vmp_restart ||
		    dst_page->vmp_laundry) {
			retval = FALSE;
			goto done;
		}
		if ((cntrl_flags & UPL_REQUEST_FORCE_COHERENCY) && dst_page->vmp_written_by_kernel == TRUE) {
			retval = FALSE;
			goto done;
		}
		dst_page->vmp_reference = TRUE;

		vm_page_wire(dst_page, tag, FALSE);

		if (!(cntrl_flags & UPL_COPYOUT_FROM)) {
			SET_PAGE_DIRTY(dst_page, FALSE);
		}
		entry = (unsigned int)(dst_page->vmp_offset / PAGE_SIZE);
		assert(entry >= 0 && entry < object->resident_page_count);
		bitmap_set(upl->lite_list, entry);

		phys_page = VM_PAGE_GET_PHYS_PAGE(dst_page);

		if (phys_page > upl->highest_page) {
			upl->highest_page = phys_page;
		}

		if (user_page_list) {
			user_page_list[entry].phys_addr = phys_page;
			user_page_list[entry].absent    = dst_page->vmp_absent;
			user_page_list[entry].dirty     = dst_page->vmp_dirty;
			user_page_list[entry].free_when_done   = dst_page->vmp_free_when_done;
			user_page_list[entry].precious  = dst_page->vmp_precious;
			user_page_list[entry].device    = FALSE;
			user_page_list[entry].speculative = FALSE;
			user_page_list[entry].cs_validated = FALSE;
			user_page_list[entry].cs_tainted = FALSE;
			user_page_list[entry].cs_nx     = FALSE;
			user_page_list[entry].needed    = FALSE;
			user_page_list[entry].mark      = FALSE;
		}
		if (delayed_unlock++ > 256) {
			delayed_unlock = 0;
			lck_mtx_yield(&vm_page_queue_lock);

			VM_CHECK_MEMORYSTATUS;
		}
		dst_page = (vm_page_t)vm_page_queue_next(&dst_page->vmp_listq);
	}
done:
	vm_page_unlock_queues();

	VM_CHECK_MEMORYSTATUS;

	return retval;
}


static kern_return_t
vm_object_iopl_wire_empty(
	vm_object_t             object,
	upl_t                   upl,
	upl_page_info_array_t   user_page_list,
	upl_control_flags_t     cntrl_flags,
	vm_tag_t                tag,
	vm_object_offset_t     *dst_offset,
	int                     page_count,
	int                    *page_grab_count)
{
	vm_page_t       dst_page;
	boolean_t       no_zero_fill = FALSE;
	int             interruptible;
	int             pages_wired = 0;
	int             pages_inserted = 0;
	int             entry = 0;
	uint64_t        delayed_ledger_update = 0;
	kern_return_t   ret = KERN_SUCCESS;
	int             grab_options;
	ppnum_t         phys_page;

	vm_object_lock_assert_exclusive(object);
	assert(object->purgable != VM_PURGABLE_VOLATILE);
	assert(object->purgable != VM_PURGABLE_EMPTY);
	assert(object->pager == NULL);
	assert(object->vo_copy == NULL);
	assert(object->shadow == NULL);

	if (cntrl_flags & UPL_SET_INTERRUPTIBLE) {
		interruptible = THREAD_ABORTSAFE;
	} else {
		interruptible = THREAD_UNINT;
	}

	if (cntrl_flags & (UPL_NOZEROFILL | UPL_NOZEROFILLIO)) {
		no_zero_fill = TRUE;
	}

	grab_options = 0;
#if CONFIG_SECLUDED_MEMORY
	if (object->can_grab_secluded) {
		grab_options |= VM_PAGE_GRAB_SECLUDED;
	}
#endif /* CONFIG_SECLUDED_MEMORY */

	while (page_count--) {
		while ((dst_page = vm_page_grab_options(grab_options))
		    == VM_PAGE_NULL) {
			OSAddAtomic(page_count, &vm_upl_wait_for_pages);

			VM_DEBUG_EVENT(vm_iopl_page_wait, DBG_VM_IOPL_PAGE_WAIT, DBG_FUNC_START, vm_upl_wait_for_pages, 0, 0, 0);

			if (vm_page_wait(interruptible) == FALSE) {
				/*
				 * interrupted case
				 */
				OSAddAtomic(-page_count, &vm_upl_wait_for_pages);

				VM_DEBUG_EVENT(vm_iopl_page_wait, DBG_VM_IOPL_PAGE_WAIT, DBG_FUNC_END, vm_upl_wait_for_pages, 0, 0, -1);

				ret = MACH_SEND_INTERRUPTED;
				goto done;
			}
			OSAddAtomic(-page_count, &vm_upl_wait_for_pages);

			VM_DEBUG_EVENT(vm_iopl_page_wait, DBG_VM_IOPL_PAGE_WAIT, DBG_FUNC_END, vm_upl_wait_for_pages, 0, 0, 0);
		}
		if (no_zero_fill == FALSE) {
			vm_page_zero_fill(dst_page);
		} else {
			dst_page->vmp_absent = TRUE;
		}

		dst_page->vmp_reference = TRUE;

		if (!(cntrl_flags & UPL_COPYOUT_FROM)) {
			SET_PAGE_DIRTY(dst_page, FALSE);
		}
		if (dst_page->vmp_absent == FALSE) {
			assert(dst_page->vmp_q_state == VM_PAGE_NOT_ON_Q);
			assert(dst_page->vmp_wire_count == 0);
			dst_page->vmp_wire_count++;
			dst_page->vmp_q_state = VM_PAGE_IS_WIRED;
			assert(dst_page->vmp_wire_count);
			pages_wired++;
			vm_page_wakeup_done(object, dst_page);
		}
		pages_inserted++;

		vm_page_insert_internal(dst_page, object, *dst_offset, tag, FALSE, TRUE, TRUE, TRUE, &delayed_ledger_update);

		bitmap_set(upl->lite_list, entry);

		phys_page = VM_PAGE_GET_PHYS_PAGE(dst_page);

		if (phys_page > upl->highest_page) {
			upl->highest_page = phys_page;
		}

		if (user_page_list) {
			user_page_list[entry].phys_addr = phys_page;
			user_page_list[entry].absent    = dst_page->vmp_absent;
			user_page_list[entry].dirty     = dst_page->vmp_dirty;
			user_page_list[entry].free_when_done    = FALSE;
			user_page_list[entry].precious  = FALSE;
			user_page_list[entry].device    = FALSE;
			user_page_list[entry].speculative = FALSE;
			user_page_list[entry].cs_validated = FALSE;
			user_page_list[entry].cs_tainted = FALSE;
			user_page_list[entry].cs_nx     = FALSE;
			user_page_list[entry].needed    = FALSE;
			user_page_list[entry].mark      = FALSE;
		}
		entry++;
		*dst_offset += PAGE_SIZE_64;
	}
done:
	if (pages_wired) {
		vm_page_lockspin_queues();
		vm_page_wire_count += pages_wired;
		vm_page_unlock_queues();
	}
	if (pages_inserted) {
		if (object->internal) {
			OSAddAtomic(pages_inserted, &vm_page_internal_count);
		} else {
			OSAddAtomic(pages_inserted, &vm_page_external_count);
		}
	}
	if (delayed_ledger_update) {
		task_t          owner;
		int             ledger_idx_volatile;
		int             ledger_idx_nonvolatile;
		int             ledger_idx_volatile_compressed;
		int             ledger_idx_nonvolatile_compressed;
		int             ledger_idx_composite;
		int             ledger_idx_external_wired;
		boolean_t       do_footprint;

		owner = VM_OBJECT_OWNER(object);
		assert(owner);

		vm_object_ledger_tag_ledgers(object,
		    &ledger_idx_volatile,
		    &ledger_idx_nonvolatile,
		    &ledger_idx_volatile_compressed,
		    &ledger_idx_nonvolatile_compressed,
		    &ledger_idx_composite,
		    &ledger_idx_external_wired,
		    &do_footprint);

		if (object->internal) {
			/* more non-volatile bytes */
			ledger_credit(owner->ledger,
			    ledger_idx_nonvolatile,
			    delayed_ledger_update);
			if (do_footprint) {
				/* more footprint */
				ledger_credit(owner->ledger,
				    task_ledgers.phys_footprint,
				    delayed_ledger_update);
			} else if (ledger_idx_composite != -1) {
				ledger_credit(owner->ledger,
				    ledger_idx_composite,
				    delayed_ledger_update);
			}
		} else {
			/* more external wired bytes */
			ledger_credit(owner->ledger,
			    ledger_idx_external_wired,
			    delayed_ledger_update);
			if (do_footprint) {
				/* more footprint */
				ledger_credit(owner->ledger,
				    task_ledgers.phys_footprint,
				    delayed_ledger_update);
			} else if (ledger_idx_composite != -1) {
				ledger_credit(owner->ledger,
				    ledger_idx_composite,
				    delayed_ledger_update);
			}
		}
	}

	assert(page_grab_count);
	*page_grab_count = pages_inserted;

	return ret;
}



kern_return_t
vm_object_iopl_request(
	vm_object_t             object,
	vm_object_offset_t      offset,
	upl_size_t              size,
	upl_t                   *upl_ptr,
	upl_page_info_array_t   user_page_list,
	unsigned int            *page_list_count,
	upl_control_flags_t     cntrl_flags,
	vm_tag_t                tag)
{
	vm_page_t               dst_page;
	vm_object_offset_t      dst_offset;
	upl_size_t              xfer_size;
	upl_t                   upl = NULL;
	unsigned int            entry;
	int                     no_zero_fill = FALSE;
	unsigned int            size_in_pages;
	int                     page_grab_count = 0;
	u_int32_t               psize;
	kern_return_t           ret;
	vm_prot_t               prot;
	struct vm_object_fault_info fault_info = {};
	struct  vm_page_delayed_work    dw_array;
	struct  vm_page_delayed_work    *dwp, *dwp_start;
	bool                    dwp_finish_ctx = TRUE;
	int                     dw_count;
	int                     dw_limit;
	int                     dw_index;
	boolean_t               caller_lookup;
	int                     io_tracking_flag = 0;
	int                     interruptible;
	ppnum_t                 phys_page;

	boolean_t               set_cache_attr_needed = FALSE;
	boolean_t               free_wired_pages = FALSE;
	boolean_t               fast_path_empty_req = FALSE;
	boolean_t               fast_path_full_req = FALSE;

#if DEVELOPMENT || DEBUG
	task_t                  task = current_task();
#endif /* DEVELOPMENT || DEBUG */

	dwp_start = dwp = NULL;

	vm_object_offset_t original_offset = offset;
	upl_size_t original_size = size;

//	DEBUG4K_UPL("object %p offset 0x%llx size 0x%llx cntrl_flags 0x%llx\n", object, (uint64_t)offset, (uint64_t)size, cntrl_flags);

	size = (upl_size_t)(vm_object_round_page(offset + size) - vm_object_trunc_page(offset));
	offset = vm_object_trunc_page(offset);
	if (size != original_size || offset != original_offset) {
		DEBUG4K_IOKIT("flags 0x%llx object %p offset 0x%llx size 0x%x -> offset 0x%llx size 0x%x\n", cntrl_flags, object, original_offset, original_size, offset, size);
	}

	if (cntrl_flags & ~UPL_VALID_FLAGS) {
		/*
		 * For forward compatibility's sake,
		 * reject any unknown flag.
		 */
		return KERN_INVALID_VALUE;
	}
	if (vm_lopage_needed == FALSE) {
		cntrl_flags &= ~UPL_NEED_32BIT_ADDR;
	}

	if (cntrl_flags & UPL_NEED_32BIT_ADDR) {
		if ((cntrl_flags & (UPL_SET_IO_WIRE | UPL_SET_LITE)) != (UPL_SET_IO_WIRE | UPL_SET_LITE)) {
			return KERN_INVALID_VALUE;
		}

		if (object->phys_contiguous) {
			if ((offset + object->vo_shadow_offset) >= (vm_object_offset_t)max_valid_dma_address) {
				return KERN_INVALID_ADDRESS;
			}

			if (((offset + object->vo_shadow_offset) + size) >= (vm_object_offset_t)max_valid_dma_address) {
				return KERN_INVALID_ADDRESS;
			}
		}
	}
	if (cntrl_flags & (UPL_NOZEROFILL | UPL_NOZEROFILLIO)) {
		no_zero_fill = TRUE;
	}

	if (cntrl_flags & UPL_COPYOUT_FROM) {
		prot = VM_PROT_READ;
	} else {
		prot = VM_PROT_READ | VM_PROT_WRITE;
	}

	if ((!object->internal) && (object->paging_offset != 0)) {
		panic("vm_object_iopl_request: external object with non-zero paging offset");
	}


	VM_DEBUG_CONSTANT_EVENT(vm_object_iopl_request, DBG_VM_IOPL_REQUEST, DBG_FUNC_START, size, cntrl_flags, prot, 0);

#if CONFIG_IOSCHED || UPL_DEBUG
	if ((object->io_tracking && !is_kernel_object(object)) || upl_debug_enabled) {
		io_tracking_flag |= UPL_CREATE_IO_TRACKING;
	}
#endif

#if CONFIG_IOSCHED
	if (object->io_tracking) {
		/* Check if we're dealing with the kernel object. We do not support expedite on kernel object UPLs */
		if (!is_kernel_object(object)) {
			io_tracking_flag |= UPL_CREATE_EXPEDITE_SUP;
		}
	}
#endif

	if (object->phys_contiguous) {
		psize = PAGE_SIZE;
	} else {
		psize = size;

		dw_count = 0;
		dw_limit = DELAYED_WORK_LIMIT(DEFAULT_DELAYED_WORK_LIMIT);
		dwp_start = vm_page_delayed_work_get_ctx();
		if (dwp_start == NULL) {
			dwp_start = &dw_array;
			dw_limit = 1;
			dwp_finish_ctx = FALSE;
		}

		dwp = dwp_start;
	}

	if (cntrl_flags & UPL_SET_INTERNAL) {
		upl = upl_create(UPL_CREATE_INTERNAL | UPL_CREATE_LITE | io_tracking_flag, UPL_IO_WIRE, psize);
		user_page_list = size ? upl->page_list : NULL;
	} else {
		upl = upl_create(UPL_CREATE_LITE | io_tracking_flag, UPL_IO_WIRE, psize);
	}
	if (user_page_list) {
		user_page_list[0].device = FALSE;
	}
	*upl_ptr = upl;

	if (cntrl_flags & UPL_NOZEROFILLIO) {
		DTRACE_VM4(upl_nozerofillio,
		    vm_object_t, object,
		    vm_object_offset_t, offset,
		    upl_size_t, size,
		    upl_t, upl);
	}

	upl->map_object = object;
	upl->u_offset = original_offset;
	upl->u_size = original_size;

	size_in_pages = size / PAGE_SIZE;

	if (is_kernel_object(object) &&
	    !(cntrl_flags & (UPL_NEED_32BIT_ADDR | UPL_BLOCK_ACCESS))) {
		upl->flags |= UPL_KERNEL_OBJECT;
#if UPL_DEBUG
		vm_object_lock(object);
#else
		vm_object_lock_shared(object);
#endif
	} else {
		vm_object_lock(object);
		vm_object_activity_begin(object);
	}
	/*
	 * paging in progress also protects the paging_offset
	 */
	upl->u_offset = original_offset + object->paging_offset;

	if (cntrl_flags & UPL_BLOCK_ACCESS) {
		/*
		 * The user requested that access to the pages in this UPL
		 * be blocked until the UPL is commited or aborted.
		 */
		upl->flags |= UPL_ACCESS_BLOCKED;
	}

#if CONFIG_IOSCHED || UPL_DEBUG
	if ((upl->flags & UPL_TRACKED_BY_OBJECT) || upl_debug_enabled) {
		vm_object_activity_begin(object);
		queue_enter(&object->uplq, upl, upl_t, uplq);
	}
#endif

	if (object->phys_contiguous) {
		if (upl->flags & UPL_ACCESS_BLOCKED) {
			assert(!object->blocked_access);
			object->blocked_access = TRUE;
		}

		vm_object_unlock(object);

		/*
		 * don't need any shadow mappings for this one
		 * since it is already I/O memory
		 */
		upl->flags |= UPL_DEVICE_MEMORY;

		upl->highest_page = (ppnum_t) ((offset + object->vo_shadow_offset + size - 1) >> PAGE_SHIFT);

		if (user_page_list) {
			user_page_list[0].phys_addr = (ppnum_t) ((offset + object->vo_shadow_offset) >> PAGE_SHIFT);
			user_page_list[0].device = TRUE;
		}
		if (page_list_count != NULL) {
			if (upl->flags & UPL_INTERNAL) {
				*page_list_count = 0;
			} else {
				*page_list_count = 1;
			}
		}

		VM_DEBUG_CONSTANT_EVENT(vm_object_iopl_request, DBG_VM_IOPL_REQUEST, DBG_FUNC_END, page_grab_count, KERN_SUCCESS, 0, 0);
#if DEVELOPMENT || DEBUG
		if (task != NULL) {
			ledger_credit(task->ledger, task_ledgers.pages_grabbed_iopl, page_grab_count);
		}
#endif /* DEVELOPMENT || DEBUG */
		return KERN_SUCCESS;
	}
	if (!is_kernel_object(object) && object != compressor_object) {
		/*
		 * Protect user space from future COW operations
		 */
#if VM_OBJECT_TRACKING_OP_TRUESHARE
		if (!object->true_share &&
		    vm_object_tracking_btlog) {
			btlog_record(vm_object_tracking_btlog, object,
			    VM_OBJECT_TRACKING_OP_TRUESHARE,
			    btref_get(__builtin_frame_address(0), 0));
		}
#endif /* VM_OBJECT_TRACKING_OP_TRUESHARE */

		vm_object_lock_assert_exclusive(object);
		VM_OBJECT_SET_TRUE_SHARE(object, TRUE);

		if (object->copy_strategy == MEMORY_OBJECT_COPY_SYMMETRIC) {
			object->copy_strategy = MEMORY_OBJECT_COPY_DELAY;
		}
	}

	if (!(cntrl_flags & UPL_COPYOUT_FROM) &&
	    object->vo_copy != VM_OBJECT_NULL) {
		/*
		 * Honor copy-on-write obligations
		 *
		 * The caller is gathering these pages and
		 * might modify their contents.  We need to
		 * make sure that the copy object has its own
		 * private copies of these pages before we let
		 * the caller modify them.
		 *
		 * NOTE: someone else could map the original object
		 * after we've done this copy-on-write here, and they
		 * could then see an inconsistent picture of the memory
		 * while it's being modified via the UPL.  To prevent this,
		 * we would have to block access to these pages until the
		 * UPL is released.  We could use the UPL_BLOCK_ACCESS
		 * code path for that...
		 */
		vm_object_update(object,
		    offset,
		    size,
		    NULL,
		    NULL,
		    FALSE,              /* should_return */
		    MEMORY_OBJECT_COPY_SYNC,
		    VM_PROT_NO_CHANGE);
		VM_PAGEOUT_DEBUG(iopl_cow, 1);
		VM_PAGEOUT_DEBUG(iopl_cow_pages, (size >> PAGE_SHIFT));
	}
	if (!(cntrl_flags & (UPL_NEED_32BIT_ADDR | UPL_BLOCK_ACCESS)) &&
	    object->purgable != VM_PURGABLE_VOLATILE &&
	    object->purgable != VM_PURGABLE_EMPTY &&
	    object->vo_copy == NULL &&
	    size == object->vo_size &&
	    offset == 0 &&
	    object->shadow == NULL &&
	    object->pager == NULL) {
		if (object->resident_page_count == size_in_pages) {
			assert(object != compressor_object);
			assert(!is_kernel_object(object));
			fast_path_full_req = TRUE;
		} else if (object->resident_page_count == 0) {
			assert(object != compressor_object);
			assert(!is_kernel_object(object));
			fast_path_empty_req = TRUE;
			set_cache_attr_needed = TRUE;
		}
	}

	if (cntrl_flags & UPL_SET_INTERRUPTIBLE) {
		interruptible = THREAD_ABORTSAFE;
	} else {
		interruptible = THREAD_UNINT;
	}

	entry = 0;

	xfer_size = size;
	dst_offset = offset;

	if (fast_path_full_req) {
		if (vm_object_iopl_wire_full(object, upl, user_page_list, cntrl_flags, tag) == TRUE) {
			goto finish;
		}
		/*
		 * we couldn't complete the processing of this request on the fast path
		 * so fall through to the slow path and finish up
		 */
	} else if (fast_path_empty_req) {
		if (cntrl_flags & UPL_REQUEST_NO_FAULT) {
			ret = KERN_MEMORY_ERROR;
			goto return_err;
		}
		ret = vm_object_iopl_wire_empty(object, upl, user_page_list,
		    cntrl_flags, tag, &dst_offset, size_in_pages, &page_grab_count);

		if (ret) {
			free_wired_pages = TRUE;
			goto return_err;
		}
		goto finish;
	}

	fault_info.behavior = VM_BEHAVIOR_SEQUENTIAL;
	fault_info.lo_offset = offset;
	fault_info.hi_offset = offset + xfer_size;
	fault_info.mark_zf_absent = TRUE;
	fault_info.interruptible = interruptible;
	fault_info.batch_pmap_op = TRUE;

	while (xfer_size) {
		vm_fault_return_t       result;

		dwp->dw_mask = 0;

		if (fast_path_full_req) {
			/*
			 * if we get here, it means that we ran into a page
			 * state we couldn't handle in the fast path and
			 * bailed out to the slow path... since the order
			 * we look at pages is different between the 2 paths,
			 * the following check is needed to determine whether
			 * this page was already processed in the fast path
			 */
			if (bitmap_test(upl->lite_list, entry)) {
				goto skip_page;
			}
		}
		dst_page = vm_page_lookup(object, dst_offset);

		if (dst_page == VM_PAGE_NULL ||
		    dst_page->vmp_busy ||
		    VMP_ERROR_GET(dst_page) ||
		    dst_page->vmp_restart ||
		    dst_page->vmp_absent ||
		    dst_page->vmp_fictitious) {
			if (is_kernel_object(object)) {
				panic("vm_object_iopl_request: missing/bad page in kernel object");
			}
			if (object == compressor_object) {
				panic("vm_object_iopl_request: missing/bad page in compressor object");
			}

			if (cntrl_flags & UPL_REQUEST_NO_FAULT) {
				ret = KERN_MEMORY_ERROR;
				goto return_err;
			}
			set_cache_attr_needed = TRUE;

			/*
			 * We just looked up the page and the result remains valid
			 * until the object lock is release, so send it to
			 * vm_fault_page() (as "dst_page"), to avoid having to
			 * look it up again there.
			 */
			caller_lookup = TRUE;

			do {
				vm_page_t       top_page;
				kern_return_t   error_code;

				fault_info.cluster_size = xfer_size;

				vm_object_paging_begin(object);

				result = vm_fault_page(object, dst_offset,
				    prot | VM_PROT_WRITE, FALSE,
				    caller_lookup,
				    &prot, &dst_page, &top_page,
				    (int *)0,
				    &error_code, no_zero_fill,
				    &fault_info);

				/* our lookup is no longer valid at this point */
				caller_lookup = FALSE;

				switch (result) {
				case VM_FAULT_SUCCESS:
					page_grab_count++;

					if (!dst_page->vmp_absent) {
						vm_page_wakeup_done(object, dst_page);
					} else {
						/*
						 * we only get back an absent page if we
						 * requested that it not be zero-filled
						 * because we are about to fill it via I/O
						 *
						 * absent pages should be left BUSY
						 * to prevent them from being faulted
						 * into an address space before we've
						 * had a chance to complete the I/O on
						 * them since they may contain info that
						 * shouldn't be seen by the faulting task
						 */
					}
					/*
					 *	Release paging references and
					 *	top-level placeholder page, if any.
					 */
					if (top_page != VM_PAGE_NULL) {
						vm_object_t local_object;

						local_object = VM_PAGE_OBJECT(top_page);

						/*
						 * comparing 2 packed pointers
						 */
						if (top_page->vmp_object != dst_page->vmp_object) {
							vm_object_lock(local_object);
							VM_PAGE_FREE(top_page);
							vm_object_paging_end(local_object);
							vm_object_unlock(local_object);
						} else {
							VM_PAGE_FREE(top_page);
							vm_object_paging_end(local_object);
						}
					}
					vm_object_paging_end(object);
					break;

				case VM_FAULT_RETRY:
					vm_object_lock(object);
					break;

				case VM_FAULT_MEMORY_SHORTAGE:
					OSAddAtomic((size_in_pages - entry), &vm_upl_wait_for_pages);

					VM_DEBUG_EVENT(vm_iopl_page_wait, DBG_VM_IOPL_PAGE_WAIT, DBG_FUNC_START, vm_upl_wait_for_pages, 0, 0, 0);

					if (vm_page_wait(interruptible)) {
						OSAddAtomic(-(size_in_pages - entry), &vm_upl_wait_for_pages);

						VM_DEBUG_EVENT(vm_iopl_page_wait, DBG_VM_IOPL_PAGE_WAIT, DBG_FUNC_END, vm_upl_wait_for_pages, 0, 0, 0);
						vm_object_lock(object);

						break;
					}
					OSAddAtomic(-(size_in_pages - entry), &vm_upl_wait_for_pages);

					VM_DEBUG_EVENT(vm_iopl_page_wait, DBG_VM_IOPL_PAGE_WAIT, DBG_FUNC_END, vm_upl_wait_for_pages, 0, 0, -1);
					ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_FAULT_OBJIOPLREQ_MEMORY_SHORTAGE), 0 /* arg */);
					OS_FALLTHROUGH;

				case VM_FAULT_INTERRUPTED:
					error_code = MACH_SEND_INTERRUPTED;
					OS_FALLTHROUGH;
				case VM_FAULT_MEMORY_ERROR:
memory_error:
					ret = (error_code ? error_code: KERN_MEMORY_ERROR);

					vm_object_lock(object);
					goto return_err;

				case VM_FAULT_SUCCESS_NO_VM_PAGE:
					/* success but no page: fail */
					vm_object_paging_end(object);
					vm_object_unlock(object);
					goto memory_error;

				default:
					panic("vm_object_iopl_request: unexpected error"
					    " 0x%x from vm_fault_page()\n", result);
				}
			} while (result != VM_FAULT_SUCCESS);
		}
		phys_page = VM_PAGE_GET_PHYS_PAGE(dst_page);

		if (upl->flags & UPL_KERNEL_OBJECT) {
			goto record_phys_addr;
		}

		if (dst_page->vmp_q_state == VM_PAGE_USED_BY_COMPRESSOR) {
			dst_page->vmp_busy = TRUE;
			goto record_phys_addr;
		}

		if (dst_page->vmp_cleaning) {
			/*
			 * Someone else is cleaning this page in place.
			 * In theory, we should be able to  proceed and use this
			 * page but they'll probably end up clearing the "busy"
			 * bit on it in upl_commit_range() but they didn't set
			 * it, so they would clear our "busy" bit and open
			 * us to race conditions.
			 * We'd better wait for the cleaning to complete and
			 * then try again.
			 */
			VM_PAGEOUT_DEBUG(vm_object_iopl_request_sleep_for_cleaning, 1);
			vm_page_sleep(object, dst_page, THREAD_UNINT, LCK_SLEEP_EXCLUSIVE);
			continue;
		}
		if (dst_page->vmp_laundry) {
			vm_pageout_steal_laundry(dst_page, FALSE);
		}

		if ((cntrl_flags & UPL_NEED_32BIT_ADDR) &&
		    phys_page >= (max_valid_dma_address >> PAGE_SHIFT)) {
			vm_page_t       low_page;
			int             refmod;

			/*
			 * support devices that can't DMA above 32 bits
			 * by substituting pages from a pool of low address
			 * memory for any pages we find above the 4G mark
			 * can't substitute if the page is already wired because
			 * we don't know whether that physical address has been
			 * handed out to some other 64 bit capable DMA device to use
			 */
			if (VM_PAGE_WIRED(dst_page)) {
				ret = KERN_PROTECTION_FAILURE;
				goto return_err;
			}
			low_page = vm_page_grablo();

			if (low_page == VM_PAGE_NULL) {
				ret = KERN_RESOURCE_SHORTAGE;
				goto return_err;
			}
			/*
			 * from here until the vm_page_replace completes
			 * we musn't drop the object lock... we don't
			 * want anyone refaulting this page in and using
			 * it after we disconnect it... we want the fault
			 * to find the new page being substituted.
			 */
			if (dst_page->vmp_pmapped) {
				refmod = pmap_disconnect(phys_page);
			} else {
				refmod = 0;
			}

			if (!dst_page->vmp_absent) {
				vm_page_copy(dst_page, low_page);
			}

			low_page->vmp_reference = dst_page->vmp_reference;
			low_page->vmp_dirty     = dst_page->vmp_dirty;
			low_page->vmp_absent    = dst_page->vmp_absent;

			if (refmod & VM_MEM_REFERENCED) {
				low_page->vmp_reference = TRUE;
			}
			if (refmod & VM_MEM_MODIFIED) {
				SET_PAGE_DIRTY(low_page, FALSE);
			}

			vm_page_replace(low_page, object, dst_offset);

			dst_page = low_page;
			/*
			 * vm_page_grablo returned the page marked
			 * BUSY... we don't need a PAGE_WAKEUP_DONE
			 * here, because we've never dropped the object lock
			 */
			if (!dst_page->vmp_absent) {
				dst_page->vmp_busy = FALSE;
			}

			phys_page = VM_PAGE_GET_PHYS_PAGE(dst_page);
		}
		if (!dst_page->vmp_busy) {
			dwp->dw_mask |= DW_vm_page_wire;
		}

		if (cntrl_flags & UPL_BLOCK_ACCESS) {
			/*
			 * Mark the page "busy" to block any future page fault
			 * on this page in addition to wiring it.
			 * We'll also remove the mapping
			 * of all these pages before leaving this routine.
			 */
			assert(!dst_page->vmp_fictitious);
			dst_page->vmp_busy = TRUE;
		}
		/*
		 * expect the page to be used
		 * page queues lock must be held to set 'reference'
		 */
		dwp->dw_mask |= DW_set_reference;

		if (!(cntrl_flags & UPL_COPYOUT_FROM)) {
			SET_PAGE_DIRTY(dst_page, TRUE);
			/*
			 * Page belonging to a code-signed object is about to
			 * be written. Mark it tainted and disconnect it from
			 * all pmaps so processes have to fault it back in and
			 * deal with the tainted bit.
			 */
			if (object->code_signed && dst_page->vmp_cs_tainted != VMP_CS_ALL_TRUE) {
				dst_page->vmp_cs_tainted = VMP_CS_ALL_TRUE;
				vm_page_iopl_tainted++;
				if (dst_page->vmp_pmapped) {
					int refmod = pmap_disconnect(VM_PAGE_GET_PHYS_PAGE(dst_page));
					if (refmod & VM_MEM_REFERENCED) {
						dst_page->vmp_reference = TRUE;
					}
				}
			}
		}
		if ((cntrl_flags & UPL_REQUEST_FORCE_COHERENCY) && dst_page->vmp_written_by_kernel == TRUE) {
			pmap_sync_page_attributes_phys(phys_page);
			dst_page->vmp_written_by_kernel = FALSE;
		}

record_phys_addr:
		if (dst_page->vmp_busy) {
			upl->flags |= UPL_HAS_BUSY;
		}

		bitmap_set(upl->lite_list, entry);

		if (phys_page > upl->highest_page) {
			upl->highest_page = phys_page;
		}

		if (user_page_list) {
			user_page_list[entry].phys_addr = phys_page;
			user_page_list[entry].free_when_done    = dst_page->vmp_free_when_done;
			user_page_list[entry].absent    = dst_page->vmp_absent;
			user_page_list[entry].dirty     = dst_page->vmp_dirty;
			user_page_list[entry].precious  = dst_page->vmp_precious;
			user_page_list[entry].device    = FALSE;
			user_page_list[entry].needed    = FALSE;
			if (dst_page->vmp_clustered == TRUE) {
				user_page_list[entry].speculative = (dst_page->vmp_q_state == VM_PAGE_ON_SPECULATIVE_Q) ? TRUE : FALSE;
			} else {
				user_page_list[entry].speculative = FALSE;
			}
			user_page_list[entry].cs_validated = dst_page->vmp_cs_validated;
			user_page_list[entry].cs_tainted = dst_page->vmp_cs_tainted;
			user_page_list[entry].cs_nx = dst_page->vmp_cs_nx;
			user_page_list[entry].mark      = FALSE;
		}
		if (!is_kernel_object(object) && object != compressor_object) {
			/*
			 * someone is explicitly grabbing this page...
			 * update clustered and speculative state
			 *
			 */
			if (dst_page->vmp_clustered) {
				VM_PAGE_CONSUME_CLUSTERED(dst_page);
			}
		}
skip_page:
		entry++;
		dst_offset += PAGE_SIZE_64;
		xfer_size -= PAGE_SIZE;

		if (dwp->dw_mask) {
			VM_PAGE_ADD_DELAYED_WORK(dwp, dst_page, dw_count);

			if (dw_count >= dw_limit) {
				vm_page_do_delayed_work(object, tag, dwp_start, dw_count);

				dwp = dwp_start;
				dw_count = 0;
			}
		}
	}
	assert(entry == size_in_pages);

	if (dw_count) {
		vm_page_do_delayed_work(object, tag, dwp_start, dw_count);
		dwp = dwp_start;
		dw_count = 0;
	}
finish:
	if (user_page_list && set_cache_attr_needed == TRUE) {
		vm_object_set_pmap_cache_attr(object, user_page_list, size_in_pages, TRUE);
	}

	if (page_list_count != NULL) {
		if (upl->flags & UPL_INTERNAL) {
			*page_list_count = 0;
		} else if (*page_list_count > size_in_pages) {
			*page_list_count = size_in_pages;
		}
	}
	vm_object_unlock(object);

	if (cntrl_flags & UPL_BLOCK_ACCESS) {
		/*
		 * We've marked all the pages "busy" so that future
		 * page faults will block.
		 * Now remove the mapping for these pages, so that they
		 * can't be accessed without causing a page fault.
		 */
		vm_object_pmap_protect(object, offset, (vm_object_size_t)size,
		    PMAP_NULL,
		    PAGE_SIZE,
		    0, VM_PROT_NONE);
		assert(!object->blocked_access);
		object->blocked_access = TRUE;
	}

	VM_DEBUG_CONSTANT_EVENT(vm_object_iopl_request, DBG_VM_IOPL_REQUEST, DBG_FUNC_END, page_grab_count, KERN_SUCCESS, 0, 0);
#if DEVELOPMENT || DEBUG
	if (task != NULL) {
		ledger_credit(task->ledger, task_ledgers.pages_grabbed_iopl, page_grab_count);
	}
#endif /* DEVELOPMENT || DEBUG */

	if (dwp_start && dwp_finish_ctx) {
		vm_page_delayed_work_finish_ctx(dwp_start);
		dwp_start = dwp = NULL;
	}

	return KERN_SUCCESS;

return_err:
	dw_index = 0;

	for (; offset < dst_offset; offset += PAGE_SIZE) {
		boolean_t need_unwire;

		dst_page = vm_page_lookup(object, offset);

		if (dst_page == VM_PAGE_NULL) {
			panic("vm_object_iopl_request: Wired page missing.");
		}

		/*
		 * if we've already processed this page in an earlier
		 * dw_do_work, we need to undo the wiring... we will
		 * leave the dirty and reference bits on if they
		 * were set, since we don't have a good way of knowing
		 * what the previous state was and we won't get here
		 * under any normal circumstances...  we will always
		 * clear BUSY and wakeup any waiters via vm_page_free
		 * or PAGE_WAKEUP_DONE
		 */
		need_unwire = TRUE;

		if (dw_count) {
			if ((dwp_start)[dw_index].dw_m == dst_page) {
				/*
				 * still in the deferred work list
				 * which means we haven't yet called
				 * vm_page_wire on this page
				 */
				need_unwire = FALSE;

				dw_index++;
				dw_count--;
			}
		}
		vm_page_lock_queues();

		if (dst_page->vmp_absent || free_wired_pages == TRUE) {
			vm_page_free(dst_page);

			need_unwire = FALSE;
		} else {
			if (need_unwire == TRUE) {
				vm_page_unwire(dst_page, TRUE);
			}

			vm_page_wakeup_done(object, dst_page);
		}
		vm_page_unlock_queues();

		if (need_unwire == TRUE) {
			counter_inc(&vm_statistics_reactivations);
		}
	}
#if UPL_DEBUG
	upl->upl_state = 2;
#endif
	if (!(upl->flags & UPL_KERNEL_OBJECT)) {
		vm_object_activity_end(object);
		vm_object_collapse(object, 0, TRUE);
	}
	vm_object_unlock(object);
	upl_destroy(upl);

	VM_DEBUG_CONSTANT_EVENT(vm_object_iopl_request, DBG_VM_IOPL_REQUEST, DBG_FUNC_END, page_grab_count, ret, 0, 0);
#if DEVELOPMENT || DEBUG
	if (task != NULL) {
		ledger_credit(task->ledger, task_ledgers.pages_grabbed_iopl, page_grab_count);
	}
#endif /* DEVELOPMENT || DEBUG */

	if (dwp_start && dwp_finish_ctx) {
		vm_page_delayed_work_finish_ctx(dwp_start);
		dwp_start = dwp = NULL;
	}
	return ret;
}

kern_return_t
upl_transpose(
	upl_t           upl1,
	upl_t           upl2)
{
	kern_return_t           retval;
	boolean_t               upls_locked;
	vm_object_t             object1, object2;

	/* LD: Should mapped UPLs be eligible for a transpose? */
	if (upl1 == UPL_NULL || upl2 == UPL_NULL || upl1 == upl2 || ((upl1->flags & UPL_VECTOR) == UPL_VECTOR) || ((upl2->flags & UPL_VECTOR) == UPL_VECTOR)) {
		return KERN_INVALID_ARGUMENT;
	}

	upls_locked = FALSE;

	/*
	 * Since we need to lock both UPLs at the same time,
	 * avoid deadlocks by always taking locks in the same order.
	 */
	if (upl1 < upl2) {
		upl_lock(upl1);
		upl_lock(upl2);
	} else {
		upl_lock(upl2);
		upl_lock(upl1);
	}
	upls_locked = TRUE;     /* the UPLs will need to be unlocked */

	object1 = upl1->map_object;
	object2 = upl2->map_object;

	if (upl1->u_offset != 0 || upl2->u_offset != 0 ||
	    upl1->u_size != upl2->u_size) {
		/*
		 * We deal only with full objects, not subsets.
		 * That's because we exchange the entire backing store info
		 * for the objects: pager, resident pages, etc...  We can't do
		 * only part of it.
		 */
		retval = KERN_INVALID_VALUE;
		goto done;
	}

	/*
	 * Tranpose the VM objects' backing store.
	 */
	retval = vm_object_transpose(object1, object2,
	    upl_adjusted_size(upl1, PAGE_MASK));

	if (retval == KERN_SUCCESS) {
		/*
		 * Make each UPL point to the correct VM object, i.e. the
		 * object holding the pages that the UPL refers to...
		 */
#if CONFIG_IOSCHED || UPL_DEBUG
		if ((upl1->flags & UPL_TRACKED_BY_OBJECT) || (upl2->flags & UPL_TRACKED_BY_OBJECT)) {
			vm_object_lock(object1);
			vm_object_lock(object2);
		}
		if ((upl1->flags & UPL_TRACKED_BY_OBJECT) || upl_debug_enabled) {
			queue_remove(&object1->uplq, upl1, upl_t, uplq);
		}
		if ((upl2->flags & UPL_TRACKED_BY_OBJECT) || upl_debug_enabled) {
			queue_remove(&object2->uplq, upl2, upl_t, uplq);
		}
#endif
		upl1->map_object = object2;
		upl2->map_object = object1;

#if CONFIG_IOSCHED || UPL_DEBUG
		if ((upl1->flags & UPL_TRACKED_BY_OBJECT) || upl_debug_enabled) {
			queue_enter(&object2->uplq, upl1, upl_t, uplq);
		}
		if ((upl2->flags & UPL_TRACKED_BY_OBJECT) || upl_debug_enabled) {
			queue_enter(&object1->uplq, upl2, upl_t, uplq);
		}
		if ((upl1->flags & UPL_TRACKED_BY_OBJECT) || (upl2->flags & UPL_TRACKED_BY_OBJECT)) {
			vm_object_unlock(object2);
			vm_object_unlock(object1);
		}
#endif
	}

done:
	/*
	 * Cleanup.
	 */
	if (upls_locked) {
		upl_unlock(upl1);
		upl_unlock(upl2);
		upls_locked = FALSE;
	}

	return retval;
}

void
upl_range_needed(
	upl_t           upl,
	int             index,
	int             count)
{
	int             size_in_pages;

	if (!(upl->flags & UPL_INTERNAL) || count <= 0) {
		return;
	}

	size_in_pages = upl_adjusted_size(upl, PAGE_MASK) / PAGE_SIZE;

	while (count-- && index < size_in_pages) {
		upl->page_list[index++].needed = TRUE;
	}
}


/*
 * Reserve of virtual addresses in the kernel address space.
 * We need to map the physical pages in the kernel, so that we
 * can call the code-signing or slide routines with a kernel
 * virtual address.  We keep this pool of pre-allocated kernel
 * virtual addresses so that we don't have to scan the kernel's
 * virtaul address space each time we need to work with
 * a physical page.
 */
SIMPLE_LOCK_DECLARE(vm_paging_lock, 0);
#define VM_PAGING_NUM_PAGES     64
SECURITY_READ_ONLY_LATE(vm_offset_t) vm_paging_base_address = 0;
bool            vm_paging_page_inuse[VM_PAGING_NUM_PAGES] = { FALSE, };
int             vm_paging_max_index = 0;
int             vm_paging_page_waiter = 0;
int             vm_paging_page_waiter_total = 0;

unsigned long   vm_paging_no_kernel_page = 0;
unsigned long   vm_paging_objects_mapped = 0;
unsigned long   vm_paging_pages_mapped = 0;
unsigned long   vm_paging_objects_mapped_slow = 0;
unsigned long   vm_paging_pages_mapped_slow = 0;

__startup_func
static void
vm_paging_map_init(void)
{
	kmem_alloc(kernel_map, &vm_paging_base_address,
	    ptoa(VM_PAGING_NUM_PAGES),
	    KMA_DATA | KMA_NOFAIL | KMA_KOBJECT | KMA_PERMANENT | KMA_PAGEABLE,
	    VM_KERN_MEMORY_NONE);
}
STARTUP(ZALLOC, STARTUP_RANK_LAST, vm_paging_map_init);

/*
 * vm_paging_map_object:
 *	Maps part of a VM object's pages in the kernel
 *      virtual address space, using the pre-allocated
 *	kernel virtual addresses, if possible.
 * Context:
 *      The VM object is locked.  This lock will get
 *      dropped and re-acquired though, so the caller
 *      must make sure the VM object is kept alive
 *	(by holding a VM map that has a reference
 *      on it, for example, or taking an extra reference).
 *      The page should also be kept busy to prevent
 *	it from being reclaimed.
 */
kern_return_t
vm_paging_map_object(
	vm_page_t               page,
	vm_object_t             object,
	vm_object_offset_t      offset,
	vm_prot_t               protection,
	boolean_t               can_unlock_object,
	vm_map_size_t           *size,          /* IN/OUT */
	vm_map_offset_t         *address,       /* OUT */
	boolean_t               *need_unmap)    /* OUT */
{
	kern_return_t           kr;
	vm_map_offset_t         page_map_offset;
	vm_map_size_t           map_size;
	vm_object_offset_t      object_offset;
	int                     i;

	if (page != VM_PAGE_NULL && *size == PAGE_SIZE) {
		/* use permanent 1-to-1 kernel mapping of physical memory ? */
		*address = (vm_map_offset_t)
		    phystokv((pmap_paddr_t)VM_PAGE_GET_PHYS_PAGE(page) << PAGE_SHIFT);
		*need_unmap = FALSE;
		return KERN_SUCCESS;

		assert(page->vmp_busy);
		/*
		 * Use one of the pre-allocated kernel virtual addresses
		 * and just enter the VM page in the kernel address space
		 * at that virtual address.
		 */
		simple_lock(&vm_paging_lock, &vm_pageout_lck_grp);

		/*
		 * Try and find an available kernel virtual address
		 * from our pre-allocated pool.
		 */
		page_map_offset = 0;
		for (;;) {
			for (i = 0; i < VM_PAGING_NUM_PAGES; i++) {
				if (vm_paging_page_inuse[i] == FALSE) {
					page_map_offset =
					    vm_paging_base_address +
					    (i * PAGE_SIZE);
					break;
				}
			}
			if (page_map_offset != 0) {
				/* found a space to map our page ! */
				break;
			}

			if (can_unlock_object) {
				/*
				 * If we can afford to unlock the VM object,
				 * let's take the slow path now...
				 */
				break;
			}
			/*
			 * We can't afford to unlock the VM object, so
			 * let's wait for a space to become available...
			 */
			vm_paging_page_waiter_total++;
			vm_paging_page_waiter++;
			kr = assert_wait((event_t)&vm_paging_page_waiter, THREAD_UNINT);
			if (kr == THREAD_WAITING) {
				simple_unlock(&vm_paging_lock);
				kr = thread_block(THREAD_CONTINUE_NULL);
				simple_lock(&vm_paging_lock, &vm_pageout_lck_grp);
			}
			vm_paging_page_waiter--;
			/* ... and try again */
		}

		if (page_map_offset != 0) {
			/*
			 * We found a kernel virtual address;
			 * map the physical page to that virtual address.
			 */
			if (i > vm_paging_max_index) {
				vm_paging_max_index = i;
			}
			vm_paging_page_inuse[i] = TRUE;
			simple_unlock(&vm_paging_lock);

			page->vmp_pmapped = TRUE;

			/*
			 * Keep the VM object locked over the PMAP_ENTER
			 * and the actual use of the page by the kernel,
			 * or this pmap mapping might get undone by a
			 * vm_object_pmap_protect() call...
			 */
			kr = pmap_enter_check(kernel_pmap,
			    page_map_offset,
			    page,
			    protection,
			    VM_PROT_NONE,
			    0,
			    TRUE);
			assert(kr == KERN_SUCCESS);
			vm_paging_objects_mapped++;
			vm_paging_pages_mapped++;
			*address = page_map_offset;
			*need_unmap = TRUE;

#if KASAN
			kasan_notify_address(page_map_offset, PAGE_SIZE);
#endif

			/* all done and mapped, ready to use ! */
			return KERN_SUCCESS;
		}

		/*
		 * We ran out of pre-allocated kernel virtual
		 * addresses.  Just map the page in the kernel
		 * the slow and regular way.
		 */
		vm_paging_no_kernel_page++;
		simple_unlock(&vm_paging_lock);
	}

	if (!can_unlock_object) {
		*address = 0;
		*size = 0;
		*need_unmap = FALSE;
		return KERN_NOT_SUPPORTED;
	}

	object_offset = vm_object_trunc_page(offset);
	map_size = vm_map_round_page(*size,
	    VM_MAP_PAGE_MASK(kernel_map));

	/*
	 * Try and map the required range of the object
	 * in the kernel_map. Given that allocation is
	 * for pageable memory, it shouldn't contain
	 * pointers and is mapped into the data range.
	 */

	vm_object_reference_locked(object);     /* for the map entry */
	vm_object_unlock(object);

	kr = vm_map_enter(kernel_map,
	    address,
	    map_size,
	    0,
	    VM_MAP_KERNEL_FLAGS_DATA_ANYWHERE(),
	    object,
	    object_offset,
	    FALSE,
	    protection,
	    VM_PROT_ALL,
	    VM_INHERIT_NONE);
	if (kr != KERN_SUCCESS) {
		*address = 0;
		*size = 0;
		*need_unmap = FALSE;
		vm_object_deallocate(object);   /* for the map entry */
		vm_object_lock(object);
		return kr;
	}

	*size = map_size;

	/*
	 * Enter the mapped pages in the page table now.
	 */
	vm_object_lock(object);
	/*
	 * VM object must be kept locked from before PMAP_ENTER()
	 * until after the kernel is done accessing the page(s).
	 * Otherwise, the pmap mappings in the kernel could be
	 * undone by a call to vm_object_pmap_protect().
	 */

	for (page_map_offset = 0;
	    map_size != 0;
	    map_size -= PAGE_SIZE_64, page_map_offset += PAGE_SIZE_64) {
		page = vm_page_lookup(object, offset + page_map_offset);
		if (page == VM_PAGE_NULL) {
			printf("vm_paging_map_object: no page !?");
			vm_object_unlock(object);
			vm_map_remove(kernel_map, *address, *size);
			*address = 0;
			*size = 0;
			*need_unmap = FALSE;
			vm_object_lock(object);
			return KERN_MEMORY_ERROR;
		}
		page->vmp_pmapped = TRUE;

		kr = pmap_enter_check(kernel_pmap,
		    *address + page_map_offset,
		    page,
		    protection,
		    VM_PROT_NONE,
		    0,
		    TRUE);
		assert(kr == KERN_SUCCESS);
#if KASAN
		kasan_notify_address(*address + page_map_offset, PAGE_SIZE);
#endif
	}

	vm_paging_objects_mapped_slow++;
	vm_paging_pages_mapped_slow += (unsigned long) (map_size / PAGE_SIZE_64);

	*need_unmap = TRUE;

	return KERN_SUCCESS;
}

/*
 * vm_paging_unmap_object:
 *	Unmaps part of a VM object's pages from the kernel
 *      virtual address space.
 * Context:
 *      The VM object is locked.  This lock will get
 *      dropped and re-acquired though.
 */
void
vm_paging_unmap_object(
	vm_object_t     object,
	vm_map_offset_t start,
	vm_map_offset_t end)
{
	int             i;

	if ((vm_paging_base_address == 0) ||
	    (start < vm_paging_base_address) ||
	    (end > (vm_paging_base_address
	    + (VM_PAGING_NUM_PAGES * PAGE_SIZE)))) {
		/*
		 * We didn't use our pre-allocated pool of
		 * kernel virtual address.  Deallocate the
		 * virtual memory.
		 */
		if (object != VM_OBJECT_NULL) {
			vm_object_unlock(object);
		}
		vm_map_remove(kernel_map, start, end);
		if (object != VM_OBJECT_NULL) {
			vm_object_lock(object);
		}
	} else {
		/*
		 * We used a kernel virtual address from our
		 * pre-allocated pool.  Put it back in the pool
		 * for next time.
		 */
		assert(end - start == PAGE_SIZE);
		i = (int) ((start - vm_paging_base_address) >> PAGE_SHIFT);
		assert(i >= 0 && i < VM_PAGING_NUM_PAGES);

		/* undo the pmap mapping */
		pmap_remove(kernel_pmap, start, end);

		simple_lock(&vm_paging_lock, &vm_pageout_lck_grp);
		vm_paging_page_inuse[i] = FALSE;
		if (vm_paging_page_waiter) {
			thread_wakeup(&vm_paging_page_waiter);
		}
		simple_unlock(&vm_paging_lock);
	}
}


/*
 * page->vmp_object must be locked
 */
void
vm_pageout_steal_laundry(vm_page_t page, boolean_t queues_locked)
{
	if (!queues_locked) {
		vm_page_lockspin_queues();
	}

	page->vmp_free_when_done = FALSE;
	/*
	 * need to drop the laundry count...
	 * we may also need to remove it
	 * from the I/O paging queue...
	 * vm_pageout_throttle_up handles both cases
	 *
	 * the laundry and pageout_queue flags are cleared...
	 */
	vm_pageout_throttle_up(page);

	if (!queues_locked) {
		vm_page_unlock_queues();
	}
}

#define VECTOR_UPL_ELEMENTS_UPPER_LIMIT 64

upl_t
vector_upl_create(vm_offset_t upl_offset, uint32_t max_upls)
{
	int i = 0;
	upl_t   upl;

	assert(max_upls > 0);
	if (max_upls == 0) {
		return NULL;
	}

	if (max_upls > VECTOR_UPL_ELEMENTS_UPPER_LIMIT) {
		max_upls = VECTOR_UPL_ELEMENTS_UPPER_LIMIT;
	}
	vector_upl_t vector_upl = kalloc_type(struct _vector_upl, typeof(vector_upl->upls[0]), max_upls, Z_WAITOK | Z_NOFAIL);

	upl = upl_create(0, UPL_VECTOR, 0);
	upl->vector_upl = vector_upl;
	upl->u_offset = upl_offset;
	vector_upl->size = 0;
	vector_upl->offset = upl_offset;
	vector_upl->invalid_upls = 0;
	vector_upl->num_upls = 0;
	vector_upl->pagelist = NULL;
	vector_upl->max_upls = max_upls;

	for (i = 0; i < max_upls; i++) {
		vector_upl->upls[i].iostate.size = 0;
		vector_upl->upls[i].iostate.offset = 0;
	}
	return upl;
}

upl_size_t
vector_upl_get_size(const upl_t upl)
{
	if (!vector_upl_is_valid(upl)) {
		return upl_get_size(upl);
	} else {
		return round_page_32(upl->vector_upl->size);
	}
}

uint32_t
vector_upl_max_upls(const upl_t upl)
{
	if (!vector_upl_is_valid(upl)) {
		return 0;
	}
	return ((vector_upl_t)(upl->vector_upl))->max_upls;
}

void
vector_upl_deallocate(upl_t upl)
{
	vector_upl_t vector_upl = upl->vector_upl;

	assert(vector_upl_is_valid(upl));

	if (vector_upl->invalid_upls != vector_upl->num_upls) {
		panic("Deallocating non-empty Vectored UPL");
	}
	uint32_t max_upls = vector_upl->max_upls;
	kfree_type(struct upl_page_info, atop(vector_upl->size), vector_upl->pagelist);
	kfree_type(struct _vector_upl, typeof(vector_upl->upls[0]), max_upls, vector_upl);
	upl->vector_upl = NULL;
}

boolean_t
vector_upl_is_valid(upl_t upl)
{
	return upl && (upl->flags & UPL_VECTOR) && upl->vector_upl;
}

boolean_t
vector_upl_set_subupl(upl_t upl, upl_t subupl, uint32_t io_size)
{
	if (vector_upl_is_valid(upl)) {
		vector_upl_t vector_upl = upl->vector_upl;

		if (vector_upl) {
			if (subupl) {
				if (io_size) {
					if (io_size < PAGE_SIZE) {
						io_size = PAGE_SIZE;
					}
					subupl->vector_upl = (void*)vector_upl;
					vector_upl->upls[vector_upl->num_upls++].elem = subupl;
					vector_upl->size += io_size;
					upl->u_size += io_size;
				} else {
					uint32_t i = 0, invalid_upls = 0;
					for (i = 0; i < vector_upl->num_upls; i++) {
						if (vector_upl->upls[i].elem == subupl) {
							break;
						}
					}
					if (i == vector_upl->num_upls) {
						panic("Trying to remove sub-upl when none exists");
					}

					vector_upl->upls[i].elem = NULL;
					invalid_upls = os_atomic_inc(&(vector_upl)->invalid_upls,
					    relaxed);
					if (invalid_upls == vector_upl->num_upls) {
						return TRUE;
					} else {
						return FALSE;
					}
				}
			} else {
				panic("vector_upl_set_subupl was passed a NULL upl element");
			}
		} else {
			panic("vector_upl_set_subupl was passed a non-vectored upl");
		}
	} else {
		panic("vector_upl_set_subupl was passed a NULL upl");
	}

	return FALSE;
}

void
vector_upl_set_pagelist(upl_t upl)
{
	if (vector_upl_is_valid(upl)) {
		uint32_t i = 0;
		vector_upl_t vector_upl = upl->vector_upl;

		if (vector_upl) {
			vm_offset_t pagelist_size = 0, cur_upl_pagelist_size = 0;

			vector_upl->pagelist = kalloc_type(struct upl_page_info,
			    atop(vector_upl->size), Z_WAITOK);

			for (i = 0; i < vector_upl->num_upls; i++) {
				cur_upl_pagelist_size = sizeof(struct upl_page_info) * upl_adjusted_size(vector_upl->upls[i].elem, PAGE_MASK) / PAGE_SIZE;
				bcopy(vector_upl->upls[i].elem->page_list, (char*)vector_upl->pagelist + pagelist_size, cur_upl_pagelist_size);
				pagelist_size += cur_upl_pagelist_size;
				if (vector_upl->upls[i].elem->highest_page > upl->highest_page) {
					upl->highest_page = vector_upl->upls[i].elem->highest_page;
				}
			}
			assert( pagelist_size == (sizeof(struct upl_page_info) * (vector_upl->size / PAGE_SIZE)));
		} else {
			panic("vector_upl_set_pagelist was passed a non-vectored upl");
		}
	} else {
		panic("vector_upl_set_pagelist was passed a NULL upl");
	}
}

upl_t
vector_upl_subupl_byindex(upl_t upl, uint32_t index)
{
	if (vector_upl_is_valid(upl)) {
		vector_upl_t vector_upl = upl->vector_upl;
		if (vector_upl) {
			if (index < vector_upl->num_upls) {
				return vector_upl->upls[index].elem;
			}
		} else {
			panic("vector_upl_subupl_byindex was passed a non-vectored upl");
		}
	}
	return NULL;
}

upl_t
vector_upl_subupl_byoffset(upl_t upl, upl_offset_t *upl_offset, upl_size_t *upl_size)
{
	if (vector_upl_is_valid(upl)) {
		uint32_t i = 0;
		vector_upl_t vector_upl = upl->vector_upl;

		if (vector_upl) {
			upl_t subupl = NULL;
			vector_upl_iostates_t subupl_state;

			for (i = 0; i < vector_upl->num_upls; i++) {
				subupl = vector_upl->upls[i].elem;
				subupl_state = vector_upl->upls[i].iostate;
				if (*upl_offset <= (subupl_state.offset + subupl_state.size - 1)) {
					/* We could have been passed an offset/size pair that belongs
					 * to an UPL element that has already been committed/aborted.
					 * If so, return NULL.
					 */
					if (subupl == NULL) {
						return NULL;
					}
					if ((subupl_state.offset + subupl_state.size) < (*upl_offset + *upl_size)) {
						*upl_size = (subupl_state.offset + subupl_state.size) - *upl_offset;
						if (*upl_size > subupl_state.size) {
							*upl_size = subupl_state.size;
						}
					}
					if (*upl_offset >= subupl_state.offset) {
						*upl_offset -= subupl_state.offset;
					} else if (i) {
						panic("Vector UPL offset miscalculation");
					}
					return subupl;
				}
			}
		} else {
			panic("vector_upl_subupl_byoffset was passed a non-vectored UPL");
		}
	}
	return NULL;
}

void
vector_upl_get_submap(upl_t upl, vm_map_t *v_upl_submap, vm_offset_t *submap_dst_addr)
{
	*v_upl_submap = NULL;

	if (vector_upl_is_valid(upl)) {
		vector_upl_t vector_upl = upl->vector_upl;
		if (vector_upl) {
			*v_upl_submap = vector_upl->submap;
			*submap_dst_addr = vector_upl->submap_dst_addr;
		} else {
			panic("vector_upl_get_submap was passed a non-vectored UPL");
		}
	} else {
		panic("vector_upl_get_submap was passed a null UPL");
	}
}

void
vector_upl_set_submap(upl_t upl, vm_map_t submap, vm_offset_t submap_dst_addr)
{
	if (vector_upl_is_valid(upl)) {
		vector_upl_t vector_upl = upl->vector_upl;
		if (vector_upl) {
			vector_upl->submap = submap;
			vector_upl->submap_dst_addr = submap_dst_addr;
		} else {
			panic("vector_upl_get_submap was passed a non-vectored UPL");
		}
	} else {
		panic("vector_upl_get_submap was passed a NULL UPL");
	}
}

void
vector_upl_set_iostate(upl_t upl, upl_t subupl, upl_offset_t offset, upl_size_t size)
{
	if (vector_upl_is_valid(upl)) {
		uint32_t i = 0;
		vector_upl_t vector_upl = upl->vector_upl;

		if (vector_upl) {
			for (i = 0; i < vector_upl->num_upls; i++) {
				if (vector_upl->upls[i].elem == subupl) {
					break;
				}
			}

			if (i == vector_upl->num_upls) {
				panic("setting sub-upl iostate when none exists");
			}

			vector_upl->upls[i].iostate.offset = offset;
			if (size < PAGE_SIZE) {
				size = PAGE_SIZE;
			}
			vector_upl->upls[i].iostate.size = size;
		} else {
			panic("vector_upl_set_iostate was passed a non-vectored UPL");
		}
	} else {
		panic("vector_upl_set_iostate was passed a NULL UPL");
	}
}

void
vector_upl_get_iostate(upl_t upl, upl_t subupl, upl_offset_t *offset, upl_size_t *size)
{
	if (vector_upl_is_valid(upl)) {
		uint32_t i = 0;
		vector_upl_t vector_upl = upl->vector_upl;

		if (vector_upl) {
			for (i = 0; i < vector_upl->num_upls; i++) {
				if (vector_upl->upls[i].elem == subupl) {
					break;
				}
			}

			if (i == vector_upl->num_upls) {
				panic("getting sub-upl iostate when none exists");
			}

			*offset = vector_upl->upls[i].iostate.offset;
			*size = vector_upl->upls[i].iostate.size;
		} else {
			panic("vector_upl_get_iostate was passed a non-vectored UPL");
		}
	} else {
		panic("vector_upl_get_iostate was passed a NULL UPL");
	}
}

void
vector_upl_get_iostate_byindex(upl_t upl, uint32_t index, upl_offset_t *offset, upl_size_t *size)
{
	if (vector_upl_is_valid(upl)) {
		vector_upl_t vector_upl = upl->vector_upl;
		if (vector_upl) {
			if (index < vector_upl->num_upls) {
				*offset = vector_upl->upls[index].iostate.offset;
				*size = vector_upl->upls[index].iostate.size;
			} else {
				*offset = *size = 0;
			}
		} else {
			panic("vector_upl_get_iostate_byindex was passed a non-vectored UPL");
		}
	} else {
		panic("vector_upl_get_iostate_byindex was passed a NULL UPL");
	}
}

void *
upl_get_internal_vectorupl(upl_t upl)
{
	return upl->vector_upl;
}

upl_page_info_t *
upl_get_internal_vectorupl_pagelist(upl_t upl)
{
	return upl->vector_upl->pagelist;
}

upl_page_info_t *
upl_get_internal_page_list(upl_t upl)
{
	return upl->vector_upl ? upl->vector_upl->pagelist : upl->page_list;
}

void
upl_clear_dirty(
	upl_t           upl,
	boolean_t       value)
{
	if (value) {
		upl->flags |= UPL_CLEAR_DIRTY;
	} else {
		upl->flags &= ~UPL_CLEAR_DIRTY;
	}
}

void
upl_set_referenced(
	upl_t           upl,
	boolean_t       value)
{
	upl_lock(upl);
	if (value) {
		upl->ext_ref_count++;
	} else {
		if (!upl->ext_ref_count) {
			panic("upl_set_referenced not %p", upl);
		}
		upl->ext_ref_count--;
	}
	upl_unlock(upl);
}

void
upl_set_map_exclusive(upl_t upl)
{
	upl_lock(upl);
	while (upl->map_addr_owner) {
		upl->flags |= UPL_MAP_EXCLUSIVE_WAIT;
		upl_lock_sleep(upl, &upl->map_addr_owner, ctid_get_thread(upl->map_addr_owner));
	}
	upl->map_addr_owner = thread_get_ctid(current_thread());
	upl_unlock(upl);
}

void
upl_clear_map_exclusive(upl_t upl)
{
	assert(upl->map_addr_owner == thread_get_ctid(current_thread()));
	upl_lock(upl);
	if (upl->flags & UPL_MAP_EXCLUSIVE_WAIT) {
		upl->flags &= ~UPL_MAP_EXCLUSIVE_WAIT;
		upl_wakeup(&upl->map_addr_owner);
	}
	upl->map_addr_owner = 0;
	upl_unlock(upl);
}

#if CONFIG_IOSCHED
void
upl_set_blkno(
	upl_t           upl,
	vm_offset_t     upl_offset,
	int             io_size,
	int64_t         blkno)
{
	int i, j;
	if ((upl->flags & UPL_EXPEDITE_SUPPORTED) == 0) {
		return;
	}

	assert(upl->upl_reprio_info != 0);
	for (i = (int)(upl_offset / PAGE_SIZE), j = 0; j < io_size; i++, j += PAGE_SIZE) {
		UPL_SET_REPRIO_INFO(upl, i, blkno, io_size);
	}
}
#endif

void inline
memoryshot(unsigned int event, unsigned int control)
{
	if (vm_debug_events) {
		KERNEL_DEBUG_CONSTANT1((MACHDBG_CODE(DBG_MACH_VM_PRESSURE, event)) | control,
		    vm_page_active_count, vm_page_inactive_count,
		    vm_page_free_count, vm_page_speculative_count,
		    vm_page_throttled_count);
	} else {
		(void) event;
		(void) control;
	}
}

#ifdef MACH_BSD

boolean_t
upl_device_page(upl_page_info_t *upl)
{
	return UPL_DEVICE_PAGE(upl);
}
boolean_t
upl_page_present(upl_page_info_t *upl, int index)
{
	return UPL_PAGE_PRESENT(upl, index);
}
boolean_t
upl_speculative_page(upl_page_info_t *upl, int index)
{
	return UPL_SPECULATIVE_PAGE(upl, index);
}
boolean_t
upl_dirty_page(upl_page_info_t *upl, int index)
{
	return UPL_DIRTY_PAGE(upl, index);
}
boolean_t
upl_valid_page(upl_page_info_t *upl, int index)
{
	return UPL_VALID_PAGE(upl, index);
}
ppnum_t
upl_phys_page(upl_page_info_t *upl, int index)
{
	return UPL_PHYS_PAGE(upl, index);
}

void
upl_page_set_mark(upl_page_info_t *upl, int index, boolean_t v)
{
	upl[index].mark = v;
}

boolean_t
upl_page_get_mark(upl_page_info_t *upl, int index)
{
	return upl[index].mark;
}

void
vm_countdirtypages(void)
{
	vm_page_t m;
	int dpages;
	int pgopages;
	int precpages;


	dpages = 0;
	pgopages = 0;
	precpages = 0;

	vm_page_lock_queues();
	m = (vm_page_t) vm_page_queue_first(&vm_page_queue_inactive);
	do {
		if (m == (vm_page_t)0) {
			break;
		}

		if (m->vmp_dirty) {
			dpages++;
		}
		if (m->vmp_free_when_done) {
			pgopages++;
		}
		if (m->vmp_precious) {
			precpages++;
		}

		assert(!is_kernel_object(VM_PAGE_OBJECT(m)));
		m = (vm_page_t) vm_page_queue_next(&m->vmp_pageq);
		if (m == (vm_page_t)0) {
			break;
		}
	} while (!vm_page_queue_end(&vm_page_queue_inactive, (vm_page_queue_entry_t) m));
	vm_page_unlock_queues();

	vm_page_lock_queues();
	m = (vm_page_t) vm_page_queue_first(&vm_page_queue_throttled);
	do {
		if (m == (vm_page_t)0) {
			break;
		}

		dpages++;
		assert(m->vmp_dirty);
		assert(!m->vmp_free_when_done);
		assert(!is_kernel_object(VM_PAGE_OBJECT(m)));
		m = (vm_page_t) vm_page_queue_next(&m->vmp_pageq);
		if (m == (vm_page_t)0) {
			break;
		}
	} while (!vm_page_queue_end(&vm_page_queue_throttled, (vm_page_queue_entry_t) m));
	vm_page_unlock_queues();

	vm_page_lock_queues();
	m = (vm_page_t) vm_page_queue_first(&vm_page_queue_anonymous);
	do {
		if (m == (vm_page_t)0) {
			break;
		}

		if (m->vmp_dirty) {
			dpages++;
		}
		if (m->vmp_free_when_done) {
			pgopages++;
		}
		if (m->vmp_precious) {
			precpages++;
		}

		assert(!is_kernel_object(VM_PAGE_OBJECT(m)));
		m = (vm_page_t) vm_page_queue_next(&m->vmp_pageq);
		if (m == (vm_page_t)0) {
			break;
		}
	} while (!vm_page_queue_end(&vm_page_queue_anonymous, (vm_page_queue_entry_t) m));
	vm_page_unlock_queues();

	printf("IN Q: %d : %d : %d\n", dpages, pgopages, precpages);

	dpages = 0;
	pgopages = 0;
	precpages = 0;

	vm_page_lock_queues();
	m = (vm_page_t) vm_page_queue_first(&vm_page_queue_active);

	do {
		if (m == (vm_page_t)0) {
			break;
		}
		if (m->vmp_dirty) {
			dpages++;
		}
		if (m->vmp_free_when_done) {
			pgopages++;
		}
		if (m->vmp_precious) {
			precpages++;
		}

		assert(!is_kernel_object(VM_PAGE_OBJECT(m)));
		m = (vm_page_t) vm_page_queue_next(&m->vmp_pageq);
		if (m == (vm_page_t)0) {
			break;
		}
	} while (!vm_page_queue_end(&vm_page_queue_active, (vm_page_queue_entry_t) m));
	vm_page_unlock_queues();

	printf("AC Q: %d : %d : %d\n", dpages, pgopages, precpages);
}
#endif /* MACH_BSD */


#if CONFIG_IOSCHED
int
upl_get_cached_tier(upl_t  upl)
{
	assert(upl);
	if (upl->flags & UPL_TRACKED_BY_OBJECT) {
		return upl->upl_priority;
	}
	return -1;
}
#endif /* CONFIG_IOSCHED */


void
upl_callout_iodone(upl_t upl)
{
	struct upl_io_completion *upl_ctx = upl->upl_iodone;

	if (upl_ctx) {
		void    (*iodone_func)(void *, int) = upl_ctx->io_done;

		assert(upl_ctx->io_done);

		(*iodone_func)(upl_ctx->io_context, upl_ctx->io_error);
	}
}

void
upl_set_iodone(upl_t upl, void *upl_iodone)
{
	upl->upl_iodone = (struct upl_io_completion *)upl_iodone;
}

void
upl_set_iodone_error(upl_t upl, int error)
{
	struct upl_io_completion *upl_ctx = upl->upl_iodone;

	if (upl_ctx) {
		upl_ctx->io_error = error;
	}
}


ppnum_t
upl_get_highest_page(
	upl_t                      upl)
{
	return upl->highest_page;
}

upl_size_t
upl_get_size(
	upl_t                      upl)
{
	return upl_adjusted_size(upl, PAGE_MASK);
}

upl_size_t
upl_adjusted_size(
	upl_t upl,
	vm_map_offset_t pgmask)
{
	vm_object_offset_t start_offset, end_offset;

	start_offset = trunc_page_mask_64(upl->u_offset, pgmask);
	end_offset = round_page_mask_64(upl->u_offset + upl->u_size, pgmask);

	return (upl_size_t)(end_offset - start_offset);
}

vm_object_offset_t
upl_adjusted_offset(
	upl_t upl,
	vm_map_offset_t pgmask)
{
	return trunc_page_mask_64(upl->u_offset, pgmask);
}

vm_object_offset_t
upl_get_data_offset(
	upl_t upl)
{
	return upl->u_offset - upl_adjusted_offset(upl, PAGE_MASK);
}

upl_t
upl_associated_upl(upl_t upl)
{
	return upl->associated_upl;
}

void
upl_set_associated_upl(upl_t upl, upl_t associated_upl)
{
	upl->associated_upl = associated_upl;
}

struct vnode *
upl_lookup_vnode(upl_t upl)
{
	if (!upl->map_object->internal) {
		return vnode_pager_lookup_vnode(upl->map_object->pager);
	} else {
		return NULL;
	}
}

#if UPL_DEBUG
kern_return_t
upl_ubc_alias_set(upl_t upl, uintptr_t alias1, uintptr_t alias2)
{
	upl->ubc_alias1 = alias1;
	upl->ubc_alias2 = alias2;
	return KERN_SUCCESS;
}
int
upl_ubc_alias_get(upl_t upl, uintptr_t * al, uintptr_t * al2)
{
	if (al) {
		*al = upl->ubc_alias1;
	}
	if (al2) {
		*al2 = upl->ubc_alias2;
	}
	return KERN_SUCCESS;
}
#endif /* UPL_DEBUG */

#if VM_PRESSURE_EVENTS
/*
 * Upward trajectory.
 */

boolean_t
VM_PRESSURE_NORMAL_TO_WARNING(void)
{
	if (!VM_CONFIG_COMPRESSOR_IS_ACTIVE) {
		/* Available pages below our threshold */
		if (memorystatus_available_pages < memorystatus_available_pages_pressure) {
#if CONFIG_FREEZE
			/* No frozen processes to kill */
			if (memorystatus_frozen_count == 0) {
				/* Not enough suspended processes available. */
				if (memorystatus_suspended_count < MEMORYSTATUS_SUSPENDED_THRESHOLD) {
					return TRUE;
				}
			}
#else /* CONFIG_FREEZE */
			return TRUE;
#endif /* CONFIG_FREEZE */
		}
		return FALSE;
	} else {
		return (AVAILABLE_NON_COMPRESSED_MEMORY < VM_PAGE_COMPRESSOR_COMPACT_THRESHOLD) ? 1 : 0;
	}
}

boolean_t
VM_PRESSURE_WARNING_TO_CRITICAL(void)
{
	if (!VM_CONFIG_COMPRESSOR_IS_ACTIVE) {
		/* Available pages below our threshold */
		if (memorystatus_available_pages < memorystatus_available_pages_critical) {
			return TRUE;
		}
		return FALSE;
	} else {
		return vm_compressor_low_on_space() || (AVAILABLE_NON_COMPRESSED_MEMORY < ((12 * VM_PAGE_COMPRESSOR_SWAP_UNTHROTTLE_THRESHOLD) / 10)) ? 1 : 0;
	}
}

/*
 * Downward trajectory.
 */
boolean_t
VM_PRESSURE_WARNING_TO_NORMAL(void)
{
	if (!VM_CONFIG_COMPRESSOR_IS_ACTIVE) {
		/* Available pages above our threshold */
		unsigned int target_threshold = (unsigned int) (memorystatus_available_pages_pressure + ((15 * memorystatus_available_pages_pressure) / 100));
		if (memorystatus_available_pages > target_threshold) {
			return TRUE;
		}
		return FALSE;
	} else {
		return (AVAILABLE_NON_COMPRESSED_MEMORY > ((12 * VM_PAGE_COMPRESSOR_COMPACT_THRESHOLD) / 10)) ? 1 : 0;
	}
}

boolean_t
VM_PRESSURE_CRITICAL_TO_WARNING(void)
{
	if (!VM_CONFIG_COMPRESSOR_IS_ACTIVE) {
		/* Available pages above our threshold */
		unsigned int target_threshold = (unsigned int)(memorystatus_available_pages_critical + ((15 * memorystatus_available_pages_critical) / 100));
		if (memorystatus_available_pages > target_threshold) {
			return TRUE;
		}
		return FALSE;
	} else {
		return (AVAILABLE_NON_COMPRESSED_MEMORY > ((14 * VM_PAGE_COMPRESSOR_SWAP_UNTHROTTLE_THRESHOLD) / 10)) ? 1 : 0;
	}
}
#endif /* VM_PRESSURE_EVENTS */

#if DEVELOPMENT || DEBUG
bool compressor_running_perf_test;
uint64_t compressor_perf_test_pages_processed;

static kern_return_t
move_pages_to_queue(
	vm_map_t map,
	user_addr_t start_addr,
	size_t buffer_size,
	vm_page_queue_head_t *queue,
	size_t *pages_moved)
{
	kern_return_t err = KERN_SUCCESS;
	vm_map_entry_t curr_entry = VM_MAP_ENTRY_NULL;
	boolean_t addr_in_map = FALSE;
	user_addr_t end_addr = USER_ADDR_NULL, curr_addr = USER_ADDR_NULL;
	vm_object_t curr_object = VM_OBJECT_NULL;
	*pages_moved = 0;


	if (VM_MAP_PAGE_SIZE(map) != PAGE_SIZE_64) {
		/*
		 * We don't currently support benchmarking maps with a different page size
		 * than the kernel.
		 */
		return KERN_INVALID_ARGUMENT;
	}

	if (os_add_overflow(start_addr, buffer_size, &end_addr)) {
		return KERN_INVALID_ARGUMENT;
	}

	vm_map_lock_read(map);
	curr_addr = vm_map_trunc_page_mask(start_addr, VM_MAP_PAGE_MASK(map));
	end_addr = vm_map_round_page_mask(start_addr + buffer_size, VM_MAP_PAGE_MASK(map));


	while (curr_addr < end_addr) {
		addr_in_map = vm_map_lookup_entry(map, curr_addr, &curr_entry);
		if (!addr_in_map) {
			err = KERN_INVALID_ARGUMENT;
			break;
		}
		curr_object = VME_OBJECT(curr_entry);
		if (curr_object) {
			vm_object_lock(curr_object);
			/* We really only want anonymous memory that's in the top level map and object here. */
			if (curr_entry->is_sub_map || curr_entry->wired_count != 0 ||
			    curr_object->shadow != VM_OBJECT_NULL || !curr_object->internal) {
				err = KERN_INVALID_ARGUMENT;
				vm_object_unlock(curr_object);
				break;
			}
			vm_map_offset_t start_offset = (curr_addr - curr_entry->vme_start) + VME_OFFSET(curr_entry);
			vm_map_offset_t end_offset = MIN(curr_entry->vme_end, end_addr) -
			    (curr_entry->vme_start + VME_OFFSET(curr_entry));
			vm_map_offset_t curr_offset = start_offset;
			vm_page_t curr_page;
			while (curr_offset < end_offset) {
				curr_page = vm_page_lookup(curr_object, vm_object_trunc_page(curr_offset));
				if (curr_page != VM_PAGE_NULL) {
					vm_page_lock_queues();
					if (curr_page->vmp_laundry) {
						vm_pageout_steal_laundry(curr_page, TRUE);
					}
					/*
					 * we've already factored out pages in the laundry which
					 * means this page can't be on the pageout queue so it's
					 * safe to do the vm_page_queues_remove
					 */
					bool donate = (curr_page->vmp_on_specialq == VM_PAGE_SPECIAL_Q_DONATE);
					vm_page_queues_remove(curr_page, TRUE);
					if (donate) {
						/*
						 * The compressor needs to see this bit to know
						 * where this page needs to land. Also if stolen,
						 * this bit helps put the page back in the right
						 * special queue where it belongs.
						 */
						curr_page->vmp_on_specialq = VM_PAGE_SPECIAL_Q_DONATE;
					}
					// Clear the referenced bit so we ensure this gets paged out
					curr_page->vmp_reference = false;
					if (curr_page->vmp_pmapped) {
						pmap_clear_refmod_options(VM_PAGE_GET_PHYS_PAGE(curr_page),
						    VM_MEM_REFERENCED, PMAP_OPTIONS_NOFLUSH, (void*)NULL);
					}
					vm_page_queue_enter(queue, curr_page, vmp_pageq);
					vm_page_unlock_queues();
					*pages_moved += 1;
				}
				curr_offset += PAGE_SIZE_64;
				curr_addr += PAGE_SIZE_64;
			}
		}
		vm_object_unlock(curr_object);
	}
	vm_map_unlock_read(map);
	return err;
}

/*
 * Local queue for processing benchmark pages.
 * Can't be allocated on the stack because the pointer has to
 * be packable.
 */
vm_page_queue_head_t compressor_perf_test_queue VM_PAGE_PACKED_ALIGNED;
kern_return_t
run_compressor_perf_test(
	user_addr_t buf,
	size_t buffer_size,
	uint64_t *time,
	uint64_t *bytes_compressed,
	uint64_t *compressor_growth)
{
	kern_return_t err = KERN_SUCCESS;
	if (!VM_CONFIG_COMPRESSOR_IS_ACTIVE) {
		return KERN_NOT_SUPPORTED;
	}
	if (current_task() == kernel_task) {
		return KERN_INVALID_ARGUMENT;
	}
	vm_page_lock_queues();
	if (compressor_running_perf_test) {
		/* Only run one instance of the benchmark at a time. */
		vm_page_unlock_queues();
		return KERN_RESOURCE_SHORTAGE;
	}
	vm_page_unlock_queues();
	size_t page_count = 0;
	vm_map_t map;
	vm_page_t p, next;
	uint64_t compressor_perf_test_start = 0, compressor_perf_test_end = 0;
	uint64_t compressed_bytes_start = 0, compressed_bytes_end = 0;
	*bytes_compressed = *compressor_growth = 0;

	vm_page_queue_init(&compressor_perf_test_queue);
	map = current_task()->map;
	err = move_pages_to_queue(map, buf, buffer_size, &compressor_perf_test_queue, &page_count);
	if (err != KERN_SUCCESS) {
		goto out;
	}

	vm_page_lock_queues();
	compressor_running_perf_test = true;
	compressor_perf_test_pages_processed = 0;
	/*
	 * At this point the compressor threads should only process the benchmark queue
	 * so we can look at the difference in c_segment_compressed_bytes while the perf test is running
	 * to determine how many compressed bytes we ended up using.
	 */
	compressed_bytes_start = os_atomic_load(&c_segment_compressed_bytes, relaxed);
	vm_page_unlock_queues();

	page_count = vm_pageout_page_queue(&compressor_perf_test_queue, page_count, true);

	vm_page_lock_queues();
	compressor_perf_test_start = mach_absolute_time();

	// Wake up the compressor thread(s)
	sched_cond_signal(&pgo_iothread_internal_state[0].pgo_wakeup,
	    pgo_iothread_internal_state[0].pgo_iothread);

	/*
	 * Depending on when this test is run we could overshoot or be right on the mark
	 * with our page_count. So the comparison is of the _less than_ variety.
	 */
	while (compressor_perf_test_pages_processed < page_count) {
		assert_wait((event_t) &compressor_perf_test_pages_processed, THREAD_UNINT);
		vm_page_unlock_queues();
		thread_block(THREAD_CONTINUE_NULL);
		vm_page_lock_queues();
	}
	compressor_perf_test_end = mach_absolute_time();
	compressed_bytes_end = os_atomic_load(&c_segment_compressed_bytes, relaxed);
	vm_page_unlock_queues();


out:
	/*
	 * If we errored out above, then we could still have some pages
	 * on the local queue. Make sure to put them back on the active queue before
	 * returning so they're not orphaned.
	 */
	vm_page_lock_queues();
	absolutetime_to_nanoseconds(compressor_perf_test_end - compressor_perf_test_start, time);
	p = (vm_page_t) vm_page_queue_first(&compressor_perf_test_queue);
	while (p && !vm_page_queue_end(&compressor_perf_test_queue, (vm_page_queue_entry_t)p)) {
		next = (vm_page_t)VM_PAGE_UNPACK_PTR(p->vmp_pageq.next);

		vm_page_enqueue_active(p, FALSE);
		p = next;
	}

	compressor_running_perf_test = false;
	vm_page_unlock_queues();
	if (err == KERN_SUCCESS) {
		*bytes_compressed = page_count * PAGE_SIZE_64;
		*compressor_growth = compressed_bytes_end - compressed_bytes_start;
	}

	/*
	 * pageout_scan will consider waking the compactor swapper
	 * before it blocks. Do the same thing here before we return
	 * to ensure that back to back benchmark runs can't overly fragment the
	 * compressor pool.
	 */
	vm_consider_waking_compactor_swapper();
	return err;
}
#endif /* DEVELOPMENT || DEBUG */
