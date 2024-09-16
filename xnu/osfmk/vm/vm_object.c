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
 *	File:	vm/vm_object.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Virtual memory object module.
 */

#include <debug.h>

#include <mach/mach_types.h>
#include <mach/memory_object.h>
#include <mach/vm_param.h>

#include <mach/sdt.h>

#include <ipc/ipc_types.h>
#include <ipc/ipc_port.h>

#include <kern/kern_types.h>
#include <kern/assert.h>
#include <kern/queue.h>
#include <kern/kalloc.h>
#include <kern/zalloc.h>
#include <kern/host.h>
#include <kern/host_statistics.h>
#include <kern/processor.h>
#include <kern/misc_protos.h>
#include <kern/policy_internal.h>

#include <sys/kdebug.h>
#include <sys/kdebug_triage.h>

#include <vm/memory_object_internal.h>
#include <vm/vm_compressor_pager_internal.h>
#include <vm/vm_fault_internal.h>
#include <vm/vm_map.h>
#include <vm/vm_object_internal.h>
#include <vm/vm_page_internal.h>
#include <vm/vm_pageout_internal.h>
#include <vm/vm_protos_internal.h>
#include <vm/vm_purgeable_internal.h>
#include <vm/vm_ubc.h>

#include <vm/vm_compressor_xnu.h>

#if CONFIG_PHANTOM_CACHE
#include <vm/vm_phantom_cache_internal.h>
#endif

#if VM_OBJECT_ACCESS_TRACKING
uint64_t vm_object_access_tracking_reads = 0;
uint64_t vm_object_access_tracking_writes = 0;
#endif /* VM_OBJECT_ACCESS_TRACKING */

boolean_t vm_object_collapse_compressor_allowed = TRUE;

struct vm_counters vm_counters;

#if DEVELOPMENT || DEBUG
extern struct memory_object_pager_ops shared_region_pager_ops;
extern unsigned int shared_region_pagers_resident_count;
extern unsigned int shared_region_pagers_resident_peak;
#endif /* DEVELOPMENT || DEBUG */

#if VM_OBJECT_TRACKING
btlog_t vm_object_tracking_btlog;

void
vm_object_tracking_init(void)
{
	int vm_object_tracking;

	vm_object_tracking = 1;
	PE_parse_boot_argn("vm_object_tracking", &vm_object_tracking,
	    sizeof(vm_object_tracking));

	if (vm_object_tracking) {
		vm_object_tracking_btlog = btlog_create(BTLOG_HASH,
		    VM_OBJECT_TRACKING_NUM_RECORDS);
		assert(vm_object_tracking_btlog);
	}
}
#endif /* VM_OBJECT_TRACKING */

/*
 *	Virtual memory objects maintain the actual data
 *	associated with allocated virtual memory.  A given
 *	page of memory exists within exactly one object.
 *
 *	An object is only deallocated when all "references"
 *	are given up.
 *
 *	Associated with each object is a list of all resident
 *	memory pages belonging to that object; this list is
 *	maintained by the "vm_page" module, but locked by the object's
 *	lock.
 *
 *	Each object also records the memory object reference
 *	that is used by the kernel to request and write
 *	back data (the memory object, field "pager"), etc...
 *
 *	Virtual memory objects are allocated to provide
 *	zero-filled memory (vm_allocate) or map a user-defined
 *	memory object into a virtual address space (vm_map).
 *
 *	Virtual memory objects that refer to a user-defined
 *	memory object are called "permanent", because all changes
 *	made in virtual memory are reflected back to the
 *	memory manager, which may then store it permanently.
 *	Other virtual memory objects are called "temporary",
 *	meaning that changes need be written back only when
 *	necessary to reclaim pages, and that storage associated
 *	with the object can be discarded once it is no longer
 *	mapped.
 *
 *	A permanent memory object may be mapped into more
 *	than one virtual address space.  Moreover, two threads
 *	may attempt to make the first mapping of a memory
 *	object concurrently.  Only one thread is allowed to
 *	complete this mapping; all others wait for the
 *	"pager_initialized" field is asserted, indicating
 *	that the first thread has initialized all of the
 *	necessary fields in the virtual memory object structure.
 *
 *	The kernel relies on a *default memory manager* to
 *	provide backing storage for the zero-filled virtual
 *	memory objects.  The pager memory objects associated
 *	with these temporary virtual memory objects are only
 *	requested from the default memory manager when it
 *	becomes necessary.  Virtual memory objects
 *	that depend on the default memory manager are called
 *	"internal".  The "pager_created" field is provided to
 *	indicate whether these ports have ever been allocated.
 *
 *	The kernel may also create virtual memory objects to
 *	hold changed pages after a copy-on-write operation.
 *	In this case, the virtual memory object (and its
 *	backing storage -- its memory object) only contain
 *	those pages that have been changed.  The "shadow"
 *	field refers to the virtual memory object that contains
 *	the remainder of the contents.  The "shadow_offset"
 *	field indicates where in the "shadow" these contents begin.
 *	The "copy" field refers to a virtual memory object
 *	to which changed pages must be copied before changing
 *	this object, in order to implement another form
 *	of copy-on-write optimization.
 *
 *	The virtual memory object structure also records
 *	the attributes associated with its memory object.
 *	The "pager_ready", "can_persist" and "copy_strategy"
 *	fields represent those attributes.  The "cached_list"
 *	field is used in the implementation of the persistence
 *	attribute.
 *
 * ZZZ Continue this comment.
 */

/* Forward declarations for internal functions. */
static kern_return_t    vm_object_terminate(
	vm_object_t     object);

static void             vm_object_do_collapse(
	vm_object_t     object,
	vm_object_t     backing_object);

static void             vm_object_do_bypass(
	vm_object_t     object,
	vm_object_t     backing_object);

static void             vm_object_release_pager(
	memory_object_t pager);

SECURITY_READ_ONLY_LATE(zone_t) vm_object_zone; /* vm backing store zone */

/*
 * Wired-down kernel memory belongs to this memory object (kernel_object)
 * by default to avoid wasting data structures.
 */
static struct vm_object                 kernel_object_store VM_PAGE_PACKED_ALIGNED;
const vm_object_t                       kernel_object_default = &kernel_object_store;

static struct vm_object                 compressor_object_store VM_PAGE_PACKED_ALIGNED;
const vm_object_t                       compressor_object = &compressor_object_store;

/*
 * This object holds all pages that have been retired due to errors like ECC.
 * The system should never use the page or look at its contents. The offset
 * in this object is the same as the page's physical address.
 */
static struct vm_object                 retired_pages_object_store VM_PAGE_PACKED_ALIGNED;
const vm_object_t                       retired_pages_object = &retired_pages_object_store;


static struct vm_object                 exclaves_object_store VM_PAGE_PACKED_ALIGNED;
const vm_object_t                       exclaves_object = &exclaves_object_store;


/*
 *	Virtual memory objects are initialized from
 *	a template (see vm_object_allocate).
 *
 *	When adding a new field to the virtual memory
 *	object structure, be sure to add initialization
 *	(see _vm_object_allocate()).
 */
static const struct vm_object vm_object_template = {
	.memq.prev = 0,
	.memq.next = 0,
	/*
	 * The lock will be initialized for each allocated object in
	 * _vm_object_allocate(), so we don't need to initialize it in
	 * the vm_object_template.
	 */
	.vo_size = 0,
	.memq_hint = VM_PAGE_NULL,
	.ref_count = 1,
	.resident_page_count = 0,
	.wired_page_count = 0,
	.reusable_page_count = 0,
	.vo_copy = VM_OBJECT_NULL,
	.vo_copy_version = 0,
	.vo_inherit_copy_none = false,
	.shadow = VM_OBJECT_NULL,
	.vo_shadow_offset = (vm_object_offset_t) 0,
	.pager = MEMORY_OBJECT_NULL,
	.paging_offset = 0,
	.pager_control = MEMORY_OBJECT_CONTROL_NULL,
	.copy_strategy = MEMORY_OBJECT_COPY_SYMMETRIC,
	.paging_in_progress = 0,
	.vo_size_delta = 0,
	.activity_in_progress = 0,

	/* Begin bitfields */
	.all_wanted = 0, /* all bits FALSE */
	.pager_created = FALSE,
	.pager_initialized = FALSE,
	.pager_ready = FALSE,
	.pager_trusted = FALSE,
	.can_persist = FALSE,
	.internal = TRUE,
	.private = FALSE,
	.pageout = FALSE,
	.alive = TRUE,
	.purgable = VM_PURGABLE_DENY,
	.purgeable_when_ripe = FALSE,
	.purgeable_only_by_kernel = FALSE,
	.shadowed = FALSE,
	.true_share = FALSE,
	.terminating = FALSE,
	.named = FALSE,
	.shadow_severed = FALSE,
	.phys_contiguous = FALSE,
	.nophyscache = FALSE,
	/* End bitfields */

	.cached_list.prev = NULL,
	.cached_list.next = NULL,

	.last_alloc = (vm_object_offset_t) 0,
	.sequential = (vm_object_offset_t) 0,
	.pages_created = 0,
	.pages_used = 0,
	.scan_collisions = 0,
#if CONFIG_PHANTOM_CACHE
	.phantom_object_id = 0,
#endif
	.cow_hint = ~(vm_offset_t)0,

	/* cache bitfields */
	.wimg_bits = VM_WIMG_USE_DEFAULT,
	.set_cache_attr = FALSE,
	.object_is_shared_cache = FALSE,
	.code_signed = FALSE,
	.transposed = FALSE,
	.mapping_in_progress = FALSE,
	.phantom_isssd = FALSE,
	.volatile_empty = FALSE,
	.volatile_fault = FALSE,
	.all_reusable = FALSE,
	.blocked_access = FALSE,
	.vo_ledger_tag = VM_LEDGER_TAG_NONE,
	.vo_no_footprint = FALSE,
#if CONFIG_IOSCHED || UPL_DEBUG
	.uplq.prev = NULL,
	.uplq.next = NULL,
#endif /* UPL_DEBUG */
#ifdef VM_PIP_DEBUG
	.pip_holders = {0},
#endif /* VM_PIP_DEBUG */

	.objq.next = NULL,
	.objq.prev = NULL,
	.task_objq.next = NULL,
	.task_objq.prev = NULL,

	.purgeable_queue_type = PURGEABLE_Q_TYPE_MAX,
	.purgeable_queue_group = 0,

	.wire_tag = VM_KERN_MEMORY_NONE,
#if !VM_TAG_ACTIVE_UPDATE
	.wired_objq.next = NULL,
	.wired_objq.prev = NULL,
#endif /* ! VM_TAG_ACTIVE_UPDATE */

	.io_tracking = FALSE,

#if CONFIG_SECLUDED_MEMORY
	.eligible_for_secluded = FALSE,
	.can_grab_secluded = FALSE,
#else /* CONFIG_SECLUDED_MEMORY */
	.__object3_unused_bits = 0,
#endif /* CONFIG_SECLUDED_MEMORY */

	.for_realtime = false,
	.no_pager_reason = VM_OBJECT_DESTROY_UNKNOWN_REASON,

#if VM_OBJECT_ACCESS_TRACKING
	.access_tracking = FALSE,
	.access_tracking_reads = 0,
	.access_tracking_writes = 0,
#endif /* VM_OBJECT_ACCESS_TRACKING */

#if DEBUG
	.purgeable_owner_bt = {0},
	.vo_purgeable_volatilizer = NULL,
	.purgeable_volatilizer_bt = {0},
#endif /* DEBUG */
};

LCK_GRP_DECLARE(vm_object_lck_grp, "vm_object");
LCK_GRP_DECLARE(vm_object_cache_lck_grp, "vm_object_cache");
LCK_ATTR_DECLARE(vm_object_lck_attr, 0, 0);
LCK_ATTR_DECLARE(kernel_object_lck_attr, 0, LCK_ATTR_DEBUG);
LCK_ATTR_DECLARE(compressor_object_lck_attr, 0, LCK_ATTR_DEBUG);

unsigned int vm_page_purged_wired = 0;
unsigned int vm_page_purged_busy = 0;
unsigned int vm_page_purged_others = 0;

static queue_head_t     vm_object_cached_list;
static uint32_t         vm_object_cache_pages_freed = 0;
static uint32_t         vm_object_cache_pages_moved = 0;
static uint32_t         vm_object_cache_pages_skipped = 0;
static uint32_t         vm_object_cache_adds = 0;
static uint32_t         vm_object_cached_count = 0;
static LCK_MTX_DECLARE_ATTR(vm_object_cached_lock_data,
    &vm_object_cache_lck_grp, &vm_object_lck_attr);

static uint32_t         vm_object_page_grab_failed = 0;
static uint32_t         vm_object_page_grab_skipped = 0;
static uint32_t         vm_object_page_grab_returned = 0;
static uint32_t         vm_object_page_grab_pmapped = 0;
static uint32_t         vm_object_page_grab_reactivations = 0;

#define vm_object_cache_lock_spin()             \
	        lck_mtx_lock_spin(&vm_object_cached_lock_data)
#define vm_object_cache_unlock()        \
	        lck_mtx_unlock(&vm_object_cached_lock_data)

static void     vm_object_cache_remove_locked(vm_object_t);


static void vm_object_reap(vm_object_t object);
static void vm_object_reap_async(vm_object_t object);
static void vm_object_reaper_thread(void);

static LCK_MTX_DECLARE_ATTR(vm_object_reaper_lock_data,
    &vm_object_lck_grp, &vm_object_lck_attr);

static queue_head_t vm_object_reaper_queue; /* protected by vm_object_reaper_lock() */
unsigned int vm_object_reap_count = 0;
unsigned int vm_object_reap_count_async = 0;

#define vm_object_reaper_lock()         \
	        lck_mtx_lock(&vm_object_reaper_lock_data)
#define vm_object_reaper_lock_spin()            \
	        lck_mtx_lock_spin(&vm_object_reaper_lock_data)
#define vm_object_reaper_unlock()       \
	        lck_mtx_unlock(&vm_object_reaper_lock_data)

#if CONFIG_IOSCHED
/* I/O Re-prioritization request list */
struct mpsc_daemon_queue io_reprioritize_q;

ZONE_DEFINE_TYPE(io_reprioritize_req_zone, "io_reprioritize_req",
    struct io_reprioritize_req, ZC_NONE);

/* I/O re-prioritization MPSC callback */
static void io_reprioritize(mpsc_queue_chain_t elm, mpsc_daemon_queue_t dq);

void vm_page_request_reprioritize(vm_object_t, uint64_t, uint32_t, int);
void vm_page_handle_prio_inversion(vm_object_t, vm_page_t);
void vm_decmp_upl_reprioritize(upl_t, int);
#endif

void
vm_object_set_size(
	vm_object_t             object,
	vm_object_size_t        outer_size,
	vm_object_size_t        inner_size)
{
	object->vo_size = vm_object_round_page(outer_size);
#if KASAN
	assert(object->vo_size - inner_size <= USHRT_MAX);
	object->vo_size_delta = (unsigned short)(object->vo_size - inner_size);
#else
	(void)inner_size;
#endif
}


/*
 *	vm_object_allocate:
 *
 *	Returns a new object with the given size.
 */

__private_extern__ void
_vm_object_allocate(
	vm_object_size_t        size,
	vm_object_t             object)
{
	*object = vm_object_template;
	vm_page_queue_init(&object->memq);
#if UPL_DEBUG || CONFIG_IOSCHED
	queue_init(&object->uplq);
#endif
	vm_object_lock_init(object);
	vm_object_set_size(object, size, size);

#if VM_OBJECT_TRACKING_OP_CREATED
	if (vm_object_tracking_btlog) {
		btlog_record(vm_object_tracking_btlog, object,
		    VM_OBJECT_TRACKING_OP_CREATED,
		    btref_get(__builtin_frame_address(0), 0));
	}
#endif /* VM_OBJECT_TRACKING_OP_CREATED */
}

__private_extern__ vm_object_t
vm_object_allocate(
	vm_object_size_t        size)
{
	vm_object_t object;

	object = zalloc_flags(vm_object_zone, Z_WAITOK | Z_NOFAIL);
	_vm_object_allocate(size, object);

	return object;
}

TUNABLE(bool, workaround_41447923, "workaround_41447923", false);

/*
 *	vm_object_bootstrap:
 *
 *	Initialize the VM objects module.
 */
__startup_func
void
vm_object_bootstrap(void)
{
	vm_size_t       vm_object_size;

	assert(sizeof(mo_ipc_object_bits_t) == sizeof(ipc_object_bits_t));

	vm_object_size = (sizeof(struct vm_object) + (VM_PAGE_PACKED_PTR_ALIGNMENT - 1)) &
	    ~(VM_PAGE_PACKED_PTR_ALIGNMENT - 1);

	vm_object_zone = zone_create("vm objects", vm_object_size,
	    ZC_NOENCRYPT | ZC_ALIGNMENT_REQUIRED | ZC_VM | ZC_NO_TBI_TAG);

	queue_init(&vm_object_cached_list);

	queue_init(&vm_object_reaper_queue);

	/*
	 *	Initialize the "kernel object"
	 */

	/*
	 * Note that in the following size specifications, we need to add 1 because
	 * VM_MAX_KERNEL_ADDRESS (vm_last_addr) is a maximum address, not a size.
	 */
	_vm_object_allocate(VM_MAX_KERNEL_ADDRESS + 1, kernel_object_default);
	_vm_object_allocate(VM_MAX_KERNEL_ADDRESS + 1, compressor_object);
	kernel_object_default->copy_strategy = MEMORY_OBJECT_COPY_NONE;
	compressor_object->copy_strategy = MEMORY_OBJECT_COPY_NONE;
	kernel_object_default->no_tag_update = TRUE;

	/*
	 * The object to hold retired VM pages.
	 */
	_vm_object_allocate(VM_MAX_KERNEL_ADDRESS + 1, retired_pages_object);
	retired_pages_object->copy_strategy = MEMORY_OBJECT_COPY_NONE;

	/**
	 * The object to hold pages owned by exclaves.
	 */
	_vm_object_allocate(VM_MAX_KERNEL_ADDRESS + 1, exclaves_object);
	exclaves_object->copy_strategy = MEMORY_OBJECT_COPY_NONE;
}

#if CONFIG_IOSCHED
void
vm_io_reprioritize_init(void)
{
	kern_return_t   result;

	result = mpsc_daemon_queue_init_with_thread(&io_reprioritize_q, io_reprioritize, BASEPRI_KERNEL,
	    "VM_io_reprioritize_thread", MPSC_DAEMON_INIT_NONE);
	if (result != KERN_SUCCESS) {
		panic("Unable to start I/O reprioritization thread (%d)", result);
	}
}
#endif

void
vm_object_reaper_init(void)
{
	kern_return_t   kr;
	thread_t        thread;

	kr = kernel_thread_start_priority(
		(thread_continue_t) vm_object_reaper_thread,
		NULL,
		BASEPRI_VM,
		&thread);
	if (kr != KERN_SUCCESS) {
		panic("failed to launch vm_object_reaper_thread kr=0x%x", kr);
	}
	thread_set_thread_name(thread, "VM_object_reaper_thread");
	thread_deallocate(thread);
}


/*
 *	vm_object_deallocate:
 *
 *	Release a reference to the specified object,
 *	gained either through a vm_object_allocate
 *	or a vm_object_reference call.  When all references
 *	are gone, storage associated with this object
 *	may be relinquished.
 *
 *	No object may be locked.
 */
unsigned long vm_object_deallocate_shared_successes = 0;
unsigned long vm_object_deallocate_shared_failures = 0;
unsigned long vm_object_deallocate_shared_swap_failures = 0;

__private_extern__ void
vm_object_deallocate(
	vm_object_t     object)
{
	vm_object_t     shadow = VM_OBJECT_NULL;

//	if(object)dbgLog(object, object->ref_count, object->can_persist, 3);	/* (TEST/DEBUG) */
//	else dbgLog(object, 0, 0, 3);	/* (TEST/DEBUG) */

	if (object == VM_OBJECT_NULL) {
		return;
	}

	if (is_kernel_object(object) || object == compressor_object || object == retired_pages_object) {
		vm_object_lock_shared(object);

		OSAddAtomic(-1, &object->ref_count);

		if (object->ref_count == 0) {
			if (is_kernel_object(object)) {
				panic("vm_object_deallocate: losing a kernel_object");
			} else if (object == retired_pages_object) {
				panic("vm_object_deallocate: losing retired_pages_object");
			} else {
				panic("vm_object_deallocate: losing compressor_object");
			}
		}
		vm_object_unlock(object);
		return;
	}

	if (object->ref_count == 2 &&
	    object->named) {
		/*
		 * This "named" object's reference count is about to
		 * drop from 2 to 1:
		 * we'll need to call memory_object_last_unmap().
		 */
	} else if (object->ref_count == 2 &&
	    object->internal &&
	    object->shadow != VM_OBJECT_NULL) {
		/*
		 * This internal object's reference count is about to
		 * drop from 2 to 1 and it has a shadow object:
		 * we'll want to try and collapse this object with its
		 * shadow.
		 */
	} else if (object->ref_count >= 2) {
		UInt32          original_ref_count;
		volatile UInt32 *ref_count_p;
		Boolean         atomic_swap;

		/*
		 * The object currently looks like it is not being
		 * kept alive solely by the reference we're about to release.
		 * Let's try and release our reference without taking
		 * all the locks we would need if we had to terminate the
		 * object (cache lock + exclusive object lock).
		 * Lock the object "shared" to make sure we don't race with
		 * anyone holding it "exclusive".
		 */
		vm_object_lock_shared(object);
		ref_count_p = (volatile UInt32 *) &object->ref_count;
		original_ref_count = object->ref_count;
		/*
		 * Test again as "ref_count" could have changed.
		 * "named" shouldn't change.
		 */
		if (original_ref_count == 2 &&
		    object->named) {
			/* need to take slow path for m_o_last_unmap() */
			atomic_swap = FALSE;
		} else if (original_ref_count == 2 &&
		    object->internal &&
		    object->shadow != VM_OBJECT_NULL) {
			/* need to take slow path for vm_object_collapse() */
			atomic_swap = FALSE;
		} else if (original_ref_count < 2) {
			/* need to take slow path for vm_object_terminate() */
			atomic_swap = FALSE;
		} else {
			/* try an atomic update with the shared lock */
			atomic_swap = OSCompareAndSwap(
				original_ref_count,
				original_ref_count - 1,
				(UInt32 *) &object->ref_count);
			if (atomic_swap == FALSE) {
				vm_object_deallocate_shared_swap_failures++;
				/* fall back to the slow path... */
			}
		}

		vm_object_unlock(object);

		if (atomic_swap) {
			/*
			 * ref_count was updated atomically !
			 */
			vm_object_deallocate_shared_successes++;
			return;
		}

		/*
		 * Someone else updated the ref_count at the same
		 * time and we lost the race.  Fall back to the usual
		 * slow but safe path...
		 */
		vm_object_deallocate_shared_failures++;
	}

	while (object != VM_OBJECT_NULL) {
		vm_object_lock(object);

		assert(object->ref_count > 0);

		/*
		 *	If the object has a named reference, and only
		 *	that reference would remain, inform the pager
		 *	about the last "mapping" reference going away.
		 */
		if ((object->ref_count == 2) && (object->named)) {
			memory_object_t pager = object->pager;

			/* Notify the Pager that there are no */
			/* more mappers for this object */

			if (pager != MEMORY_OBJECT_NULL) {
				vm_object_mapping_wait(object, THREAD_UNINT);
				vm_object_mapping_begin(object);
				vm_object_unlock(object);

				memory_object_last_unmap(pager);

				vm_object_lock(object);
				vm_object_mapping_end(object);
			}
			assert(object->ref_count > 0);
		}

		/*
		 *	Lose the reference. If other references
		 *	remain, then we are done, unless we need
		 *	to retry a cache trim.
		 *	If it is the last reference, then keep it
		 *	until any pending initialization is completed.
		 */

		/* if the object is terminating, it cannot go into */
		/* the cache and we obviously should not call      */
		/* terminate again.  */

		if ((object->ref_count > 1) || object->terminating) {
			vm_object_lock_assert_exclusive(object);
			object->ref_count--;

			if (object->ref_count == 1 &&
			    object->shadow != VM_OBJECT_NULL) {
				/*
				 * There's only one reference left on this
				 * VM object.  We can't tell if it's a valid
				 * one (from a mapping for example) or if this
				 * object is just part of a possibly stale and
				 * useless shadow chain.
				 * We would like to try and collapse it into
				 * its parent, but we don't have any pointers
				 * back to this parent object.
				 * But we can try and collapse this object with
				 * its own shadows, in case these are useless
				 * too...
				 * We can't bypass this object though, since we
				 * don't know if this last reference on it is
				 * meaningful or not.
				 */
				vm_object_collapse(object, 0, FALSE);
			}
			vm_object_unlock(object);
			return;
		}

		/*
		 *	We have to wait for initialization
		 *	before destroying or caching the object.
		 */

		if (object->pager_created && !object->pager_initialized) {
			assert(!object->can_persist);
			vm_object_sleep(object,
			    VM_OBJECT_EVENT_PAGER_INIT,
			    THREAD_UNINT,
			    LCK_SLEEP_UNLOCK);
			continue;
		}

		/*
		 *	Terminate this object. If it had a shadow,
		 *	then deallocate it; otherwise, if we need
		 *	to retry a cache trim, do so now; otherwise,
		 *	we are done. "pageout" objects have a shadow,
		 *	but maintain a "paging reference" rather than
		 *	a normal reference.
		 */
		shadow = object->pageout?VM_OBJECT_NULL:object->shadow;

		if (vm_object_terminate(object) != KERN_SUCCESS) {
			return;
		}
		if (shadow != VM_OBJECT_NULL) {
			object = shadow;
			continue;
		}
		return;
	}
}



vm_page_t
vm_object_page_grab(
	vm_object_t     object)
{
	vm_page_t       p, next_p;
	int             p_limit = 0;
	int             p_skipped = 0;

	vm_object_lock_assert_exclusive(object);

	next_p = (vm_page_t)vm_page_queue_first(&object->memq);
	p_limit = MIN(50, object->resident_page_count);

	while (!vm_page_queue_end(&object->memq, (vm_page_queue_entry_t)next_p) && --p_limit > 0) {
		p = next_p;
		next_p = (vm_page_t)vm_page_queue_next(&next_p->vmp_listq);

		if (VM_PAGE_WIRED(p) || p->vmp_busy || p->vmp_cleaning || p->vmp_laundry || p->vmp_fictitious) {
			goto move_page_in_obj;
		}

		if (p->vmp_pmapped || p->vmp_dirty || p->vmp_precious) {
			vm_page_lockspin_queues();

			if (p->vmp_pmapped) {
				int refmod_state;

				vm_object_page_grab_pmapped++;

				if (p->vmp_reference == FALSE || p->vmp_dirty == FALSE) {
					refmod_state = pmap_get_refmod(VM_PAGE_GET_PHYS_PAGE(p));

					if (refmod_state & VM_MEM_REFERENCED) {
						p->vmp_reference = TRUE;
					}
					if (refmod_state & VM_MEM_MODIFIED) {
						SET_PAGE_DIRTY(p, FALSE);
					}
				}
				if (p->vmp_dirty == FALSE && p->vmp_precious == FALSE) {
					vm_page_lockconvert_queues();
					refmod_state = pmap_disconnect(VM_PAGE_GET_PHYS_PAGE(p));

					if (refmod_state & VM_MEM_REFERENCED) {
						p->vmp_reference = TRUE;
					}
					if (refmod_state & VM_MEM_MODIFIED) {
						SET_PAGE_DIRTY(p, FALSE);
					}

					if (p->vmp_dirty == FALSE) {
						goto take_page;
					}
				}
			}
			if ((p->vmp_q_state != VM_PAGE_ON_ACTIVE_Q) && p->vmp_reference == TRUE) {
				vm_page_activate(p);

				counter_inc(&vm_statistics_reactivations);
				vm_object_page_grab_reactivations++;
			}
			vm_page_unlock_queues();
move_page_in_obj:
			vm_page_queue_remove(&object->memq, p, vmp_listq);
			vm_page_queue_enter(&object->memq, p, vmp_listq);

			p_skipped++;
			continue;
		}
		vm_page_lockspin_queues();
take_page:
		vm_page_free_prepare_queues(p);
		vm_object_page_grab_returned++;
		vm_object_page_grab_skipped += p_skipped;

		vm_page_unlock_queues();

		vm_page_free_prepare_object(p, TRUE);

		return p;
	}
	vm_object_page_grab_skipped += p_skipped;
	vm_object_page_grab_failed++;

	return NULL;
}



#define EVICT_PREPARE_LIMIT     64
#define EVICT_AGE               10

static  clock_sec_t     vm_object_cache_aging_ts = 0;

static void
vm_object_cache_remove_locked(
	vm_object_t     object)
{
	assert(object->purgable == VM_PURGABLE_DENY);

	queue_remove(&vm_object_cached_list, object, vm_object_t, cached_list);
	object->cached_list.next = NULL;
	object->cached_list.prev = NULL;

	vm_object_cached_count--;
}

void
vm_object_cache_remove(
	vm_object_t     object)
{
	vm_object_cache_lock_spin();

	if (object->cached_list.next &&
	    object->cached_list.prev) {
		vm_object_cache_remove_locked(object);
	}

	vm_object_cache_unlock();
}

void
vm_object_cache_add(
	vm_object_t     object)
{
	clock_sec_t sec;
	clock_nsec_t nsec;

	assert(object->purgable == VM_PURGABLE_DENY);

	if (object->resident_page_count == 0) {
		return;
	}
	if (object->vo_ledger_tag) {
		/*
		 * We can't add an "owned" object to the cache because
		 * the "vo_owner" and "vo_cache_ts" fields are part of the
		 * same "union" and can't be used at the same time.
		 */
		return;
	}
	clock_get_system_nanotime(&sec, &nsec);

	vm_object_cache_lock_spin();

	if (object->cached_list.next == NULL &&
	    object->cached_list.prev == NULL) {
		queue_enter(&vm_object_cached_list, object, vm_object_t, cached_list);
		object->vo_cache_ts = sec + EVICT_AGE;
		object->vo_cache_pages_to_scan = object->resident_page_count;

		vm_object_cached_count++;
		vm_object_cache_adds++;
	}
	vm_object_cache_unlock();
}

int
vm_object_cache_evict(
	int     num_to_evict,
	int     max_objects_to_examine)
{
	vm_object_t     object = VM_OBJECT_NULL;
	vm_object_t     next_obj = VM_OBJECT_NULL;
	vm_page_t       local_free_q = VM_PAGE_NULL;
	vm_page_t       p;
	vm_page_t       next_p;
	int             object_cnt = 0;
	vm_page_t       ep_array[EVICT_PREPARE_LIMIT];
	int             ep_count;
	int             ep_limit;
	int             ep_index;
	int             ep_freed = 0;
	int             ep_moved = 0;
	uint32_t        ep_skipped = 0;
	clock_sec_t     sec;
	clock_nsec_t    nsec;

	KDBG_DEBUG(0x13001ec | DBG_FUNC_START);
	/*
	 * do a couple of quick checks to see if it's
	 * worthwhile grabbing the lock
	 */
	if (queue_empty(&vm_object_cached_list)) {
		KDBG_DEBUG(0x13001ec | DBG_FUNC_END);
		return 0;
	}
	clock_get_system_nanotime(&sec, &nsec);

	/*
	 * the object on the head of the queue has not
	 * yet sufficiently aged
	 */
	if (sec < vm_object_cache_aging_ts) {
		KDBG_DEBUG(0x13001ec | DBG_FUNC_END);
		return 0;
	}
	/*
	 * don't need the queue lock to find
	 * and lock an object on the cached list
	 */
	vm_page_unlock_queues();

	vm_object_cache_lock_spin();

	for (;;) {  /* loop for as long as we have objects to process */
		next_obj = (vm_object_t)queue_first(&vm_object_cached_list);

		/* loop to find the next target in the cache_list */
		while (!queue_end(&vm_object_cached_list, (queue_entry_t)next_obj) && object_cnt++ < max_objects_to_examine) {
			object = next_obj;
			next_obj = (vm_object_t)queue_next(&next_obj->cached_list);

			assert(object->purgable == VM_PURGABLE_DENY);

			if (sec < object->vo_cache_ts) { // reached the point in the queue beyond the time we started
				KDBG_DEBUG(0x130020c, object, object->resident_page_count, object->vo_cache_ts, sec);

				vm_object_cache_aging_ts = object->vo_cache_ts;
				object = VM_OBJECT_NULL; /* this will cause to break away from the outer loop */
				break;
			}
			if (!vm_object_lock_try_scan(object)) {
				/*
				 * just skip over this guy for now... if we find
				 * an object to steal pages from, we'll revist in a bit...
				 * hopefully, the lock will have cleared
				 */
				KDBG_DEBUG(0x13001f8, object, object->resident_page_count);

				object = VM_OBJECT_NULL;
				continue;
			}
			if (vm_page_queue_empty(&object->memq) || object->vo_cache_pages_to_scan == 0) {
				/*
				 * this case really shouldn't happen, but it's not fatal
				 * so deal with it... if we don't remove the object from
				 * the list, we'll never move past it.
				 */
				KDBG_DEBUG(0x13001fc, object, object->resident_page_count, ep_freed, ep_moved);

				vm_object_cache_remove_locked(object);
				vm_object_unlock(object);
				object = VM_OBJECT_NULL;
				continue;
			}
			/*
			 * we have a locked object with pages...
			 * time to start harvesting
			 */
			break;
		}
		vm_object_cache_unlock();

		if (object == VM_OBJECT_NULL) {
			break;
		}

		/*
		 * object is locked at this point and
		 * has resident pages
		 */
		next_p = (vm_page_t)vm_page_queue_first(&object->memq);

		/*
		 * break the page scan into 2 pieces to minimize the time spent
		 * behind the page queue lock...
		 * the list of pages on these unused objects is likely to be cold
		 * w/r to the cpu cache which increases the time to scan the list
		 * tenfold...  and we may have a 'run' of pages we can't utilize that
		 * needs to be skipped over...
		 */
		if ((ep_limit = num_to_evict - (ep_freed + ep_moved)) > EVICT_PREPARE_LIMIT) {
			ep_limit = EVICT_PREPARE_LIMIT;
		}
		ep_count = 0;

		while (!vm_page_queue_end(&object->memq, (vm_page_queue_entry_t)next_p) && object->vo_cache_pages_to_scan && ep_count < ep_limit) {
			p = next_p;
			next_p = (vm_page_t)vm_page_queue_next(&next_p->vmp_listq);

			object->vo_cache_pages_to_scan--;

			if (VM_PAGE_WIRED(p) || p->vmp_busy || p->vmp_cleaning || p->vmp_laundry) {
				vm_page_queue_remove(&object->memq, p, vmp_listq);
				vm_page_queue_enter(&object->memq, p, vmp_listq);

				ep_skipped++;
				continue;
			}
			if (p->vmp_wpmapped || p->vmp_dirty || p->vmp_precious) {
				vm_page_queue_remove(&object->memq, p, vmp_listq);
				vm_page_queue_enter(&object->memq, p, vmp_listq);

				pmap_clear_reference(VM_PAGE_GET_PHYS_PAGE(p));
			}
			ep_array[ep_count++] = p;
		}
		KDBG_DEBUG(0x13001f4 | DBG_FUNC_START, object, object->resident_page_count, ep_freed, ep_moved);

		vm_page_lockspin_queues();

		for (ep_index = 0; ep_index < ep_count; ep_index++) {
			p = ep_array[ep_index];

			if (p->vmp_wpmapped || p->vmp_dirty || p->vmp_precious) {
				p->vmp_reference = FALSE;
				p->vmp_no_cache = FALSE;

				/*
				 * we've already filtered out pages that are in the laundry
				 * so if we get here, this page can't be on the pageout queue
				 */
				vm_page_queues_remove(p, FALSE);
				vm_page_enqueue_inactive(p, TRUE);

				ep_moved++;
			} else {
#if CONFIG_PHANTOM_CACHE
				vm_phantom_cache_add_ghost(p);
#endif
				vm_page_free_prepare_queues(p);

				assert(p->vmp_pageq.next == 0 && p->vmp_pageq.prev == 0);
				/*
				 * Add this page to our list of reclaimed pages,
				 * to be freed later.
				 */
				p->vmp_snext = local_free_q;
				local_free_q = p;

				ep_freed++;
			}
		}
		vm_page_unlock_queues();

		KDBG_DEBUG(0x13001f4 | DBG_FUNC_END, object, object->resident_page_count, ep_freed, ep_moved);

		if (local_free_q) {
			vm_page_free_list(local_free_q, TRUE);
			local_free_q = VM_PAGE_NULL;
		}
		if (object->vo_cache_pages_to_scan == 0) {
			KDBG_DEBUG(0x1300208, object, object->resident_page_count, ep_freed, ep_moved);

			vm_object_cache_remove(object);

			KDBG_DEBUG(0x13001fc, object, object->resident_page_count, ep_freed, ep_moved);
		}
		/*
		 * done with this object
		 */
		vm_object_unlock(object);
		object = VM_OBJECT_NULL;

		/*
		 * at this point, we are not holding any locks
		 */
		if ((ep_freed + ep_moved) >= num_to_evict) {
			/*
			 * we've reached our target for the
			 * number of pages to evict
			 */
			break;
		}
		vm_object_cache_lock_spin();
	}
	/*
	 * put the page queues lock back to the caller's
	 * idea of it
	 */
	vm_page_lock_queues();

	vm_object_cache_pages_freed += ep_freed;
	vm_object_cache_pages_moved += ep_moved;
	vm_object_cache_pages_skipped += ep_skipped;

	KDBG_DEBUG(0x13001ec | DBG_FUNC_END, ep_freed);
	return ep_freed;
}

/*
 *	Routine:	vm_object_terminate
 *	Purpose:
 *		Free all resources associated with a vm_object.
 *	In/out conditions:
 *		Upon entry, the object must be locked,
 *		and the object must have exactly one reference.
 *
 *		The shadow object reference is left alone.
 *
 *		The object must be unlocked if its found that pages
 *		must be flushed to a backing object.  If someone
 *		manages to map the object while it is being flushed
 *		the object is returned unlocked and unchanged.  Otherwise,
 *		upon exit, the cache will be unlocked, and the
 *		object will cease to exist.
 */
static kern_return_t
vm_object_terminate(
	vm_object_t     object)
{
	vm_object_t     shadow_object;

	vm_object_lock_assert_exclusive(object);

	if (!object->pageout && (!object->internal && object->can_persist) &&
	    (object->pager != NULL || object->shadow_severed)) {
		/*
		 * Clear pager_trusted bit so that the pages get yanked
		 * out of the object instead of cleaned in place.  This
		 * prevents a deadlock in XMM and makes more sense anyway.
		 */
		VM_OBJECT_SET_PAGER_TRUSTED(object, FALSE);

		vm_object_reap_pages(object, REAP_TERMINATE);
	}
	/*
	 *	Make sure the object isn't already being terminated
	 */
	if (object->terminating) {
		vm_object_lock_assert_exclusive(object);
		object->ref_count--;
		assert(object->ref_count > 0);
		vm_object_unlock(object);
		return KERN_FAILURE;
	}

	/*
	 * Did somebody get a reference to the object while we were
	 * cleaning it?
	 */
	if (object->ref_count != 1) {
		vm_object_lock_assert_exclusive(object);
		object->ref_count--;
		assert(object->ref_count > 0);
		vm_object_unlock(object);
		return KERN_FAILURE;
	}

	/*
	 *	Make sure no one can look us up now.
	 */

	VM_OBJECT_SET_TERMINATING(object, TRUE);
	VM_OBJECT_SET_ALIVE(object, FALSE);

	if (!object->internal &&
	    object->cached_list.next &&
	    object->cached_list.prev) {
		vm_object_cache_remove(object);
	}

	/*
	 *	Detach the object from its shadow if we are the shadow's
	 *	copy. The reference we hold on the shadow must be dropped
	 *	by our caller.
	 */
	if (((shadow_object = object->shadow) != VM_OBJECT_NULL) &&
	    !(object->pageout)) {
		vm_object_lock(shadow_object);
		if (shadow_object->vo_copy == object) {
			VM_OBJECT_COPY_SET(shadow_object, VM_OBJECT_NULL);
		}
		vm_object_unlock(shadow_object);
	}

	if (object->paging_in_progress != 0 ||
	    object->activity_in_progress != 0) {
		/*
		 * There are still some paging_in_progress references
		 * on this object, meaning that there are some paging
		 * or other I/O operations in progress for this VM object.
		 * Such operations take some paging_in_progress references
		 * up front to ensure that the object doesn't go away, but
		 * they may also need to acquire a reference on the VM object,
		 * to map it in kernel space, for example.  That means that
		 * they may end up releasing the last reference on the VM
		 * object, triggering its termination, while still holding
		 * paging_in_progress references.  Waiting for these
		 * pending paging_in_progress references to go away here would
		 * deadlock.
		 *
		 * To avoid deadlocking, we'll let the vm_object_reaper_thread
		 * complete the VM object termination if it still holds
		 * paging_in_progress references at this point.
		 *
		 * No new paging_in_progress should appear now that the
		 * VM object is "terminating" and not "alive".
		 */
		vm_object_reap_async(object);
		vm_object_unlock(object);
		/*
		 * Return KERN_FAILURE to let the caller know that we
		 * haven't completed the termination and it can't drop this
		 * object's reference on its shadow object yet.
		 * The reaper thread will take care of that once it has
		 * completed this object's termination.
		 */
		return KERN_FAILURE;
	}
	/*
	 * complete the VM object termination
	 */
	vm_object_reap(object);
	object = VM_OBJECT_NULL;

	/*
	 * the object lock was released by vm_object_reap()
	 *
	 * KERN_SUCCESS means that this object has been terminated
	 * and no longer needs its shadow object but still holds a
	 * reference on it.
	 * The caller is responsible for dropping that reference.
	 * We can't call vm_object_deallocate() here because that
	 * would create a recursion.
	 */
	return KERN_SUCCESS;
}


/*
 * vm_object_reap():
 *
 * Complete the termination of a VM object after it's been marked
 * as "terminating" and "!alive" by vm_object_terminate().
 *
 * The VM object must be locked by caller.
 * The lock will be released on return and the VM object is no longer valid.
 */

void
vm_object_reap(
	vm_object_t object)
{
	memory_object_t         pager;

	vm_object_lock_assert_exclusive(object);
	assert(object->paging_in_progress == 0);
	assert(object->activity_in_progress == 0);

	vm_object_reap_count++;

	/*
	 * Disown this purgeable object to cleanup its owner's purgeable
	 * ledgers.  We need to do this before disconnecting the object
	 * from its pager, to properly account for compressed pages.
	 */
	if (/* object->internal && */
		(object->purgable != VM_PURGABLE_DENY ||
		object->vo_ledger_tag)) {
		int ledger_flags;
		kern_return_t kr;

		ledger_flags = 0;
		assert(!object->alive);
		assert(object->terminating);
		kr = vm_object_ownership_change(object,
		    VM_LEDGER_TAG_NONE,
		    NULL,                    /* no owner */
		    ledger_flags,
		    FALSE);                  /* task_objq not locked */
		assert(kr == KERN_SUCCESS);
		assert(object->vo_owner == NULL);
	}

#if DEVELOPMENT || DEBUG
	if (object->object_is_shared_cache &&
	    object->pager != NULL &&
	    object->pager->mo_pager_ops == &shared_region_pager_ops) {
		OSAddAtomic(-object->resident_page_count, &shared_region_pagers_resident_count);
	}
#endif /* DEVELOPMENT || DEBUG */

	pager = object->pager;
	object->pager = MEMORY_OBJECT_NULL;

	if (pager != MEMORY_OBJECT_NULL) {
		memory_object_control_disable(&object->pager_control);
	}

	object->ref_count--;
	assert(object->ref_count == 0);

	/*
	 * remove from purgeable queue if it's on
	 */
	if (object->internal) {
		assert(VM_OBJECT_OWNER(object) == TASK_NULL);

		VM_OBJECT_UNWIRED(object);

		if (object->purgable == VM_PURGABLE_DENY) {
			/* not purgeable: nothing to do */
		} else if (object->purgable == VM_PURGABLE_VOLATILE) {
			purgeable_q_t queue;

			queue = vm_purgeable_object_remove(object);
			assert(queue);

			if (object->purgeable_when_ripe) {
				/*
				 * Must take page lock for this -
				 * using it to protect token queue
				 */
				vm_page_lock_queues();
				vm_purgeable_token_delete_first(queue);

				assert(queue->debug_count_objects >= 0);
				vm_page_unlock_queues();
			}

			/*
			 * Update "vm_page_purgeable_count" in bulk and mark
			 * object as VM_PURGABLE_EMPTY to avoid updating
			 * "vm_page_purgeable_count" again in vm_page_remove()
			 * when reaping the pages.
			 */
			unsigned int delta;
			assert(object->resident_page_count >=
			    object->wired_page_count);
			delta = (object->resident_page_count -
			    object->wired_page_count);
			if (delta != 0) {
				assert(vm_page_purgeable_count >= delta);
				OSAddAtomic(-delta,
				    (SInt32 *)&vm_page_purgeable_count);
			}
			if (object->wired_page_count != 0) {
				assert(vm_page_purgeable_wired_count >=
				    object->wired_page_count);
				OSAddAtomic(-object->wired_page_count,
				    (SInt32 *)&vm_page_purgeable_wired_count);
			}
			VM_OBJECT_SET_PURGABLE(object, VM_PURGABLE_EMPTY);
		} else if (object->purgable == VM_PURGABLE_NONVOLATILE ||
		    object->purgable == VM_PURGABLE_EMPTY) {
			/* remove from nonvolatile queue */
			vm_purgeable_nonvolatile_dequeue(object);
		} else {
			panic("object %p in unexpected purgeable state 0x%x",
			    object, object->purgable);
		}
		if (object->transposed &&
		    object->cached_list.next != NULL &&
		    object->cached_list.prev == NULL) {
			/*
			 * object->cached_list.next "points" to the
			 * object that was transposed with this object.
			 */
		} else {
			assert(object->cached_list.next == NULL);
		}
		assert(object->cached_list.prev == NULL);
	}

	if (object->pageout) {
		/*
		 * free all remaining pages tabled on
		 * this object
		 * clean up it's shadow
		 */
		assert(object->shadow != VM_OBJECT_NULL);

		vm_pageout_object_terminate(object);
	} else if (object->resident_page_count) {
		/*
		 * free all remaining pages tabled on
		 * this object
		 */
		vm_object_reap_pages(object, REAP_REAP);
	}
	assert(vm_page_queue_empty(&object->memq));
	assert(object->paging_in_progress == 0);
	assert(object->activity_in_progress == 0);
	assert(object->ref_count == 0);

	/*
	 * If the pager has not already been released by
	 * vm_object_destroy, we need to terminate it and
	 * release our reference to it here.
	 */
	if (pager != MEMORY_OBJECT_NULL) {
		vm_object_unlock(object);
		vm_object_release_pager(pager);
		vm_object_lock(object);
	}

	/* kick off anyone waiting on terminating */
	VM_OBJECT_SET_TERMINATING(object, FALSE);
	vm_object_paging_begin(object);
	vm_object_paging_end(object);
	vm_object_unlock(object);

	object->shadow = VM_OBJECT_NULL;

#if VM_OBJECT_TRACKING
	if (vm_object_tracking_btlog) {
		btlog_erase(vm_object_tracking_btlog, object);
	}
#endif /* VM_OBJECT_TRACKING */

	vm_object_lock_destroy(object);
	/*
	 *	Free the space for the object.
	 */
	zfree(vm_object_zone, object);
	object = VM_OBJECT_NULL;
}


unsigned int vm_max_batch = 256;

#define V_O_R_MAX_BATCH 128

#define BATCH_LIMIT(max)        (vm_max_batch >= max ? max : vm_max_batch)

static inline vm_page_t
vm_object_reap_freelist(vm_page_t local_free_q, bool do_disconnect, bool set_cache_attr)
{
	if (local_free_q) {
		if (do_disconnect) {
			vm_page_t m;
			for (m = local_free_q;
			    m != VM_PAGE_NULL;
			    m = m->vmp_snext) {
				if (m->vmp_pmapped) {
					pmap_disconnect(VM_PAGE_GET_PHYS_PAGE(m));
				}
			}
		}
		if (set_cache_attr) {
			const unified_page_list_t pmap_batch_list = {
				.page_slist = local_free_q,
				.type = UNIFIED_PAGE_LIST_TYPE_VM_PAGE_LIST,
			};
			pmap_batch_set_cache_attributes(&pmap_batch_list, 0);
		}
		vm_page_free_list(local_free_q, TRUE);
	}
	return VM_PAGE_NULL;
}

void
vm_object_reap_pages(
	vm_object_t     object,
	int             reap_type)
{
	vm_page_t       p;
	vm_page_t       next;
	vm_page_t       local_free_q = VM_PAGE_NULL;
	int             loop_count;
	bool            disconnect_on_release;
	bool            set_cache_attr_needed;
	pmap_flush_context      pmap_flush_context_storage;

	if (reap_type == REAP_DATA_FLUSH) {
		/*
		 * We need to disconnect pages from all pmaps before
		 * releasing them to the free list
		 */
		disconnect_on_release = true;
	} else {
		/*
		 * Either the caller has already disconnected the pages
		 * from all pmaps, or we disconnect them here as we add
		 * them to out local list of pages to be released.
		 * No need to re-disconnect them when we release the pages
		 * to the free list.
		 */
		disconnect_on_release = false;
	}

restart_after_sleep:
	set_cache_attr_needed = false;
	if (object->set_cache_attr) {
		/**
		 * If the cache attributes need to be reset for the pages to
		 * be freed, we clear object->set_cache_attr here so that
		 * our call to vm_page_free_list (which will ultimately call
		 * vm_page_remove() on each page) won't try to reset the
		 * cache attributes on each page individually.  Depending on
		 * the architecture, it may be much faster for us to call
		 * pmap_batch_set_cache_attributes() instead.  Note that
		 * this function must restore object->set_cache_attr in any
		 * case where it is required to drop the object lock, e.g.
		 * to wait for a busy page.
		 */
		object->set_cache_attr = FALSE;
		set_cache_attr_needed = true;
	}

	if (vm_page_queue_empty(&object->memq)) {
		return;
	}
	loop_count = BATCH_LIMIT(V_O_R_MAX_BATCH);

	if (reap_type == REAP_PURGEABLE) {
		pmap_flush_context_init(&pmap_flush_context_storage);
	}

	vm_page_lock_queues();

	next = (vm_page_t)vm_page_queue_first(&object->memq);

	while (!vm_page_queue_end(&object->memq, (vm_page_queue_entry_t)next)) {
		p = next;
		next = (vm_page_t)vm_page_queue_next(&next->vmp_listq);

		if (--loop_count == 0) {
			vm_page_unlock_queues();

			if (local_free_q) {
				if (reap_type == REAP_PURGEABLE) {
					pmap_flush(&pmap_flush_context_storage);
					pmap_flush_context_init(&pmap_flush_context_storage);
				}
				/*
				 * Free the pages we reclaimed so far
				 * and take a little break to avoid
				 * hogging the page queue lock too long
				 */
				local_free_q = vm_object_reap_freelist(local_free_q,
				    disconnect_on_release, set_cache_attr_needed);
			} else {
				mutex_pause(0);
			}

			loop_count = BATCH_LIMIT(V_O_R_MAX_BATCH);

			vm_page_lock_queues();
		}
		if (reap_type == REAP_DATA_FLUSH || reap_type == REAP_TERMINATE) {
			if (p->vmp_busy || p->vmp_cleaning) {
				vm_page_unlock_queues();
				/*
				 * free the pages reclaimed so far
				 */
				local_free_q = vm_object_reap_freelist(local_free_q,
				    disconnect_on_release, set_cache_attr_needed);

				if (set_cache_attr_needed) {
					object->set_cache_attr = TRUE;
				}
				vm_page_sleep(object, p, THREAD_UNINT, LCK_SLEEP_DEFAULT);

				goto restart_after_sleep;
			}
			if (p->vmp_laundry) {
				vm_pageout_steal_laundry(p, TRUE);
			}
		}
		switch (reap_type) {
		case REAP_DATA_FLUSH:
			if (VM_PAGE_WIRED(p)) {
				/*
				 * this is an odd case... perhaps we should
				 * zero-fill this page since we're conceptually
				 * tossing its data at this point, but leaving
				 * it on the object to honor the 'wire' contract
				 */
				continue;
			}
			break;

		case REAP_PURGEABLE:
			if (VM_PAGE_WIRED(p)) {
				/*
				 * can't purge a wired page
				 */
				vm_page_purged_wired++;
				continue;
			}
			if (p->vmp_laundry && !p->vmp_busy && !p->vmp_cleaning) {
				vm_pageout_steal_laundry(p, TRUE);
			}

			if (p->vmp_cleaning || p->vmp_laundry || p->vmp_absent) {
				/*
				 * page is being acted upon,
				 * so don't mess with it
				 */
				vm_page_purged_others++;
				continue;
			}
			if (p->vmp_busy) {
				/*
				 * We can't reclaim a busy page but we can
				 * make it more likely to be paged (it's not wired) to make
				 * sure that it gets considered by
				 * vm_pageout_scan() later.
				 */
				if (VM_PAGE_PAGEABLE(p)) {
					vm_page_deactivate(p);
				}
				vm_page_purged_busy++;
				continue;
			}

			assert(!is_kernel_object(VM_PAGE_OBJECT(p)));

			/*
			 * we can discard this page...
			 */
			if (p->vmp_pmapped == TRUE) {
				/*
				 * unmap the page
				 */
				pmap_disconnect_options(VM_PAGE_GET_PHYS_PAGE(p), PMAP_OPTIONS_NOFLUSH | PMAP_OPTIONS_NOREFMOD, (void *)&pmap_flush_context_storage);
			}
			vm_page_purged_count++;

			break;

		case REAP_TERMINATE:
			if (p->vmp_absent || p->vmp_private) {
				/*
				 *	For private pages, VM_PAGE_FREE just
				 *	leaves the page structure around for
				 *	its owner to clean up.  For absent
				 *	pages, the structure is returned to
				 *	the appropriate pool.
				 */
				break;
			}
			if (p->vmp_fictitious) {
				assert(VM_PAGE_GET_PHYS_PAGE(p) == vm_page_guard_addr);
				break;
			}
			if (!p->vmp_dirty && p->vmp_wpmapped) {
				p->vmp_dirty = pmap_is_modified(VM_PAGE_GET_PHYS_PAGE(p));
			}

			if ((p->vmp_dirty || p->vmp_precious) && !VMP_ERROR_GET(p) && object->alive) {
				assert(!object->internal);

				p->vmp_free_when_done = TRUE;

				if (!p->vmp_laundry) {
					vm_page_queues_remove(p, TRUE);
					/*
					 * flush page... page will be freed
					 * upon completion of I/O
					 */
					vm_pageout_cluster(p);
				}
				vm_page_unlock_queues();
				/*
				 * free the pages reclaimed so far
				 */
				local_free_q = vm_object_reap_freelist(local_free_q,
				    disconnect_on_release, set_cache_attr_needed);

				if (set_cache_attr_needed) {
					object->set_cache_attr = TRUE;
				}
				vm_object_paging_wait(object, THREAD_UNINT);

				goto restart_after_sleep;
			}
			break;

		case REAP_REAP:
			break;
		}
		vm_page_free_prepare_queues(p);
		assert(p->vmp_pageq.next == 0 && p->vmp_pageq.prev == 0);
		/*
		 * Add this page to our list of reclaimed pages,
		 * to be freed later.
		 */
		p->vmp_snext = local_free_q;
		local_free_q = p;
	}
	vm_page_unlock_queues();

	/*
	 * Free the remaining reclaimed pages
	 */
	if (reap_type == REAP_PURGEABLE) {
		pmap_flush(&pmap_flush_context_storage);
	}

	vm_object_reap_freelist(local_free_q,
	    disconnect_on_release, set_cache_attr_needed);
	if (set_cache_attr_needed) {
		object->set_cache_attr = TRUE;
	}
}


void
vm_object_reap_async(
	vm_object_t     object)
{
	vm_object_lock_assert_exclusive(object);

	vm_object_reaper_lock_spin();

	vm_object_reap_count_async++;

	/* enqueue the VM object... */
	queue_enter(&vm_object_reaper_queue, object,
	    vm_object_t, cached_list);

	vm_object_reaper_unlock();

	/* ... and wake up the reaper thread */
	thread_wakeup((event_t) &vm_object_reaper_queue);
}


void
vm_object_reaper_thread(void)
{
	vm_object_t     object, shadow_object;

	vm_object_reaper_lock_spin();

	while (!queue_empty(&vm_object_reaper_queue)) {
		queue_remove_first(&vm_object_reaper_queue,
		    object,
		    vm_object_t,
		    cached_list);

		vm_object_reaper_unlock();
		vm_object_lock(object);

		assert(object->terminating);
		assert(!object->alive);

		/*
		 * The pageout daemon might be playing with our pages.
		 * Now that the object is dead, it won't touch any more
		 * pages, but some pages might already be on their way out.
		 * Hence, we wait until the active paging activities have
		 * ceased before we break the association with the pager
		 * itself.
		 */
		vm_object_paging_wait(object, THREAD_UNINT);

		shadow_object =
		    object->pageout ? VM_OBJECT_NULL : object->shadow;

		vm_object_reap(object);
		/* cache is unlocked and object is no longer valid */
		object = VM_OBJECT_NULL;

		if (shadow_object != VM_OBJECT_NULL) {
			/*
			 * Drop the reference "object" was holding on
			 * its shadow object.
			 */
			vm_object_deallocate(shadow_object);
			shadow_object = VM_OBJECT_NULL;
		}
		vm_object_reaper_lock_spin();
	}

	/* wait for more work... */
	assert_wait((event_t) &vm_object_reaper_queue, THREAD_UNINT);

	vm_object_reaper_unlock();

	thread_block((thread_continue_t) vm_object_reaper_thread);
	/*NOTREACHED*/
}

/*
 *	Routine:	vm_object_release_pager
 *	Purpose:	Terminate the pager and, upon completion,
 *			release our last reference to it.
 */
static void
vm_object_release_pager(
	memory_object_t pager)
{
	/*
	 *	Terminate the pager.
	 */

	(void) memory_object_terminate(pager);

	/*
	 *	Release reference to pager.
	 */
	memory_object_deallocate(pager);
}

/*
 *	Routine:	vm_object_destroy
 *	Purpose:
 *		Shut down a VM object, despite the
 *		presence of address map (or other) references
 *		to the vm_object.
 */
#if FBDP_DEBUG_OBJECT_NO_PAGER
extern uint32_t system_inshutdown;
int fbdp_no_panic = 1;
#endif /* FBDP_DEBUG_OBJECT_NO_PAGER */
kern_return_t
vm_object_destroy(
	vm_object_t                                     object,
	vm_object_destroy_reason_t   reason)
{
	memory_object_t         old_pager;

	if (object == VM_OBJECT_NULL) {
		return KERN_SUCCESS;
	}

	/*
	 *	Remove the pager association immediately.
	 *
	 *	This will prevent the memory manager from further
	 *	meddling.  [If it wanted to flush data or make
	 *	other changes, it should have done so before performing
	 *	the destroy call.]
	 */

	vm_object_lock(object);

#if FBDP_DEBUG_OBJECT_NO_PAGER
	static bool fbdp_no_panic_retrieved = false;
	if (!fbdp_no_panic_retrieved) {
		PE_parse_boot_argn("fbdp_no_panic4", &fbdp_no_panic, sizeof(fbdp_no_panic));
		fbdp_no_panic_retrieved = true;
	}

	bool forced_unmount = false;
	if (object->named &&
	    object->ref_count > 2 &&
	    object->pager != NULL &&
	    vnode_pager_get_forced_unmount(object->pager, &forced_unmount) == KERN_SUCCESS &&
	    forced_unmount == false) {
		if (!fbdp_no_panic) {
			panic("FBDP rdar://99829401 object %p refs %d pager %p (no forced unmount)\n", object, object->ref_count, object->pager);
		}
		DTRACE_VM3(vm_object_destroy_no_forced_unmount,
		    vm_object_t, object,
		    int, object->ref_count,
		    memory_object_t, object->pager);
	}

	if (object->fbdp_tracked) {
		if (object->ref_count > 2 && !system_inshutdown) {
			if (!fbdp_no_panic) {
				panic("FBDP/4 rdar://99829401 object %p refs %d pager %p (tracked)\n", object, object->ref_count, object->pager);
			}
		}
		VM_OBJECT_SET_FBDP_TRACKED(object, false);
	}
#endif /* FBDP_DEBUG_OBJECT_NO_PAGER */

	VM_OBJECT_SET_NO_PAGER_REASON(object, reason);

	VM_OBJECT_SET_CAN_PERSIST(object, FALSE);
	VM_OBJECT_SET_NAMED(object, FALSE);
#if 00
	VM_OBJECT_SET_ALIVE(object, FALSE);
#endif /* 00 */

#if DEVELOPMENT || DEBUG
	if (object->object_is_shared_cache &&
	    object->pager != NULL &&
	    object->pager->mo_pager_ops == &shared_region_pager_ops) {
		OSAddAtomic(-object->resident_page_count, &shared_region_pagers_resident_count);
	}
#endif /* DEVELOPMENT || DEBUG */

	old_pager = object->pager;
	object->pager = MEMORY_OBJECT_NULL;
	if (old_pager != MEMORY_OBJECT_NULL) {
		memory_object_control_disable(&object->pager_control);
	}

	/*
	 * Wait for the existing paging activity (that got
	 * through before we nulled out the pager) to subside.
	 */

	vm_object_paging_wait(object, THREAD_UNINT);
	vm_object_unlock(object);

	/*
	 *	Terminate the object now.
	 */
	if (old_pager != MEMORY_OBJECT_NULL) {
		vm_object_release_pager(old_pager);

		/*
		 * JMM - Release the caller's reference.  This assumes the
		 * caller had a reference to release, which is a big (but
		 * currently valid) assumption if this is driven from the
		 * vnode pager (it is holding a named reference when making
		 * this call)..
		 */
		vm_object_deallocate(object);
	}
	return KERN_SUCCESS;
}

/*
 * The "chunk" macros are used by routines below when looking for pages to deactivate.  These
 * exist because of the need to handle shadow chains.  When deactivating pages, we only
 * want to deactive the ones at the top most level in the object chain.  In order to do
 * this efficiently, the specified address range is divided up into "chunks" and we use
 * a bit map to keep track of which pages have already been processed as we descend down
 * the shadow chain.  These chunk macros hide the details of the bit map implementation
 * as much as we can.
 *
 * For convenience, we use a 64-bit data type as the bit map, and therefore a chunk is
 * set to 64 pages.  The bit map is indexed from the low-order end, so that the lowest
 * order bit represents page 0 in the current range and highest order bit represents
 * page 63.
 *
 * For further convenience, we also use negative logic for the page state in the bit map.
 * The bit is set to 1 to indicate it has not yet been seen, and to 0 to indicate it has
 * been processed.  This way we can simply test the 64-bit long word to see if it's zero
 * to easily tell if the whole range has been processed.  Therefore, the bit map starts
 * out with all the bits set.  The macros below hide all these details from the caller.
 */

#define PAGES_IN_A_CHUNK        64      /* The number of pages in the chunk must */
                                        /* be the same as the number of bits in  */
                                        /* the chunk_state_t type. We use 64     */
                                        /* just for convenience.		 */

#define CHUNK_SIZE      (PAGES_IN_A_CHUNK * PAGE_SIZE_64)       /* Size of a chunk in bytes */

typedef uint64_t        chunk_state_t;

/*
 * The bit map uses negative logic, so we start out with all 64 bits set to indicate
 * that no pages have been processed yet.  Also, if len is less than the full CHUNK_SIZE,
 * then we mark pages beyond the len as having been "processed" so that we don't waste time
 * looking at pages in that range.  This can save us from unnecessarily chasing down the
 * shadow chain.
 */

#define CHUNK_INIT(c, len)                                              \
	MACRO_BEGIN                                                     \
	uint64_t p;                                                     \
                                                                        \
	(c) = 0xffffffffffffffffLL;                                     \
                                                                        \
	for (p = (len) / PAGE_SIZE_64; p < PAGES_IN_A_CHUNK; p++)       \
	        MARK_PAGE_HANDLED(c, p);                                \
	MACRO_END


/*
 * Return true if all pages in the chunk have not yet been processed.
 */

#define CHUNK_NOT_COMPLETE(c)   ((c) != 0)

/*
 * Return true if the page at offset 'p' in the bit map has already been handled
 * while processing a higher level object in the shadow chain.
 */

#define PAGE_ALREADY_HANDLED(c, p)      (((c) & (1ULL << (p))) == 0)

/*
 * Mark the page at offset 'p' in the bit map as having been processed.
 */

#define MARK_PAGE_HANDLED(c, p) \
MACRO_BEGIN \
	(c) = (c) & ~(1ULL << (p)); \
MACRO_END


/*
 * Return true if the page at the given offset has been paged out.  Object is
 * locked upon entry and returned locked.
 *
 * NB: It is the callers responsibility to ensure that the offset in question
 * is not in the process of being paged in/out (i.e. not busy or no backing
 * page)
 */
static bool
page_is_paged_out(
	vm_object_t             object,
	vm_object_offset_t      offset)
{
	if (object->internal &&
	    object->alive &&
	    !object->terminating &&
	    object->pager_ready) {
		if (vm_object_compressor_pager_state_get(object, offset)
		    == VM_EXTERNAL_STATE_EXISTS) {
			return true;
		}
	}
	return false;
}



/*
 * madvise_free_debug
 *
 * To help debug madvise(MADV_FREE*) mis-usage, this triggers a
 * zero-fill as soon as a page is affected by a madvise(MADV_FREE*), to
 * simulate the loss of the page's contents as if the page had been
 * reclaimed and then re-faulted.
 */
#if DEVELOPMENT || DEBUG
int madvise_free_debug = 0;
int madvise_free_debug_sometimes = 1;
#else /* DEBUG */
int madvise_free_debug = 0;
int madvise_free_debug_sometimes = 0;
#endif /* DEBUG */
int madvise_free_counter = 0;

__options_decl(deactivate_flags_t, uint32_t, {
	DEACTIVATE_KILL         = 0x1,
	DEACTIVATE_REUSABLE     = 0x2,
	DEACTIVATE_ALL_REUSABLE = 0x4,
	DEACTIVATE_CLEAR_REFMOD = 0x8,
	DEACTIVATE_REUSABLE_NO_WRITE = 0x10
});

/*
 * Deactivate the pages in the specified object and range.  If kill_page is set, also discard any
 * page modified state from the pmap.  Update the chunk_state as we go along.  The caller must specify
 * a size that is less than or equal to the CHUNK_SIZE.
 */

static void
deactivate_pages_in_object(
	vm_object_t             object,
	vm_object_offset_t      offset,
	vm_object_size_t        size,
	deactivate_flags_t      flags,
	chunk_state_t           *chunk_state,
	pmap_flush_context      *pfc,
	struct pmap             *pmap,
	vm_map_offset_t         pmap_offset)
{
	vm_page_t       m;
	int             p;
	struct  vm_page_delayed_work    dw_array;
	struct  vm_page_delayed_work    *dwp, *dwp_start;
	bool            dwp_finish_ctx = TRUE;
	int             dw_count;
	int             dw_limit;
	unsigned int    reusable = 0;

	/*
	 * Examine each page in the chunk.  The variable 'p' is the page number relative to the start of the
	 * chunk.  Since this routine is called once for each level in the shadow chain, the chunk_state may
	 * have pages marked as having been processed already.  We stop the loop early if we find we've handled
	 * all the pages in the chunk.
	 */

	dwp_start = dwp = NULL;
	dw_count = 0;
	dw_limit = DELAYED_WORK_LIMIT(DEFAULT_DELAYED_WORK_LIMIT);
	dwp_start = vm_page_delayed_work_get_ctx();
	if (dwp_start == NULL) {
		dwp_start = &dw_array;
		dw_limit = 1;
		dwp_finish_ctx = FALSE;
	}

	dwp = dwp_start;

	for (p = 0; size && CHUNK_NOT_COMPLETE(*chunk_state); p++, size -= PAGE_SIZE_64, offset += PAGE_SIZE_64, pmap_offset += PAGE_SIZE_64) {
		/*
		 * If this offset has already been found and handled in a higher level object, then don't
		 * do anything with it in the current shadow object.
		 */

		if (PAGE_ALREADY_HANDLED(*chunk_state, p)) {
			continue;
		}

		/*
		 * See if the page at this offset is around.  First check to see if the page is resident,
		 * then if not, check the existence map or with the pager.
		 */

		if ((m = vm_page_lookup(object, offset)) != VM_PAGE_NULL) {
			/*
			 * We found a page we were looking for.  Mark it as "handled" now in the chunk_state
			 * so that we won't bother looking for a page at this offset again if there are more
			 * shadow objects.  Then deactivate the page.
			 */

			MARK_PAGE_HANDLED(*chunk_state, p);

			if ((!VM_PAGE_WIRED(m)) && (!m->vmp_private) && (!m->vmp_gobbled) && (!m->vmp_busy) &&
			    (!m->vmp_laundry) && (!m->vmp_cleaning) && !(m->vmp_free_when_done)) {
				int     clear_refmod_mask;
				int     pmap_options;
				dwp->dw_mask = 0;

				pmap_options = 0;
				clear_refmod_mask = VM_MEM_REFERENCED;
				dwp->dw_mask |= DW_clear_reference;

				if ((flags & DEACTIVATE_KILL) && (object->internal)) {
					if (!(flags & DEACTIVATE_REUSABLE_NO_WRITE) &&
					    (madvise_free_debug ||
					    (madvise_free_debug_sometimes &&
					    madvise_free_counter++ & 0x1))) {
						/*
						 * zero-fill the page (or every
						 * other page) now to simulate
						 * it being reclaimed and
						 * re-faulted.
						 */
#if CONFIG_TRACK_UNMODIFIED_ANON_PAGES
						if (!m->vmp_unmodified_ro) {
#else /* CONFIG_TRACK_UNMODIFIED_ANON_PAGES */
						if (true) {
#endif /* CONFIG_TRACK_UNMODIFIED_ANON_PAGES */
							pmap_zero_page(VM_PAGE_GET_PHYS_PAGE(m));
						}
					}
					m->vmp_precious = FALSE;
					m->vmp_dirty = FALSE;

					clear_refmod_mask |= VM_MEM_MODIFIED;
					if (m->vmp_q_state == VM_PAGE_ON_THROTTLED_Q) {
						/*
						 * This page is now clean and
						 * reclaimable.  Move it out
						 * of the throttled queue, so
						 * that vm_pageout_scan() can
						 * find it.
						 */
						dwp->dw_mask |= DW_move_page;
					}

#if 0
#if CONFIG_TRACK_UNMODIFIED_ANON_PAGES
					/*
					 * COMMENT BLOCK ON WHY THIS SHOULDN'T BE DONE.
					 *
					 * Since we are about to do a vm_object_compressor_pager_state_clr
					 * below for this page, which drops any existing compressor
					 * storage of this page (eg side-effect of a CoW operation or
					 * a collapse operation), it is tempting to think that we should
					 * treat this page as if it was just decompressed (during which
					 * we also drop existing compressor storage) and so start its life
					 * out with vmp_unmodified_ro set to FALSE.
					 *
					 * However, we can't do that here because we could swing around
					 * and re-access this page in a read-only fault.
					 * Clearing this bit means we'll try to zero it up above
					 * and fail.
					 *
					 * Note that clearing the bit is unnecessary regardless because
					 * dirty state has been cleared. During the next soft fault, the
					 * right state will be restored and things will progress just fine.
					 */
					if (m->vmp_unmodified_ro == true) {
						/* Need object and pageq locks for bit manipulation*/
						m->vmp_unmodified_ro = false;
						os_atomic_dec(&compressor_ro_uncompressed);
					}
#endif /* CONFIG_TRACK_UNMODIFIED_ANON_PAGES */
#endif /* 0 */
					vm_object_compressor_pager_state_clr(object, offset);

					if ((flags & DEACTIVATE_REUSABLE) && !m->vmp_reusable) {
						assert(!(flags & DEACTIVATE_ALL_REUSABLE));
						assert(!object->all_reusable);
						m->vmp_reusable = TRUE;
						object->reusable_page_count++;
						assert(object->resident_page_count >= object->reusable_page_count);
						reusable++;
						/*
						 * Tell pmap this page is now
						 * "reusable" (to update pmap
						 * stats for all mappings).
						 */
						pmap_options |= PMAP_OPTIONS_SET_REUSABLE;
					}
				}
				if (flags & DEACTIVATE_CLEAR_REFMOD) {
					/*
					 * The caller didn't clear the refmod bits in advance.
					 * Clear them for this page now.
					 */
					pmap_options |= PMAP_OPTIONS_NOFLUSH;
					pmap_clear_refmod_options(VM_PAGE_GET_PHYS_PAGE(m),
					    clear_refmod_mask,
					    pmap_options,
					    (void *)pfc);
				}

				if ((m->vmp_q_state != VM_PAGE_ON_THROTTLED_Q) &&
				    !(flags & (DEACTIVATE_REUSABLE | DEACTIVATE_ALL_REUSABLE))) {
					dwp->dw_mask |= DW_move_page;
				}

				if (dwp->dw_mask) {
					VM_PAGE_ADD_DELAYED_WORK(dwp, m,
					    dw_count);
				}

				if (dw_count >= dw_limit) {
					if (reusable) {
						OSAddAtomic(reusable,
						    &vm_page_stats_reusable.reusable_count);
						vm_page_stats_reusable.reusable += reusable;
						reusable = 0;
					}
					vm_page_do_delayed_work(object, VM_KERN_MEMORY_NONE, dwp_start, dw_count);

					dwp = dwp_start;
					dw_count = 0;
				}
			}
		} else {
			/*
			 * The page at this offset isn't memory resident, check to see if it's
			 * been paged out.  If so, mark it as handled so we don't bother looking
			 * for it in the shadow chain.
			 */

			if (page_is_paged_out(object, offset)) {
				MARK_PAGE_HANDLED(*chunk_state, p);

				/*
				 * If we're killing a non-resident page, then clear the page in the existence
				 * map so we don't bother paging it back in if it's touched again in the future.
				 */

				if ((flags & DEACTIVATE_KILL) && (object->internal)) {
					vm_object_compressor_pager_state_clr(object, offset);

					if (pmap != PMAP_NULL) {
						/*
						 * Tell pmap that this page
						 * is no longer mapped, to
						 * adjust the footprint ledger
						 * because this page is no
						 * longer compressed.
						 */
						pmap_remove_options(
							pmap,
							pmap_offset,
							(pmap_offset +
							PAGE_SIZE),
							PMAP_OPTIONS_REMOVE);
					}
				}
			}
		}
	}

	if (reusable) {
		OSAddAtomic(reusable, &vm_page_stats_reusable.reusable_count);
		vm_page_stats_reusable.reusable += reusable;
		reusable = 0;
	}

	if (dw_count) {
		vm_page_do_delayed_work(object, VM_KERN_MEMORY_NONE, dwp_start, dw_count);
		dwp = dwp_start;
		dw_count = 0;
	}

	if (dwp_start && dwp_finish_ctx) {
		vm_page_delayed_work_finish_ctx(dwp_start);
		dwp_start = dwp = NULL;
	}
}


/*
 * Deactive a "chunk" of the given range of the object starting at offset.  A "chunk"
 * will always be less than or equal to the given size.  The total range is divided up
 * into chunks for efficiency and performance related to the locks and handling the shadow
 * chain.  This routine returns how much of the given "size" it actually processed.  It's
 * up to the caler to loop and keep calling this routine until the entire range they want
 * to process has been done.
 * Iff clear_refmod is true, pmap_clear_refmod_options is called for each physical page in this range.
 */

static vm_object_size_t
deactivate_a_chunk(
	vm_object_t             orig_object,
	vm_object_offset_t      offset,
	vm_object_size_t        size,
	deactivate_flags_t      flags,
	pmap_flush_context      *pfc,
	struct pmap             *pmap,
	vm_map_offset_t         pmap_offset)
{
	vm_object_t             object;
	vm_object_t             tmp_object;
	vm_object_size_t        length;
	chunk_state_t           chunk_state;


	/*
	 * Get set to do a chunk.  We'll do up to CHUNK_SIZE, but no more than the
	 * remaining size the caller asked for.
	 */

	length = MIN(size, CHUNK_SIZE);

	/*
	 * The chunk_state keeps track of which pages we've already processed if there's
	 * a shadow chain on this object.  At this point, we haven't done anything with this
	 * range of pages yet, so initialize the state to indicate no pages processed yet.
	 */

	CHUNK_INIT(chunk_state, length);
	object = orig_object;

	/*
	 * Start at the top level object and iterate around the loop once for each object
	 * in the shadow chain.  We stop processing early if we've already found all the pages
	 * in the range.  Otherwise we stop when we run out of shadow objects.
	 */

	while (object && CHUNK_NOT_COMPLETE(chunk_state)) {
		vm_object_paging_begin(object);

		deactivate_pages_in_object(object, offset, length, flags, &chunk_state, pfc, pmap, pmap_offset);

		vm_object_paging_end(object);

		/*
		 * We've finished with this object, see if there's a shadow object.  If
		 * there is, update the offset and lock the new object.  We also turn off
		 * kill_page at this point since we only kill pages in the top most object.
		 */

		tmp_object = object->shadow;

		if (tmp_object) {
			assert(!(flags & DEACTIVATE_KILL) || (flags & DEACTIVATE_CLEAR_REFMOD));
			flags &= ~(DEACTIVATE_KILL | DEACTIVATE_REUSABLE | DEACTIVATE_ALL_REUSABLE);
			offset += object->vo_shadow_offset;
			vm_object_lock(tmp_object);
		}

		if (object != orig_object) {
			vm_object_unlock(object);
		}

		object = tmp_object;
	}

	if (object && object != orig_object) {
		vm_object_unlock(object);
	}

	return length;
}



/*
 * Move any resident pages in the specified range to the inactive queue.  If kill_page is set,
 * we also clear the modified status of the page and "forget" any changes that have been made
 * to the page.
 */

__private_extern__ void
vm_object_deactivate_pages(
	vm_object_t             object,
	vm_object_offset_t      offset,
	vm_object_size_t        size,
	boolean_t               kill_page,
	boolean_t               reusable_page,
	boolean_t               reusable_no_write,
	struct pmap             *pmap,
	vm_map_offset_t         pmap_offset)
{
	vm_object_size_t        length;
	boolean_t               all_reusable;
	pmap_flush_context      pmap_flush_context_storage;
	unsigned int pmap_clear_refmod_mask = VM_MEM_REFERENCED;
	unsigned int pmap_clear_refmod_options = 0;
	deactivate_flags_t flags = DEACTIVATE_CLEAR_REFMOD;
	bool refmod_cleared = false;
	if (kill_page) {
		flags |= DEACTIVATE_KILL;
	}
	if (reusable_page) {
		flags |= DEACTIVATE_REUSABLE;
	}
	if (reusable_no_write) {
		flags |= DEACTIVATE_REUSABLE_NO_WRITE;
	}

	/*
	 * We break the range up into chunks and do one chunk at a time.  This is for
	 * efficiency and performance while handling the shadow chains and the locks.
	 * The deactivate_a_chunk() function returns how much of the range it processed.
	 * We keep calling this routine until the given size is exhausted.
	 */


	all_reusable = FALSE;
#if 11
	/*
	 * For the sake of accurate "reusable" pmap stats, we need
	 * to tell pmap about each page that is no longer "reusable",
	 * so we can't do the "all_reusable" optimization.
	 *
	 * If we do go with the all_reusable optimization, we can't
	 * return if size is 0 since we could have "all_reusable == TRUE"
	 * In this case, we save the overhead of doing the pmap_flush_context
	 * work.
	 */
	if (size == 0) {
		return;
	}
#else
	if (reusable_page &&
	    object->internal &&
	    object->vo_size != 0 &&
	    object->vo_size == size &&
	    object->reusable_page_count == 0) {
		all_reusable = TRUE;
		reusable_page = FALSE;
		flags |= DEACTIVATE_ALL_REUSABLE;
	}
#endif

	if ((reusable_page || all_reusable) && object->all_reusable) {
		/* This means MADV_FREE_REUSABLE has been called twice, which
		 * is probably illegal. */
		return;
	}


	pmap_flush_context_init(&pmap_flush_context_storage);

	/*
	 * If we're deactivating multiple pages, try to perform one bulk pmap operation.
	 * We can't do this if we're killing pages and there's a shadow chain as
	 * we don't yet know which pages are in the top object (pages in shadow copies aren't
	 * safe to kill).
	 * And we can only do this on hardware that supports it.
	 */
	if (size > PAGE_SIZE && (!kill_page || !object->shadow)) {
		if (kill_page && object->internal) {
			pmap_clear_refmod_mask |= VM_MEM_MODIFIED;
		}
		if (reusable_page) {
			pmap_clear_refmod_options |= PMAP_OPTIONS_SET_REUSABLE;
		}

		refmod_cleared = pmap_clear_refmod_range_options(pmap, pmap_offset, pmap_offset + size, pmap_clear_refmod_mask, pmap_clear_refmod_options);
		if (refmod_cleared) {
			// We were able to clear all the refmod bits. So deactivate_a_chunk doesn't need to do it.
			flags &= ~DEACTIVATE_CLEAR_REFMOD;
		}
	}

	while (size) {
		length = deactivate_a_chunk(object, offset, size, flags,
		    &pmap_flush_context_storage, pmap, pmap_offset);

		size -= length;
		offset += length;
		pmap_offset += length;
	}
	pmap_flush(&pmap_flush_context_storage);

	if (all_reusable) {
		if (!object->all_reusable) {
			unsigned int reusable;

			object->all_reusable = TRUE;
			assert(object->reusable_page_count == 0);
			/* update global stats */
			reusable = object->resident_page_count;
			OSAddAtomic(reusable,
			    &vm_page_stats_reusable.reusable_count);
			vm_page_stats_reusable.reusable += reusable;
			vm_page_stats_reusable.all_reusable_calls++;
		}
	} else if (reusable_page) {
		vm_page_stats_reusable.partial_reusable_calls++;
	}
}

void
vm_object_reuse_pages(
	vm_object_t             object,
	vm_object_offset_t      start_offset,
	vm_object_offset_t      end_offset,
	boolean_t               allow_partial_reuse)
{
	vm_object_offset_t      cur_offset;
	vm_page_t               m;
	unsigned int            reused, reusable;

#define VM_OBJECT_REUSE_PAGE(object, m, reused)                         \
	MACRO_BEGIN                                                     \
	        if ((m) != VM_PAGE_NULL &&                              \
	            (m)->vmp_reusable) {                                \
	                assert((object)->reusable_page_count <=         \
	                       (object)->resident_page_count);          \
	                assert((object)->reusable_page_count > 0);      \
	                (object)->reusable_page_count--;                \
	                (m)->vmp_reusable = FALSE;                      \
	                (reused)++;                                     \
	/* \
	 * Tell pmap that this page is no longer \
	 * "reusable", to update the "reusable" stats \
	 * for all the pmaps that have mapped this \
	 * page. \
	 */                                                             \
	                pmap_clear_refmod_options(VM_PAGE_GET_PHYS_PAGE((m)), \
	                                          0, /* refmod */       \
	                                          (PMAP_OPTIONS_CLEAR_REUSABLE \
	                                           | PMAP_OPTIONS_NOFLUSH), \
	                                          NULL);                \
	        }                                                       \
	MACRO_END

	reused = 0;
	reusable = 0;

	vm_object_lock_assert_exclusive(object);

	if (object->all_reusable) {
		panic("object %p all_reusable: can't update pmap stats",
		    object);
		assert(object->reusable_page_count == 0);
		object->all_reusable = FALSE;
		if (end_offset - start_offset == object->vo_size ||
		    !allow_partial_reuse) {
			vm_page_stats_reusable.all_reuse_calls++;
			reused = object->resident_page_count;
		} else {
			vm_page_stats_reusable.partial_reuse_calls++;
			vm_page_queue_iterate(&object->memq, m, vmp_listq) {
				if (m->vmp_offset < start_offset ||
				    m->vmp_offset >= end_offset) {
					m->vmp_reusable = TRUE;
					object->reusable_page_count++;
					assert(object->resident_page_count >= object->reusable_page_count);
					continue;
				} else {
					assert(!m->vmp_reusable);
					reused++;
				}
			}
		}
	} else if (object->resident_page_count >
	    ((end_offset - start_offset) >> PAGE_SHIFT)) {
		vm_page_stats_reusable.partial_reuse_calls++;
		for (cur_offset = start_offset;
		    cur_offset < end_offset;
		    cur_offset += PAGE_SIZE_64) {
			if (object->reusable_page_count == 0) {
				break;
			}
			m = vm_page_lookup(object, cur_offset);
			VM_OBJECT_REUSE_PAGE(object, m, reused);
		}
	} else {
		vm_page_stats_reusable.partial_reuse_calls++;
		vm_page_queue_iterate(&object->memq, m, vmp_listq) {
			if (object->reusable_page_count == 0) {
				break;
			}
			if (m->vmp_offset < start_offset ||
			    m->vmp_offset >= end_offset) {
				continue;
			}
			VM_OBJECT_REUSE_PAGE(object, m, reused);
		}
	}

	/* update global stats */
	OSAddAtomic(reusable - reused, &vm_page_stats_reusable.reusable_count);
	vm_page_stats_reusable.reused += reused;
	vm_page_stats_reusable.reusable += reusable;
}

/*
 * This function determines if the zero operation can be run on the
 * object. The checks on the entry have already been performed by
 * vm_map_zero_entry_preflight.
 */
static kern_return_t
vm_object_zero_preflight(
	vm_object_t                     object,
	vm_object_offset_t              start,
	vm_object_offset_t              end)
{
	/*
	 * Zeroing is further restricted to anonymous memory.
	 */
	if (!object->internal) {
		return KERN_PROTECTION_FAILURE;
	}

	/*
	 * Zeroing for copy on write isn't yet supported
	 */
	if (object->shadow != NULL ||
	    object->vo_copy != NULL) {
		return KERN_NO_ACCESS;
	}

	/*
	 * Ensure the that bounds makes sense wrt the object
	 */
	if (end - start > object->vo_size) {
		return KERN_INVALID_ADDRESS;
	}

	if (object->terminating || !object->alive) {
		return KERN_ABORTED;
	}

	return KERN_SUCCESS;
}

static void
vm_object_zero_page(vm_page_t m)
{
	if (m != VM_PAGE_NULL) {
		ppnum_t phy_page_num = VM_PAGE_GET_PHYS_PAGE(m);

		/*
		 * Skip fictitious guard pages
		 */
		if (m->vmp_fictitious) {
			assert(phy_page_num == vm_page_guard_addr);
			return;
		}
		pmap_zero_page(phy_page_num);
	}
}

/*
 * This function iterates the range of pages specified in the object and
 * discards the ones that are compressed and zeroes the ones that are wired.
 * This function may drop the object lock while waiting for a page that is
 * busy and will restart the operation for the specific offset.
 */
kern_return_t
vm_object_zero(
	vm_object_t                     object,
	vm_object_offset_t              cur_offset,
	vm_object_offset_t              end_offset)
{
	kern_return_t ret;

	vm_object_lock_assert_exclusive(object);
	ret = vm_object_zero_preflight(object, cur_offset, end_offset);
	if (ret != KERN_SUCCESS) {
		return ret;
	}

	while (cur_offset < end_offset) {
		vm_page_t m = vm_page_lookup(object, cur_offset);

		if (m != VM_PAGE_NULL && m->vmp_busy) {
			vm_page_sleep(object, m, THREAD_UNINT, LCK_SLEEP_DEFAULT);
			/* Object lock was dropped -- reverify validity */
			ret = vm_object_zero_preflight(object, cur_offset, end_offset);
			if (ret != KERN_SUCCESS) {
				return ret;
			}
			continue;
		}

		/*
		 * If the compressor has the page then just discard it instead
		 * of faulting it in and zeroing it else zero the page if it exists. If
		 * we dropped the object lock during the lookup retry the lookup for the
		 * cur_offset.
		 */
		if (page_is_paged_out(object, cur_offset)) {
			vm_object_compressor_pager_state_clr(object, cur_offset);
		} else {
			vm_object_zero_page(m);
		}
		cur_offset += PAGE_SIZE_64;
		/*
		 * TODO: May need a vm_object_lock_yield_shared in this loop if it takes
		 * too long, as holding the object lock for too long can stall pageout
		 * scan (or other users of the object)
		 */
	}

	return KERN_SUCCESS;
}

/*
 *	Routine:	vm_object_pmap_protect
 *
 *	Purpose:
 *		Reduces the permission for all physical
 *		pages in the specified object range.
 *
 *		If removing write permission only, it is
 *		sufficient to protect only the pages in
 *		the top-level object; only those pages may
 *		have write permission.
 *
 *		If removing all access, we must follow the
 *		shadow chain from the top-level object to
 *		remove access to all pages in shadowed objects.
 *
 *		The object must *not* be locked.  The object must
 *		be internal.
 *
 *              If pmap is not NULL, this routine assumes that
 *              the only mappings for the pages are in that
 *              pmap.
 */

__private_extern__ void
vm_object_pmap_protect(
	vm_object_t                     object,
	vm_object_offset_t              offset,
	vm_object_size_t                size,
	pmap_t                          pmap,
	vm_map_size_t                   pmap_page_size,
	vm_map_offset_t                 pmap_start,
	vm_prot_t                       prot)
{
	vm_object_pmap_protect_options(object, offset, size, pmap,
	    pmap_page_size,
	    pmap_start, prot, 0);
}

__private_extern__ void
vm_object_pmap_protect_options(
	vm_object_t                     object,
	vm_object_offset_t              offset,
	vm_object_size_t                size,
	pmap_t                          pmap,
	vm_map_size_t                   pmap_page_size,
	vm_map_offset_t                 pmap_start,
	vm_prot_t                       prot,
	int                             options)
{
	pmap_flush_context      pmap_flush_context_storage;
	boolean_t               delayed_pmap_flush = FALSE;
	vm_object_offset_t      offset_in_object;
	vm_object_size_t        size_in_object;

	if (object == VM_OBJECT_NULL) {
		return;
	}
	if (pmap_page_size > PAGE_SIZE) {
		/* for 16K map on 4K device... */
		pmap_page_size = PAGE_SIZE;
	}
	/*
	 * If we decide to work on the object itself, extend the range to
	 * cover a full number of native pages.
	 */
	size_in_object = vm_object_round_page(offset + size) - vm_object_trunc_page(offset);
	offset_in_object = vm_object_trunc_page(offset);
	/*
	 * If we decide to work on the pmap, use the exact range specified,
	 * so no rounding/truncating offset and size.  They should already
	 * be aligned to pmap_page_size.
	 */
	assertf(!(offset & (pmap_page_size - 1)) && !(size & (pmap_page_size - 1)),
	    "offset 0x%llx size 0x%llx pmap_page_size 0x%llx",
	    offset, size, (uint64_t)pmap_page_size);

	vm_object_lock(object);

	if (object->phys_contiguous) {
		if (pmap != NULL) {
			vm_object_unlock(object);
			pmap_protect_options(pmap,
			    pmap_start,
			    pmap_start + size,
			    prot,
			    options & ~PMAP_OPTIONS_NOFLUSH,
			    NULL);
		} else {
			vm_object_offset_t phys_start, phys_end, phys_addr;

			phys_start = object->vo_shadow_offset + offset_in_object;
			phys_end = phys_start + size_in_object;
			assert(phys_start <= phys_end);
			assert(phys_end <= object->vo_shadow_offset + object->vo_size);
			vm_object_unlock(object);

			pmap_flush_context_init(&pmap_flush_context_storage);
			delayed_pmap_flush = FALSE;

			for (phys_addr = phys_start;
			    phys_addr < phys_end;
			    phys_addr += PAGE_SIZE_64) {
				pmap_page_protect_options(
					(ppnum_t) (phys_addr >> PAGE_SHIFT),
					prot,
					options | PMAP_OPTIONS_NOFLUSH,
					(void *)&pmap_flush_context_storage);
				delayed_pmap_flush = TRUE;
			}
			if (delayed_pmap_flush == TRUE) {
				pmap_flush(&pmap_flush_context_storage);
			}
		}
		return;
	}

	assert(object->internal);

	while (TRUE) {
		if (ptoa_64(object->resident_page_count) > size_in_object / 2 && pmap != PMAP_NULL) {
			vm_object_unlock(object);
			if (pmap_page_size < PAGE_SIZE) {
				DEBUG4K_PMAP("pmap %p start 0x%llx end 0x%llx prot 0x%x: pmap_protect()\n", pmap, (uint64_t)pmap_start, pmap_start + size, prot);
			}
			pmap_protect_options(pmap, pmap_start, pmap_start + size, prot,
			    options & ~PMAP_OPTIONS_NOFLUSH, NULL);
			return;
		}

		if (pmap_page_size < PAGE_SIZE) {
			DEBUG4K_PMAP("pmap %p start 0x%llx end 0x%llx prot 0x%x: offset 0x%llx size 0x%llx object %p offset 0x%llx size 0x%llx\n", pmap, (uint64_t)pmap_start, pmap_start + size, prot, offset, size, object, offset_in_object, size_in_object);
		}

		pmap_flush_context_init(&pmap_flush_context_storage);
		delayed_pmap_flush = FALSE;

		/*
		 * if we are doing large ranges with respect to resident
		 * page count then we should interate over pages otherwise
		 * inverse page look-up will be faster
		 */
		if (ptoa_64(object->resident_page_count / 4) < size_in_object) {
			vm_page_t               p;
			vm_object_offset_t      end;

			end = offset_in_object + size_in_object;

			vm_page_queue_iterate(&object->memq, p, vmp_listq) {
				if (!p->vmp_fictitious && (offset_in_object <= p->vmp_offset) && (p->vmp_offset < end)) {
					vm_map_offset_t start;

					/*
					 * XXX FBDP 4K: intentionally using "offset" here instead
					 * of "offset_in_object", since "start" is a pmap address.
					 */
					start = pmap_start + p->vmp_offset - offset;

					if (pmap != PMAP_NULL) {
						vm_map_offset_t curr;
						for (curr = start;
						    curr < start + PAGE_SIZE_64;
						    curr += pmap_page_size) {
							if (curr < pmap_start) {
								continue;
							}
							if (curr >= pmap_start + size) {
								break;
							}
							pmap_protect_options(
								pmap,
								curr,
								curr + pmap_page_size,
								prot,
								options | PMAP_OPTIONS_NOFLUSH,
								&pmap_flush_context_storage);
						}
					} else {
						pmap_page_protect_options(
							VM_PAGE_GET_PHYS_PAGE(p),
							prot,
							options | PMAP_OPTIONS_NOFLUSH,
							&pmap_flush_context_storage);
					}
					delayed_pmap_flush = TRUE;
				}
			}
		} else {
			vm_page_t               p;
			vm_object_offset_t      end;
			vm_object_offset_t      target_off;

			end = offset_in_object + size_in_object;

			for (target_off = offset_in_object;
			    target_off < end; target_off += PAGE_SIZE) {
				p = vm_page_lookup(object, target_off);

				if (p != VM_PAGE_NULL) {
					vm_object_offset_t start;

					/*
					 * XXX FBDP 4K: intentionally using "offset" here instead
					 * of "offset_in_object", since "start" is a pmap address.
					 */
					start = pmap_start + (p->vmp_offset - offset);

					if (pmap != PMAP_NULL) {
						vm_map_offset_t curr;
						for (curr = start;
						    curr < start + PAGE_SIZE;
						    curr += pmap_page_size) {
							if (curr < pmap_start) {
								continue;
							}
							if (curr >= pmap_start + size) {
								break;
							}
							pmap_protect_options(
								pmap,
								curr,
								curr + pmap_page_size,
								prot,
								options | PMAP_OPTIONS_NOFLUSH,
								&pmap_flush_context_storage);
						}
					} else {
						pmap_page_protect_options(
							VM_PAGE_GET_PHYS_PAGE(p),
							prot,
							options | PMAP_OPTIONS_NOFLUSH,
							&pmap_flush_context_storage);
					}
					delayed_pmap_flush = TRUE;
				}
			}
		}
		if (delayed_pmap_flush == TRUE) {
			pmap_flush(&pmap_flush_context_storage);
		}

		if (prot == VM_PROT_NONE) {
			/*
			 * Must follow shadow chain to remove access
			 * to pages in shadowed objects.
			 */
			vm_object_t     next_object;

			next_object = object->shadow;
			if (next_object != VM_OBJECT_NULL) {
				offset_in_object += object->vo_shadow_offset;
				offset += object->vo_shadow_offset;
				vm_object_lock(next_object);
				vm_object_unlock(object);
				object = next_object;
			} else {
				/*
				 * End of chain - we are done.
				 */
				break;
			}
		} else {
			/*
			 * Pages in shadowed objects may never have
			 * write permission - we may stop here.
			 */
			break;
		}
	}

	vm_object_unlock(object);
}

uint32_t vm_page_busy_absent_skipped = 0;

/*
 *	Routine:	vm_object_copy_slowly
 *
 *	Description:
 *		Copy the specified range of the source
 *		virtual memory object without using
 *		protection-based optimizations (such
 *		as copy-on-write).  The pages in the
 *		region are actually copied.
 *
 *	In/out conditions:
 *		The caller must hold a reference and a lock
 *		for the source virtual memory object.  The source
 *		object will be returned *unlocked*.
 *
 *	Results:
 *		If the copy is completed successfully, KERN_SUCCESS is
 *		returned.  If the caller asserted the interruptible
 *		argument, and an interruption occurred while waiting
 *		for a user-generated event, MACH_SEND_INTERRUPTED is
 *		returned.  Other values may be returned to indicate
 *		hard errors during the copy operation.
 *
 *		A new virtual memory object is returned in a
 *		parameter (_result_object).  The contents of this
 *		new object, starting at a zero offset, are a copy
 *		of the source memory region.  In the event of
 *		an error, this parameter will contain the value
 *		VM_OBJECT_NULL.
 */
__private_extern__ kern_return_t
vm_object_copy_slowly(
	vm_object_t             src_object,
	vm_object_offset_t      src_offset,
	vm_object_size_t        size,
	boolean_t               interruptible,
	vm_object_t             *_result_object)        /* OUT */
{
	vm_object_t             new_object;
	vm_object_offset_t      new_offset;

	struct vm_object_fault_info fault_info = {};

	if (size == 0) {
		vm_object_unlock(src_object);
		*_result_object = VM_OBJECT_NULL;
		return KERN_INVALID_ARGUMENT;
	}

	/*
	 *	Prevent destruction of the source object while we copy.
	 */

	vm_object_reference_locked(src_object);
	vm_object_unlock(src_object);

	/*
	 *	Create a new object to hold the copied pages.
	 *	A few notes:
	 *		We fill the new object starting at offset 0,
	 *		 regardless of the input offset.
	 *		We don't bother to lock the new object within
	 *		 this routine, since we have the only reference.
	 */

	size = vm_object_round_page(src_offset + size) - vm_object_trunc_page(src_offset);
	src_offset = vm_object_trunc_page(src_offset);
	new_object = vm_object_allocate(size);
	new_offset = 0;
	if (src_object->copy_strategy == MEMORY_OBJECT_COPY_NONE &&
	    src_object->vo_inherit_copy_none) {
		new_object->copy_strategy = MEMORY_OBJECT_COPY_NONE;
		new_object->vo_inherit_copy_none = true;
	}

	assert(size == trunc_page_64(size));    /* Will the loop terminate? */

	fault_info.interruptible = interruptible;
	fault_info.behavior  = VM_BEHAVIOR_SEQUENTIAL;
	fault_info.lo_offset = src_offset;
	fault_info.hi_offset = src_offset + size;
	fault_info.stealth = TRUE;

	for (;
	    size != 0;
	    src_offset += PAGE_SIZE_64,
	    new_offset += PAGE_SIZE_64, size -= PAGE_SIZE_64
	    ) {
		vm_page_t       new_page;
		vm_fault_return_t result;

		vm_object_lock(new_object);

		while ((new_page = vm_page_alloc(new_object, new_offset))
		    == VM_PAGE_NULL) {
			vm_object_unlock(new_object);

			if (!vm_page_wait(interruptible)) {
				vm_object_deallocate(new_object);
				vm_object_deallocate(src_object);
				*_result_object = VM_OBJECT_NULL;
				return MACH_SEND_INTERRUPTED;
			}
			vm_object_lock(new_object);
		}
		vm_object_unlock(new_object);

		do {
			vm_prot_t       prot = VM_PROT_READ;
			vm_page_t       _result_page;
			vm_page_t       top_page;
			vm_page_t       result_page;
			kern_return_t   error_code;
			vm_object_t     result_page_object;


			vm_object_lock(src_object);

			if (src_object->internal &&
			    src_object->shadow == VM_OBJECT_NULL &&
			    (src_object->pager == NULL ||
			    (vm_object_compressor_pager_state_get(src_object,
			    src_offset) ==
			    VM_EXTERNAL_STATE_ABSENT))) {
				boolean_t can_skip_page;

				_result_page = vm_page_lookup(src_object,
				    src_offset);
				if (_result_page == VM_PAGE_NULL) {
					/*
					 * This page is neither resident nor
					 * compressed and there's no shadow
					 * object below "src_object", so this
					 * page is really missing.
					 * There's no need to zero-fill it just
					 * to copy it:  let's leave it missing
					 * in "new_object" and get zero-filled
					 * on demand.
					 */
					can_skip_page = TRUE;
				} else if (workaround_41447923 &&
				    src_object->pager == NULL &&
				    _result_page != VM_PAGE_NULL &&
				    _result_page->vmp_busy &&
				    _result_page->vmp_absent &&
				    src_object->purgable == VM_PURGABLE_DENY &&
				    !src_object->blocked_access) {
					/*
					 * This page is "busy" and "absent"
					 * but not because we're waiting for
					 * it to be decompressed.  It must
					 * be because it's a "no zero fill"
					 * page that is currently not
					 * accessible until it gets overwritten
					 * by a device driver.
					 * Since its initial state would have
					 * been "zero-filled", let's leave the
					 * copy page missing and get zero-filled
					 * on demand.
					 */
					assert(src_object->internal);
					assert(src_object->shadow == NULL);
					assert(src_object->pager == NULL);
					can_skip_page = TRUE;
					vm_page_busy_absent_skipped++;
				} else {
					can_skip_page = FALSE;
				}
				if (can_skip_page) {
					vm_object_unlock(src_object);
					/* free the unused "new_page"... */
					vm_object_lock(new_object);
					VM_PAGE_FREE(new_page);
					new_page = VM_PAGE_NULL;
					vm_object_unlock(new_object);
					/* ...and go to next page in "src_object" */
					result = VM_FAULT_SUCCESS;
					break;
				}
			}

			vm_object_paging_begin(src_object);

			/* cap size at maximum UPL size */
			upl_size_t cluster_size;
			if (os_convert_overflow(size, &cluster_size)) {
				cluster_size = 0 - (upl_size_t)PAGE_SIZE;
			}
			fault_info.cluster_size = cluster_size;

			_result_page = VM_PAGE_NULL;
			result = vm_fault_page(src_object, src_offset,
			    VM_PROT_READ, FALSE,
			    FALSE,     /* page not looked up */
			    &prot, &_result_page, &top_page,
			    (int *)0,
			    &error_code, FALSE, &fault_info);

			switch (result) {
			case VM_FAULT_SUCCESS:
				result_page = _result_page;
				result_page_object = VM_PAGE_OBJECT(result_page);

				/*
				 *	Copy the page to the new object.
				 *
				 *	POLICY DECISION:
				 *		If result_page is clean,
				 *		we could steal it instead
				 *		of copying.
				 */

				vm_page_copy(result_page, new_page);
				vm_object_unlock(result_page_object);

				/*
				 *	Let go of both pages (make them
				 *	not busy, perform wakeup, activate).
				 */
				vm_object_lock(new_object);
				SET_PAGE_DIRTY(new_page, FALSE);
				vm_page_wakeup_done(new_object, new_page);
				vm_object_unlock(new_object);

				vm_object_lock(result_page_object);
				vm_page_wakeup_done(result_page_object, result_page);

				vm_page_lockspin_queues();
				if ((result_page->vmp_q_state == VM_PAGE_ON_SPECULATIVE_Q) ||
				    (result_page->vmp_q_state == VM_PAGE_NOT_ON_Q)) {
					vm_page_activate(result_page);
				}
				vm_page_activate(new_page);
				vm_page_unlock_queues();

				/*
				 *	Release paging references and
				 *	top-level placeholder page, if any.
				 */

				vm_fault_cleanup(result_page_object,
				    top_page);

				break;

			case VM_FAULT_RETRY:
				break;

			case VM_FAULT_MEMORY_SHORTAGE:
				if (vm_page_wait(interruptible)) {
					break;
				}
				ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_FAULT_OBJCOPYSLOWLY_MEMORY_SHORTAGE), 0 /* arg */);
				OS_FALLTHROUGH;

			case VM_FAULT_INTERRUPTED:
				vm_object_lock(new_object);
				VM_PAGE_FREE(new_page);
				vm_object_unlock(new_object);

				vm_object_deallocate(new_object);
				vm_object_deallocate(src_object);
				*_result_object = VM_OBJECT_NULL;
				return MACH_SEND_INTERRUPTED;

			case VM_FAULT_SUCCESS_NO_VM_PAGE:
				/* success but no VM page: fail */
				vm_object_paging_end(src_object);
				vm_object_unlock(src_object);
				OS_FALLTHROUGH;
			case VM_FAULT_MEMORY_ERROR:
				/*
				 * A policy choice:
				 *	(a) ignore pages that we can't
				 *	    copy
				 *	(b) return the null object if
				 *	    any page fails [chosen]
				 */

				vm_object_lock(new_object);
				VM_PAGE_FREE(new_page);
				vm_object_unlock(new_object);

				vm_object_deallocate(new_object);
				vm_object_deallocate(src_object);
				*_result_object = VM_OBJECT_NULL;
				return error_code ? error_code:
				       KERN_MEMORY_ERROR;

			default:
				panic("vm_object_copy_slowly: unexpected error"
				    " 0x%x from vm_fault_page()\n", result);
			}
		} while (result != VM_FAULT_SUCCESS);
	}

	/*
	 *	Lose the extra reference, and return our object.
	 */
	vm_object_deallocate(src_object);
	*_result_object = new_object;
	return KERN_SUCCESS;
}

/*
 *	Routine:	vm_object_copy_quickly
 *
 *	Purpose:
 *		Copy the specified range of the source virtual
 *		memory object, if it can be done without waiting
 *		for user-generated events.
 *
 *	Results:
 *		If the copy is successful, the copy is returned in
 *		the arguments; otherwise, the arguments are not
 *		affected.
 *
 *	In/out conditions:
 *		The object should be unlocked on entry and exit.
 */

/*ARGSUSED*/
__private_extern__ boolean_t
vm_object_copy_quickly(
	vm_object_t             object,               /* IN */
	__unused vm_object_offset_t     offset, /* IN */
	__unused vm_object_size_t       size,   /* IN */
	boolean_t               *_src_needs_copy,       /* OUT */
	boolean_t               *_dst_needs_copy)       /* OUT */
{
	memory_object_copy_strategy_t copy_strategy;

	if (object == VM_OBJECT_NULL) {
		*_src_needs_copy = FALSE;
		*_dst_needs_copy = FALSE;
		return TRUE;
	}

	vm_object_lock(object);

	copy_strategy = object->copy_strategy;

	switch (copy_strategy) {
	case MEMORY_OBJECT_COPY_SYMMETRIC:

		/*
		 *	Symmetric copy strategy.
		 *	Make another reference to the object.
		 *	Leave object/offset unchanged.
		 */

		vm_object_reference_locked(object);
		VM_OBJECT_SET_SHADOWED(object, TRUE);
		vm_object_unlock(object);

		/*
		 *	Both source and destination must make
		 *	shadows, and the source must be made
		 *	read-only if not already.
		 */

		*_src_needs_copy = TRUE;
		*_dst_needs_copy = TRUE;

		break;

	case MEMORY_OBJECT_COPY_DELAY:
		vm_object_unlock(object);
		return FALSE;

	default:
		vm_object_unlock(object);
		return FALSE;
	}
	return TRUE;
}

static uint32_t copy_delayed_lock_collisions;
static uint32_t copy_delayed_max_collisions;
static uint32_t copy_delayed_lock_contention;
static uint32_t copy_delayed_protect_iterate;

/*
 *	Routine:	vm_object_copy_delayed [internal]
 *
 *	Description:
 *		Copy the specified virtual memory object, using
 *		the asymmetric copy-on-write algorithm.
 *
 *	In/out conditions:
 *		The src_object must be locked on entry.  It will be unlocked
 *		on exit - so the caller must also hold a reference to it.
 *
 *		This routine will not block waiting for user-generated
 *		events.  It is not interruptible.
 */
__private_extern__ vm_object_t
vm_object_copy_delayed(
	vm_object_t             src_object,
	vm_object_offset_t      src_offset,
	vm_object_size_t        size,
	boolean_t               src_object_shared)
{
	vm_object_t             new_copy = VM_OBJECT_NULL;
	vm_object_t             old_copy;
	vm_page_t               p;
	vm_object_size_t        copy_size = src_offset + size;
	pmap_flush_context      pmap_flush_context_storage;
	boolean_t               delayed_pmap_flush = FALSE;


	uint32_t collisions = 0;
	/*
	 *	The user-level memory manager wants to see all of the changes
	 *	to this object, but it has promised not to make any changes on
	 *	its own.
	 *
	 *	Perform an asymmetric copy-on-write, as follows:
	 *		Create a new object, called a "copy object" to hold
	 *		 pages modified by the new mapping  (i.e., the copy,
	 *		 not the original mapping).
	 *		Record the original object as the backing object for
	 *		 the copy object.  If the original mapping does not
	 *		 change a page, it may be used read-only by the copy.
	 *		Record the copy object in the original object.
	 *		 When the original mapping causes a page to be modified,
	 *		 it must be copied to a new page that is "pushed" to
	 *		 the copy object.
	 *		Mark the new mapping (the copy object) copy-on-write.
	 *		 This makes the copy object itself read-only, allowing
	 *		 it to be reused if the original mapping makes no
	 *		 changes, and simplifying the synchronization required
	 *		 in the "push" operation described above.
	 *
	 *	The copy-on-write is said to be assymetric because the original
	 *	object is *not* marked copy-on-write. A copied page is pushed
	 *	to the copy object, regardless which party attempted to modify
	 *	the page.
	 *
	 *	Repeated asymmetric copy operations may be done. If the
	 *	original object has not been changed since the last copy, its
	 *	copy object can be reused. Otherwise, a new copy object can be
	 *	inserted between the original object and its previous copy
	 *	object.  Since any copy object is read-only, this cannot affect
	 *	affect the contents of the previous copy object.
	 *
	 *	Note that a copy object is higher in the object tree than the
	 *	original object; therefore, use of the copy object recorded in
	 *	the original object must be done carefully, to avoid deadlock.
	 */

	copy_size = vm_object_round_page(copy_size);
Retry:

	/*
	 * Wait for paging in progress.
	 */
	if (!src_object->true_share &&
	    (src_object->paging_in_progress != 0 ||
	    src_object->activity_in_progress != 0)) {
		if (src_object_shared == TRUE) {
			vm_object_unlock(src_object);
			vm_object_lock(src_object);
			src_object_shared = FALSE;
			goto Retry;
		}
		vm_object_paging_wait(src_object, THREAD_UNINT);
	}
	/*
	 *	See whether we can reuse the result of a previous
	 *	copy operation.
	 */

	old_copy = src_object->vo_copy;
	if (old_copy != VM_OBJECT_NULL) {
		int lock_granted;

		/*
		 *	Try to get the locks (out of order)
		 */
		if (src_object_shared == TRUE) {
			lock_granted = vm_object_lock_try_shared(old_copy);
		} else {
			lock_granted = vm_object_lock_try(old_copy);
		}

		if (!lock_granted) {
			vm_object_unlock(src_object);

			if (collisions++ == 0) {
				copy_delayed_lock_contention++;
			}
			mutex_pause(collisions);

			/* Heisenberg Rules */
			copy_delayed_lock_collisions++;

			if (collisions > copy_delayed_max_collisions) {
				copy_delayed_max_collisions = collisions;
			}

			if (src_object_shared == TRUE) {
				vm_object_lock_shared(src_object);
			} else {
				vm_object_lock(src_object);
			}

			goto Retry;
		}

		/*
		 *	Determine whether the old copy object has
		 *	been modified.
		 */

		if (old_copy->resident_page_count == 0 &&
		    !old_copy->pager_created) {
			/*
			 *	It has not been modified.
			 *
			 *	Return another reference to
			 *	the existing copy-object if
			 *	we can safely grow it (if
			 *	needed).
			 */

			if (old_copy->vo_size < copy_size) {
				if (src_object_shared == TRUE) {
					vm_object_unlock(old_copy);
					vm_object_unlock(src_object);

					vm_object_lock(src_object);
					src_object_shared = FALSE;
					goto Retry;
				}
				/*
				 * We can't perform a delayed copy if any of the
				 * pages in the extended range are wired (because
				 * we can't safely take write permission away from
				 * wired pages).  If the pages aren't wired, then
				 * go ahead and protect them.
				 */
				copy_delayed_protect_iterate++;

				pmap_flush_context_init(&pmap_flush_context_storage);
				delayed_pmap_flush = FALSE;

				vm_page_queue_iterate(&src_object->memq, p, vmp_listq) {
					if (!p->vmp_fictitious &&
					    p->vmp_offset >= old_copy->vo_size &&
					    p->vmp_offset < copy_size) {
						if (VM_PAGE_WIRED(p)) {
							vm_object_unlock(old_copy);
							vm_object_unlock(src_object);

							if (new_copy != VM_OBJECT_NULL) {
								vm_object_unlock(new_copy);
								vm_object_deallocate(new_copy);
							}
							if (delayed_pmap_flush == TRUE) {
								pmap_flush(&pmap_flush_context_storage);
							}

							return VM_OBJECT_NULL;
						} else {
							pmap_page_protect_options(VM_PAGE_GET_PHYS_PAGE(p),
							    (p->vmp_xpmapped ? (VM_PROT_READ | VM_PROT_EXECUTE) : VM_PROT_READ),
							    PMAP_OPTIONS_NOFLUSH, (void *)&pmap_flush_context_storage);
							delayed_pmap_flush = TRUE;
						}
					}
				}
				if (delayed_pmap_flush == TRUE) {
					pmap_flush(&pmap_flush_context_storage);
				}

				assertf(page_aligned(copy_size),
				    "object %p size 0x%llx",
				    old_copy, (uint64_t)copy_size);
				old_copy->vo_size = copy_size;

				/*
				 * src_object's "vo_copy" object now covers
				 * a larger portion of src_object.
				 * Increment src_object's "vo_copy_version"
				 * to make any racing vm_fault() on
				 * "src_object" re-check if it needs to honor
				 * any new copy-on-write obligation.
				 */
				src_object->vo_copy_version++;
			}
			if (src_object_shared == TRUE) {
				vm_object_reference_shared(old_copy);
			} else {
				vm_object_reference_locked(old_copy);
			}
			vm_object_unlock(old_copy);
			vm_object_unlock(src_object);

			if (new_copy != VM_OBJECT_NULL) {
				vm_object_unlock(new_copy);
				vm_object_deallocate(new_copy);
			}
			return old_copy;
		}



		/*
		 * Adjust the size argument so that the newly-created
		 * copy object will be large enough to back either the
		 * old copy object or the new mapping.
		 */
		if (old_copy->vo_size > copy_size) {
			copy_size = old_copy->vo_size;
		}

		if (new_copy == VM_OBJECT_NULL) {
			vm_object_unlock(old_copy);
			vm_object_unlock(src_object);
			new_copy = vm_object_allocate(copy_size);
			vm_object_lock(src_object);
			vm_object_lock(new_copy);

			src_object_shared = FALSE;
			goto Retry;
		}
		assertf(page_aligned(copy_size),
		    "object %p size 0x%llx",
		    new_copy, (uint64_t)copy_size);
		new_copy->vo_size = copy_size;

		/*
		 *	The copy-object is always made large enough to
		 *	completely shadow the original object, since
		 *	it may have several users who want to shadow
		 *	the original object at different points.
		 */

		assert((old_copy->shadow == src_object) &&
		    (old_copy->vo_shadow_offset == (vm_object_offset_t) 0));
	} else if (new_copy == VM_OBJECT_NULL) {
		vm_object_unlock(src_object);
		new_copy = vm_object_allocate(copy_size);
		vm_object_lock(src_object);
		vm_object_lock(new_copy);

		src_object_shared = FALSE;
		goto Retry;
	}

	/*
	 * We now have the src object locked, and the new copy object
	 * allocated and locked (and potentially the old copy locked).
	 * Before we go any further, make sure we can still perform
	 * a delayed copy, as the situation may have changed.
	 *
	 * Specifically, we can't perform a delayed copy if any of the
	 * pages in the range are wired (because we can't safely take
	 * write permission away from wired pages).  If the pages aren't
	 * wired, then go ahead and protect them.
	 */
	copy_delayed_protect_iterate++;

	pmap_flush_context_init(&pmap_flush_context_storage);
	delayed_pmap_flush = FALSE;

	vm_page_queue_iterate(&src_object->memq, p, vmp_listq) {
		if (!p->vmp_fictitious && p->vmp_offset < copy_size) {
			if (VM_PAGE_WIRED(p)) {
				if (old_copy) {
					vm_object_unlock(old_copy);
				}
				vm_object_unlock(src_object);
				vm_object_unlock(new_copy);
				vm_object_deallocate(new_copy);

				if (delayed_pmap_flush == TRUE) {
					pmap_flush(&pmap_flush_context_storage);
				}

				return VM_OBJECT_NULL;
			} else {
				pmap_page_protect_options(VM_PAGE_GET_PHYS_PAGE(p),
				    (p->vmp_xpmapped ? (VM_PROT_READ | VM_PROT_EXECUTE) : VM_PROT_READ),
				    PMAP_OPTIONS_NOFLUSH, (void *)&pmap_flush_context_storage);
				delayed_pmap_flush = TRUE;
			}
		}
	}
	if (delayed_pmap_flush == TRUE) {
		pmap_flush(&pmap_flush_context_storage);
	}

	if (old_copy != VM_OBJECT_NULL) {
		/*
		 *	Make the old copy-object shadow the new one.
		 *	It will receive no more pages from the original
		 *	object.
		 */

		/* remove ref. from old_copy */
		vm_object_lock_assert_exclusive(src_object);
		src_object->ref_count--;
		assert(src_object->ref_count > 0);
		vm_object_lock_assert_exclusive(old_copy);
		old_copy->shadow = new_copy;
		vm_object_lock_assert_exclusive(new_copy);
		assert(new_copy->ref_count > 0);
		new_copy->ref_count++;          /* for old_copy->shadow ref. */

		vm_object_unlock(old_copy);     /* done with old_copy */
	}

	/*
	 *	Point the new copy at the existing object.
	 */
	vm_object_lock_assert_exclusive(new_copy);
	new_copy->shadow = src_object;
	new_copy->vo_shadow_offset = 0;
	VM_OBJECT_SET_SHADOWED(new_copy, TRUE);      /* caller must set needs_copy */

	vm_object_lock_assert_exclusive(src_object);
	vm_object_reference_locked(src_object);
	VM_OBJECT_COPY_SET(src_object, new_copy);
	vm_object_unlock(src_object);
	vm_object_unlock(new_copy);

	return new_copy;
}

/*
 *	Routine:	vm_object_copy_strategically
 *
 *	Purpose:
 *		Perform a copy according to the source object's
 *		declared strategy.  This operation may block,
 *		and may be interrupted.
 */
__private_extern__ kern_return_t
vm_object_copy_strategically(
	vm_object_t             src_object,
	vm_object_offset_t      src_offset,
	vm_object_size_t        size,
	bool                    forking,
	vm_object_t             *dst_object,    /* OUT */
	vm_object_offset_t      *dst_offset,    /* OUT */
	boolean_t               *dst_needs_copy) /* OUT */
{
	boolean_t       result;
	boolean_t       interruptible = THREAD_ABORTSAFE; /* XXX */
	boolean_t       object_lock_shared = FALSE;
	memory_object_copy_strategy_t copy_strategy;

	assert(src_object != VM_OBJECT_NULL);

	copy_strategy = src_object->copy_strategy;

	if (copy_strategy == MEMORY_OBJECT_COPY_DELAY) {
		vm_object_lock_shared(src_object);
		object_lock_shared = TRUE;
	} else {
		vm_object_lock(src_object);
	}

	/*
	 *	The copy strategy is only valid if the memory manager
	 *	is "ready". Internal objects are always ready.
	 */

	while (!src_object->internal && !src_object->pager_ready) {
		wait_result_t wait_result;

		if (object_lock_shared == TRUE) {
			vm_object_unlock(src_object);
			vm_object_lock(src_object);
			object_lock_shared = FALSE;
			continue;
		}
		wait_result = vm_object_sleep(  src_object,
		    VM_OBJECT_EVENT_PAGER_READY,
		    interruptible, LCK_SLEEP_EXCLUSIVE);
		if (wait_result != THREAD_AWAKENED) {
			vm_object_unlock(src_object);
			*dst_object = VM_OBJECT_NULL;
			*dst_offset = 0;
			*dst_needs_copy = FALSE;
			return MACH_SEND_INTERRUPTED;
		}
	}

	/*
	 *	Use the appropriate copy strategy.
	 */

	if (copy_strategy == MEMORY_OBJECT_COPY_DELAY_FORK) {
		if (forking) {
			copy_strategy = MEMORY_OBJECT_COPY_DELAY;
		} else {
			copy_strategy = MEMORY_OBJECT_COPY_NONE;
			if (object_lock_shared) {
				vm_object_unlock(src_object);
				vm_object_lock(src_object);
				object_lock_shared = FALSE;
			}
		}
	}

	switch (copy_strategy) {
	case MEMORY_OBJECT_COPY_DELAY:
		*dst_object = vm_object_copy_delayed(src_object,
		    src_offset, size, object_lock_shared);
		if (*dst_object != VM_OBJECT_NULL) {
			*dst_offset = src_offset;
			*dst_needs_copy = TRUE;
			result = KERN_SUCCESS;
			break;
		}
		vm_object_lock(src_object);
		OS_FALLTHROUGH; /* fall thru when delayed copy not allowed */

	case MEMORY_OBJECT_COPY_NONE:
		result = vm_object_copy_slowly(src_object, src_offset, size,
		    interruptible, dst_object);
		if (result == KERN_SUCCESS) {
			*dst_offset = src_offset - vm_object_trunc_page(src_offset);
			*dst_needs_copy = FALSE;
		}
		break;

	case MEMORY_OBJECT_COPY_SYMMETRIC:
		vm_object_unlock(src_object);
		result = KERN_MEMORY_RESTART_COPY;
		break;

	default:
		panic("copy_strategically: bad strategy %d for object %p",
		    copy_strategy, src_object);
		result = KERN_INVALID_ARGUMENT;
	}
	return result;
}

/*
 *	vm_object_shadow:
 *
 *	Create a new object which is backed by the
 *	specified existing object range.  The source
 *	object reference is deallocated.
 *
 *	The new object and offset into that object
 *	are returned in the source parameters.
 */
boolean_t vm_object_shadow_check = TRUE;
uint64_t vm_object_shadow_forced = 0;
uint64_t vm_object_shadow_skipped = 0;

__private_extern__ boolean_t
vm_object_shadow(
	vm_object_t             *object,        /* IN/OUT */
	vm_object_offset_t      *offset,        /* IN/OUT */
	vm_object_size_t        length,
	boolean_t               always_shadow)
{
	vm_object_t     source;
	vm_object_t     result;

	source = *object;
	assert(source != VM_OBJECT_NULL);
	if (source == VM_OBJECT_NULL) {
		return FALSE;
	}

	assert(source->copy_strategy == MEMORY_OBJECT_COPY_SYMMETRIC);

	/*
	 *	Determine if we really need a shadow.
	 *
	 *	If the source object is larger than what we are trying
	 *	to create, then force the shadow creation even if the
	 *	ref count is 1.  This will allow us to [potentially]
	 *	collapse the underlying object away in the future
	 *	(freeing up the extra data it might contain and that
	 *	we don't need).
	 */

	assert(source->copy_strategy != MEMORY_OBJECT_COPY_NONE); /* Purgeable objects shouldn't have shadow objects. */

	/*
	 * The following optimization does not work in the context of submaps
	 * (the shared region, in particular).
	 * This object might have only 1 reference (in the submap) but that
	 * submap can itself be mapped multiple times, so the object is
	 * actually indirectly referenced more than once...
	 * The caller can specify to "always_shadow" to bypass the optimization.
	 */
	if (vm_object_shadow_check &&
	    source->vo_size == length &&
	    source->ref_count == 1) {
		if (always_shadow) {
			vm_object_shadow_forced++;
		} else {
			/*
			 * Lock the object and check again.
			 * We also check to see if there's
			 * a shadow or copy object involved.
			 * We can't do that earlier because
			 * without the object locked, there
			 * could be a collapse and the chain
			 * gets modified leaving us with an
			 * invalid pointer.
			 */
			vm_object_lock(source);
			if (source->vo_size == length &&
			    source->ref_count == 1 &&
			    (source->shadow == VM_OBJECT_NULL ||
			    source->shadow->vo_copy == VM_OBJECT_NULL)) {
				VM_OBJECT_SET_SHADOWED(source, FALSE);
				vm_object_unlock(source);
				vm_object_shadow_skipped++;
				return FALSE;
			}
			/* things changed while we were locking "source"... */
			vm_object_unlock(source);
		}
	}

	/*
	 * *offset is the map entry's offset into the VM object and
	 * is aligned to the map's page size.
	 * VM objects need to be aligned to the system's page size.
	 * Record the necessary adjustment and re-align the offset so
	 * that result->vo_shadow_offset is properly page-aligned.
	 */
	vm_object_offset_t offset_adjustment;
	offset_adjustment = *offset - vm_object_trunc_page(*offset);
	length = vm_object_round_page(length + offset_adjustment);
	*offset = vm_object_trunc_page(*offset);

	/*
	 *	Allocate a new object with the given length
	 */

	if ((result = vm_object_allocate(length)) == VM_OBJECT_NULL) {
		panic("vm_object_shadow: no object for shadowing");
	}

	/*
	 *	The new object shadows the source object, adding
	 *	a reference to it.  Our caller changes his reference
	 *	to point to the new object, removing a reference to
	 *	the source object.  Net result: no change of reference
	 *	count.
	 */
	result->shadow = source;

	/*
	 *	Store the offset into the source object,
	 *	and fix up the offset into the new object.
	 */

	result->vo_shadow_offset = *offset;
	assertf(page_aligned(result->vo_shadow_offset),
	    "result %p shadow offset 0x%llx",
	    result, result->vo_shadow_offset);

	/*
	 *	Return the new things
	 */

	*offset = 0;
	if (offset_adjustment) {
		/*
		 * Make the map entry point to the equivalent offset
		 * in the new object.
		 */
		DEBUG4K_COPY("adjusting offset @ %p from 0x%llx to 0x%llx for object %p length: 0x%llx\n", offset, *offset, *offset + offset_adjustment, result, length);
		*offset += offset_adjustment;
	}
	*object = result;
	return TRUE;
}

/*
 *	The relationship between vm_object structures and
 *	the memory_object requires careful synchronization.
 *
 *	All associations are created by memory_object_create_named
 *  for external pagers and vm_object_compressor_pager_create for internal
 *  objects as follows:
 *
 *		pager:	the memory_object itself, supplied by
 *			the user requesting a mapping (or the kernel,
 *			when initializing internal objects); the
 *			kernel simulates holding send rights by keeping
 *			a port reference;
 *
 *		pager_request:
 *			the memory object control port,
 *			created by the kernel; the kernel holds
 *			receive (and ownership) rights to this
 *			port, but no other references.
 *
 *	When initialization is complete, the "initialized" field
 *	is asserted.  Other mappings using a particular memory object,
 *	and any references to the vm_object gained through the
 *	port association must wait for this initialization to occur.
 *
 *	In order to allow the memory manager to set attributes before
 *	requests (notably virtual copy operations, but also data or
 *	unlock requests) are made, a "ready" attribute is made available.
 *	Only the memory manager may affect the value of this attribute.
 *	Its value does not affect critical kernel functions, such as
 *	internal object initialization or destruction.  [Furthermore,
 *	memory objects created by the kernel are assumed to be ready
 *	immediately; the default memory manager need not explicitly
 *	set the "ready" attribute.]
 *
 *	[Both the "initialized" and "ready" attribute wait conditions
 *	use the "pager" field as the wait event.]
 *
 *	The port associations can be broken down by any of the
 *	following routines:
 *		vm_object_terminate:
 *			No references to the vm_object remain, and
 *			the object cannot (or will not) be cached.
 *			This is the normal case, and is done even
 *			though one of the other cases has already been
 *			done.
 *		memory_object_destroy:
 *			The memory manager has requested that the
 *			kernel relinquish references to the memory
 *			object. [The memory manager may not want to
 *			destroy the memory object, but may wish to
 *			refuse or tear down existing memory mappings.]
 *
 *	Each routine that breaks an association must break all of
 *	them at once.  At some later time, that routine must clear
 *	the pager field and release the memory object references.
 *	[Furthermore, each routine must cope with the simultaneous
 *	or previous operations of the others.]
 *
 *	Because the pager field may be cleared spontaneously, it
 *	cannot be used to determine whether a memory object has
 *	ever been associated with a particular vm_object.  [This
 *	knowledge is important to the shadow object mechanism.]
 *	For this reason, an additional "created" attribute is
 *	provided.
 *
 *	During various paging operations, the pager reference found in the
 *	vm_object must be valid.  To prevent this from being released,
 *	(other than being removed, i.e., made null), routines may use
 *	the vm_object_paging_begin/end routines [actually, macros].
 *	The implementation uses the "paging_in_progress" and "wanted" fields.
 *	[Operations that alter the validity of the pager values include the
 *	termination routines and vm_object_collapse.]
 */


/*
 *	Routine:	vm_object_memory_object_associate
 *	Purpose:
 *		Associate a VM object to the given pager.
 *		If a VM object is not provided, create one.
 *		Initialize the pager.
 */
vm_object_t
vm_object_memory_object_associate(
	memory_object_t         pager,
	vm_object_t             object,
	vm_object_size_t        size,
	boolean_t               named)
{
	memory_object_control_t control;

	assert(pager != MEMORY_OBJECT_NULL);

	if (object != VM_OBJECT_NULL) {
		vm_object_lock(object);
		assert(object->internal);
		assert(object->pager_created);
		assert(!object->pager_initialized);
		assert(!object->pager_ready);
		assert(object->pager_trusted);
	} else {
		object = vm_object_allocate(size);
		assert(object != VM_OBJECT_NULL);
		vm_object_lock(object);
		VM_OBJECT_SET_INTERNAL(object, FALSE);
		VM_OBJECT_SET_PAGER_TRUSTED(object, FALSE);
		/* copy strategy invalid until set by memory manager */
		object->copy_strategy = MEMORY_OBJECT_COPY_INVALID;
	}

	/*
	 *	Allocate request port.
	 */

	control = memory_object_control_allocate(object);
	assert(control != MEMORY_OBJECT_CONTROL_NULL);

	assert(!object->pager_ready);
	assert(!object->pager_initialized);
	assert(object->pager == NULL);
	assert(object->pager_control == NULL);

	/*
	 *	Copy the reference we were given.
	 */

	memory_object_reference(pager);
	VM_OBJECT_SET_PAGER_CREATED(object, TRUE);
	object->pager = pager;
	object->pager_control = control;
	VM_OBJECT_SET_PAGER_READY(object, FALSE);

	vm_object_unlock(object);

	/*
	 *	Let the pager know we're using it.
	 */

	(void) memory_object_init(pager,
	    object->pager_control,
	    PAGE_SIZE);

	vm_object_lock(object);
	if (named) {
		VM_OBJECT_SET_NAMED(object, TRUE);
	}
	if (object->internal) {
		VM_OBJECT_SET_PAGER_READY(object, TRUE);
		vm_object_wakeup(object, VM_OBJECT_EVENT_PAGER_READY);
	}

	VM_OBJECT_SET_PAGER_INITIALIZED(object, TRUE);
	vm_object_wakeup(object, VM_OBJECT_EVENT_PAGER_INIT);

	vm_object_unlock(object);

	return object;
}

/*
 *	Routine:	vm_object_compressor_pager_create
 *	Purpose:
 *		Create a memory object for an internal object.
 *	In/out conditions:
 *		The object is locked on entry and exit;
 *		it may be unlocked within this call.
 *	Limitations:
 *		Only one thread may be performing a
 *		vm_object_compressor_pager_create on an object at
 *		a time.  Presumably, only the pageout
 *		daemon will be using this routine.
 */

void
vm_object_compressor_pager_create(
	vm_object_t     object)
{
	memory_object_t         pager;
	vm_object_t             pager_object = VM_OBJECT_NULL;

	assert(!is_kernel_object(object));

	/*
	 *	Prevent collapse or termination by holding a paging reference
	 */

	vm_object_paging_begin(object);
	if (object->pager_created) {
		/*
		 *	Someone else got to it first...
		 *	wait for them to finish initializing the ports
		 */
		while (!object->pager_initialized) {
			vm_object_sleep(object,
			    VM_OBJECT_EVENT_PAGER_INIT,
			    THREAD_UNINT, LCK_SLEEP_EXCLUSIVE);
		}
		vm_object_paging_end(object);
		return;
	}

	if ((uint32_t) (object->vo_size / PAGE_SIZE) !=
	    (object->vo_size / PAGE_SIZE)) {
#if DEVELOPMENT || DEBUG
		printf("vm_object_compressor_pager_create(%p): "
		    "object size 0x%llx >= 0x%llx\n",
		    object,
		    (uint64_t) object->vo_size,
		    0x0FFFFFFFFULL * PAGE_SIZE);
#endif /* DEVELOPMENT || DEBUG */
		vm_object_paging_end(object);
		return;
	}

	/*
	 *	Indicate that a memory object has been assigned
	 *	before dropping the lock, to prevent a race.
	 */

	VM_OBJECT_SET_PAGER_CREATED(object, TRUE);
	VM_OBJECT_SET_PAGER_TRUSTED(object, TRUE);
	object->paging_offset = 0;

	vm_object_unlock(object);

	/*
	 *	Create the [internal] pager, and associate it with this object.
	 *
	 *	We make the association here so that vm_object_enter()
	 *      can look up the object to complete initializing it.  No
	 *	user will ever map this object.
	 */
	{
		/* create our new memory object */
		assert((uint32_t) (object->vo_size / PAGE_SIZE) ==
		    (object->vo_size / PAGE_SIZE));
		(void) compressor_memory_object_create(
			(memory_object_size_t) object->vo_size,
			&pager);
		if (pager == NULL) {
			panic("vm_object_compressor_pager_create(): "
			    "no pager for object %p size 0x%llx\n",
			    object, (uint64_t) object->vo_size);
		}
	}

	/*
	 *	A reference was returned by
	 *	memory_object_create(), and it is
	 *	copied by vm_object_memory_object_associate().
	 */

	pager_object = vm_object_memory_object_associate(pager,
	    object,
	    object->vo_size,
	    FALSE);
	if (pager_object != object) {
		panic("vm_object_compressor_pager_create: mismatch (pager: %p, pager_object: %p, orig_object: %p, orig_object size: 0x%llx)", pager, pager_object, object, (uint64_t) object->vo_size);
	}

	/*
	 *	Drop the reference we were passed.
	 */
	memory_object_deallocate(pager);

	vm_object_lock(object);

	/*
	 *	Release the paging reference
	 */
	vm_object_paging_end(object);
}

vm_external_state_t
vm_object_compressor_pager_state_get(
	vm_object_t        object,
	vm_object_offset_t offset)
{
	if (__probable(not_in_kdp)) {
		vm_object_lock_assert_held(object);
	}
	if (object->internal &&
	    object->pager != NULL &&
	    !object->terminating &&
	    object->alive) {
		return vm_compressor_pager_state_get(object->pager,
		           offset + object->paging_offset);
	} else {
		return VM_EXTERNAL_STATE_UNKNOWN;
	}
}

void
vm_object_compressor_pager_state_clr(
	vm_object_t        object,
	vm_object_offset_t offset)
{
	unsigned int num_pages_cleared;
	vm_object_lock_assert_exclusive(object);
	if (object->internal &&
	    object->pager != NULL &&
	    !object->terminating &&
	    object->alive) {
		num_pages_cleared = vm_compressor_pager_state_clr(object->pager,
		    offset + object->paging_offset);
		if (num_pages_cleared) {
			vm_compressor_pager_count(object->pager,
			    -num_pages_cleared,
			    FALSE, /* shared */
			    object);
		}
		if (num_pages_cleared &&
		    (object->purgable != VM_PURGABLE_DENY || object->vo_ledger_tag)) {
			/* less compressed purgeable/tagged pages */
			assert3u(num_pages_cleared, ==, 1);
			vm_object_owner_compressed_update(object, -num_pages_cleared);
		}
	}
}

/*
 *	Global variables for vm_object_collapse():
 *
 *		Counts for normal collapses and bypasses.
 *		Debugging variables, to watch or disable collapse.
 */
static long     object_collapses = 0;
static long     object_bypasses  = 0;

static boolean_t        vm_object_collapse_allowed = TRUE;
static boolean_t        vm_object_bypass_allowed = TRUE;

void vm_object_do_collapse_compressor(vm_object_t object,
    vm_object_t backing_object);
void
vm_object_do_collapse_compressor(
	vm_object_t object,
	vm_object_t backing_object)
{
	vm_object_offset_t new_offset, backing_offset;
	vm_object_size_t size;

	vm_counters.do_collapse_compressor++;

	vm_object_lock_assert_exclusive(object);
	vm_object_lock_assert_exclusive(backing_object);

	size = object->vo_size;

	/*
	 *	Move all compressed pages from backing_object
	 *	to the parent.
	 */

	for (backing_offset = object->vo_shadow_offset;
	    backing_offset < object->vo_shadow_offset + object->vo_size;
	    backing_offset += PAGE_SIZE) {
		memory_object_offset_t backing_pager_offset;

		/* find the next compressed page at or after this offset */
		backing_pager_offset = (backing_offset +
		    backing_object->paging_offset);
		backing_pager_offset = vm_compressor_pager_next_compressed(
			backing_object->pager,
			backing_pager_offset);
		if (backing_pager_offset == (memory_object_offset_t) -1) {
			/* no more compressed pages */
			break;
		}
		backing_offset = (backing_pager_offset -
		    backing_object->paging_offset);

		new_offset = backing_offset - object->vo_shadow_offset;

		if (new_offset >= object->vo_size) {
			/* we're out of the scope of "object": done */
			break;
		}

		if ((vm_page_lookup(object, new_offset) != VM_PAGE_NULL) ||
		    (vm_compressor_pager_state_get(object->pager,
		    (new_offset +
		    object->paging_offset)) ==
		    VM_EXTERNAL_STATE_EXISTS)) {
			/*
			 * This page already exists in object, resident or
			 * compressed.
			 * We don't need this compressed page in backing_object
			 * and it will be reclaimed when we release
			 * backing_object.
			 */
			continue;
		}

		/*
		 * backing_object has this page in the VM compressor and
		 * we need to transfer it to object.
		 */
		vm_counters.do_collapse_compressor_pages++;
		vm_compressor_pager_transfer(
			/* destination: */
			object->pager,
			(new_offset + object->paging_offset),
			/* source: */
			backing_object->pager,
			(backing_offset + backing_object->paging_offset));
	}
}

/*
 *	Routine:	vm_object_do_collapse
 *	Purpose:
 *		Collapse an object with the object backing it.
 *		Pages in the backing object are moved into the
 *		parent, and the backing object is deallocated.
 *	Conditions:
 *		Both objects and the cache are locked; the page
 *		queues are unlocked.
 *
 */
static void
vm_object_do_collapse(
	vm_object_t object,
	vm_object_t backing_object)
{
	vm_page_t p, pp;
	vm_object_offset_t new_offset, backing_offset;
	vm_object_size_t size;

	vm_object_lock_assert_exclusive(object);
	vm_object_lock_assert_exclusive(backing_object);

	assert(object->purgable == VM_PURGABLE_DENY);
	assert(backing_object->purgable == VM_PURGABLE_DENY);

	backing_offset = object->vo_shadow_offset;
	size = object->vo_size;

	/*
	 *	Move all in-memory pages from backing_object
	 *	to the parent.  Pages that have been paged out
	 *	will be overwritten by any of the parent's
	 *	pages that shadow them.
	 */

	while (!vm_page_queue_empty(&backing_object->memq)) {
		p = (vm_page_t) vm_page_queue_first(&backing_object->memq);

		new_offset = (p->vmp_offset - backing_offset);

		assert(!p->vmp_busy || p->vmp_absent);

		/*
		 *	If the parent has a page here, or if
		 *	this page falls outside the parent,
		 *	dispose of it.
		 *
		 *	Otherwise, move it as planned.
		 */

		if (p->vmp_offset < backing_offset || new_offset >= size) {
			VM_PAGE_FREE(p);
		} else {
			pp = vm_page_lookup(object, new_offset);
			if (pp == VM_PAGE_NULL) {
				if (vm_object_compressor_pager_state_get(object,
				    new_offset)
				    == VM_EXTERNAL_STATE_EXISTS) {
					/*
					 * Parent object has this page
					 * in the VM compressor.
					 * Throw away the backing
					 * object's page.
					 */
					VM_PAGE_FREE(p);
				} else {
					/*
					 *	Parent now has no page.
					 *	Move the backing object's page
					 *      up.
					 */
					vm_page_rename(p, object, new_offset);
				}
			} else {
				assert(!pp->vmp_absent);

				/*
				 *	Parent object has a real page.
				 *	Throw away the backing object's
				 *	page.
				 */
				VM_PAGE_FREE(p);
			}
		}
	}

	if (vm_object_collapse_compressor_allowed &&
	    object->pager != MEMORY_OBJECT_NULL &&
	    backing_object->pager != MEMORY_OBJECT_NULL) {
		/* move compressed pages from backing_object to object */
		vm_object_do_collapse_compressor(object, backing_object);
	} else if (backing_object->pager != MEMORY_OBJECT_NULL) {
		assert((!object->pager_created &&
		    (object->pager == MEMORY_OBJECT_NULL)) ||
		    (!backing_object->pager_created &&
		    (backing_object->pager == MEMORY_OBJECT_NULL)));
		/*
		 *	Move the pager from backing_object to object.
		 *
		 *	XXX We're only using part of the paging space
		 *	for keeps now... we ought to discard the
		 *	unused portion.
		 */

		assert(!object->paging_in_progress);
		assert(!object->activity_in_progress);
		assert(!object->pager_created);
		assert(object->pager == NULL);
		object->pager = backing_object->pager;

		VM_OBJECT_SET_PAGER_CREATED(object, backing_object->pager_created);
		object->pager_control = backing_object->pager_control;
		VM_OBJECT_SET_PAGER_READY(object, backing_object->pager_ready);
		VM_OBJECT_SET_PAGER_INITIALIZED(object, backing_object->pager_initialized);
		object->paging_offset =
		    backing_object->paging_offset + backing_offset;
		if (object->pager_control != MEMORY_OBJECT_CONTROL_NULL) {
			memory_object_control_collapse(&object->pager_control,
			    object);
		}
		/* the backing_object has lost its pager: reset all fields */
		VM_OBJECT_SET_PAGER_CREATED(backing_object, FALSE);
		backing_object->pager_control = NULL;
		VM_OBJECT_SET_PAGER_READY(backing_object, FALSE);
		backing_object->paging_offset = 0;
		backing_object->pager = NULL;
	}
	/*
	 *	Object now shadows whatever backing_object did.
	 *	Note that the reference to backing_object->shadow
	 *	moves from within backing_object to within object.
	 */

	assert(!object->phys_contiguous);
	assert(!backing_object->phys_contiguous);
	object->shadow = backing_object->shadow;
	if (object->shadow) {
		assertf(page_aligned(object->vo_shadow_offset),
		    "object %p shadow_offset 0x%llx",
		    object, object->vo_shadow_offset);
		assertf(page_aligned(backing_object->vo_shadow_offset),
		    "backing_object %p shadow_offset 0x%llx",
		    backing_object, backing_object->vo_shadow_offset);
		object->vo_shadow_offset += backing_object->vo_shadow_offset;
		/* "backing_object" gave its shadow to "object" */
		backing_object->shadow = VM_OBJECT_NULL;
		backing_object->vo_shadow_offset = 0;
	} else {
		/* no shadow, therefore no shadow offset... */
		object->vo_shadow_offset = 0;
	}
	assert((object->shadow == VM_OBJECT_NULL) ||
	    (object->shadow->vo_copy != backing_object));

	/*
	 *	Discard backing_object.
	 *
	 *	Since the backing object has no pages, no
	 *	pager left, and no object references within it,
	 *	all that is necessary is to dispose of it.
	 */
	object_collapses++;

	assert(backing_object->ref_count == 1);
	assert(backing_object->resident_page_count == 0);
	assert(backing_object->paging_in_progress == 0);
	assert(backing_object->activity_in_progress == 0);
	assert(backing_object->shadow == VM_OBJECT_NULL);
	assert(backing_object->vo_shadow_offset == 0);

	if (backing_object->pager != MEMORY_OBJECT_NULL) {
		/* ... unless it has a pager; need to terminate pager too */
		vm_counters.do_collapse_terminate++;
		if (vm_object_terminate(backing_object) != KERN_SUCCESS) {
			vm_counters.do_collapse_terminate_failure++;
		}
		return;
	}

	assert(backing_object->pager == NULL);

	VM_OBJECT_SET_ALIVE(backing_object, FALSE);
	vm_object_unlock(backing_object);

#if VM_OBJECT_TRACKING
	if (vm_object_tracking_btlog) {
		btlog_erase(vm_object_tracking_btlog, backing_object);
	}
#endif /* VM_OBJECT_TRACKING */

	vm_object_lock_destroy(backing_object);

	zfree(vm_object_zone, backing_object);
}

static void
vm_object_do_bypass(
	vm_object_t object,
	vm_object_t backing_object)
{
	/*
	 *	Make the parent shadow the next object
	 *	in the chain.
	 */

	vm_object_lock_assert_exclusive(object);
	vm_object_lock_assert_exclusive(backing_object);

	vm_object_reference(backing_object->shadow);

	assert(!object->phys_contiguous);
	assert(!backing_object->phys_contiguous);
	object->shadow = backing_object->shadow;
	if (object->shadow) {
		assertf(page_aligned(object->vo_shadow_offset),
		    "object %p shadow_offset 0x%llx",
		    object, object->vo_shadow_offset);
		assertf(page_aligned(backing_object->vo_shadow_offset),
		    "backing_object %p shadow_offset 0x%llx",
		    backing_object, backing_object->vo_shadow_offset);
		object->vo_shadow_offset += backing_object->vo_shadow_offset;
	} else {
		/* no shadow, therefore no shadow offset... */
		object->vo_shadow_offset = 0;
	}

	/*
	 *	Backing object might have had a copy pointer
	 *	to us.  If it did, clear it.
	 */
	if (backing_object->vo_copy == object) {
		VM_OBJECT_COPY_SET(backing_object, VM_OBJECT_NULL);
	}

	/*
	 *	Drop the reference count on backing_object.
	 #if	TASK_SWAPPER
	 *	Since its ref_count was at least 2, it
	 *	will not vanish; so we don't need to call
	 *	vm_object_deallocate.
	 *	[with a caveat for "named" objects]
	 *
	 *	The res_count on the backing object is
	 *	conditionally decremented.  It's possible
	 *	(via vm_pageout_scan) to get here with
	 *	a "swapped" object, which has a 0 res_count,
	 *	in which case, the backing object res_count
	 *	is already down by one.
	 #else
	 *	Don't call vm_object_deallocate unless
	 *	ref_count drops to zero.
	 *
	 *	The ref_count can drop to zero here if the
	 *	backing object could be bypassed but not
	 *	collapsed, such as when the backing object
	 *	is temporary and cachable.
	 #endif
	 */
	if (backing_object->ref_count > 2 ||
	    (!backing_object->named && backing_object->ref_count > 1)) {
		vm_object_lock_assert_exclusive(backing_object);
		backing_object->ref_count--;
		vm_object_unlock(backing_object);
	} else {
		/*
		 *	Drop locks so that we can deallocate
		 *	the backing object.
		 */

		/*
		 * vm_object_collapse (the caller of this function) is
		 * now called from contexts that may not guarantee that a
		 * valid reference is held on the object... w/o a valid
		 * reference, it is unsafe and unwise (you will definitely
		 * regret it) to unlock the object and then retake the lock
		 * since the object may be terminated and recycled in between.
		 * The "activity_in_progress" reference will keep the object
		 * 'stable'.
		 */
		vm_object_activity_begin(object);
		vm_object_unlock(object);

		vm_object_unlock(backing_object);
		vm_object_deallocate(backing_object);

		/*
		 *	Relock object. We don't have to reverify
		 *	its state since vm_object_collapse will
		 *	do that for us as it starts at the
		 *	top of its loop.
		 */

		vm_object_lock(object);
		vm_object_activity_end(object);
	}

	object_bypasses++;
}


/*
 *	vm_object_collapse:
 *
 *	Perform an object collapse or an object bypass if appropriate.
 *	The real work of collapsing and bypassing is performed in
 *	the routines vm_object_do_collapse and vm_object_do_bypass.
 *
 *	Requires that the object be locked and the page queues be unlocked.
 *
 */
static unsigned long vm_object_collapse_calls = 0;
static unsigned long vm_object_collapse_objects = 0;
static unsigned long vm_object_collapse_do_collapse = 0;
static unsigned long vm_object_collapse_do_bypass = 0;

__private_extern__ void
vm_object_collapse(
	vm_object_t                             object,
	vm_object_offset_t                      hint_offset,
	boolean_t                               can_bypass)
{
	vm_object_t                             backing_object;
	vm_object_size_t                        object_vcount, object_rcount;
	vm_object_t                             original_object;
	int                                     object_lock_type;
	int                                     backing_object_lock_type;

	vm_object_collapse_calls++;

	assertf(page_aligned(hint_offset), "hint_offset 0x%llx", hint_offset);

	if (!vm_object_collapse_allowed &&
	    !(can_bypass && vm_object_bypass_allowed)) {
		return;
	}

	if (object == VM_OBJECT_NULL) {
		return;
	}

	original_object = object;

	/*
	 * The top object was locked "exclusive" by the caller.
	 * In the first pass, to determine if we can collapse the shadow chain,
	 * take a "shared" lock on the shadow objects.  If we can collapse,
	 * we'll have to go down the chain again with exclusive locks.
	 */
	object_lock_type = OBJECT_LOCK_EXCLUSIVE;
	backing_object_lock_type = OBJECT_LOCK_SHARED;

retry:
	object = original_object;
	vm_object_lock_assert_exclusive(object);

	while (TRUE) {
		vm_object_collapse_objects++;
		/*
		 *	Verify that the conditions are right for either
		 *	collapse or bypass:
		 */

		/*
		 *	There is a backing object, and
		 */

		backing_object = object->shadow;
		if (backing_object == VM_OBJECT_NULL) {
			if (object != original_object) {
				vm_object_unlock(object);
			}
			return;
		}
		if (backing_object_lock_type == OBJECT_LOCK_SHARED) {
			vm_object_lock_shared(backing_object);
		} else {
			vm_object_lock(backing_object);
		}

		/*
		 *	No pages in the object are currently
		 *	being paged out, and
		 */
		if (object->paging_in_progress != 0 ||
		    object->activity_in_progress != 0) {
			/* try and collapse the rest of the shadow chain */
			if (object != original_object) {
				vm_object_unlock(object);
			}
			object = backing_object;
			object_lock_type = backing_object_lock_type;
			continue;
		}

		/*
		 *	...
		 *		The backing object is not read_only,
		 *		and no pages in the backing object are
		 *		currently being paged out.
		 *		The backing object is internal.
		 *
		 */

		if (!backing_object->internal ||
		    backing_object->paging_in_progress != 0 ||
		    backing_object->activity_in_progress != 0) {
			/* try and collapse the rest of the shadow chain */
			if (object != original_object) {
				vm_object_unlock(object);
			}
			object = backing_object;
			object_lock_type = backing_object_lock_type;
			continue;
		}

		/*
		 * Purgeable objects are not supposed to engage in
		 * copy-on-write activities, so should not have
		 * any shadow objects or be a shadow object to another
		 * object.
		 * Collapsing a purgeable object would require some
		 * updates to the purgeable compressed ledgers.
		 */
		if (object->purgable != VM_PURGABLE_DENY ||
		    backing_object->purgable != VM_PURGABLE_DENY) {
			panic("vm_object_collapse() attempting to collapse "
			    "purgeable object: %p(%d) %p(%d)\n",
			    object, object->purgable,
			    backing_object, backing_object->purgable);
			/* try and collapse the rest of the shadow chain */
			if (object != original_object) {
				vm_object_unlock(object);
			}
			object = backing_object;
			object_lock_type = backing_object_lock_type;
			continue;
		}

		/*
		 *	The backing object can't be a copy-object:
		 *	the shadow_offset for the copy-object must stay
		 *	as 0.  Furthermore (for the 'we have all the
		 *	pages' case), if we bypass backing_object and
		 *	just shadow the next object in the chain, old
		 *	pages from that object would then have to be copied
		 *	BOTH into the (former) backing_object and into the
		 *	parent object.
		 */
		if (backing_object->shadow != VM_OBJECT_NULL &&
		    backing_object->shadow->vo_copy == backing_object) {
			/* try and collapse the rest of the shadow chain */
			if (object != original_object) {
				vm_object_unlock(object);
			}
			object = backing_object;
			object_lock_type = backing_object_lock_type;
			continue;
		}

		/*
		 *	We can now try to either collapse the backing
		 *	object (if the parent is the only reference to
		 *	it) or (perhaps) remove the parent's reference
		 *	to it.
		 *
		 *	If there is exactly one reference to the backing
		 *	object, we may be able to collapse it into the
		 *	parent.
		 *
		 *	As long as one of the objects is still not known
		 *	to the pager, we can collapse them.
		 */
		if (backing_object->ref_count == 1 &&
		    (vm_object_collapse_compressor_allowed ||
		    !object->pager_created
		    || (!backing_object->pager_created)
		    ) && vm_object_collapse_allowed) {
			/*
			 * We need the exclusive lock on the VM objects.
			 */
			if (backing_object_lock_type != OBJECT_LOCK_EXCLUSIVE) {
				/*
				 * We have an object and its shadow locked
				 * "shared".  We can't just upgrade the locks
				 * to "exclusive", as some other thread might
				 * also have these objects locked "shared" and
				 * attempt to upgrade one or the other to
				 * "exclusive".  The upgrades would block
				 * forever waiting for the other "shared" locks
				 * to get released.
				 * So we have to release the locks and go
				 * down the shadow chain again (since it could
				 * have changed) with "exclusive" locking.
				 */
				vm_object_unlock(backing_object);
				if (object != original_object) {
					vm_object_unlock(object);
				}
				object_lock_type = OBJECT_LOCK_EXCLUSIVE;
				backing_object_lock_type = OBJECT_LOCK_EXCLUSIVE;
				goto retry;
			}

			/*
			 *	Collapse the object with its backing
			 *	object, and try again with the object's
			 *	new backing object.
			 */

			vm_object_do_collapse(object, backing_object);
			vm_object_collapse_do_collapse++;
			continue;
		}

		/*
		 *	Collapsing the backing object was not possible
		 *	or permitted, so let's try bypassing it.
		 */

		if (!(can_bypass && vm_object_bypass_allowed)) {
			/* try and collapse the rest of the shadow chain */
			if (object != original_object) {
				vm_object_unlock(object);
			}
			object = backing_object;
			object_lock_type = backing_object_lock_type;
			continue;
		}


		/*
		 *	If the object doesn't have all its pages present,
		 *	we have to make sure no pages in the backing object
		 *	"show through" before bypassing it.
		 */
		object_vcount = object->vo_size >> PAGE_SHIFT;
		object_rcount = (vm_object_size_t)object->resident_page_count;

		if (object_rcount != object_vcount) {
			vm_object_offset_t      offset;
			vm_object_offset_t      backing_offset;
			vm_object_size_t        backing_rcount, backing_vcount;

			/*
			 *	If the backing object has a pager but no pagemap,
			 *	then we cannot bypass it, because we don't know
			 *	what pages it has.
			 */
			if (backing_object->pager_created) {
				/* try and collapse the rest of the shadow chain */
				if (object != original_object) {
					vm_object_unlock(object);
				}
				object = backing_object;
				object_lock_type = backing_object_lock_type;
				continue;
			}

			/*
			 *	If the object has a pager but no pagemap,
			 *	then we cannot bypass it, because we don't know
			 *	what pages it has.
			 */
			if (object->pager_created) {
				/* try and collapse the rest of the shadow chain */
				if (object != original_object) {
					vm_object_unlock(object);
				}
				object = backing_object;
				object_lock_type = backing_object_lock_type;
				continue;
			}

			backing_offset = object->vo_shadow_offset;
			backing_vcount = backing_object->vo_size >> PAGE_SHIFT;
			backing_rcount = (vm_object_size_t)backing_object->resident_page_count;
			assert(backing_vcount >= object_vcount);

			if (backing_rcount > (backing_vcount - object_vcount) &&
			    backing_rcount - (backing_vcount - object_vcount) > object_rcount) {
				/*
				 * we have enough pages in the backing object to guarantee that
				 * at least 1 of them must be 'uncovered' by a resident page
				 * in the object we're evaluating, so move on and
				 * try to collapse the rest of the shadow chain
				 */
				if (object != original_object) {
					vm_object_unlock(object);
				}
				object = backing_object;
				object_lock_type = backing_object_lock_type;
				continue;
			}

			/*
			 *	If all of the pages in the backing object are
			 *	shadowed by the parent object, the parent
			 *	object no longer has to shadow the backing
			 *	object; it can shadow the next one in the
			 *	chain.
			 *
			 *	If the backing object has existence info,
			 *	we must check examine its existence info
			 *	as well.
			 *
			 */

#define EXISTS_IN_OBJECT(obj, off, rc)                  \
	((vm_object_compressor_pager_state_get((obj), (off))   \
	  == VM_EXTERNAL_STATE_EXISTS) ||               \
	 ((rc) && vm_page_lookup((obj), (off)) != VM_PAGE_NULL && (rc)--))

			/*
			 * Check the hint location first
			 * (since it is often the quickest way out of here).
			 */
			if (object->cow_hint != ~(vm_offset_t)0) {
				hint_offset = (vm_object_offset_t)object->cow_hint;
			} else {
				hint_offset = (hint_offset > 8 * PAGE_SIZE_64) ?
				    (hint_offset - 8 * PAGE_SIZE_64) : 0;
			}

			if (EXISTS_IN_OBJECT(backing_object, hint_offset +
			    backing_offset, backing_rcount) &&
			    !EXISTS_IN_OBJECT(object, hint_offset, object_rcount)) {
				/* dependency right at the hint */
				object->cow_hint = (vm_offset_t) hint_offset; /* atomic */
				/* try and collapse the rest of the shadow chain */
				if (object != original_object) {
					vm_object_unlock(object);
				}
				object = backing_object;
				object_lock_type = backing_object_lock_type;
				continue;
			}

			/*
			 * If the object's window onto the backing_object
			 * is large compared to the number of resident
			 * pages in the backing object, it makes sense to
			 * walk the backing_object's resident pages first.
			 *
			 * NOTE: Pages may be in both the existence map and/or
			 * resident, so if we don't find a dependency while
			 * walking the backing object's resident page list
			 * directly, and there is an existence map, we'll have
			 * to run the offset based 2nd pass.  Because we may
			 * have to run both passes, we need to be careful
			 * not to decrement 'rcount' in the 1st pass
			 */
			if (backing_rcount && backing_rcount < (object_vcount / 8)) {
				vm_object_size_t rc = object_rcount;
				vm_page_t p;

				backing_rcount = backing_object->resident_page_count;
				p = (vm_page_t)vm_page_queue_first(&backing_object->memq);
				do {
					offset = (p->vmp_offset - backing_offset);

					if (offset < object->vo_size &&
					    offset != hint_offset &&
					    !EXISTS_IN_OBJECT(object, offset, rc)) {
						/* found a dependency */
						object->cow_hint = (vm_offset_t) offset; /* atomic */

						break;
					}
					p = (vm_page_t) vm_page_queue_next(&p->vmp_listq);
				} while (--backing_rcount);
				if (backing_rcount != 0) {
					/* try and collapse the rest of the shadow chain */
					if (object != original_object) {
						vm_object_unlock(object);
					}
					object = backing_object;
					object_lock_type = backing_object_lock_type;
					continue;
				}
			}

			/*
			 * Walk through the offsets looking for pages in the
			 * backing object that show through to the object.
			 */
			if (backing_rcount) {
				offset = hint_offset;

				while ((offset =
				    (offset + PAGE_SIZE_64 < object->vo_size) ?
				    (offset + PAGE_SIZE_64) : 0) != hint_offset) {
					if (EXISTS_IN_OBJECT(backing_object, offset +
					    backing_offset, backing_rcount) &&
					    !EXISTS_IN_OBJECT(object, offset, object_rcount)) {
						/* found a dependency */
						object->cow_hint = (vm_offset_t) offset; /* atomic */
						break;
					}
				}
				if (offset != hint_offset) {
					/* try and collapse the rest of the shadow chain */
					if (object != original_object) {
						vm_object_unlock(object);
					}
					object = backing_object;
					object_lock_type = backing_object_lock_type;
					continue;
				}
			}
		}

		/*
		 * We need "exclusive" locks on the 2 VM objects.
		 */
		if (backing_object_lock_type != OBJECT_LOCK_EXCLUSIVE) {
			vm_object_unlock(backing_object);
			if (object != original_object) {
				vm_object_unlock(object);
			}
			object_lock_type = OBJECT_LOCK_EXCLUSIVE;
			backing_object_lock_type = OBJECT_LOCK_EXCLUSIVE;
			goto retry;
		}

		/* reset the offset hint for any objects deeper in the chain */
		object->cow_hint = (vm_offset_t)0;

		/*
		 *	All interesting pages in the backing object
		 *	already live in the parent or its pager.
		 *	Thus we can bypass the backing object.
		 */

		vm_object_do_bypass(object, backing_object);
		vm_object_collapse_do_bypass++;

		/*
		 *	Try again with this object's new backing object.
		 */

		continue;
	}

	/* NOT REACHED */
	/*
	 *  if (object != original_object) {
	 *       vm_object_unlock(object);
	 *  }
	 */
}

/*
 *	Routine:	vm_object_page_remove: [internal]
 *	Purpose:
 *		Removes all physical pages in the specified
 *		object range from the object's list of pages.
 *
 *	In/out conditions:
 *		The object must be locked.
 *		The object must not have paging_in_progress, usually
 *		guaranteed by not having a pager.
 */
unsigned int vm_object_page_remove_lookup = 0;
unsigned int vm_object_page_remove_iterate = 0;

__private_extern__ void
vm_object_page_remove(
	vm_object_t             object,
	vm_object_offset_t      start,
	vm_object_offset_t      end)
{
	vm_page_t       p, next;

	/*
	 *	One and two page removals are most popular.
	 *	The factor of 16 here is somewhat arbitrary.
	 *	It balances vm_object_lookup vs iteration.
	 */

	if (atop_64(end - start) < (unsigned)object->resident_page_count / 16) {
		vm_object_page_remove_lookup++;

		for (; start < end; start += PAGE_SIZE_64) {
			p = vm_page_lookup(object, start);
			if (p != VM_PAGE_NULL) {
				assert(!p->vmp_cleaning && !p->vmp_laundry);
				if (!p->vmp_fictitious && p->vmp_pmapped) {
					pmap_disconnect(VM_PAGE_GET_PHYS_PAGE(p));
				}
				VM_PAGE_FREE(p);
			}
		}
	} else {
		vm_object_page_remove_iterate++;

		p = (vm_page_t) vm_page_queue_first(&object->memq);
		while (!vm_page_queue_end(&object->memq, (vm_page_queue_entry_t) p)) {
			next = (vm_page_t) vm_page_queue_next(&p->vmp_listq);
			if ((start <= p->vmp_offset) && (p->vmp_offset < end)) {
				assert(!p->vmp_cleaning && !p->vmp_laundry);
				if (!p->vmp_fictitious && p->vmp_pmapped) {
					pmap_disconnect(VM_PAGE_GET_PHYS_PAGE(p));
				}
				VM_PAGE_FREE(p);
			}
			p = next;
		}
	}
}


/*
 *	Routine:	vm_object_coalesce
 *	Function:	Coalesces two objects backing up adjoining
 *			regions of memory into a single object.
 *
 *	returns TRUE if objects were combined.
 *
 *	NOTE:	Only works at the moment if the second object is NULL -
 *		if it's not, which object do we lock first?
 *
 *	Parameters:
 *		prev_object	First object to coalesce
 *		prev_offset	Offset into prev_object
 *		next_object	Second object into coalesce
 *		next_offset	Offset into next_object
 *
 *		prev_size	Size of reference to prev_object
 *		next_size	Size of reference to next_object
 *
 *	Conditions:
 *	The object(s) must *not* be locked. The map must be locked
 *	to preserve the reference to the object(s).
 */
static int vm_object_coalesce_count = 0;

__private_extern__ boolean_t
vm_object_coalesce(
	vm_object_t                     prev_object,
	vm_object_t                     next_object,
	vm_object_offset_t              prev_offset,
	__unused vm_object_offset_t next_offset,
	vm_object_size_t                prev_size,
	vm_object_size_t                next_size)
{
	vm_object_size_t        newsize;

#ifdef  lint
	next_offset++;
#endif  /* lint */

	if (next_object != VM_OBJECT_NULL) {
		return FALSE;
	}

	if (prev_object == VM_OBJECT_NULL) {
		return TRUE;
	}

	vm_object_lock(prev_object);

	/*
	 *	Try to collapse the object first
	 */
	vm_object_collapse(prev_object, prev_offset, TRUE);

	/*
	 *	Can't coalesce if pages not mapped to
	 *	prev_entry may be in use any way:
	 *	. more than one reference
	 *	. paged out
	 *	. shadows another object
	 *	. has a copy elsewhere
	 *	. is purgeable
	 *	. paging references (pages might be in page-list)
	 */

	if ((prev_object->ref_count > 1) ||
	    prev_object->pager_created ||
	    prev_object->phys_contiguous ||
	    (prev_object->shadow != VM_OBJECT_NULL) ||
	    (prev_object->vo_copy != VM_OBJECT_NULL) ||
	    (prev_object->true_share != FALSE) ||
	    (prev_object->purgable != VM_PURGABLE_DENY) ||
	    (prev_object->paging_in_progress != 0) ||
	    (prev_object->activity_in_progress != 0)) {
		vm_object_unlock(prev_object);
		return FALSE;
	}
	/* newsize = prev_offset + prev_size + next_size; */
	if (__improbable(os_add3_overflow(prev_offset, prev_size, next_size,
	    &newsize))) {
		vm_object_unlock(prev_object);
		return FALSE;
	}

	vm_object_coalesce_count++;

	/*
	 *	Remove any pages that may still be in the object from
	 *	a previous deallocation.
	 */
	vm_object_page_remove(prev_object,
	    prev_offset + prev_size,
	    prev_offset + prev_size + next_size);

	/*
	 *	Extend the object if necessary.
	 */
	if (newsize > prev_object->vo_size) {
		assertf(page_aligned(newsize),
		    "object %p size 0x%llx",
		    prev_object, (uint64_t)newsize);
		prev_object->vo_size = newsize;
	}

	vm_object_unlock(prev_object);
	return TRUE;
}

kern_return_t
vm_object_populate_with_private(
	vm_object_t             object,
	vm_object_offset_t      offset,
	ppnum_t                 phys_page,
	vm_size_t               size)
{
	ppnum_t                 base_page;
	vm_object_offset_t      base_offset;


	if (!object->private) {
		return KERN_FAILURE;
	}

	base_page = phys_page;

	vm_object_lock(object);

	if (!object->phys_contiguous) {
		vm_page_t       m;

		if ((base_offset = trunc_page_64(offset)) != offset) {
			vm_object_unlock(object);
			return KERN_FAILURE;
		}
		base_offset += object->paging_offset;

		while (size) {
			m = vm_page_lookup(object, base_offset);

			if (m != VM_PAGE_NULL) {
				if (m->vmp_fictitious) {
					if (VM_PAGE_GET_PHYS_PAGE(m) != vm_page_guard_addr) {
						vm_page_lockspin_queues();
						m->vmp_private = TRUE;
						vm_page_unlock_queues();

						m->vmp_fictitious = FALSE;
						VM_PAGE_SET_PHYS_PAGE(m, base_page);
					}
				} else if (VM_PAGE_GET_PHYS_PAGE(m) != base_page) {
					if (!m->vmp_private) {
						/*
						 * we'd leak a real page... that can't be right
						 */
						panic("vm_object_populate_with_private - %p not private", m);
					}
					if (m->vmp_pmapped) {
						/*
						 * pmap call to clear old mapping
						 */
						pmap_disconnect(VM_PAGE_GET_PHYS_PAGE(m));
					}
					VM_PAGE_SET_PHYS_PAGE(m, base_page);
				}
			} else {
				m = vm_page_grab_fictitious(TRUE);

				/*
				 * private normally requires lock_queues but since we
				 * are initializing the page, its not necessary here
				 */
				m->vmp_private = TRUE;
				m->vmp_fictitious = FALSE;
				VM_PAGE_SET_PHYS_PAGE(m, base_page);
				m->vmp_unusual = TRUE;
				m->vmp_busy = FALSE;

				vm_page_insert(m, object, base_offset);
			}
			base_page++;                                                                    /* Go to the next physical page */
			base_offset += PAGE_SIZE;
			size -= PAGE_SIZE;
		}
	} else {
		/* NOTE: we should check the original settings here */
		/* if we have a size > zero a pmap call should be made */
		/* to disable the range */

		/* pmap_? */

		/* shadows on contiguous memory are not allowed */
		/* we therefore can use the offset field */
		object->vo_shadow_offset = (vm_object_offset_t)phys_page << PAGE_SHIFT;
		assertf(page_aligned(size),
		    "object %p size 0x%llx",
		    object, (uint64_t)size);
		object->vo_size = size;
	}
	vm_object_unlock(object);

	return KERN_SUCCESS;
}


kern_return_t
memory_object_create_named(
	memory_object_t pager,
	memory_object_offset_t  size,
	memory_object_control_t         *control)
{
	vm_object_t             object;

	*control = MEMORY_OBJECT_CONTROL_NULL;
	if (pager == MEMORY_OBJECT_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	object = vm_object_memory_object_associate(pager,
	    VM_OBJECT_NULL,
	    size,
	    TRUE);
	if (object == VM_OBJECT_NULL) {
		return KERN_INVALID_OBJECT;
	}

	/* wait for object (if any) to be ready */
	if (object != VM_OBJECT_NULL) {
		vm_object_lock(object);
		VM_OBJECT_SET_NAMED(object, TRUE);
		while (!object->pager_ready) {
			vm_object_sleep(object,
			    VM_OBJECT_EVENT_PAGER_READY,
			    THREAD_UNINT, LCK_SLEEP_EXCLUSIVE);
		}
		*control = object->pager_control;
		vm_object_unlock(object);
	}
	return KERN_SUCCESS;
}


__private_extern__ kern_return_t
vm_object_lock_request(
	vm_object_t                     object,
	vm_object_offset_t              offset,
	vm_object_size_t                size,
	memory_object_return_t          should_return,
	int                             flags,
	vm_prot_t                       prot)
{
	__unused boolean_t      should_flush;

	should_flush = flags & MEMORY_OBJECT_DATA_FLUSH;

	/*
	 *	Check for bogus arguments.
	 */
	if (object == VM_OBJECT_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	if ((prot & ~VM_PROT_ALL) != 0 && prot != VM_PROT_NO_CHANGE) {
		return KERN_INVALID_ARGUMENT;
	}

	/*
	 * XXX TODO4K
	 * extend range for conservative operations (copy-on-write, sync, ...)
	 * truncate range for destructive operations (purge, ...)
	 */
	size = vm_object_round_page(offset + size) - vm_object_trunc_page(offset);
	offset = vm_object_trunc_page(offset);

	/*
	 *	Lock the object, and acquire a paging reference to
	 *	prevent the memory_object reference from being released.
	 */
	vm_object_lock(object);
	vm_object_paging_begin(object);

	(void)vm_object_update(object,
	    offset, size, NULL, NULL, should_return, flags, prot);

	vm_object_paging_end(object);
	vm_object_unlock(object);

	return KERN_SUCCESS;
}

/*
 * Empty a purgeable object by grabbing the physical pages assigned to it and
 * putting them on the free queue without writing them to backing store, etc.
 * When the pages are next touched they will be demand zero-fill pages.  We
 * skip pages which are busy, being paged in/out, wired, etc.  We do _not_
 * skip referenced/dirty pages, pages on the active queue, etc.  We're more
 * than happy to grab these since this is a purgeable object.  We mark the
 * object as "empty" after reaping its pages.
 *
 * On entry the object must be locked and it must be
 * purgeable with no delayed copies pending.
 */
uint64_t
vm_object_purge(vm_object_t object, int flags)
{
	unsigned int    object_page_count = 0, pgcount = 0;
	uint64_t        total_purged_pgcount = 0;
	boolean_t       skipped_object = FALSE;

	vm_object_lock_assert_exclusive(object);

	if (object->purgable == VM_PURGABLE_DENY) {
		return 0;
	}

	assert(object->vo_copy == VM_OBJECT_NULL);
	assert(object->copy_strategy == MEMORY_OBJECT_COPY_NONE);

	/*
	 * We need to set the object's state to VM_PURGABLE_EMPTY *before*
	 * reaping its pages.  We update vm_page_purgeable_count in bulk
	 * and we don't want vm_page_remove() to update it again for each
	 * page we reap later.
	 *
	 * For the purgeable ledgers, pages from VOLATILE and EMPTY objects
	 * are all accounted for in the "volatile" ledgers, so this does not
	 * make any difference.
	 * If we transitioned directly from NONVOLATILE to EMPTY,
	 * vm_page_purgeable_count must have been updated when the object
	 * was dequeued from its volatile queue and the purgeable ledgers
	 * must have also been updated accordingly at that time (in
	 * vm_object_purgable_control()).
	 */
	if (object->purgable == VM_PURGABLE_VOLATILE) {
		unsigned int delta;
		assert(object->resident_page_count >=
		    object->wired_page_count);
		delta = (object->resident_page_count -
		    object->wired_page_count);
		if (delta != 0) {
			assert(vm_page_purgeable_count >=
			    delta);
			OSAddAtomic(-delta,
			    (SInt32 *)&vm_page_purgeable_count);
		}
		if (object->wired_page_count != 0) {
			assert(vm_page_purgeable_wired_count >=
			    object->wired_page_count);
			OSAddAtomic(-object->wired_page_count,
			    (SInt32 *)&vm_page_purgeable_wired_count);
		}
		VM_OBJECT_SET_PURGABLE(object, VM_PURGABLE_EMPTY);
	}
	assert(object->purgable == VM_PURGABLE_EMPTY);

	object_page_count = object->resident_page_count;

	vm_object_reap_pages(object, REAP_PURGEABLE);

	if (object->resident_page_count >= object_page_count) {
		total_purged_pgcount = 0;
	} else {
		total_purged_pgcount = object_page_count - object->resident_page_count;
	}

	if (object->pager != NULL) {
		assert(VM_CONFIG_COMPRESSOR_IS_PRESENT);

		if (object->activity_in_progress == 0 &&
		    object->paging_in_progress == 0) {
			/*
			 * Also reap any memory coming from this object
			 * in the VM compressor.
			 *
			 * There are no operations in progress on the VM object
			 * and no operation can start while we're holding the
			 * VM object lock, so it's safe to reap the compressed
			 * pages and update the page counts.
			 */
			pgcount = vm_compressor_pager_get_count(object->pager);
			if (pgcount) {
				pgcount = vm_compressor_pager_reap_pages(object->pager, flags);
				vm_compressor_pager_count(object->pager,
				    -pgcount,
				    FALSE,                       /* shared */
				    object);
				vm_object_owner_compressed_update(object,
				    -pgcount);
			}
			if (!(flags & C_DONT_BLOCK)) {
				assert(vm_compressor_pager_get_count(object->pager)
				    == 0);
			}
		} else {
			/*
			 * There's some kind of paging activity in progress
			 * for this object, which could result in a page
			 * being compressed or decompressed, possibly while
			 * the VM object is not locked, so it could race
			 * with us.
			 *
			 * We can't really synchronize this without possibly
			 * causing a deadlock when the compressor needs to
			 * allocate or free memory while compressing or
			 * decompressing a page from a purgeable object
			 * mapped in the kernel_map...
			 *
			 * So let's not attempt to purge the compressor
			 * pager if there's any kind of operation in
			 * progress on the VM object.
			 */
			skipped_object = TRUE;
		}
	}

	vm_object_lock_assert_exclusive(object);

	total_purged_pgcount += pgcount;

	KDBG_RELEASE(VMDBG_CODE(DBG_VM_PURGEABLE_OBJECT_PURGE_ONE) | DBG_FUNC_NONE,
	    VM_KERNEL_UNSLIDE_OR_PERM(object),                   /* purged object */
	    object_page_count,
	    total_purged_pgcount,
	    skipped_object);

	return total_purged_pgcount;
}


/*
 * vm_object_purgeable_control() allows the caller to control and investigate the
 * state of a purgeable object.  A purgeable object is created via a call to
 * vm_allocate() with VM_FLAGS_PURGABLE specified.  A purgeable object will
 * never be coalesced with any other object -- even other purgeable objects --
 * and will thus always remain a distinct object.  A purgeable object has
 * special semantics when its reference count is exactly 1.  If its reference
 * count is greater than 1, then a purgeable object will behave like a normal
 * object and attempts to use this interface will result in an error return
 * of KERN_INVALID_ARGUMENT.
 *
 * A purgeable object may be put into a "volatile" state which will make the
 * object's pages elligable for being reclaimed without paging to backing
 * store if the system runs low on memory.  If the pages in a volatile
 * purgeable object are reclaimed, the purgeable object is said to have been
 * "emptied."  When a purgeable object is emptied the system will reclaim as
 * many pages from the object as it can in a convenient manner (pages already
 * en route to backing store or busy for other reasons are left as is).  When
 * a purgeable object is made volatile, its pages will generally be reclaimed
 * before other pages in the application's working set.  This semantic is
 * generally used by applications which can recreate the data in the object
 * faster than it can be paged in.  One such example might be media assets
 * which can be reread from a much faster RAID volume.
 *
 * A purgeable object may be designated as "non-volatile" which means it will
 * behave like all other objects in the system with pages being written to and
 * read from backing store as needed to satisfy system memory needs.  If the
 * object was emptied before the object was made non-volatile, that fact will
 * be returned as the old state of the purgeable object (see
 * VM_PURGABLE_SET_STATE below).  In this case, any pages of the object which
 * were reclaimed as part of emptying the object will be refaulted in as
 * zero-fill on demand.  It is up to the application to note that an object
 * was emptied and recreate the objects contents if necessary.  When a
 * purgeable object is made non-volatile, its pages will generally not be paged
 * out to backing store in the immediate future.  A purgeable object may also
 * be manually emptied.
 *
 * Finally, the current state (non-volatile, volatile, volatile & empty) of a
 * volatile purgeable object may be queried at any time.  This information may
 * be used as a control input to let the application know when the system is
 * experiencing memory pressure and is reclaiming memory.
 *
 * The specified address may be any address within the purgeable object.  If
 * the specified address does not represent any object in the target task's
 * virtual address space, then KERN_INVALID_ADDRESS will be returned.  If the
 * object containing the specified address is not a purgeable object, then
 * KERN_INVALID_ARGUMENT will be returned.  Otherwise, KERN_SUCCESS will be
 * returned.
 *
 * The control parameter may be any one of VM_PURGABLE_SET_STATE or
 * VM_PURGABLE_GET_STATE.  For VM_PURGABLE_SET_STATE, the in/out parameter
 * state is used to set the new state of the purgeable object and return its
 * old state.  For VM_PURGABLE_GET_STATE, the current state of the purgeable
 * object is returned in the parameter state.
 *
 * The in/out parameter state may be one of VM_PURGABLE_NONVOLATILE,
 * VM_PURGABLE_VOLATILE or VM_PURGABLE_EMPTY.  These, respectively, represent
 * the non-volatile, volatile and volatile/empty states described above.
 * Setting the state of a purgeable object to VM_PURGABLE_EMPTY will
 * immediately reclaim as many pages in the object as can be conveniently
 * collected (some may have already been written to backing store or be
 * otherwise busy).
 *
 * The process of making a purgeable object non-volatile and determining its
 * previous state is atomic.  Thus, if a purgeable object is made
 * VM_PURGABLE_NONVOLATILE and the old state is returned as
 * VM_PURGABLE_VOLATILE, then the purgeable object's previous contents are
 * completely intact and will remain so until the object is made volatile
 * again.  If the old state is returned as VM_PURGABLE_EMPTY then the object
 * was reclaimed while it was in a volatile state and its previous contents
 * have been lost.
 */
/*
 * The object must be locked.
 */
kern_return_t
vm_object_purgable_control(
	vm_object_t     object,
	vm_purgable_t   control,
	int             *state)
{
	int             old_state;
	int             new_state;

	if (object == VM_OBJECT_NULL) {
		/*
		 * Object must already be present or it can't be purgeable.
		 */
		return KERN_INVALID_ARGUMENT;
	}

	vm_object_lock_assert_exclusive(object);

	/*
	 * Get current state of the purgeable object.
	 */
	old_state = object->purgable;
	if (old_state == VM_PURGABLE_DENY) {
		return KERN_INVALID_ARGUMENT;
	}

	/* purgeable cant have delayed copies - now or in the future */
	assert(object->vo_copy == VM_OBJECT_NULL);
	assert(object->copy_strategy == MEMORY_OBJECT_COPY_NONE);

	/*
	 * Execute the desired operation.
	 */
	if (control == VM_PURGABLE_GET_STATE) {
		*state = old_state;
		return KERN_SUCCESS;
	}

	if (control == VM_PURGABLE_SET_STATE &&
	    object->purgeable_only_by_kernel) {
		return KERN_PROTECTION_FAILURE;
	}

	if (control != VM_PURGABLE_SET_STATE &&
	    control != VM_PURGABLE_SET_STATE_FROM_KERNEL) {
		return KERN_INVALID_ARGUMENT;
	}

	if ((*state) & VM_PURGABLE_DEBUG_EMPTY) {
		object->volatile_empty = TRUE;
	}
	if ((*state) & VM_PURGABLE_DEBUG_FAULT) {
		object->volatile_fault = TRUE;
	}

	new_state = *state & VM_PURGABLE_STATE_MASK;
	if (new_state == VM_PURGABLE_VOLATILE) {
		if (old_state == VM_PURGABLE_EMPTY) {
			/* what's been emptied must stay empty */
			new_state = VM_PURGABLE_EMPTY;
		}
		if (object->volatile_empty) {
			/* debugging mode: go straight to empty */
			new_state = VM_PURGABLE_EMPTY;
		}
	}

	switch (new_state) {
	case VM_PURGABLE_DENY:
		/*
		 * Attempting to convert purgeable memory to non-purgeable:
		 * not allowed.
		 */
		return KERN_INVALID_ARGUMENT;
	case VM_PURGABLE_NONVOLATILE:
		VM_OBJECT_SET_PURGABLE(object, new_state);

		if (old_state == VM_PURGABLE_VOLATILE) {
			unsigned int delta;

			assert(object->resident_page_count >=
			    object->wired_page_count);
			delta = (object->resident_page_count -
			    object->wired_page_count);

			assert(vm_page_purgeable_count >= delta);

			if (delta != 0) {
				OSAddAtomic(-delta,
				    (SInt32 *)&vm_page_purgeable_count);
			}
			if (object->wired_page_count != 0) {
				assert(vm_page_purgeable_wired_count >=
				    object->wired_page_count);
				OSAddAtomic(-object->wired_page_count,
				    (SInt32 *)&vm_page_purgeable_wired_count);
			}

			vm_page_lock_queues();

			/* object should be on a queue */
			assert(object->objq.next != NULL &&
			    object->objq.prev != NULL);
			purgeable_q_t queue;

			/*
			 * Move object from its volatile queue to the
			 * non-volatile queue...
			 */
			queue = vm_purgeable_object_remove(object);
			assert(queue);

			if (object->purgeable_when_ripe) {
				vm_purgeable_token_delete_last(queue);
			}
			assert(queue->debug_count_objects >= 0);

			vm_page_unlock_queues();
		}
		if (old_state == VM_PURGABLE_VOLATILE ||
		    old_state == VM_PURGABLE_EMPTY) {
			/*
			 * Transfer the object's pages from the volatile to
			 * non-volatile ledgers.
			 */
			vm_purgeable_accounting(object, VM_PURGABLE_VOLATILE);
		}

		break;

	case VM_PURGABLE_VOLATILE:
		if (object->volatile_fault) {
			vm_page_t       p;
			int             refmod;

			vm_page_queue_iterate(&object->memq, p, vmp_listq) {
				if (p->vmp_busy ||
				    VM_PAGE_WIRED(p) ||
				    p->vmp_fictitious) {
					continue;
				}
				refmod = pmap_disconnect(VM_PAGE_GET_PHYS_PAGE(p));
				if ((refmod & VM_MEM_MODIFIED) &&
				    !p->vmp_dirty) {
					SET_PAGE_DIRTY(p, FALSE);
				}
			}
		}

		assert(old_state != VM_PURGABLE_EMPTY);

		purgeable_q_t queue;

		/* find the correct queue */
		if ((*state & VM_PURGABLE_ORDERING_MASK) == VM_PURGABLE_ORDERING_OBSOLETE) {
			queue = &purgeable_queues[PURGEABLE_Q_TYPE_OBSOLETE];
		} else {
			if ((*state & VM_PURGABLE_BEHAVIOR_MASK) == VM_PURGABLE_BEHAVIOR_FIFO) {
				queue = &purgeable_queues[PURGEABLE_Q_TYPE_FIFO];
			} else {
				queue = &purgeable_queues[PURGEABLE_Q_TYPE_LIFO];
			}
		}

		if (old_state == VM_PURGABLE_NONVOLATILE ||
		    old_state == VM_PURGABLE_EMPTY) {
			unsigned int delta;

			if ((*state & VM_PURGABLE_NO_AGING_MASK) ==
			    VM_PURGABLE_NO_AGING) {
				VM_OBJECT_SET_PURGEABLE_WHEN_RIPE(object, FALSE);
			} else {
				VM_OBJECT_SET_PURGEABLE_WHEN_RIPE(object, TRUE);
			}

			if (object->purgeable_when_ripe) {
				kern_return_t result;

				/* try to add token... this can fail */
				vm_page_lock_queues();

				result = vm_purgeable_token_add(queue);
				if (result != KERN_SUCCESS) {
					vm_page_unlock_queues();
					return result;
				}
				vm_page_unlock_queues();
			}

			assert(object->resident_page_count >=
			    object->wired_page_count);
			delta = (object->resident_page_count -
			    object->wired_page_count);

			if (delta != 0) {
				OSAddAtomic(delta,
				    &vm_page_purgeable_count);
			}
			if (object->wired_page_count != 0) {
				OSAddAtomic(object->wired_page_count,
				    &vm_page_purgeable_wired_count);
			}

			VM_OBJECT_SET_PURGABLE(object, new_state);

			/* object should be on "non-volatile" queue */
			assert(object->objq.next != NULL);
			assert(object->objq.prev != NULL);
		} else if (old_state == VM_PURGABLE_VOLATILE) {
			purgeable_q_t   old_queue;
			boolean_t       purgeable_when_ripe;

			/*
			 * if reassigning priorities / purgeable groups, we don't change the
			 * token queue. So moving priorities will not make pages stay around longer.
			 * Reasoning is that the algorithm gives most priority to the most important
			 * object. If a new token is added, the most important object' priority is boosted.
			 * This biases the system already for purgeable queues that move a lot.
			 * It doesn't seem more biasing is neccessary in this case, where no new object is added.
			 */
			assert(object->objq.next != NULL && object->objq.prev != NULL); /* object should be on a queue */

			old_queue = vm_purgeable_object_remove(object);
			assert(old_queue);

			if ((*state & VM_PURGABLE_NO_AGING_MASK) ==
			    VM_PURGABLE_NO_AGING) {
				purgeable_when_ripe = FALSE;
			} else {
				purgeable_when_ripe = TRUE;
			}

			if (old_queue != queue ||
			    (purgeable_when_ripe !=
			    object->purgeable_when_ripe)) {
				kern_return_t result;

				/* Changing queue. Have to move token. */
				vm_page_lock_queues();
				if (object->purgeable_when_ripe) {
					vm_purgeable_token_delete_last(old_queue);
				}
				VM_OBJECT_SET_PURGEABLE_WHEN_RIPE(object, purgeable_when_ripe);
				if (object->purgeable_when_ripe) {
					result = vm_purgeable_token_add(queue);
					assert(result == KERN_SUCCESS);   /* this should never fail since we just freed a token */
				}
				vm_page_unlock_queues();
			}
		}
		;
		vm_purgeable_object_add(object, queue, (*state & VM_VOLATILE_GROUP_MASK) >> VM_VOLATILE_GROUP_SHIFT );
		if (old_state == VM_PURGABLE_NONVOLATILE) {
			vm_purgeable_accounting(object,
			    VM_PURGABLE_NONVOLATILE);
		}

		assert(queue->debug_count_objects >= 0);

		break;


	case VM_PURGABLE_EMPTY:
		if (object->volatile_fault) {
			vm_page_t       p;
			int             refmod;

			vm_page_queue_iterate(&object->memq, p, vmp_listq) {
				if (p->vmp_busy ||
				    VM_PAGE_WIRED(p) ||
				    p->vmp_fictitious) {
					continue;
				}
				refmod = pmap_disconnect(VM_PAGE_GET_PHYS_PAGE(p));
				if ((refmod & VM_MEM_MODIFIED) &&
				    !p->vmp_dirty) {
					SET_PAGE_DIRTY(p, FALSE);
				}
			}
		}

		if (old_state == VM_PURGABLE_VOLATILE) {
			purgeable_q_t old_queue;

			/* object should be on a queue */
			assert(object->objq.next != NULL &&
			    object->objq.prev != NULL);

			old_queue = vm_purgeable_object_remove(object);
			assert(old_queue);
			if (object->purgeable_when_ripe) {
				vm_page_lock_queues();
				vm_purgeable_token_delete_first(old_queue);
				vm_page_unlock_queues();
			}
		}

		if (old_state == VM_PURGABLE_NONVOLATILE) {
			/*
			 * This object's pages were previously accounted as
			 * "non-volatile" and now need to be accounted as
			 * "volatile".
			 */
			vm_purgeable_accounting(object,
			    VM_PURGABLE_NONVOLATILE);
			/*
			 * Set to VM_PURGABLE_EMPTY because the pages are no
			 * longer accounted in the "non-volatile" ledger
			 * and are also not accounted for in
			 * "vm_page_purgeable_count".
			 */
			VM_OBJECT_SET_PURGABLE(object, VM_PURGABLE_EMPTY);
		}

		(void) vm_object_purge(object, 0);
		assert(object->purgable == VM_PURGABLE_EMPTY);

		break;
	}

	*state = old_state;

	vm_object_lock_assert_exclusive(object);

	return KERN_SUCCESS;
}

kern_return_t
vm_object_get_page_counts(
	vm_object_t             object,
	vm_object_offset_t      offset,
	vm_object_size_t        size,
	unsigned int            *resident_page_count,
	unsigned int            *dirty_page_count)
{
	kern_return_t           kr = KERN_SUCCESS;
	boolean_t               count_dirty_pages = FALSE;
	vm_page_t               p = VM_PAGE_NULL;
	unsigned int            local_resident_count = 0;
	unsigned int            local_dirty_count = 0;
	vm_object_offset_t      cur_offset = 0;
	vm_object_offset_t      end_offset = 0;

	if (object == VM_OBJECT_NULL) {
		return KERN_INVALID_ARGUMENT;
	}


	cur_offset = offset;

	end_offset = offset + size;

	vm_object_lock_assert_exclusive(object);

	if (dirty_page_count != NULL) {
		count_dirty_pages = TRUE;
	}

	if (resident_page_count != NULL && count_dirty_pages == FALSE) {
		/*
		 * Fast path when:
		 * - we only want the resident page count, and,
		 * - the entire object is exactly covered by the request.
		 */
		if (offset == 0 && (object->vo_size == size)) {
			*resident_page_count = object->resident_page_count;
			goto out;
		}
	}

	if (object->resident_page_count <= (size >> PAGE_SHIFT)) {
		vm_page_queue_iterate(&object->memq, p, vmp_listq) {
			if (p->vmp_offset >= cur_offset && p->vmp_offset < end_offset) {
				local_resident_count++;

				if (count_dirty_pages) {
					if (p->vmp_dirty || (p->vmp_wpmapped && pmap_is_modified(VM_PAGE_GET_PHYS_PAGE(p)))) {
						local_dirty_count++;
					}
				}
			}
		}
	} else {
		for (cur_offset = offset; cur_offset < end_offset; cur_offset += PAGE_SIZE_64) {
			p = vm_page_lookup(object, cur_offset);

			if (p != VM_PAGE_NULL) {
				local_resident_count++;

				if (count_dirty_pages) {
					if (p->vmp_dirty || (p->vmp_wpmapped && pmap_is_modified(VM_PAGE_GET_PHYS_PAGE(p)))) {
						local_dirty_count++;
					}
				}
			}
		}
	}

	if (resident_page_count != NULL) {
		*resident_page_count = local_resident_count;
	}

	if (dirty_page_count != NULL) {
		*dirty_page_count = local_dirty_count;
	}

out:
	return kr;
}


/*
 *	vm_object_reference:
 *
 *	Gets another reference to the given object.
 */
#ifdef vm_object_reference
#undef vm_object_reference
#endif
__private_extern__ void
vm_object_reference(
	vm_object_t     object)
{
	if (object == VM_OBJECT_NULL) {
		return;
	}

	vm_object_lock(object);
	assert(object->ref_count > 0);
	vm_object_reference_locked(object);
	vm_object_unlock(object);
}

/*
 * vm_object_transpose
 *
 * This routine takes two VM objects of the same size and exchanges
 * their backing store.
 * The objects should be "quiesced" via a UPL operation with UPL_SET_IO_WIRE
 * and UPL_BLOCK_ACCESS if they are referenced anywhere.
 *
 * The VM objects must not be locked by caller.
 */
unsigned int vm_object_transpose_count = 0;
kern_return_t
vm_object_transpose(
	vm_object_t             object1,
	vm_object_t             object2,
	vm_object_size_t        transpose_size)
{
	vm_object_t             tmp_object;
	kern_return_t           retval;
	boolean_t               object1_locked, object2_locked;
	vm_page_t               page;
	vm_object_offset_t      page_offset;

	tmp_object = VM_OBJECT_NULL;
	object1_locked = FALSE; object2_locked = FALSE;

	if (object1 == object2 ||
	    object1 == VM_OBJECT_NULL ||
	    object2 == VM_OBJECT_NULL) {
		/*
		 * If the 2 VM objects are the same, there's
		 * no point in exchanging their backing store.
		 */
		retval = KERN_INVALID_VALUE;
		goto done;
	}

	/*
	 * Since we need to lock both objects at the same time,
	 * make sure we always lock them in the same order to
	 * avoid deadlocks.
	 */
	if (object1 > object2) {
		tmp_object = object1;
		object1 = object2;
		object2 = tmp_object;
	}

	/*
	 * Allocate a temporary VM object to hold object1's contents
	 * while we copy object2 to object1.
	 */
	tmp_object = vm_object_allocate(transpose_size);
	vm_object_lock(tmp_object);
	VM_OBJECT_SET_CAN_PERSIST(tmp_object, FALSE);


	/*
	 * Grab control of the 1st VM object.
	 */
	vm_object_lock(object1);
	object1_locked = TRUE;
	if (!object1->alive || object1->terminating ||
	    object1->vo_copy || object1->shadow || object1->shadowed ||
	    object1->purgable != VM_PURGABLE_DENY) {
		/*
		 * We don't deal with copy or shadow objects (yet).
		 */
		retval = KERN_INVALID_VALUE;
		goto done;
	}
	/*
	 * We're about to mess with the object's backing store and
	 * taking a "paging_in_progress" reference wouldn't be enough
	 * to prevent any paging activity on this object, so the caller should
	 * have "quiesced" the objects beforehand, via a UPL operation with
	 * UPL_SET_IO_WIRE (to make sure all the pages are there and wired)
	 * and UPL_BLOCK_ACCESS (to mark the pages "busy").
	 *
	 * Wait for any paging operation to complete (but only paging, not
	 * other kind of activities not linked to the pager).  After we're
	 * statisfied that there's no more paging in progress, we keep the
	 * object locked, to guarantee that no one tries to access its pager.
	 */
	vm_object_paging_only_wait(object1, THREAD_UNINT);

	/*
	 * Same as above for the 2nd object...
	 */
	vm_object_lock(object2);
	object2_locked = TRUE;
	if (!object2->alive || object2->terminating ||
	    object2->vo_copy || object2->shadow || object2->shadowed ||
	    object2->purgable != VM_PURGABLE_DENY) {
		retval = KERN_INVALID_VALUE;
		goto done;
	}
	vm_object_paging_only_wait(object2, THREAD_UNINT);


	if (object1->vo_size != object2->vo_size ||
	    object1->vo_size != transpose_size) {
		/*
		 * If the 2 objects don't have the same size, we can't
		 * exchange their backing stores or one would overflow.
		 * If their size doesn't match the caller's
		 * "transpose_size", we can't do it either because the
		 * transpose operation will affect the entire span of
		 * the objects.
		 */
		retval = KERN_INVALID_VALUE;
		goto done;
	}


	/*
	 * Transpose the lists of resident pages.
	 * This also updates the resident_page_count and the memq_hint.
	 */
	if (object1->phys_contiguous || vm_page_queue_empty(&object1->memq)) {
		/*
		 * No pages in object1, just transfer pages
		 * from object2 to object1.  No need to go through
		 * an intermediate object.
		 */
		while (!vm_page_queue_empty(&object2->memq)) {
			page = (vm_page_t) vm_page_queue_first(&object2->memq);
			vm_page_rename(page, object1, page->vmp_offset);
		}
		assert(vm_page_queue_empty(&object2->memq));
	} else if (object2->phys_contiguous || vm_page_queue_empty(&object2->memq)) {
		/*
		 * No pages in object2, just transfer pages
		 * from object1 to object2.  No need to go through
		 * an intermediate object.
		 */
		while (!vm_page_queue_empty(&object1->memq)) {
			page = (vm_page_t) vm_page_queue_first(&object1->memq);
			vm_page_rename(page, object2, page->vmp_offset);
		}
		assert(vm_page_queue_empty(&object1->memq));
	} else {
		/* transfer object1's pages to tmp_object */
		while (!vm_page_queue_empty(&object1->memq)) {
			page = (vm_page_t) vm_page_queue_first(&object1->memq);
			page_offset = page->vmp_offset;
			vm_page_remove(page, TRUE);
			page->vmp_offset = page_offset;
			vm_page_queue_enter(&tmp_object->memq, page, vmp_listq);
		}
		assert(vm_page_queue_empty(&object1->memq));
		/* transfer object2's pages to object1 */
		while (!vm_page_queue_empty(&object2->memq)) {
			page = (vm_page_t) vm_page_queue_first(&object2->memq);
			vm_page_rename(page, object1, page->vmp_offset);
		}
		assert(vm_page_queue_empty(&object2->memq));
		/* transfer tmp_object's pages to object2 */
		while (!vm_page_queue_empty(&tmp_object->memq)) {
			page = (vm_page_t) vm_page_queue_first(&tmp_object->memq);
			vm_page_queue_remove(&tmp_object->memq, page, vmp_listq);
			vm_page_insert(page, object2, page->vmp_offset);
		}
		assert(vm_page_queue_empty(&tmp_object->memq));
	}

#define __TRANSPOSE_FIELD(field)                                \
MACRO_BEGIN                                                     \
	tmp_object->field = object1->field;                     \
	object1->field = object2->field;                        \
	object2->field = tmp_object->field;                     \
MACRO_END

	/* "Lock" refers to the object not its contents */
	/* "size" should be identical */
	assert(object1->vo_size == object2->vo_size);
	/* "memq_hint" was updated above when transposing pages */
	/* "ref_count" refers to the object not its contents */
	assert(object1->ref_count >= 1);
	assert(object2->ref_count >= 1);
	/* "resident_page_count" was updated above when transposing pages */
	/* "wired_page_count" was updated above when transposing pages */
#if !VM_TAG_ACTIVE_UPDATE
	/* "wired_objq" was dealt with along with "wired_page_count" */
#endif /* ! VM_TAG_ACTIVE_UPDATE */
	/* "reusable_page_count" was updated above when transposing pages */
	/* there should be no "copy" */
	assert(!object1->vo_copy);
	assert(!object2->vo_copy);
	/* there should be no "shadow" */
	assert(!object1->shadow);
	assert(!object2->shadow);
	__TRANSPOSE_FIELD(vo_shadow_offset); /* used by phys_contiguous objects */
	__TRANSPOSE_FIELD(pager);
	__TRANSPOSE_FIELD(paging_offset);
	__TRANSPOSE_FIELD(pager_control);
	/* update the memory_objects' pointers back to the VM objects */
	if (object1->pager_control != MEMORY_OBJECT_CONTROL_NULL) {
		memory_object_control_collapse(&object1->pager_control,
		    object1);
	}
	if (object2->pager_control != MEMORY_OBJECT_CONTROL_NULL) {
		memory_object_control_collapse(&object2->pager_control,
		    object2);
	}
	__TRANSPOSE_FIELD(copy_strategy);
	/* "paging_in_progress" refers to the object not its contents */
	assert(!object1->paging_in_progress);
	assert(!object2->paging_in_progress);
	assert(object1->activity_in_progress);
	assert(object2->activity_in_progress);
	/* "all_wanted" refers to the object not its contents */
	__TRANSPOSE_FIELD(pager_created);
	__TRANSPOSE_FIELD(pager_initialized);
	__TRANSPOSE_FIELD(pager_ready);
	__TRANSPOSE_FIELD(pager_trusted);
	__TRANSPOSE_FIELD(can_persist);
	__TRANSPOSE_FIELD(internal);
	__TRANSPOSE_FIELD(private);
	__TRANSPOSE_FIELD(pageout);
	/* "alive" should be set */
	assert(object1->alive);
	assert(object2->alive);
	/* "purgeable" should be non-purgeable */
	assert(object1->purgable == VM_PURGABLE_DENY);
	assert(object2->purgable == VM_PURGABLE_DENY);
	/* "shadowed" refers to the the object not its contents */
	__TRANSPOSE_FIELD(purgeable_when_ripe);
	__TRANSPOSE_FIELD(true_share);
	/* "terminating" should not be set */
	assert(!object1->terminating);
	assert(!object2->terminating);
	/* transfer "named" reference if needed */
	if (object1->named && !object2->named) {
		assert(object1->ref_count >= 2);
		assert(object2->ref_count >= 1);
		object1->ref_count--;
		object2->ref_count++;
	} else if (!object1->named && object2->named) {
		assert(object1->ref_count >= 1);
		assert(object2->ref_count >= 2);
		object1->ref_count++;
		object2->ref_count--;
	}
	__TRANSPOSE_FIELD(named);
	/* "shadow_severed" refers to the object not its contents */
	__TRANSPOSE_FIELD(phys_contiguous);
	__TRANSPOSE_FIELD(nophyscache);
	__TRANSPOSE_FIELD(no_pager_reason);
	/* "cached_list.next" points to transposed object */
	object1->cached_list.next = (queue_entry_t) object2;
	object2->cached_list.next = (queue_entry_t) object1;
	/* "cached_list.prev" should be NULL */
	assert(object1->cached_list.prev == NULL);
	assert(object2->cached_list.prev == NULL);
	__TRANSPOSE_FIELD(last_alloc);
	__TRANSPOSE_FIELD(sequential);
	__TRANSPOSE_FIELD(pages_created);
	__TRANSPOSE_FIELD(pages_used);
	__TRANSPOSE_FIELD(scan_collisions);
	__TRANSPOSE_FIELD(cow_hint);
	__TRANSPOSE_FIELD(wimg_bits);
	__TRANSPOSE_FIELD(set_cache_attr);
	__TRANSPOSE_FIELD(code_signed);
	object1->transposed = TRUE;
	object2->transposed = TRUE;
	__TRANSPOSE_FIELD(mapping_in_progress);
	__TRANSPOSE_FIELD(volatile_empty);
	__TRANSPOSE_FIELD(volatile_fault);
	__TRANSPOSE_FIELD(all_reusable);
	assert(object1->blocked_access);
	assert(object2->blocked_access);
	__TRANSPOSE_FIELD(set_cache_attr);
	assert(!object1->object_is_shared_cache);
	assert(!object2->object_is_shared_cache);
	/* ignore purgeable_queue_type and purgeable_queue_group */
	assert(!object1->io_tracking);
	assert(!object2->io_tracking);
#if VM_OBJECT_ACCESS_TRACKING
	assert(!object1->access_tracking);
	assert(!object2->access_tracking);
#endif /* VM_OBJECT_ACCESS_TRACKING */
	__TRANSPOSE_FIELD(no_tag_update);
#if CONFIG_SECLUDED_MEMORY
	assert(!object1->eligible_for_secluded);
	assert(!object2->eligible_for_secluded);
	assert(!object1->can_grab_secluded);
	assert(!object2->can_grab_secluded);
#else /* CONFIG_SECLUDED_MEMORY */
	assert(object1->__object3_unused_bits == 0);
	assert(object2->__object3_unused_bits == 0);
#endif /* CONFIG_SECLUDED_MEMORY */
#if UPL_DEBUG
	/* "uplq" refers to the object not its contents (see upl_transpose()) */
#endif
	assert((object1->purgable == VM_PURGABLE_DENY) || (object1->objq.next == NULL));
	assert((object1->purgable == VM_PURGABLE_DENY) || (object1->objq.prev == NULL));
	assert((object2->purgable == VM_PURGABLE_DENY) || (object2->objq.next == NULL));
	assert((object2->purgable == VM_PURGABLE_DENY) || (object2->objq.prev == NULL));

#undef __TRANSPOSE_FIELD

	retval = KERN_SUCCESS;

done:
	/*
	 * Cleanup.
	 */
	if (tmp_object != VM_OBJECT_NULL) {
		vm_object_unlock(tmp_object);
		/*
		 * Re-initialize the temporary object to avoid
		 * deallocating a real pager.
		 */
		_vm_object_allocate(transpose_size, tmp_object);
		vm_object_deallocate(tmp_object);
		tmp_object = VM_OBJECT_NULL;
	}

	if (object1_locked) {
		vm_object_unlock(object1);
		object1_locked = FALSE;
	}
	if (object2_locked) {
		vm_object_unlock(object2);
		object2_locked = FALSE;
	}

	vm_object_transpose_count++;

	return retval;
}


/*
 *      vm_object_cluster_size
 *
 *      Determine how big a cluster we should issue an I/O for...
 *
 *	Inputs:   *start == offset of page needed
 *		  *length == maximum cluster pager can handle
 *	Outputs:  *start == beginning offset of cluster
 *		  *length == length of cluster to try
 *
 *	The original *start will be encompassed by the cluster
 *
 */
extern int speculative_reads_disabled;

/*
 * Try to always keep these values an even multiple of PAGE_SIZE. We use these values
 * to derive min_ph_bytes and max_ph_bytes (IMP: bytes not # of pages) and expect those values to
 * always be page-aligned. The derivation could involve operations (e.g. division)
 * that could give us non-page-size aligned values if we start out with values that
 * are odd multiples of PAGE_SIZE.
 */
#if !XNU_TARGET_OS_OSX
unsigned int preheat_max_bytes = (1024 * 512);
#else /* !XNU_TARGET_OS_OSX */
unsigned int preheat_max_bytes = MAX_UPL_TRANSFER_BYTES;
#endif /* !XNU_TARGET_OS_OSX */
unsigned int preheat_min_bytes = (1024 * 32);


__private_extern__ void
vm_object_cluster_size(vm_object_t object, vm_object_offset_t *start,
    vm_size_t *length, vm_object_fault_info_t fault_info, uint32_t *io_streaming)
{
	vm_size_t               pre_heat_size;
	vm_size_t               tail_size;
	vm_size_t               head_size;
	vm_size_t               max_length;
	vm_size_t               cluster_size;
	vm_object_offset_t      object_size;
	vm_object_offset_t      orig_start;
	vm_object_offset_t      target_start;
	vm_object_offset_t      offset;
	vm_behavior_t           behavior;
	boolean_t               look_behind = TRUE;
	boolean_t               look_ahead  = TRUE;
	boolean_t               isSSD = FALSE;
	uint32_t                throttle_limit;
	int                     sequential_run;
	int                     sequential_behavior = VM_BEHAVIOR_SEQUENTIAL;
	vm_size_t               max_ph_size;
	vm_size_t               min_ph_size;

	assert( !(*length & PAGE_MASK));
	assert( !(*start & PAGE_MASK_64));

	/*
	 * remember maxiumum length of run requested
	 */
	max_length = *length;
	/*
	 * we'll always return a cluster size of at least
	 * 1 page, since the original fault must always
	 * be processed
	 */
	*length = PAGE_SIZE;
	*io_streaming = 0;

	if (speculative_reads_disabled || fault_info == NULL) {
		/*
		 * no cluster... just fault the page in
		 */
		return;
	}
	orig_start = *start;
	target_start = orig_start;
	cluster_size = round_page(fault_info->cluster_size);
	behavior = fault_info->behavior;

	vm_object_lock(object);

	if (object->pager == MEMORY_OBJECT_NULL) {
		goto out;       /* pager is gone for this object, nothing more to do */
	}
	vnode_pager_get_isSSD(object->pager, &isSSD);

	min_ph_size = round_page(preheat_min_bytes);
	max_ph_size = round_page(preheat_max_bytes);

#if XNU_TARGET_OS_OSX
	/*
	 * If we're paging from an SSD, we cut the minimum cluster size in half
	 * and reduce the maximum size by a factor of 8. We do this because the
	 * latency to issue an I/O is a couple of orders of magnitude smaller than
	 * on spinning media, so being overly aggressive on the cluster size (to
	 * try and reduce cumulative seek penalties) isn't a good trade off over
	 * the increased memory pressure caused by the larger speculative I/Os.
	 * However, the latency isn't 0, so a small amount of clustering is still
	 * a win.
	 *
	 * If an explicit cluster size has already been provided, then we're
	 * receiving a strong hint that the entire range will be needed (e.g.
	 * wiring, willneed). In these cases, we want to maximize the I/O size
	 * to minimize the number of I/Os issued.
	 */
	if (isSSD && cluster_size <= PAGE_SIZE) {
		min_ph_size /= 2;
		max_ph_size /= 8;

		if (min_ph_size & PAGE_MASK_64) {
			min_ph_size = trunc_page(min_ph_size);
		}

		if (max_ph_size & PAGE_MASK_64) {
			max_ph_size = trunc_page(max_ph_size);
		}
	}
#endif /* XNU_TARGET_OS_OSX */

	if (min_ph_size < PAGE_SIZE) {
		min_ph_size = PAGE_SIZE;
	}

	if (max_ph_size < PAGE_SIZE) {
		max_ph_size = PAGE_SIZE;
	} else if (max_ph_size > MAX_UPL_TRANSFER_BYTES) {
		max_ph_size = MAX_UPL_TRANSFER_BYTES;
	}

	if (max_length > max_ph_size) {
		max_length = max_ph_size;
	}

	if (max_length <= PAGE_SIZE) {
		goto out;
	}

	if (object->internal) {
		object_size = object->vo_size;
	} else {
		vnode_pager_get_object_size(object->pager, &object_size);
	}

	object_size = round_page_64(object_size);

	if (orig_start >= object_size) {
		/*
		 * fault occurred beyond the EOF...
		 * we need to punt w/o changing the
		 * starting offset
		 */
		goto out;
	}
	if (object->pages_used > object->pages_created) {
		/*
		 * must have wrapped our 32 bit counters
		 * so reset
		 */
		object->pages_used = object->pages_created = 0;
	}
	if ((sequential_run = object->sequential)) {
		if (sequential_run < 0) {
			sequential_behavior = VM_BEHAVIOR_RSEQNTL;
			sequential_run = 0 - sequential_run;
		} else {
			sequential_behavior = VM_BEHAVIOR_SEQUENTIAL;
		}
	}
	switch (behavior) {
	default:
		behavior = VM_BEHAVIOR_DEFAULT;
		OS_FALLTHROUGH;

	case VM_BEHAVIOR_DEFAULT:
		if (object->internal && fault_info->user_tag == VM_MEMORY_STACK) {
			goto out;
		}

		if (sequential_run >= (3 * PAGE_SIZE)) {
			pre_heat_size = sequential_run + PAGE_SIZE;

			if (sequential_behavior == VM_BEHAVIOR_SEQUENTIAL) {
				look_behind = FALSE;
			} else {
				look_ahead = FALSE;
			}

			*io_streaming = 1;
		} else {
			if (object->pages_created < (20 * (min_ph_size >> PAGE_SHIFT))) {
				/*
				 * prime the pump
				 */
				pre_heat_size = min_ph_size;
			} else {
				/*
				 * Linear growth in PH size: The maximum size is max_length...
				 * this cacluation will result in a size that is neither a
				 * power of 2 nor a multiple of PAGE_SIZE... so round
				 * it up to the nearest PAGE_SIZE boundary
				 */
				pre_heat_size = (max_length * (uint64_t)object->pages_used) / object->pages_created;

				if (pre_heat_size < min_ph_size) {
					pre_heat_size = min_ph_size;
				} else {
					pre_heat_size = round_page(pre_heat_size);
				}
			}
		}
		break;

	case VM_BEHAVIOR_RANDOM:
		if ((pre_heat_size = cluster_size) <= PAGE_SIZE) {
			goto out;
		}
		break;

	case VM_BEHAVIOR_SEQUENTIAL:
		if ((pre_heat_size = cluster_size) == 0) {
			pre_heat_size = sequential_run + PAGE_SIZE;
		}
		look_behind = FALSE;
		*io_streaming = 1;

		break;

	case VM_BEHAVIOR_RSEQNTL:
		if ((pre_heat_size = cluster_size) == 0) {
			pre_heat_size = sequential_run + PAGE_SIZE;
		}
		look_ahead = FALSE;
		*io_streaming = 1;

		break;
	}
	throttle_limit = (uint32_t) max_length;
	assert(throttle_limit == max_length);

	if (vnode_pager_get_throttle_io_limit(object->pager, &throttle_limit) == KERN_SUCCESS) {
		if (max_length > throttle_limit) {
			max_length = throttle_limit;
		}
	}
	if (pre_heat_size > max_length) {
		pre_heat_size = max_length;
	}

	if (behavior == VM_BEHAVIOR_DEFAULT && (pre_heat_size > min_ph_size)) {
		unsigned int consider_free = vm_page_free_count + vm_page_cleaned_count;

		if (consider_free < vm_page_throttle_limit) {
			pre_heat_size = trunc_page(pre_heat_size / 16);
		} else if (consider_free < vm_page_free_target) {
			pre_heat_size = trunc_page(pre_heat_size / 4);
		}

		if (pre_heat_size < min_ph_size) {
			pre_heat_size = min_ph_size;
		}
	}
	if (look_ahead == TRUE) {
		if (look_behind == TRUE) {
			/*
			 * if we get here its due to a random access...
			 * so we want to center the original fault address
			 * within the cluster we will issue... make sure
			 * to calculate 'head_size' as a multiple of PAGE_SIZE...
			 * 'pre_heat_size' is a multiple of PAGE_SIZE but not
			 * necessarily an even number of pages so we need to truncate
			 * the result to a PAGE_SIZE boundary
			 */
			head_size = trunc_page(pre_heat_size / 2);

			if (target_start > head_size) {
				target_start -= head_size;
			} else {
				target_start = 0;
			}

			/*
			 * 'target_start' at this point represents the beginning offset
			 * of the cluster we are considering... 'orig_start' will be in
			 * the center of this cluster if we didn't have to clip the start
			 * due to running into the start of the file
			 */
		}
		if ((target_start + pre_heat_size) > object_size) {
			pre_heat_size = (vm_size_t)(round_page_64(object_size - target_start));
		}
		/*
		 * at this point caclulate the number of pages beyond the original fault
		 * address that we want to consider... this is guaranteed not to extend beyond
		 * the current EOF...
		 */
		assert((vm_size_t)(orig_start - target_start) == (orig_start - target_start));
		tail_size = pre_heat_size - (vm_size_t)(orig_start - target_start) - PAGE_SIZE;
	} else {
		if (pre_heat_size > target_start) {
			/*
			 * since pre_heat_size is always smaller then 2^32,
			 * if it is larger then target_start (a 64 bit value)
			 * it is safe to clip target_start to 32 bits
			 */
			pre_heat_size = (vm_size_t) target_start;
		}
		tail_size = 0;
	}
	assert( !(target_start & PAGE_MASK_64));
	assert( !(pre_heat_size & PAGE_MASK_64));

	if (pre_heat_size <= PAGE_SIZE) {
		goto out;
	}

	if (look_behind == TRUE) {
		/*
		 * take a look at the pages before the original
		 * faulting offset... recalculate this in case
		 * we had to clip 'pre_heat_size' above to keep
		 * from running past the EOF.
		 */
		head_size = pre_heat_size - tail_size - PAGE_SIZE;

		for (offset = orig_start - PAGE_SIZE_64; head_size; offset -= PAGE_SIZE_64, head_size -= PAGE_SIZE) {
			/*
			 * don't poke below the lowest offset
			 */
			if (offset < fault_info->lo_offset) {
				break;
			}
			/*
			 * for external objects or internal objects w/o a pager,
			 * vm_object_compressor_pager_state_get will return VM_EXTERNAL_STATE_UNKNOWN
			 */
			if (vm_object_compressor_pager_state_get(object, offset) == VM_EXTERNAL_STATE_ABSENT) {
				break;
			}
			if (vm_page_lookup(object, offset) != VM_PAGE_NULL) {
				/*
				 * don't bridge resident pages
				 */
				break;
			}
			*start = offset;
			*length += PAGE_SIZE;
		}
	}
	if (look_ahead == TRUE) {
		for (offset = orig_start + PAGE_SIZE_64; tail_size; offset += PAGE_SIZE_64, tail_size -= PAGE_SIZE) {
			/*
			 * don't poke above the highest offset
			 */
			if (offset >= fault_info->hi_offset) {
				break;
			}
			assert(offset < object_size);

			/*
			 * for external objects or internal objects w/o a pager,
			 * vm_object_compressor_pager_state_get will return VM_EXTERNAL_STATE_UNKNOWN
			 */
			if (vm_object_compressor_pager_state_get(object, offset) == VM_EXTERNAL_STATE_ABSENT) {
				break;
			}
			if (vm_page_lookup(object, offset) != VM_PAGE_NULL) {
				/*
				 * don't bridge resident pages
				 */
				break;
			}
			*length += PAGE_SIZE;
		}
	}
out:
	if (*length > max_length) {
		*length = max_length;
	}

	vm_object_unlock(object);

	DTRACE_VM1(clustersize, vm_size_t, *length);
}


/*
 * Allow manipulation of individual page state.  This is actually part of
 * the UPL regimen but takes place on the VM object rather than on a UPL
 */

kern_return_t
vm_object_page_op(
	vm_object_t             object,
	vm_object_offset_t      offset,
	int                     ops,
	ppnum_t                 *phys_entry,
	int                     *flags)
{
	vm_page_t               dst_page;

	vm_object_lock(object);

	if (ops & UPL_POP_PHYSICAL) {
		if (object->phys_contiguous) {
			if (phys_entry) {
				*phys_entry = (ppnum_t)
				    (object->vo_shadow_offset >> PAGE_SHIFT);
			}
			vm_object_unlock(object);
			return KERN_SUCCESS;
		} else {
			vm_object_unlock(object);
			return KERN_INVALID_OBJECT;
		}
	}
	if (object->phys_contiguous) {
		vm_object_unlock(object);
		return KERN_INVALID_OBJECT;
	}

	while (TRUE) {
		if ((dst_page = vm_page_lookup(object, offset)) == VM_PAGE_NULL) {
			vm_object_unlock(object);
			return KERN_FAILURE;
		}

		/* Sync up on getting the busy bit */
		if ((dst_page->vmp_busy || dst_page->vmp_cleaning) &&
		    (((ops & UPL_POP_SET) &&
		    (ops & UPL_POP_BUSY)) || (ops & UPL_POP_DUMP))) {
			/* someone else is playing with the page, we will */
			/* have to wait */
			vm_page_sleep(object, dst_page, THREAD_UNINT, LCK_SLEEP_DEFAULT);
			continue;
		}

		if (ops & UPL_POP_DUMP) {
			if (dst_page->vmp_pmapped == TRUE) {
				pmap_disconnect(VM_PAGE_GET_PHYS_PAGE(dst_page));
			}

			VM_PAGE_FREE(dst_page);
			break;
		}

		if (flags) {
			*flags = 0;

			/* Get the condition of flags before requested ops */
			/* are undertaken */

			if (dst_page->vmp_dirty) {
				*flags |= UPL_POP_DIRTY;
			}
			if (dst_page->vmp_free_when_done) {
				*flags |= UPL_POP_PAGEOUT;
			}
			if (dst_page->vmp_precious) {
				*flags |= UPL_POP_PRECIOUS;
			}
			if (dst_page->vmp_absent) {
				*flags |= UPL_POP_ABSENT;
			}
			if (dst_page->vmp_busy) {
				*flags |= UPL_POP_BUSY;
			}
		}

		/* The caller should have made a call either contingent with */
		/* or prior to this call to set UPL_POP_BUSY */
		if (ops & UPL_POP_SET) {
			/* The protection granted with this assert will */
			/* not be complete.  If the caller violates the */
			/* convention and attempts to change page state */
			/* without first setting busy we may not see it */
			/* because the page may already be busy.  However */
			/* if such violations occur we will assert sooner */
			/* or later. */
			assert(dst_page->vmp_busy || (ops & UPL_POP_BUSY));
			if (ops & UPL_POP_DIRTY) {
				SET_PAGE_DIRTY(dst_page, FALSE);
			}
			if (ops & UPL_POP_PAGEOUT) {
				dst_page->vmp_free_when_done = TRUE;
			}
			if (ops & UPL_POP_PRECIOUS) {
				dst_page->vmp_precious = TRUE;
			}
			if (ops & UPL_POP_ABSENT) {
				dst_page->vmp_absent = TRUE;
			}
			if (ops & UPL_POP_BUSY) {
				dst_page->vmp_busy = TRUE;
			}
		}

		if (ops & UPL_POP_CLR) {
			assert(dst_page->vmp_busy);
			if (ops & UPL_POP_DIRTY) {
				dst_page->vmp_dirty = FALSE;
			}
			if (ops & UPL_POP_PAGEOUT) {
				dst_page->vmp_free_when_done = FALSE;
			}
			if (ops & UPL_POP_PRECIOUS) {
				dst_page->vmp_precious = FALSE;
			}
			if (ops & UPL_POP_ABSENT) {
				dst_page->vmp_absent = FALSE;
			}
			if (ops & UPL_POP_BUSY) {
				dst_page->vmp_busy = FALSE;
				vm_page_wakeup(object, dst_page);
			}
		}
		if (phys_entry) {
			/*
			 * The physical page number will remain valid
			 * only if the page is kept busy.
			 */
			assert(dst_page->vmp_busy);
			*phys_entry = VM_PAGE_GET_PHYS_PAGE(dst_page);
		}

		break;
	}

	vm_object_unlock(object);
	return KERN_SUCCESS;
}

/*
 * vm_object_range_op offers performance enhancement over
 * vm_object_page_op for page_op functions which do not require page
 * level state to be returned from the call.  Page_op was created to provide
 * a low-cost alternative to page manipulation via UPLs when only a single
 * page was involved.  The range_op call establishes the ability in the _op
 * family of functions to work on multiple pages where the lack of page level
 * state handling allows the caller to avoid the overhead of the upl structures.
 */

kern_return_t
vm_object_range_op(
	vm_object_t             object,
	vm_object_offset_t      offset_beg,
	vm_object_offset_t      offset_end,
	int                     ops,
	uint32_t                *range)
{
	vm_object_offset_t      offset;
	vm_page_t               dst_page;

	if (object->resident_page_count == 0) {
		if (range) {
			if (ops & UPL_ROP_PRESENT) {
				*range = 0;
			} else {
				*range = (uint32_t) (offset_end - offset_beg);
				assert(*range == (offset_end - offset_beg));
			}
		}
		return KERN_SUCCESS;
	}
	vm_object_lock(object);

	if (object->phys_contiguous) {
		vm_object_unlock(object);
		return KERN_INVALID_OBJECT;
	}

	offset = offset_beg & ~PAGE_MASK_64;

	while (offset < offset_end) {
		dst_page = vm_page_lookup(object, offset);
		if (dst_page != VM_PAGE_NULL) {
			if (ops & UPL_ROP_DUMP) {
				if (dst_page->vmp_busy || dst_page->vmp_cleaning) {
					/*
					 * someone else is playing with the
					 * page, we will have to wait
					 */
					vm_page_sleep(object, dst_page, THREAD_UNINT, LCK_SLEEP_DEFAULT);
					/*
					 * need to relook the page up since it's
					 * state may have changed while we slept
					 * it might even belong to a different object
					 * at this point
					 */
					continue;
				}
				if (dst_page->vmp_laundry) {
					vm_pageout_steal_laundry(dst_page, FALSE);
				}

				if (dst_page->vmp_pmapped == TRUE) {
					pmap_disconnect(VM_PAGE_GET_PHYS_PAGE(dst_page));
				}

				VM_PAGE_FREE(dst_page);
			} else if ((ops & UPL_ROP_ABSENT)
			    && (!dst_page->vmp_absent || dst_page->vmp_busy)) {
				break;
			}
		} else if (ops & UPL_ROP_PRESENT) {
			break;
		}

		offset += PAGE_SIZE;
	}
	vm_object_unlock(object);

	if (range) {
		if (offset > offset_end) {
			offset = offset_end;
		}
		if (offset > offset_beg) {
			*range = (uint32_t) (offset - offset_beg);
			assert(*range == (offset - offset_beg));
		} else {
			*range = 0;
		}
	}
	return KERN_SUCCESS;
}

/*
 * Used to point a pager directly to a range of memory (when the pager may be associated
 *   with a non-device vnode).  Takes a virtual address, an offset, and a size.  We currently
 *   expect that the virtual address will denote the start of a range that is physically contiguous.
 */
kern_return_t
pager_map_to_phys_contiguous(
	memory_object_control_t object,
	memory_object_offset_t  offset,
	addr64_t                base_vaddr,
	vm_size_t               size)
{
	ppnum_t page_num;
	boolean_t clobbered_private;
	kern_return_t retval;
	vm_object_t pager_object;

	page_num = pmap_find_phys(kernel_pmap, base_vaddr);

	if (!page_num) {
		retval = KERN_FAILURE;
		goto out;
	}

	pager_object = memory_object_control_to_vm_object(object);

	if (!pager_object) {
		retval = KERN_FAILURE;
		goto out;
	}

	clobbered_private = pager_object->private;
	if (pager_object->private != TRUE) {
		vm_object_lock(pager_object);
		VM_OBJECT_SET_PRIVATE(pager_object, TRUE);
		vm_object_unlock(pager_object);
	}
	retval = vm_object_populate_with_private(pager_object, offset, page_num, size);

	if (retval != KERN_SUCCESS) {
		if (pager_object->private != clobbered_private) {
			vm_object_lock(pager_object);
			VM_OBJECT_SET_PRIVATE(pager_object, clobbered_private);
			vm_object_unlock(pager_object);
		}
	}

out:
	return retval;
}

uint32_t scan_object_collision = 0;

void
vm_object_lock(vm_object_t object)
{
	if (object == vm_pageout_scan_wants_object) {
		scan_object_collision++;
		mutex_pause(2);
	}
	DTRACE_VM(vm_object_lock_w);
	lck_rw_lock_exclusive(&object->Lock);
}

boolean_t
vm_object_lock_avoid(vm_object_t object)
{
	if (object == vm_pageout_scan_wants_object) {
		scan_object_collision++;
		return TRUE;
	}
	return FALSE;
}

boolean_t
_vm_object_lock_try(vm_object_t object)
{
	boolean_t       retval;

	retval = lck_rw_try_lock_exclusive(&object->Lock);
#if DEVELOPMENT || DEBUG
	if (retval == TRUE) {
		DTRACE_VM(vm_object_lock_w);
	}
#endif
	return retval;
}

boolean_t
vm_object_lock_try(vm_object_t object)
{
	/*
	 * Called from hibernate path so check before blocking.
	 */
	if (vm_object_lock_avoid(object) && ml_get_interrupts_enabled() && get_preemption_level() == 0) {
		mutex_pause(2);
	}
	return _vm_object_lock_try(object);
}

/*
 * Lock the object exclusive.
 *
 * Returns true iff the thread had to spin or block before
 * acquiring the lock.
 */
bool
vm_object_lock_check_contended(vm_object_t object)
{
	if (object == vm_pageout_scan_wants_object) {
		scan_object_collision++;
		mutex_pause(2);
	}
	DTRACE_VM(vm_object_lock_w);
	return lck_rw_lock_exclusive_check_contended(&object->Lock);
}

void
vm_object_lock_shared(vm_object_t object)
{
	if (vm_object_lock_avoid(object)) {
		mutex_pause(2);
	}
	DTRACE_VM(vm_object_lock_r);
	lck_rw_lock_shared(&object->Lock);
}

boolean_t
vm_object_lock_yield_shared(vm_object_t object)
{
	boolean_t retval = FALSE, force_yield = FALSE;

	vm_object_lock_assert_shared(object);

	force_yield = vm_object_lock_avoid(object);

	retval = lck_rw_lock_yield_shared(&object->Lock, force_yield);
	if (retval) {
		DTRACE_VM(vm_object_lock_yield);
	}

	return retval;
}

boolean_t
vm_object_lock_try_shared(vm_object_t object)
{
	boolean_t retval;

	if (vm_object_lock_avoid(object)) {
		mutex_pause(2);
	}
	retval = lck_rw_try_lock_shared(&object->Lock);
	if (retval) {
		DTRACE_VM(vm_object_lock_r);
	}
	return retval;
}

boolean_t
vm_object_lock_upgrade(vm_object_t object)
{
	boolean_t       retval;

	retval = lck_rw_lock_shared_to_exclusive(&object->Lock);
#if DEVELOPMENT || DEBUG
	if (retval == TRUE) {
		DTRACE_VM(vm_object_lock_w);
	}
#endif
	return retval;
}

void
vm_object_unlock(vm_object_t object)
{
#if DEVELOPMENT || DEBUG
	DTRACE_VM(vm_object_unlock);
#endif
	lck_rw_done(&object->Lock);
}


unsigned int vm_object_change_wimg_mode_count = 0;

/*
 * The object must be locked
 */
void
vm_object_change_wimg_mode(vm_object_t object, unsigned int wimg_mode)
{
	vm_object_lock_assert_exclusive(object);

	vm_object_paging_only_wait(object, THREAD_UNINT);

	const unified_page_list_t pmap_batch_list = {
		.pageq = &object->memq,
		.type = UNIFIED_PAGE_LIST_TYPE_VM_PAGE_OBJ_Q,
	};
	pmap_batch_set_cache_attributes(&pmap_batch_list, wimg_mode);
	if (wimg_mode == VM_WIMG_USE_DEFAULT) {
		object->set_cache_attr = FALSE;
	} else {
		object->set_cache_attr = TRUE;
	}

	object->wimg_bits = wimg_mode;

	vm_object_change_wimg_mode_count++;
}

#if CONFIG_FREEZE

extern struct freezer_context   freezer_context_global;

/*
 * This routine does the "relocation" of previously
 * compressed pages belonging to this object that are
 * residing in a number of compressed segments into
 * a set of compressed segments dedicated to hold
 * compressed pages belonging to this object.
 */

extern AbsoluteTime c_freezer_last_yield_ts;

#define MAX_FREE_BATCH  32
#define FREEZER_DUTY_CYCLE_ON_MS        5
#define FREEZER_DUTY_CYCLE_OFF_MS       5

static int c_freezer_should_yield(void);


static int
c_freezer_should_yield()
{
	AbsoluteTime    cur_time;
	uint64_t        nsecs;

	assert(c_freezer_last_yield_ts);
	clock_get_uptime(&cur_time);

	SUB_ABSOLUTETIME(&cur_time, &c_freezer_last_yield_ts);
	absolutetime_to_nanoseconds(cur_time, &nsecs);

	if (nsecs > 1000 * 1000 * FREEZER_DUTY_CYCLE_ON_MS) {
		return 1;
	}
	return 0;
}


void
vm_object_compressed_freezer_done()
{
	vm_compressor_finished_filling( &(freezer_context_global.freezer_ctx_chead));
}


uint32_t
vm_object_compressed_freezer_pageout(
	vm_object_t object, uint32_t dirty_budget)
{
	vm_page_t                       p;
	vm_page_t                       local_freeq = NULL;
	int                             local_freed = 0;
	kern_return_t                   retval = KERN_SUCCESS;
	int                             obj_resident_page_count_snapshot = 0;
	uint32_t                        paged_out_count = 0;

	assert(object != VM_OBJECT_NULL);
	assert(object->internal);

	vm_object_lock(object);

	if (!object->pager_initialized || object->pager == MEMORY_OBJECT_NULL) {
		if (!object->pager_initialized) {
			vm_object_collapse(object, (vm_object_offset_t) 0, TRUE);

			if (!object->pager_initialized) {
				vm_object_compressor_pager_create(object);
			}
		}

		if (!object->pager_initialized || object->pager == MEMORY_OBJECT_NULL) {
			vm_object_unlock(object);
			return paged_out_count;
		}
	}

	/*
	 * We could be freezing a shared internal object that might
	 * be part of some other thread's current VM operations.
	 * We skip it if there's a paging-in-progress or activity-in-progress
	 * because we could be here a long time with the map lock held.
	 *
	 * Note: We are holding the map locked while we wait.
	 * This is fine in the freezer path because the task
	 * is suspended and so this latency is acceptable.
	 */
	if (object->paging_in_progress || object->activity_in_progress) {
		vm_object_unlock(object);
		return paged_out_count;
	}

	if (VM_CONFIG_FREEZER_SWAP_IS_ACTIVE) {
		vm_object_offset_t      curr_offset = 0;

		/*
		 * Go through the object and make sure that any
		 * previously compressed pages are relocated into
		 * a compressed segment associated with our "freezer_chead".
		 */
		while (curr_offset < object->vo_size) {
			curr_offset = vm_compressor_pager_next_compressed(object->pager, curr_offset);

			if (curr_offset == (vm_object_offset_t) -1) {
				break;
			}

			retval = vm_compressor_pager_relocate(object->pager, curr_offset, &(freezer_context_global.freezer_ctx_chead));

			if (retval != KERN_SUCCESS) {
				break;
			}

			curr_offset += PAGE_SIZE_64;
		}
	}

	/*
	 * We can't hold the object lock while heading down into the compressed pager
	 * layer because we might need the kernel map lock down there to allocate new
	 * compressor data structures. And if this same object is mapped in the kernel
	 * and there's a fault on it, then that thread will want the object lock while
	 * holding the kernel map lock.
	 *
	 * Since we are going to drop/grab the object lock repeatedly, we must make sure
	 * we won't be stuck in an infinite loop if the same page(s) keep getting
	 * decompressed. So we grab a snapshot of the number of pages in the object and
	 * we won't process any more than that number of pages.
	 */

	obj_resident_page_count_snapshot = object->resident_page_count;

	vm_object_activity_begin(object);

	while ((obj_resident_page_count_snapshot--) && !vm_page_queue_empty(&object->memq) && paged_out_count < dirty_budget) {
		p = (vm_page_t)vm_page_queue_first(&object->memq);

		KDBG_DEBUG(0xe0430004 | DBG_FUNC_START, object, local_freed);

		vm_page_lockspin_queues();

		if (p->vmp_cleaning || p->vmp_fictitious || p->vmp_busy || p->vmp_absent || p->vmp_unusual || VMP_ERROR_GET(p) || VM_PAGE_WIRED(p)) {
			vm_page_unlock_queues();

			KDBG_DEBUG(0xe0430004 | DBG_FUNC_END, object, local_freed, 1);

			vm_page_queue_remove(&object->memq, p, vmp_listq);
			vm_page_queue_enter(&object->memq, p, vmp_listq);

			continue;
		}

		if (p->vmp_pmapped == TRUE) {
			int refmod_state, pmap_flags;

			if (p->vmp_dirty || p->vmp_precious) {
				pmap_flags = PMAP_OPTIONS_COMPRESSOR;
			} else {
				pmap_flags = PMAP_OPTIONS_COMPRESSOR_IFF_MODIFIED;
			}

			vm_page_lockconvert_queues();
			refmod_state = pmap_disconnect_options(VM_PAGE_GET_PHYS_PAGE(p), pmap_flags, NULL);
			if (refmod_state & VM_MEM_MODIFIED) {
				SET_PAGE_DIRTY(p, FALSE);
			}
		}

		if (p->vmp_dirty == FALSE && p->vmp_precious == FALSE) {
			/*
			 * Clean and non-precious page.
			 */
			vm_page_unlock_queues();
			VM_PAGE_FREE(p);

			KDBG_DEBUG(0xe0430004 | DBG_FUNC_END, object, local_freed, 2);
			continue;
		}

		if (p->vmp_laundry) {
			vm_pageout_steal_laundry(p, TRUE);
		}

		vm_page_queues_remove(p, TRUE);

		vm_page_unlock_queues();


		/*
		 * In case the compressor fails to compress this page, we need it at
		 * the back of the object memq so that we don't keep trying to process it.
		 * Make the move here while we have the object lock held.
		 */

		vm_page_queue_remove(&object->memq, p, vmp_listq);
		vm_page_queue_enter(&object->memq, p, vmp_listq);

		/*
		 * Grab an activity_in_progress here for vm_pageout_compress_page() to consume.
		 *
		 * Mark the page busy so no one messes with it while we have the object lock dropped.
		 */
		p->vmp_busy = TRUE;

		vm_object_activity_begin(object);

		vm_object_unlock(object);

		if (vm_pageout_compress_page(&(freezer_context_global.freezer_ctx_chead),
		    (freezer_context_global.freezer_ctx_compressor_scratch_buf),
		    p) == KERN_SUCCESS) {
			/*
			 * page has already been un-tabled from the object via 'vm_page_remove'
			 */
			p->vmp_snext = local_freeq;
			local_freeq = p;
			local_freed++;
			paged_out_count++;

			if (local_freed >= MAX_FREE_BATCH) {
				OSAddAtomic64(local_freed, &vm_pageout_vminfo.vm_pageout_compressions);

				vm_page_free_list(local_freeq, TRUE);

				local_freeq = NULL;
				local_freed = 0;
			}
			freezer_context_global.freezer_ctx_uncompressed_pages++;
		}
		KDBG_DEBUG(0xe0430004 | DBG_FUNC_END, object, local_freed);

		if (local_freed == 0 && c_freezer_should_yield()) {
			thread_yield_internal(FREEZER_DUTY_CYCLE_OFF_MS);
			clock_get_uptime(&c_freezer_last_yield_ts);
		}

		vm_object_lock(object);
	}

	if (local_freeq) {
		OSAddAtomic64(local_freed, &vm_pageout_vminfo.vm_pageout_compressions);

		vm_page_free_list(local_freeq, TRUE);

		local_freeq = NULL;
		local_freed = 0;
	}

	vm_object_activity_end(object);

	vm_object_unlock(object);

	if (c_freezer_should_yield()) {
		thread_yield_internal(FREEZER_DUTY_CYCLE_OFF_MS);
		clock_get_uptime(&c_freezer_last_yield_ts);
	}
	return paged_out_count;
}

#endif /* CONFIG_FREEZE */


void
vm_object_pageout(
	vm_object_t object)
{
	vm_page_t                       p, next;
	struct  vm_pageout_queue        *iq;

	if (!VM_CONFIG_COMPRESSOR_IS_PRESENT) {
		return;
	}

	iq = &vm_pageout_queue_internal;

	assert(object != VM_OBJECT_NULL );

	vm_object_lock(object);

	if (!object->internal ||
	    object->terminating ||
	    !object->alive) {
		vm_object_unlock(object);
		return;
	}

	if (!object->pager_initialized || object->pager == MEMORY_OBJECT_NULL) {
		if (!object->pager_initialized) {
			vm_object_collapse(object, (vm_object_offset_t) 0, TRUE);

			if (!object->pager_initialized) {
				vm_object_compressor_pager_create(object);
			}
		}

		if (!object->pager_initialized || object->pager == MEMORY_OBJECT_NULL) {
			vm_object_unlock(object);
			return;
		}
	}

ReScan:
	next = (vm_page_t)vm_page_queue_first(&object->memq);

	while (!vm_page_queue_end(&object->memq, (vm_page_queue_entry_t)next)) {
		p = next;
		next = (vm_page_t)vm_page_queue_next(&next->vmp_listq);

		assert(p->vmp_q_state != VM_PAGE_ON_FREE_Q);

		if ((p->vmp_q_state == VM_PAGE_ON_THROTTLED_Q) ||
		    p->vmp_cleaning ||
		    p->vmp_laundry ||
		    p->vmp_busy ||
		    p->vmp_absent ||
		    VMP_ERROR_GET(p) ||
		    p->vmp_fictitious ||
		    VM_PAGE_WIRED(p)) {
			/*
			 * Page is already being cleaned or can't be cleaned.
			 */
			continue;
		}
		if (vm_compressor_low_on_space()) {
			break;
		}

		/* Throw to the pageout queue */

		vm_page_lockspin_queues();

		if (VM_PAGE_Q_THROTTLED(iq)) {
			iq->pgo_draining = TRUE;

			assert_wait((event_t) (&iq->pgo_laundry + 1),
			    THREAD_INTERRUPTIBLE);
			vm_page_unlock_queues();
			vm_object_unlock(object);

			thread_block(THREAD_CONTINUE_NULL);

			vm_object_lock(object);
			goto ReScan;
		}

		assert(!p->vmp_fictitious);
		assert(!p->vmp_busy);
		assert(!p->vmp_absent);
		assert(!p->vmp_unusual);
		assert(!VMP_ERROR_GET(p));      /* XXX there's a window here where we could have an ECC error! */
		assert(!VM_PAGE_WIRED(p));
		assert(!p->vmp_cleaning);

		if (p->vmp_pmapped == TRUE) {
			int refmod_state;
			int pmap_options;

			/*
			 * Tell pmap the page should be accounted
			 * for as "compressed" if it's been modified.
			 */
			pmap_options =
			    PMAP_OPTIONS_COMPRESSOR_IFF_MODIFIED;
			if (p->vmp_dirty || p->vmp_precious) {
				/*
				 * We already know it's been modified,
				 * so tell pmap to account for it
				 * as "compressed".
				 */
				pmap_options = PMAP_OPTIONS_COMPRESSOR;
			}
			vm_page_lockconvert_queues();
			refmod_state = pmap_disconnect_options(VM_PAGE_GET_PHYS_PAGE(p),
			    pmap_options,
			    NULL);
			if (refmod_state & VM_MEM_MODIFIED) {
				SET_PAGE_DIRTY(p, FALSE);
			}
		}

		if (!p->vmp_dirty && !p->vmp_precious) {
			vm_page_unlock_queues();
			VM_PAGE_FREE(p);
			continue;
		}
		vm_page_queues_remove(p, TRUE);

		vm_pageout_cluster(p);

		vm_page_unlock_queues();
	}
	vm_object_unlock(object);
}


#if CONFIG_IOSCHED

void
vm_page_request_reprioritize(vm_object_t o, uint64_t blkno, uint32_t len, int prio)
{
	io_reprioritize_req_t   req;
	struct vnode            *devvp = NULL;

	if (vnode_pager_get_object_devvp(o->pager, (uintptr_t *)&devvp) != KERN_SUCCESS) {
		return;
	}

	/*
	 * Create the request for I/O reprioritization.
	 * We use the noblock variant of zalloc because we're holding the object
	 * lock here and we could cause a deadlock in low memory conditions.
	 */
	req = (io_reprioritize_req_t)zalloc_noblock(io_reprioritize_req_zone);
	if (req == NULL) {
		return;
	}
	req->blkno = blkno;
	req->len = len;
	req->priority = prio;
	req->devvp = devvp;

	/* Insert request into the reprioritization list */
	mpsc_daemon_enqueue(&io_reprioritize_q, &req->iorr_elm, MPSC_QUEUE_DISABLE_PREEMPTION);

	return;
}

void
vm_decmp_upl_reprioritize(upl_t upl, int prio)
{
	int offset;
	vm_object_t object;
	io_reprioritize_req_t   req;
	struct vnode            *devvp = NULL;
	uint64_t                blkno;
	uint32_t                len;
	upl_t                   io_upl;
	uint64_t                *io_upl_reprio_info;
	int                     io_upl_size;

	if ((upl->flags & UPL_TRACKED_BY_OBJECT) == 0 || (upl->flags & UPL_EXPEDITE_SUPPORTED) == 0) {
		return;
	}

	/*
	 * We dont want to perform any allocations with the upl lock held since that might
	 * result in a deadlock. If the system is low on memory, the pageout thread would
	 * try to pageout stuff and might wait on this lock. If we are waiting for the memory to
	 * be freed up by the pageout thread, it would be a deadlock.
	 */


	/* First step is just to get the size of the upl to find out how big the reprio info is */
	if (!upl_try_lock(upl)) {
		return;
	}

	if (upl->decmp_io_upl == NULL) {
		/* The real I/O upl was destroyed by the time we came in here. Nothing to do. */
		upl_unlock(upl);
		return;
	}

	io_upl = upl->decmp_io_upl;
	assert((io_upl->flags & UPL_DECMP_REAL_IO) != 0);
	assertf(page_aligned(io_upl->u_offset) && page_aligned(io_upl->u_size),
	    "upl %p offset 0x%llx size 0x%x\n",
	    io_upl, io_upl->u_offset, io_upl->u_size);
	io_upl_size = io_upl->u_size;
	upl_unlock(upl);

	/* Now perform the allocation */
	io_upl_reprio_info = kalloc_data(sizeof(uint64_t) * atop(io_upl_size), Z_WAITOK);
	if (io_upl_reprio_info == NULL) {
		return;
	}

	/* Now again take the lock, recheck the state and grab out the required info */
	if (!upl_try_lock(upl)) {
		goto out;
	}

	if (upl->decmp_io_upl == NULL || upl->decmp_io_upl != io_upl) {
		/* The real I/O upl was destroyed by the time we came in here. Nothing to do. */
		upl_unlock(upl);
		goto out;
	}
	memcpy(io_upl_reprio_info, io_upl->upl_reprio_info,
	    sizeof(uint64_t) * atop(io_upl_size));

	/* Get the VM object for this UPL */
	if (io_upl->flags & UPL_SHADOWED) {
		object = io_upl->map_object->shadow;
	} else {
		object = io_upl->map_object;
	}

	/* Get the dev vnode ptr for this object */
	if (!object || !object->pager ||
	    vnode_pager_get_object_devvp(object->pager, (uintptr_t *)&devvp) != KERN_SUCCESS) {
		upl_unlock(upl);
		goto out;
	}

	upl_unlock(upl);

	/* Now we have all the information needed to do the expedite */

	offset = 0;
	while (offset < io_upl_size) {
		blkno   = io_upl_reprio_info[atop(offset)] & UPL_REPRIO_INFO_MASK;
		len     = (io_upl_reprio_info[atop(offset)] >> UPL_REPRIO_INFO_SHIFT) & UPL_REPRIO_INFO_MASK;

		/*
		 * This implementation may cause some spurious expedites due to the
		 * fact that we dont cleanup the blkno & len from the upl_reprio_info
		 * even after the I/O is complete.
		 */

		if (blkno != 0 && len != 0) {
			/* Create the request for I/O reprioritization */
			req = zalloc_flags(io_reprioritize_req_zone,
			    Z_WAITOK | Z_NOFAIL);
			req->blkno = blkno;
			req->len = len;
			req->priority = prio;
			req->devvp = devvp;

			/* Insert request into the reprioritization list */
			mpsc_daemon_enqueue(&io_reprioritize_q, &req->iorr_elm, MPSC_QUEUE_DISABLE_PREEMPTION);

			offset += len;
		} else {
			offset += PAGE_SIZE;
		}
	}

out:
	kfree_data(io_upl_reprio_info, sizeof(uint64_t) * atop(io_upl_size));
}

void
vm_page_handle_prio_inversion(vm_object_t o, vm_page_t m)
{
	upl_t upl;
	upl_page_info_t *pl;
	unsigned int i, num_pages;
	int cur_tier;

	cur_tier = proc_get_effective_thread_policy(current_thread(), TASK_POLICY_IO);

	/*
	 *  Scan through all UPLs associated with the object to find the
	 *  UPL containing the contended page.
	 */
	queue_iterate(&o->uplq, upl, upl_t, uplq) {
		if (((upl->flags & UPL_EXPEDITE_SUPPORTED) == 0) || upl->upl_priority <= cur_tier) {
			continue;
		}
		pl = UPL_GET_INTERNAL_PAGE_LIST(upl);
		assertf(page_aligned(upl->u_offset) && page_aligned(upl->u_size),
		    "upl %p offset 0x%llx size 0x%x\n",
		    upl, upl->u_offset, upl->u_size);
		num_pages = (upl->u_size / PAGE_SIZE);

		/*
		 *  For each page in the UPL page list, see if it matches the contended
		 *  page and was issued as a low prio I/O.
		 */
		for (i = 0; i < num_pages; i++) {
			if (UPL_PAGE_PRESENT(pl, i) && VM_PAGE_GET_PHYS_PAGE(m) == pl[i].phys_addr) {
				if ((upl->flags & UPL_DECMP_REQ) && upl->decmp_io_upl) {
					KDBG((VMDBG_CODE(DBG_VM_PAGE_EXPEDITE)) | DBG_FUNC_NONE, VM_KERNEL_UNSLIDE_OR_PERM(upl->upl_creator), VM_KERNEL_UNSLIDE_OR_PERM(m),
					    VM_KERNEL_UNSLIDE_OR_PERM(upl), upl->upl_priority);
					vm_decmp_upl_reprioritize(upl, cur_tier);
					break;
				}
				KDBG((VMDBG_CODE(DBG_VM_PAGE_EXPEDITE)) | DBG_FUNC_NONE, VM_KERNEL_UNSLIDE_OR_PERM(upl->upl_creator), VM_KERNEL_UNSLIDE_OR_PERM(m),
				    upl->upl_reprio_info[i], upl->upl_priority);
				if (UPL_REPRIO_INFO_BLKNO(upl, i) != 0 && UPL_REPRIO_INFO_LEN(upl, i) != 0) {
					vm_page_request_reprioritize(o, UPL_REPRIO_INFO_BLKNO(upl, i), UPL_REPRIO_INFO_LEN(upl, i), cur_tier);
				}
				break;
			}
		}
		/* Check if we found any hits */
		if (i != num_pages) {
			break;
		}
	}

	return;
}

void
kdp_vm_object_sleep_find_owner(
	event64_t          wait_event,
	block_hint_t       wait_type,
	thread_waitinfo_t *waitinfo)
{
	assert(wait_type >= kThreadWaitPagerInit && wait_type <= kThreadWaitPageInThrottle);
	vm_object_wait_reason_t wait_reason = wait_type - kThreadWaitPagerInit;
	vm_object_t object = (vm_object_t)((uintptr_t)wait_event - wait_reason);
	waitinfo->context = VM_KERNEL_ADDRPERM(object);
	/*
	 * There is currently no non-trivial way to ascertain the thread(s)
	 * currently operating on this object.
	 */
	waitinfo->owner = 0;
}


wait_result_t
vm_object_sleep(
	vm_object_t             object,
	vm_object_wait_reason_t reason,
	wait_interrupt_t        interruptible,
	lck_sleep_action_t      action)
{
	wait_result_t wr;
	block_hint_t block_hint;
	event_t wait_event;

	vm_object_lock_assert_exclusive(object);
	assert(reason >= 0 && reason <= VM_OBJECT_EVENT_MAX);
	switch (reason) {
	case VM_OBJECT_EVENT_PAGER_INIT:
		block_hint = kThreadWaitPagerInit;
		break;
	case VM_OBJECT_EVENT_PAGER_READY:
		block_hint = kThreadWaitPagerReady;
		break;
	case VM_OBJECT_EVENT_PAGING_IN_PROGRESS:
		block_hint = kThreadWaitPagingActivity;
		break;
	case VM_OBJECT_EVENT_MAPPING_IN_PROGRESS:
		block_hint = kThreadWaitMappingInProgress;
		break;
	case VM_OBJECT_EVENT_UNBLOCKED:
		block_hint = kThreadWaitMemoryBlocked;
		break;
	case VM_OBJECT_EVENT_PAGING_ONLY_IN_PROGRESS:
		block_hint = kThreadWaitPagingInProgress;
		break;
	case VM_OBJECT_EVENT_PAGEIN_THROTTLE:
		block_hint = kThreadWaitPageInThrottle;
		break;
	default:
		panic("Unexpected wait reason %u", reason);
	}
	thread_set_pending_block_hint(current_thread(), block_hint);

	KDBG_FILTERED(VMDBG_CODE(DBG_VM_OBJECT_SLEEP) | DBG_FUNC_START, VM_KERNEL_ADDRHIDE(object), reason);

	vm_object_set_wanted(object, reason);
	wait_event = (event_t)((uintptr_t)object + (uintptr_t)reason);
	wr = lck_rw_sleep(&object->Lock, LCK_SLEEP_PROMOTED_PRI | action, wait_event, interruptible);

	KDBG_FILTERED(VMDBG_CODE(DBG_VM_OBJECT_SLEEP) | DBG_FUNC_END, VM_KERNEL_ADDRHIDE(object), reason, wr);
	return wr;
}


wait_result_t
vm_object_paging_wait(vm_object_t object, wait_interrupt_t interruptible)
{
	wait_result_t wr = THREAD_NOT_WAITING;
	vm_object_lock_assert_exclusive(object);
	while (object->paging_in_progress != 0 ||
	    object->activity_in_progress != 0) {
		wr = vm_object_sleep((object),
		    VM_OBJECT_EVENT_PAGING_IN_PROGRESS,
		    interruptible,
		    LCK_SLEEP_EXCLUSIVE);
		if (wr != THREAD_AWAKENED) {
			break;
		}
	}
	return wr;
}

wait_result_t
vm_object_paging_only_wait(vm_object_t object, wait_interrupt_t interruptible)
{
	wait_result_t wr = THREAD_NOT_WAITING;
	vm_object_lock_assert_exclusive(object);
	while (object->paging_in_progress != 0) {
		wr = vm_object_sleep(object,
		    VM_OBJECT_EVENT_PAGING_ONLY_IN_PROGRESS,
		    interruptible,
		    LCK_SLEEP_EXCLUSIVE);
		if (wr != THREAD_AWAKENED) {
			break;
		}
	}
	return wr;
}

wait_result_t
vm_object_paging_throttle_wait(vm_object_t object, wait_interrupt_t interruptible)
{
	wait_result_t wr = THREAD_NOT_WAITING;
	vm_object_lock_assert_exclusive(object);
	/*
	 * TODO: consider raising the throttle limit specifically for
	 * shared-cache objects, which are expected to be highly contended.
	 * (rdar://127899888)
	 */
	while (object->paging_in_progress >= vm_object_pagein_throttle) {
		wr = vm_object_sleep(object,
		    VM_OBJECT_EVENT_PAGEIN_THROTTLE,
		    interruptible,
		    LCK_SLEEP_EXCLUSIVE);
		if (wr != THREAD_AWAKENED) {
			break;
		}
	}
	return wr;
}

wait_result_t
vm_object_mapping_wait(vm_object_t object, wait_interrupt_t interruptible)
{
	wait_result_t wr = THREAD_NOT_WAITING;
	vm_object_lock_assert_exclusive(object);
	while (object->mapping_in_progress) {
		wr = vm_object_sleep(object,
		    VM_OBJECT_EVENT_MAPPING_IN_PROGRESS,
		    interruptible,
		    LCK_SLEEP_EXCLUSIVE);
		if (wr != THREAD_AWAKENED) {
			break;
		}
	}
	return wr;
}

void
vm_object_wakeup(
	vm_object_t             object,
	vm_object_wait_reason_t reason)
{
	vm_object_lock_assert_exclusive(object);
	assert(reason >= 0 && reason <= VM_OBJECT_EVENT_MAX);

	if (vm_object_wanted(object, reason)) {
		thread_wakeup((event_t)((uintptr_t)object + (uintptr_t)reason));
	}
	object->all_wanted &= ~(1 << reason);
}


void
kdp_vm_page_sleep_find_owner(event64_t wait_event, thread_waitinfo_t *waitinfo)
{
	vm_page_t m = (vm_page_t)wait_event;
	waitinfo->context = VM_KERNEL_ADDRPERM(m);
	/*
	 * There is not currently a non-trivial way to identify the thread
	 * holding a page busy.
	 */
	waitinfo->owner = 0;
}

wait_result_t
vm_page_sleep(vm_object_t object, vm_page_t m, wait_interrupt_t interruptible, lck_sleep_action_t action)
{
	wait_result_t ret;

	KDBG_FILTERED((VMDBG_CODE(DBG_VM_PAGE_SLEEP)) | DBG_FUNC_START, VM_KERNEL_ADDRHIDE(object), m->vmp_offset, VM_KERNEL_ADDRHIDE(m));
#if CONFIG_IOSCHED
	if (object->io_tracking && ((m->vmp_busy == TRUE) || (m->vmp_cleaning == TRUE) || VM_PAGE_WIRED(m))) {
		/*
		 *  Indicates page is busy due to an I/O. Issue a reprioritize request if necessary.
		 */
		vm_page_handle_prio_inversion(object, m);
	}
#endif /* CONFIG_IOSCHED */
	m->vmp_wanted = TRUE;
	thread_set_pending_block_hint(current_thread(), kThreadWaitPageBusy);
	ret = lck_rw_sleep(&object->Lock, LCK_SLEEP_PROMOTED_PRI | action, (event_t)m, interruptible);
	KDBG_FILTERED((VMDBG_CODE(DBG_VM_PAGE_SLEEP)) | DBG_FUNC_END, VM_KERNEL_ADDRHIDE(object), m->vmp_offset, VM_KERNEL_ADDRHIDE(m));
	return ret;
}

void
vm_page_wakeup(vm_object_t object, vm_page_t m)
{
	assert(m);
	/*
	 * The page may have been freed from its object before this wakeup is issued
	 */
	if (object != VM_OBJECT_NULL) {
		vm_object_lock_assert_exclusive(object);
	}

	KDBG(VMDBG_CODE(DBG_VM_PAGE_WAKEUP) | DBG_FUNC_NONE,
	    VM_KERNEL_ADDRHIDE(object), VM_KERNEL_ADDRHIDE(m),
	    m->vmp_wanted);
	if (m->vmp_wanted) {
		m->vmp_wanted = false;
		thread_wakeup((event_t)m);
	}
}

void
vm_page_wakeup_done(__assert_only vm_object_t object, vm_page_t m)
{
	assert(object);
	assert(m->vmp_busy);
	vm_object_lock_assert_exclusive(object);

	KDBG(VMDBG_CODE(DBG_VM_PAGE_WAKEUP_DONE) | DBG_FUNC_NONE,
	    VM_KERNEL_ADDRHIDE(object), VM_KERNEL_ADDRHIDE(m),
	    m->vmp_wanted);
	m->vmp_busy = false;                          \

	if (m->vmp_wanted) {
		m->vmp_wanted = false;
		thread_wakeup((event_t)m);
	}
}

static void
io_reprioritize(mpsc_queue_chain_t elm, __assert_only mpsc_daemon_queue_t dq)
{
	assert3p(dq, ==, &io_reprioritize_q);
	io_reprioritize_req_t req = mpsc_queue_element(elm, struct io_reprioritize_req, iorr_elm);
	vnode_pager_issue_reprioritize_io(req->devvp, req->blkno, req->len, req->priority);
	zfree(io_reprioritize_req_zone, req);
}

#endif /* CONFIG_IOSCHED */

#if VM_OBJECT_ACCESS_TRACKING
void
vm_object_access_tracking(
	vm_object_t     object,
	int             *access_tracking_p,
	uint32_t        *access_tracking_reads_p,
	uint32_t        *access_tracking_writes_p)
{
	int     access_tracking;

	access_tracking = !!*access_tracking_p;

	vm_object_lock(object);
	*access_tracking_p = object->access_tracking;
	if (access_tracking_reads_p) {
		*access_tracking_reads_p = object->access_tracking_reads;
	}
	if (access_tracking_writes_p) {
		*access_tracking_writes_p = object->access_tracking_writes;
	}
	object->access_tracking = access_tracking;
	object->access_tracking_reads = 0;
	object->access_tracking_writes = 0;
	vm_object_unlock(object);

	if (access_tracking) {
		vm_object_pmap_protect_options(object,
		    0,
		    object->vo_size,
		    PMAP_NULL,
		    PAGE_SIZE,
		    0,
		    VM_PROT_NONE,
		    0);
	}
}
#endif /* VM_OBJECT_ACCESS_TRACKING */

void
vm_object_ledger_tag_ledgers(
	vm_object_t     object,
	int             *ledger_idx_volatile,
	int             *ledger_idx_nonvolatile,
	int             *ledger_idx_volatile_compressed,
	int             *ledger_idx_nonvolatile_compressed,
	int             *ledger_idx_composite,
	int             *ledger_idx_external_wired,
	boolean_t       *do_footprint)
{
	assert(object->shadow == VM_OBJECT_NULL);

	*ledger_idx_volatile = -1;
	*ledger_idx_nonvolatile = -1;
	*ledger_idx_volatile_compressed = -1;
	*ledger_idx_nonvolatile_compressed = -1;
	*ledger_idx_composite = -1;
	*ledger_idx_external_wired = -1;
	*do_footprint = !object->vo_no_footprint;

	if (!object->internal) {
		switch (object->vo_ledger_tag) {
		case VM_LEDGER_TAG_DEFAULT:
			if (*do_footprint) {
				*ledger_idx_external_wired = task_ledgers.tagged_footprint;
			} else {
				*ledger_idx_external_wired = task_ledgers.tagged_nofootprint;
			}
			break;
		case VM_LEDGER_TAG_NETWORK:
			*do_footprint = FALSE;
			*ledger_idx_external_wired = task_ledgers.network_nonvolatile;
			break;
		case VM_LEDGER_TAG_MEDIA:
			if (*do_footprint) {
				*ledger_idx_external_wired = task_ledgers.media_footprint;
			} else {
				*ledger_idx_external_wired = task_ledgers.media_nofootprint;
			}
			break;
		case VM_LEDGER_TAG_GRAPHICS:
			if (*do_footprint) {
				*ledger_idx_external_wired = task_ledgers.graphics_footprint;
			} else {
				*ledger_idx_external_wired = task_ledgers.graphics_nofootprint;
			}
			break;
		case VM_LEDGER_TAG_NEURAL:
			*ledger_idx_composite = task_ledgers.neural_nofootprint_total;
			if (*do_footprint) {
				*ledger_idx_external_wired = task_ledgers.neural_footprint;
			} else {
				*ledger_idx_external_wired = task_ledgers.neural_nofootprint;
			}
			break;
		case VM_LEDGER_TAG_NONE:
		default:
			panic("%s: external object %p has unsupported ledger_tag %d",
			    __FUNCTION__, object, object->vo_ledger_tag);
		}
		return;
	}

	assert(object->internal);
	switch (object->vo_ledger_tag) {
	case VM_LEDGER_TAG_NONE:
		/*
		 * Regular purgeable memory:
		 * counts in footprint only when nonvolatile.
		 */
		*do_footprint = TRUE;
		assert(object->purgable != VM_PURGABLE_DENY);
		*ledger_idx_volatile = task_ledgers.purgeable_volatile;
		*ledger_idx_nonvolatile = task_ledgers.purgeable_nonvolatile;
		*ledger_idx_volatile_compressed = task_ledgers.purgeable_volatile_compressed;
		*ledger_idx_nonvolatile_compressed = task_ledgers.purgeable_nonvolatile_compressed;
		break;
	case VM_LEDGER_TAG_DEFAULT:
		/*
		 * "default" tagged memory:
		 * counts in footprint only when nonvolatile and not marked
		 * as "no_footprint".
		 */
		*ledger_idx_volatile = task_ledgers.tagged_nofootprint;
		*ledger_idx_volatile_compressed = task_ledgers.tagged_nofootprint_compressed;
		if (*do_footprint) {
			*ledger_idx_nonvolatile = task_ledgers.tagged_footprint;
			*ledger_idx_nonvolatile_compressed = task_ledgers.tagged_footprint_compressed;
		} else {
			*ledger_idx_nonvolatile = task_ledgers.tagged_nofootprint;
			*ledger_idx_nonvolatile_compressed = task_ledgers.tagged_nofootprint_compressed;
		}
		break;
	case VM_LEDGER_TAG_NETWORK:
		/*
		 * "network" tagged memory:
		 * never counts in footprint.
		 */
		*do_footprint = FALSE;
		*ledger_idx_volatile = task_ledgers.network_volatile;
		*ledger_idx_volatile_compressed = task_ledgers.network_volatile_compressed;
		*ledger_idx_nonvolatile = task_ledgers.network_nonvolatile;
		*ledger_idx_nonvolatile_compressed = task_ledgers.network_nonvolatile_compressed;
		break;
	case VM_LEDGER_TAG_MEDIA:
		/*
		 * "media" tagged memory:
		 * counts in footprint only when nonvolatile and not marked
		 * as "no footprint".
		 */
		*ledger_idx_volatile = task_ledgers.media_nofootprint;
		*ledger_idx_volatile_compressed = task_ledgers.media_nofootprint_compressed;
		if (*do_footprint) {
			*ledger_idx_nonvolatile = task_ledgers.media_footprint;
			*ledger_idx_nonvolatile_compressed = task_ledgers.media_footprint_compressed;
		} else {
			*ledger_idx_nonvolatile = task_ledgers.media_nofootprint;
			*ledger_idx_nonvolatile_compressed = task_ledgers.media_nofootprint_compressed;
		}
		break;
	case VM_LEDGER_TAG_GRAPHICS:
		/*
		 * "graphics" tagged memory:
		 * counts in footprint only when nonvolatile and not marked
		 * as "no footprint".
		 */
		*ledger_idx_volatile = task_ledgers.graphics_nofootprint;
		*ledger_idx_volatile_compressed = task_ledgers.graphics_nofootprint_compressed;
		if (*do_footprint) {
			*ledger_idx_nonvolatile = task_ledgers.graphics_footprint;
			*ledger_idx_nonvolatile_compressed = task_ledgers.graphics_footprint_compressed;
		} else {
			*ledger_idx_nonvolatile = task_ledgers.graphics_nofootprint;
			*ledger_idx_nonvolatile_compressed = task_ledgers.graphics_nofootprint_compressed;
		}
		break;
	case VM_LEDGER_TAG_NEURAL:
		/*
		 * "neural" tagged memory:
		 * counts in footprint only when nonvolatile and not marked
		 * as "no footprint".
		 */
		*ledger_idx_composite = task_ledgers.neural_nofootprint_total;
		*ledger_idx_volatile = task_ledgers.neural_nofootprint;
		*ledger_idx_volatile_compressed = task_ledgers.neural_nofootprint_compressed;
		if (*do_footprint) {
			*ledger_idx_nonvolatile = task_ledgers.neural_footprint;
			*ledger_idx_nonvolatile_compressed = task_ledgers.neural_footprint_compressed;
		} else {
			*ledger_idx_nonvolatile = task_ledgers.neural_nofootprint;
			*ledger_idx_nonvolatile_compressed = task_ledgers.neural_nofootprint_compressed;
		}
		break;
	default:
		panic("%s: object %p has unsupported ledger_tag %d",
		    __FUNCTION__, object, object->vo_ledger_tag);
	}
}

kern_return_t
vm_object_ownership_change(
	vm_object_t     object,
	int             new_ledger_tag,
	task_t          new_owner,
	int             new_ledger_flags,
	boolean_t       old_task_objq_locked)
{
	int             old_ledger_tag;
	task_t          old_owner;
	int             resident_count, wired_count;
	unsigned int    compressed_count;
	int             ledger_idx_volatile;
	int             ledger_idx_nonvolatile;
	int             ledger_idx_volatile_compressed;
	int             ledger_idx_nonvolatile_compressed;
	int             ledger_idx;
	int             ledger_idx_compressed;
	int             ledger_idx_composite;
	int             ledger_idx_external_wired;
	boolean_t       do_footprint, old_no_footprint, new_no_footprint;
	boolean_t       new_task_objq_locked;

	vm_object_lock_assert_exclusive(object);

	if (new_owner != VM_OBJECT_OWNER_DISOWNED &&
	    new_owner != TASK_NULL) {
		if (new_ledger_tag == VM_LEDGER_TAG_NONE &&
		    object->purgable == VM_PURGABLE_DENY) {
			/* non-purgeable memory must have a valid non-zero ledger tag */
			return KERN_INVALID_ARGUMENT;
		}
		if (!object->internal
		    && !memory_object_is_vnode_pager(object->pager)) {
			/* non-file-backed "external" objects can't be owned */
			return KERN_INVALID_ARGUMENT;
		}
	}
	if (new_owner == VM_OBJECT_OWNER_UNCHANGED) {
		/* leave owner unchanged */
		new_owner = VM_OBJECT_OWNER(object);
	}
	if (new_ledger_tag == VM_LEDGER_TAG_UNCHANGED) {
		/* leave ledger_tag unchanged */
		new_ledger_tag = object->vo_ledger_tag;
	}
	if (new_ledger_tag < 0 ||
	    new_ledger_tag > VM_LEDGER_TAG_MAX) {
		return KERN_INVALID_ARGUMENT;
	}
	if (new_ledger_flags & ~VM_LEDGER_FLAGS_ALL) {
		return KERN_INVALID_ARGUMENT;
	}
	if (object->internal &&
	    object->vo_ledger_tag == VM_LEDGER_TAG_NONE &&
	    object->purgable == VM_PURGABLE_DENY) {
		/*
		 * This VM object is neither ledger-tagged nor purgeable.
		 * We can convert it to "ledger tag" ownership iff it
		 * has not been used at all yet (no resident pages and
		 * no pager) and it's going to be assigned to a valid task.
		 */
		if (object->resident_page_count != 0 ||
		    object->pager != NULL ||
		    object->pager_created ||
		    object->ref_count != 1 ||
		    object->vo_owner != TASK_NULL ||
		    object->copy_strategy != MEMORY_OBJECT_COPY_NONE ||
		    new_owner == TASK_NULL) {
			return KERN_FAILURE;
		}
	}

	if (new_ledger_flags & VM_LEDGER_FLAG_NO_FOOTPRINT) {
		new_no_footprint = TRUE;
	} else {
		new_no_footprint = FALSE;
	}
#if __arm64__
	if (!new_no_footprint &&
	    object->purgable != VM_PURGABLE_DENY &&
	    new_owner != TASK_NULL &&
	    new_owner != VM_OBJECT_OWNER_DISOWNED &&
	    new_owner->task_legacy_footprint) {
		/*
		 * This task has been granted "legacy footprint" and should
		 * not be charged for its IOKit purgeable memory.  Since we
		 * might now change the accounting of such memory to the
		 * "graphics" ledger, for example, give it the "no footprint"
		 * option.
		 */
		new_no_footprint = TRUE;
	}
#endif /* __arm64__ */
	assert(object->copy_strategy != MEMORY_OBJECT_COPY_SYMMETRIC);
	assert(object->shadow == VM_OBJECT_NULL);
	if (object->internal) {
		assert(object->copy_strategy == MEMORY_OBJECT_COPY_NONE);
		assert(object->vo_copy == VM_OBJECT_NULL);
	}

	old_ledger_tag = object->vo_ledger_tag;
	old_no_footprint = object->vo_no_footprint;
	old_owner = VM_OBJECT_OWNER(object);

	if (__improbable(vm_debug_events)) {
		DTRACE_VM8(object_ownership_change,
		    vm_object_t, object,
		    task_t, old_owner,
		    int, old_ledger_tag,
		    int, old_no_footprint,
		    task_t, new_owner,
		    int, new_ledger_tag,
		    int, new_no_footprint,
		    int, VM_OBJECT_ID(object));
	}

	resident_count = object->resident_page_count - object->wired_page_count;
	wired_count = object->wired_page_count;
	if (object->internal) {
		compressed_count = vm_compressor_pager_get_count(object->pager);
	} else {
		compressed_count = 0;
	}

	/*
	 * Deal with the old owner and/or ledger tag, if needed.
	 */
	if (old_owner != TASK_NULL &&
	    ((old_owner != new_owner)           /* new owner ... */
	    ||                                  /* ... or ... */
	    (old_no_footprint != new_no_footprint) /* new "no_footprint" */
	    ||                                  /* ... or ... */
	    old_ledger_tag != new_ledger_tag)) { /* ... new ledger */
		/*
		 * Take this object off of the old owner's ledgers.
		 */
		vm_object_ledger_tag_ledgers(object,
		    &ledger_idx_volatile,
		    &ledger_idx_nonvolatile,
		    &ledger_idx_volatile_compressed,
		    &ledger_idx_nonvolatile_compressed,
		    &ledger_idx_composite,
		    &ledger_idx_external_wired,
		    &do_footprint);
		if (object->internal) {
			if (object->purgable == VM_PURGABLE_VOLATILE ||
			    object->purgable == VM_PURGABLE_EMPTY) {
				ledger_idx = ledger_idx_volatile;
				ledger_idx_compressed = ledger_idx_volatile_compressed;
			} else {
				ledger_idx = ledger_idx_nonvolatile;
				ledger_idx_compressed = ledger_idx_nonvolatile_compressed;
			}
			if (resident_count) {
				/*
				 * Adjust the appropriate old owners's ledgers by the
				 * number of resident pages.
				 */
				ledger_debit(old_owner->ledger,
				    ledger_idx,
				    ptoa_64(resident_count));
				/* adjust old owner's footprint */
				if (object->purgable != VM_PURGABLE_VOLATILE &&
				    object->purgable != VM_PURGABLE_EMPTY) {
					if (do_footprint) {
						ledger_debit(old_owner->ledger,
						    task_ledgers.phys_footprint,
						    ptoa_64(resident_count));
					} else if (ledger_idx_composite != -1) {
						ledger_debit(old_owner->ledger,
						    ledger_idx_composite,
						    ptoa_64(resident_count));
					}
				}
			}
			if (wired_count) {
				/* wired pages are always nonvolatile */
				ledger_debit(old_owner->ledger,
				    ledger_idx_nonvolatile,
				    ptoa_64(wired_count));
				if (do_footprint) {
					ledger_debit(old_owner->ledger,
					    task_ledgers.phys_footprint,
					    ptoa_64(wired_count));
				} else if (ledger_idx_composite != -1) {
					ledger_debit(old_owner->ledger,
					    ledger_idx_composite,
					    ptoa_64(wired_count));
				}
			}
			if (compressed_count) {
				/*
				 * Adjust the appropriate old owner's ledgers
				 * by the number of compressed pages.
				 */
				ledger_debit(old_owner->ledger,
				    ledger_idx_compressed,
				    ptoa_64(compressed_count));
				if (object->purgable != VM_PURGABLE_VOLATILE &&
				    object->purgable != VM_PURGABLE_EMPTY) {
					if (do_footprint) {
						ledger_debit(old_owner->ledger,
						    task_ledgers.phys_footprint,
						    ptoa_64(compressed_count));
					} else if (ledger_idx_composite != -1) {
						ledger_debit(old_owner->ledger,
						    ledger_idx_composite,
						    ptoa_64(compressed_count));
					}
				}
			}
		} else {
			/* external but owned object: count wired pages */
			if (wired_count) {
				ledger_debit(old_owner->ledger,
				    ledger_idx_external_wired,
				    ptoa_64(wired_count));
				if (do_footprint) {
					ledger_debit(old_owner->ledger,
					    task_ledgers.phys_footprint,
					    ptoa_64(wired_count));
				} else if (ledger_idx_composite != -1) {
					ledger_debit(old_owner->ledger,
					    ledger_idx_composite,
					    ptoa_64(wired_count));
				}
			}
		}
		if (old_owner != new_owner) {
			/* remove object from old_owner's list of owned objects */
			DTRACE_VM2(object_owner_remove,
			    vm_object_t, object,
			    task_t, old_owner);
			if (!old_task_objq_locked) {
				task_objq_lock(old_owner);
			}
			old_owner->task_owned_objects--;
			queue_remove(&old_owner->task_objq, object,
			    vm_object_t, task_objq);
			switch (object->purgable) {
			case VM_PURGABLE_NONVOLATILE:
			case VM_PURGABLE_EMPTY:
				vm_purgeable_nonvolatile_owner_update(old_owner,
				    -1);
				break;
			case VM_PURGABLE_VOLATILE:
				vm_purgeable_volatile_owner_update(old_owner,
				    -1);
				break;
			default:
				break;
			}
			if (!old_task_objq_locked) {
				task_objq_unlock(old_owner);
			}
		}
	}

	/*
	 * Switch to new ledger tag and/or owner.
	 */

	new_task_objq_locked = FALSE;
	if (new_owner != old_owner &&
	    new_owner != TASK_NULL &&
	    new_owner != VM_OBJECT_OWNER_DISOWNED) {
		/*
		 * If the new owner is not accepting new objects ("disowning"),
		 * the object becomes "disowned" and will be added to
		 * the kernel's task_objq.
		 *
		 * Check first without locking, to avoid blocking while the
		 * task is disowning its objects.
		 */
		if (new_owner->task_objects_disowning) {
			new_owner = VM_OBJECT_OWNER_DISOWNED;
		} else {
			task_objq_lock(new_owner);
			/* check again now that we have the lock */
			if (new_owner->task_objects_disowning) {
				new_owner = VM_OBJECT_OWNER_DISOWNED;
				task_objq_unlock(new_owner);
			} else {
				new_task_objq_locked = TRUE;
			}
		}
	}

	object->vo_ledger_tag = new_ledger_tag;
	object->vo_owner = new_owner;
	object->vo_no_footprint = new_no_footprint;

	if (new_owner == VM_OBJECT_OWNER_DISOWNED) {
		/*
		 * Disowned objects are added to the kernel's task_objq but
		 * are marked as owned by "VM_OBJECT_OWNER_DISOWNED" to
		 * differentiate them from objects intentionally owned by
		 * the kernel.
		 */
		assert(old_owner != kernel_task);
		new_owner = kernel_task;
		assert(!new_task_objq_locked);
		task_objq_lock(new_owner);
		new_task_objq_locked = TRUE;
	}

	/*
	 * Deal with the new owner and/or ledger tag, if needed.
	 */
	if (new_owner != TASK_NULL &&
	    ((new_owner != old_owner)           /* new owner ... */
	    ||                                  /* ... or ... */
	    (new_no_footprint != old_no_footprint) /* ... new "no_footprint" */
	    ||                                  /* ... or ... */
	    new_ledger_tag != old_ledger_tag)) { /* ... new ledger */
		/*
		 * Add this object to the new owner's ledgers.
		 */
		vm_object_ledger_tag_ledgers(object,
		    &ledger_idx_volatile,
		    &ledger_idx_nonvolatile,
		    &ledger_idx_volatile_compressed,
		    &ledger_idx_nonvolatile_compressed,
		    &ledger_idx_composite,
		    &ledger_idx_external_wired,
		    &do_footprint);
		if (object->internal) {
			if (object->purgable == VM_PURGABLE_VOLATILE ||
			    object->purgable == VM_PURGABLE_EMPTY) {
				ledger_idx = ledger_idx_volatile;
				ledger_idx_compressed = ledger_idx_volatile_compressed;
			} else {
				ledger_idx = ledger_idx_nonvolatile;
				ledger_idx_compressed = ledger_idx_nonvolatile_compressed;
			}
			if (resident_count) {
				/*
				 * Adjust the appropriate new owners's ledgers by the
				 * number of resident pages.
				 */
				ledger_credit(new_owner->ledger,
				    ledger_idx,
				    ptoa_64(resident_count));
				/* adjust new owner's footprint */
				if (object->purgable != VM_PURGABLE_VOLATILE &&
				    object->purgable != VM_PURGABLE_EMPTY) {
					if (do_footprint) {
						ledger_credit(new_owner->ledger,
						    task_ledgers.phys_footprint,
						    ptoa_64(resident_count));
					} else if (ledger_idx_composite != -1) {
						ledger_credit(new_owner->ledger,
						    ledger_idx_composite,
						    ptoa_64(resident_count));
					}
				}
			}
			if (wired_count) {
				/* wired pages are always nonvolatile */
				ledger_credit(new_owner->ledger,
				    ledger_idx_nonvolatile,
				    ptoa_64(wired_count));
				if (do_footprint) {
					ledger_credit(new_owner->ledger,
					    task_ledgers.phys_footprint,
					    ptoa_64(wired_count));
				} else if (ledger_idx_composite != -1) {
					ledger_credit(new_owner->ledger,
					    ledger_idx_composite,
					    ptoa_64(wired_count));
				}
			}
			if (compressed_count) {
				/*
				 * Adjust the new owner's ledgers by the number of
				 * compressed pages.
				 */
				ledger_credit(new_owner->ledger,
				    ledger_idx_compressed,
				    ptoa_64(compressed_count));
				if (object->purgable != VM_PURGABLE_VOLATILE &&
				    object->purgable != VM_PURGABLE_EMPTY) {
					if (do_footprint) {
						ledger_credit(new_owner->ledger,
						    task_ledgers.phys_footprint,
						    ptoa_64(compressed_count));
					} else if (ledger_idx_composite != -1) {
						ledger_credit(new_owner->ledger,
						    ledger_idx_composite,
						    ptoa_64(compressed_count));
					}
				}
			}
		} else {
			/* external but owned object: count wired pages */
			if (wired_count) {
				ledger_credit(new_owner->ledger,
				    ledger_idx_external_wired,
				    ptoa_64(wired_count));
				if (do_footprint) {
					ledger_credit(new_owner->ledger,
					    task_ledgers.phys_footprint,
					    ptoa_64(wired_count));
				} else if (ledger_idx_composite != -1) {
					ledger_credit(new_owner->ledger,
					    ledger_idx_composite,
					    ptoa_64(wired_count));
				}
			}
		}
		if (new_owner != old_owner) {
			/* add object to new_owner's list of owned objects */
			DTRACE_VM2(object_owner_add,
			    vm_object_t, object,
			    task_t, new_owner);
			assert(new_task_objq_locked);
			new_owner->task_owned_objects++;
			queue_enter(&new_owner->task_objq, object,
			    vm_object_t, task_objq);
			switch (object->purgable) {
			case VM_PURGABLE_NONVOLATILE:
			case VM_PURGABLE_EMPTY:
				vm_purgeable_nonvolatile_owner_update(new_owner,
				    +1);
				break;
			case VM_PURGABLE_VOLATILE:
				vm_purgeable_volatile_owner_update(new_owner,
				    +1);
				break;
			default:
				break;
			}
		}
	}

	if (new_task_objq_locked) {
		task_objq_unlock(new_owner);
	}

	return KERN_SUCCESS;
}

void
vm_owned_objects_disown(
	task_t  task)
{
	vm_object_t     next_object;
	vm_object_t     object;
	int             collisions;
	kern_return_t   kr;

	if (task == NULL) {
		return;
	}

	collisions = 0;

again:
	if (task->task_objects_disowned) {
		/* task has already disowned its owned objects */
		assert(task->task_volatile_objects == 0);
		assert(task->task_nonvolatile_objects == 0);
		assert(task->task_owned_objects == 0);
		return;
	}

	task_objq_lock(task);

	task->task_objects_disowning = TRUE;

	for (object = (vm_object_t) queue_first(&task->task_objq);
	    !queue_end(&task->task_objq, (queue_entry_t) object);
	    object = next_object) {
		if (task->task_nonvolatile_objects == 0 &&
		    task->task_volatile_objects == 0 &&
		    task->task_owned_objects == 0) {
			/* no more objects owned by "task" */
			break;
		}

		next_object = (vm_object_t) queue_next(&object->task_objq);

#if DEBUG
		assert(object->vo_purgeable_volatilizer == NULL);
#endif /* DEBUG */
		assert(object->vo_owner == task);
		if (!vm_object_lock_try(object)) {
			task_objq_unlock(task);
			mutex_pause(collisions++);
			goto again;
		}
		/* transfer ownership to the kernel */
		assert(VM_OBJECT_OWNER(object) != kernel_task);
		kr = vm_object_ownership_change(
			object,
			object->vo_ledger_tag, /* unchanged */
			VM_OBJECT_OWNER_DISOWNED, /* new owner */
			0, /* new_ledger_flags */
			TRUE);  /* old_owner->task_objq locked */
		assert(kr == KERN_SUCCESS);
		assert(object->vo_owner == VM_OBJECT_OWNER_DISOWNED);
		vm_object_unlock(object);
	}

	if (__improbable(task->task_owned_objects != 0)) {
		panic("%s(%p): volatile=%d nonvolatile=%d owned=%d q=%p q_first=%p q_last=%p",
		    __FUNCTION__,
		    task,
		    task->task_volatile_objects,
		    task->task_nonvolatile_objects,
		    task->task_owned_objects,
		    &task->task_objq,
		    queue_first(&task->task_objq),
		    queue_last(&task->task_objq));
	}

	/* there shouldn't be any objects owned by task now */
	assert(task->task_volatile_objects == 0);
	assert(task->task_nonvolatile_objects == 0);
	assert(task->task_owned_objects == 0);
	assert(task->task_objects_disowning);

	/* and we don't need to try and disown again */
	task->task_objects_disowned = TRUE;

	task_objq_unlock(task);
}

void
vm_object_wired_page_update_ledgers(
	vm_object_t object,
	int64_t wired_delta)
{
	task_t owner;

	vm_object_lock_assert_exclusive(object);
	if (wired_delta == 0) {
		/* no change in number of wired pages */
		return;
	}
	if (object->internal) {
		/* no extra accounting needed for internal objects */
		return;
	}
	if (!object->vo_ledger_tag) {
		/* external object but not owned: no extra accounting */
		return;
	}

	/*
	 * For an explicitly-owned external VM object, account for
	 * wired pages in one of the owner's ledgers.
	 */
	owner = VM_OBJECT_OWNER(object);
	if (owner) {
		int ledger_idx_volatile;
		int ledger_idx_nonvolatile;
		int ledger_idx_volatile_compressed;
		int ledger_idx_nonvolatile_compressed;
		int ledger_idx_composite;
		int ledger_idx_external_wired;
		boolean_t do_footprint;

		/* ask which ledgers need an update */
		vm_object_ledger_tag_ledgers(object,
		    &ledger_idx_volatile,
		    &ledger_idx_nonvolatile,
		    &ledger_idx_volatile_compressed,
		    &ledger_idx_nonvolatile_compressed,
		    &ledger_idx_composite,
		    &ledger_idx_external_wired,
		    &do_footprint);
		if (wired_delta > 0) {
			/* more external wired bytes */
			ledger_credit(owner->ledger,
			    ledger_idx_external_wired,
			    ptoa(wired_delta));
			if (do_footprint) {
				/* more footprint */
				ledger_credit(owner->ledger,
				    task_ledgers.phys_footprint,
				    ptoa(wired_delta));
			} else if (ledger_idx_composite != -1) {
				ledger_credit(owner->ledger,
				    ledger_idx_composite,
				    ptoa(wired_delta));
			}
		} else {
			/* less external wired bytes */
			ledger_debit(owner->ledger,
			    ledger_idx_external_wired,
			    ptoa(-wired_delta));
			if (do_footprint) {
				/* more footprint */
				ledger_debit(owner->ledger,
				    task_ledgers.phys_footprint,
				    ptoa(-wired_delta));
			} else if (ledger_idx_composite != -1) {
				ledger_debit(owner->ledger,
				    ledger_idx_composite,
				    ptoa(-wired_delta));
			}
		}
	}
}
