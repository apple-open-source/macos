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
 *	File:	vm/vm_page.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Resident memory management module.
 */

#include <debug.h>
#include <libkern/OSAtomic.h>
#include <libkern/OSDebug.h>

#include <mach/clock_types.h>
#include <mach/vm_prot.h>
#include <mach/vm_statistics.h>
#include <mach/sdt.h>
#include <kern/counter.h>
#include <kern/host_statistics.h>
#include <kern/sched_prim.h>
#include <kern/policy_internal.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/kalloc.h>
#include <kern/zalloc_internal.h>
#include <kern/ledger.h>
#include <kern/ecc.h>
#include <vm/pmap.h>
#include <vm/vm_init_xnu.h>
#include <vm/vm_map_internal.h>
#include <vm/vm_page_internal.h>
#include <vm/vm_pageout_internal.h>
#include <vm/vm_kern_xnu.h>                 /* kmem_alloc() */
#include <vm/vm_compressor_pager_internal.h>
#include <kern/misc_protos.h>
#include <mach_debug/zone_info.h>
#include <vm/cpm_internal.h>
#include <pexpert/pexpert.h>
#include <pexpert/device_tree.h>
#include <san/kasan.h>
#include <os/log.h>

#include <vm/vm_protos_internal.h>
#include <vm/memory_object.h>
#include <vm/vm_purgeable_internal.h>
#include <vm/vm_compressor_internal.h>
#include <vm/vm_iokit.h>
#include <vm/vm_object_internal.h>
#if defined (__x86_64__)
#include <i386/misc_protos.h>
#endif

#if CONFIG_PHANTOM_CACHE
#include <vm/vm_phantom_cache_internal.h>
#endif

#if HIBERNATION
#include <IOKit/IOHibernatePrivate.h>
#include <machine/pal_hibernate.h>
#endif /* HIBERNATION */

#include <sys/kdebug.h>

#if defined(HAS_APPLE_PAC)
#include <ptrauth.h>
#endif
#if defined(__arm64__)
#include <arm/cpu_internal.h>
#endif /* defined(__arm64__) */

#if MACH_ASSERT

TUNABLE(bool, vm_check_refs_on_free, "vm_check_refs_on_free", true);
#define ASSERT_PMAP_FREE(mem) pmap_assert_free(VM_PAGE_GET_PHYS_PAGE(mem))

#else /* MACH_ASSERT */

#define ASSERT_PMAP_FREE(mem) /* nothing */

#endif /* MACH_ASSERT */

extern boolean_t vm_pageout_running;
extern thread_t  vm_pageout_scan_thread;
extern bool vps_dynamic_priority_enabled;

char    vm_page_inactive_states[VM_PAGE_Q_STATE_ARRAY_SIZE];
char    vm_page_pageable_states[VM_PAGE_Q_STATE_ARRAY_SIZE];
char    vm_page_non_speculative_pageable_states[VM_PAGE_Q_STATE_ARRAY_SIZE];
char    vm_page_active_or_inactive_states[VM_PAGE_Q_STATE_ARRAY_SIZE];

#if CONFIG_SECLUDED_MEMORY
struct vm_page_secluded_data vm_page_secluded;
#endif /* CONFIG_SECLUDED_MEMORY */

#if DEVELOPMENT || DEBUG
extern struct memory_object_pager_ops shared_region_pager_ops;
unsigned int shared_region_pagers_resident_count = 0;
unsigned int shared_region_pagers_resident_peak = 0;
#endif /* DEVELOPMENT || DEBUG */



int             PERCPU_DATA(start_color);
vm_page_t       PERCPU_DATA(free_pages);
boolean_t       hibernate_cleaning_in_progress = FALSE;

uint32_t        vm_lopage_free_count = 0;
uint32_t        vm_lopage_free_limit = 0;
uint32_t        vm_lopage_lowater    = 0;
boolean_t       vm_lopage_refill = FALSE;
boolean_t       vm_lopage_needed = FALSE;

int             speculative_age_index = 0;
int             speculative_steal_index = 0;
struct vm_speculative_age_q vm_page_queue_speculative[VM_PAGE_RESERVED_SPECULATIVE_AGE_Q + 1];

boolean_t       hibernation_vmqueues_inspection = FALSE; /* Tracks if the hibernation code is looking at the VM queues.
                                                          * Updated and checked behind the vm_page_queues_lock. */

static void             vm_page_free_prepare(vm_page_t  page);
static vm_page_t        vm_page_grab_fictitious_common(ppnum_t, boolean_t);

static void vm_tag_init(void);

/* for debugging purposes */
SECURITY_READ_ONLY_EARLY(uint32_t) vm_packed_from_vm_pages_array_mask =
    VM_PAGE_PACKED_FROM_ARRAY;
SECURITY_READ_ONLY_EARLY(vm_packing_params_t) vm_page_packing_params =
    VM_PACKING_PARAMS(VM_PAGE_PACKED_PTR);

/*
 *	Associated with page of user-allocatable memory is a
 *	page structure.
 */

/*
 *	These variables record the values returned by vm_page_bootstrap,
 *	for debugging purposes.  The implementation of pmap_steal_memory
 *	and pmap_startup here also uses them internally.
 */

vm_offset_t virtual_space_start;
vm_offset_t virtual_space_end;
uint32_t        vm_page_pages;

/*
 *	The vm_page_lookup() routine, which provides for fast
 *	(virtual memory object, offset) to page lookup, employs
 *	the following hash table.  The vm_page_{insert,remove}
 *	routines install and remove associations in the table.
 *	[This table is often called the virtual-to-physical,
 *	or VP, table.]
 */
typedef struct {
	vm_page_packed_t page_list;
#if     MACH_PAGE_HASH_STATS
	int             cur_count;              /* current count */
	int             hi_count;               /* high water mark */
#endif /* MACH_PAGE_HASH_STATS */
} vm_page_bucket_t;


#define BUCKETS_PER_LOCK        16

SECURITY_READ_ONLY_LATE(vm_page_bucket_t *) vm_page_buckets;                /* Array of buckets */
SECURITY_READ_ONLY_LATE(unsigned int)       vm_page_bucket_count = 0;       /* How big is array? */
SECURITY_READ_ONLY_LATE(unsigned int)       vm_page_hash_mask;              /* Mask for hash function */
SECURITY_READ_ONLY_LATE(unsigned int)       vm_page_hash_shift;             /* Shift for hash function */
SECURITY_READ_ONLY_LATE(uint32_t)           vm_page_bucket_hash;            /* Basic bucket hash */
SECURITY_READ_ONLY_LATE(unsigned int)       vm_page_bucket_lock_count = 0;  /* How big is array of locks? */

#ifndef VM_TAG_ACTIVE_UPDATE
#error VM_TAG_ACTIVE_UPDATE
#endif
#ifndef VM_TAG_SIZECLASSES
#error VM_TAG_SIZECLASSES
#endif

/* for debugging */
SECURITY_READ_ONLY_LATE(bool) vm_tag_active_update = VM_TAG_ACTIVE_UPDATE;
SECURITY_READ_ONLY_LATE(lck_spin_t *) vm_page_bucket_locks;

vm_allocation_site_t            vm_allocation_sites_static[VM_KERN_MEMORY_FIRST_DYNAMIC + 1];
vm_allocation_site_t *          vm_allocation_sites[VM_MAX_TAG_VALUE];
#if VM_TAG_SIZECLASSES
static vm_allocation_zone_total_t **vm_allocation_zone_totals;
#endif /* VM_TAG_SIZECLASSES */

vm_tag_t vm_allocation_tag_highest;

#if VM_PAGE_BUCKETS_CHECK
boolean_t vm_page_buckets_check_ready = FALSE;
#if VM_PAGE_FAKE_BUCKETS
vm_page_bucket_t *vm_page_fake_buckets; /* decoy buckets */
vm_map_offset_t vm_page_fake_buckets_start, vm_page_fake_buckets_end;
#endif /* VM_PAGE_FAKE_BUCKETS */
#endif /* VM_PAGE_BUCKETS_CHECK */

#if     MACH_PAGE_HASH_STATS
/* This routine is only for debug.  It is intended to be called by
 * hand by a developer using a kernel debugger.  This routine prints
 * out vm_page_hash table statistics to the kernel debug console.
 */
void
hash_debug(void)
{
	int     i;
	int     numbuckets = 0;
	int     highsum = 0;
	int     maxdepth = 0;

	for (i = 0; i < vm_page_bucket_count; i++) {
		if (vm_page_buckets[i].hi_count) {
			numbuckets++;
			highsum += vm_page_buckets[i].hi_count;
			if (vm_page_buckets[i].hi_count > maxdepth) {
				maxdepth = vm_page_buckets[i].hi_count;
			}
		}
	}
	printf("Total number of buckets: %d\n", vm_page_bucket_count);
	printf("Number used buckets:     %d = %d%%\n",
	    numbuckets, 100 * numbuckets / vm_page_bucket_count);
	printf("Number unused buckets:   %d = %d%%\n",
	    vm_page_bucket_count - numbuckets,
	    100 * (vm_page_bucket_count - numbuckets) / vm_page_bucket_count);
	printf("Sum of bucket max depth: %d\n", highsum);
	printf("Average bucket depth:    %d.%2d\n",
	    highsum / vm_page_bucket_count,
	    highsum % vm_page_bucket_count);
	printf("Maximum bucket depth:    %d\n", maxdepth);
}
#endif /* MACH_PAGE_HASH_STATS */

/*
 *	The virtual page size is currently implemented as a runtime
 *	variable, but is constant once initialized using vm_set_page_size.
 *	This initialization must be done in the machine-dependent
 *	bootstrap sequence, before calling other machine-independent
 *	initializations.
 *
 *	All references to the virtual page size outside this
 *	module must use the PAGE_SIZE, PAGE_MASK and PAGE_SHIFT
 *	constants.
 */
#if defined(__arm64__)
vm_size_t       page_size;
vm_size_t       page_mask;
int             page_shift;
#else
vm_size_t       page_size  = PAGE_SIZE;
vm_size_t       page_mask  = PAGE_MASK;
int             page_shift = PAGE_SHIFT;
#endif

SECURITY_READ_ONLY_LATE(vm_page_t) vm_pages = VM_PAGE_NULL;
SECURITY_READ_ONLY_LATE(vm_page_t) vm_page_array_beginning_addr;
vm_page_t                          vm_page_array_ending_addr;

unsigned int    vm_pages_count = 0;

/*
 *	Resident pages that represent real memory
 *	are allocated from a set of free lists,
 *	one per color.
 */
unsigned int    vm_colors;
unsigned int    vm_color_mask;                  /* mask is == (vm_colors-1) */
unsigned int    vm_cache_geometry_colors = 0;   /* set by hw dependent code during startup */
unsigned int    vm_free_magazine_refill_limit = 0;


struct vm_page_queue_free_head {
	vm_page_queue_head_t    qhead;
} VM_PAGE_PACKED_ALIGNED;

struct vm_page_queue_free_head  vm_page_queue_free[MAX_COLORS];


unsigned int    vm_page_free_wanted;
unsigned int    vm_page_free_wanted_privileged;
#if CONFIG_SECLUDED_MEMORY
unsigned int    vm_page_free_wanted_secluded;
#endif /* CONFIG_SECLUDED_MEMORY */
unsigned int    vm_page_free_count;

unsigned int    vm_page_realtime_count;

/*
 *	Occasionally, the virtual memory system uses
 *	resident page structures that do not refer to
 *	real pages, for example to leave a page with
 *	important state information in the VP table.
 *
 *	These page structures are allocated the way
 *	most other kernel structures are.
 */
SECURITY_READ_ONLY_LATE(zone_t) vm_page_zone;
vm_locks_array_t vm_page_locks;

LCK_ATTR_DECLARE(vm_page_lck_attr, 0, 0);
LCK_GRP_DECLARE(vm_page_lck_grp_free, "vm_page_free");
LCK_GRP_DECLARE(vm_page_lck_grp_queue, "vm_page_queue");
LCK_GRP_DECLARE(vm_page_lck_grp_local, "vm_page_queue_local");
LCK_GRP_DECLARE(vm_page_lck_grp_purge, "vm_page_purge");
LCK_GRP_DECLARE(vm_page_lck_grp_alloc, "vm_page_alloc");
LCK_GRP_DECLARE(vm_page_lck_grp_bucket, "vm_page_bucket");
LCK_SPIN_DECLARE_ATTR(vm_objects_wired_lock, &vm_page_lck_grp_bucket, &vm_page_lck_attr);
LCK_TICKET_DECLARE(vm_allocation_sites_lock, &vm_page_lck_grp_bucket);

unsigned int    vm_page_local_q_soft_limit = 250;
unsigned int    vm_page_local_q_hard_limit = 500;
struct vpl     *__zpercpu vm_page_local_q;

/* N.B. Guard and fictitious pages must not
 * be assigned a zero phys_page value.
 */
/*
 *	Fictitious pages don't have a physical address,
 *	but we must initialize phys_page to something.
 *	For debugging, this should be a strange value
 *	that the pmap module can recognize in assertions.
 */
const ppnum_t vm_page_fictitious_addr = (ppnum_t) -1;

/*
 *	Guard pages are not accessible so they don't
 *      need a physical address, but we need to enter
 *	one in the pmap.
 *	Let's make it recognizable and make sure that
 *	we don't use a real physical page with that
 *	physical address.
 */
const ppnum_t vm_page_guard_addr = (ppnum_t) -2;

/*
 *	Resident page structures are also chained on
 *	queues that are used by the page replacement
 *	system (pageout daemon).  These queues are
 *	defined here, but are shared by the pageout
 *	module.  The inactive queue is broken into
 *	file backed and anonymous for convenience as the
 *	pageout daemon often assignes a higher
 *	importance to anonymous pages (less likely to pick)
 */
vm_page_queue_head_t    vm_page_queue_active VM_PAGE_PACKED_ALIGNED;
vm_page_queue_head_t    vm_page_queue_inactive VM_PAGE_PACKED_ALIGNED;
#if CONFIG_SECLUDED_MEMORY
vm_page_queue_head_t    vm_page_queue_secluded VM_PAGE_PACKED_ALIGNED;
#endif /* CONFIG_SECLUDED_MEMORY */
vm_page_queue_head_t    vm_page_queue_anonymous VM_PAGE_PACKED_ALIGNED;  /* inactive memory queue for anonymous pages */
vm_page_queue_head_t    vm_page_queue_throttled VM_PAGE_PACKED_ALIGNED;

queue_head_t    vm_objects_wired;

vm_page_queue_head_t    vm_page_queue_donate VM_PAGE_PACKED_ALIGNED;
uint32_t        vm_page_donate_mode;
uint32_t        vm_page_donate_target, vm_page_donate_target_high, vm_page_donate_target_low;
uint32_t        vm_page_donate_count;
bool            vm_page_donate_queue_ripe;


vm_page_queue_head_t    vm_page_queue_background VM_PAGE_PACKED_ALIGNED;
uint32_t        vm_page_background_target;
uint32_t        vm_page_background_target_snapshot;
uint32_t        vm_page_background_count;
uint64_t        vm_page_background_promoted_count;

uint32_t        vm_page_background_internal_count;
uint32_t        vm_page_background_external_count;

uint32_t        vm_page_background_mode;
uint32_t        vm_page_background_exclude_external;

unsigned int    vm_page_active_count;
unsigned int    vm_page_inactive_count;
unsigned int    vm_page_kernelcache_count;
#if CONFIG_SECLUDED_MEMORY
unsigned int    vm_page_secluded_count;
unsigned int    vm_page_secluded_count_free;
unsigned int    vm_page_secluded_count_inuse;
unsigned int    vm_page_secluded_count_over_target;
#endif /* CONFIG_SECLUDED_MEMORY */
unsigned int    vm_page_anonymous_count;
unsigned int    vm_page_throttled_count;
unsigned int    vm_page_speculative_count;

unsigned int    vm_page_wire_count;
unsigned int    vm_page_wire_count_on_boot = 0;
unsigned int    vm_page_stolen_count = 0;
unsigned int    vm_page_wire_count_initial;
unsigned int    vm_page_gobble_count = 0;
unsigned int    vm_page_kern_lpage_count = 0;

uint64_t        booter_size;  /* external so it can be found in core dumps */

#define VM_PAGE_WIRE_COUNT_WARNING      0
#define VM_PAGE_GOBBLE_COUNT_WARNING    0

unsigned int    vm_page_purgeable_count = 0; /* # of pages purgeable now */
unsigned int    vm_page_purgeable_wired_count = 0; /* # of purgeable pages that are wired now */
uint64_t        vm_page_purged_count = 0;    /* total count of purged pages */

unsigned int    vm_page_xpmapped_external_count = 0;
unsigned int    vm_page_external_count = 0;
unsigned int    vm_page_internal_count = 0;
unsigned int    vm_page_pageable_external_count = 0;
unsigned int    vm_page_pageable_internal_count = 0;

#if DEVELOPMENT || DEBUG
unsigned int    vm_page_speculative_recreated = 0;
unsigned int    vm_page_speculative_created = 0;
unsigned int    vm_page_speculative_used = 0;
#endif

vm_page_queue_head_t    vm_page_queue_cleaned VM_PAGE_PACKED_ALIGNED;

unsigned int    vm_page_cleaned_count = 0;

uint64_t        max_valid_dma_address = 0xffffffffffffffffULL;
ppnum_t         max_valid_low_ppnum = PPNUM_MAX;


/*
 *	Several page replacement parameters are also
 *	shared with this module, so that page allocation
 *	(done here in vm_page_alloc) can trigger the
 *	pageout daemon.
 */
unsigned int    vm_page_free_target = 0;
unsigned int    vm_page_free_min = 0;
unsigned int    vm_page_throttle_limit = 0;
unsigned int    vm_page_inactive_target = 0;
#if CONFIG_SECLUDED_MEMORY
unsigned int    vm_page_secluded_target = 0;
#endif /* CONFIG_SECLUDED_MEMORY */
unsigned int    vm_page_anonymous_min = 0;
unsigned int    vm_page_free_reserved = 0;


/*
 *	The VM system has a couple of heuristics for deciding
 *	that pages are "uninteresting" and should be placed
 *	on the inactive queue as likely candidates for replacement.
 *	These variables let the heuristics be controlled at run-time
 *	to make experimentation easier.
 */

boolean_t vm_page_deactivate_hint = TRUE;

struct vm_page_stats_reusable vm_page_stats_reusable;

/*
 *	vm_set_page_size:
 *
 *	Sets the page size, perhaps based upon the memory
 *	size.  Must be called before any use of page-size
 *	dependent functions.
 *
 *	Sets page_shift and page_mask from page_size.
 */
void
vm_set_page_size(void)
{
	page_size  = PAGE_SIZE;
	page_mask  = PAGE_MASK;
	page_shift = PAGE_SHIFT;

	if ((page_mask & page_size) != 0) {
		panic("vm_set_page_size: page size not a power of two");
	}

	for (page_shift = 0;; page_shift++) {
		if ((1U << page_shift) == page_size) {
			break;
		}
	}
}

#if defined (__x86_64__)

#define MAX_CLUMP_SIZE      16
#define DEFAULT_CLUMP_SIZE  4

unsigned int vm_clump_size, vm_clump_mask, vm_clump_shift, vm_clump_promote_threshold;

#if DEVELOPMENT || DEBUG
unsigned long vm_clump_stats[MAX_CLUMP_SIZE + 1];
unsigned long vm_clump_allocs, vm_clump_inserts, vm_clump_inrange, vm_clump_promotes;

static inline void
vm_clump_update_stats(unsigned int c)
{
	assert(c <= vm_clump_size);
	if (c > 0 && c <= vm_clump_size) {
		vm_clump_stats[c] += c;
	}
	vm_clump_allocs += c;
}
#endif  /*  if DEVELOPMENT || DEBUG */

/* Called once to setup the VM clump knobs */
static void
vm_page_setup_clump( void )
{
	unsigned int override, n;

	vm_clump_size = DEFAULT_CLUMP_SIZE;
	if (PE_parse_boot_argn("clump_size", &override, sizeof(override))) {
		vm_clump_size = override;
	}

	if (vm_clump_size > MAX_CLUMP_SIZE) {
		panic("vm_page_setup_clump:: clump_size is too large!");
	}
	if (vm_clump_size < 1) {
		panic("vm_page_setup_clump:: clump_size must be >= 1");
	}
	if ((vm_clump_size & (vm_clump_size - 1)) != 0) {
		panic("vm_page_setup_clump:: clump_size must be a power of 2");
	}

	vm_clump_promote_threshold = vm_clump_size;
	vm_clump_mask = vm_clump_size - 1;
	for (vm_clump_shift = 0, n = vm_clump_size; n > 1; n >>= 1, vm_clump_shift++) {
		;
	}

#if DEVELOPMENT || DEBUG
	bzero(vm_clump_stats, sizeof(vm_clump_stats));
	vm_clump_allocs = vm_clump_inserts = vm_clump_inrange = vm_clump_promotes = 0;
#endif  /*  if DEVELOPMENT || DEBUG */
}

#endif  /* #if defined (__x86_64__) */

#define COLOR_GROUPS_TO_STEAL   4

/* Called once during statup, once the cache geometry is known.
 */
static void
vm_page_set_colors( void )
{
	unsigned int    n, override;

#if defined (__x86_64__)
	/* adjust #colors because we need to color outside the clump boundary */
	vm_cache_geometry_colors >>= vm_clump_shift;
#endif
	if (PE_parse_boot_argn("colors", &override, sizeof(override))) {                /* colors specified as a boot-arg? */
		n = override;
	} else if (vm_cache_geometry_colors) {                  /* do we know what the cache geometry is? */
		n = vm_cache_geometry_colors;
	} else {
		n = DEFAULT_COLORS;                             /* use default if all else fails */
	}
	if (n == 0) {
		n = 1;
	}
	if (n > MAX_COLORS) {
		n = MAX_COLORS;
	}

	/* the count must be a power of 2  */
	if ((n & (n - 1)) != 0) {
		n = DEFAULT_COLORS;                             /* use default if all else fails */
	}
	vm_colors = n;
	vm_color_mask = n - 1;

	vm_free_magazine_refill_limit = vm_colors * COLOR_GROUPS_TO_STEAL;

#if defined (__x86_64__)
	/* adjust for reduction in colors due to clumping and multiple cores */
	if (real_ncpus) {
		vm_free_magazine_refill_limit *= (vm_clump_size * real_ncpus);
	}
#endif
}

/*
 * During single threaded early boot we don't initialize all pages.
 * This avoids some delay during boot. They'll be initialized and
 * added to the free list as needed or after we are multithreaded by
 * what becomes the pageout thread.
 */
static boolean_t fill = FALSE;
static unsigned int fillval;
uint_t vm_delayed_count = 0;    /* when non-zero, indicates we may have more pages to init */
ppnum_t delay_above_pnum = PPNUM_MAX;

/*
 * For x86 first 8 Gig initializes quickly and gives us lots of lowmem + mem above to start off with.
 * If ARM ever uses delayed page initialization, this value may need to be quite different.
 */
#define DEFAULT_DELAY_ABOVE_PHYS_GB (8)

/*
 * When we have to dip into more delayed pages due to low memory, free up
 * a large chunk to get things back to normal. This avoids contention on the
 * delayed code allocating page by page.
 */
#define VM_DELAY_PAGE_CHUNK ((1024 * 1024 * 1024) / PAGE_SIZE)

/*
 * Get and initialize the next delayed page.
 */
static vm_page_t
vm_get_delayed_page(int grab_options)
{
	vm_page_t p;
	ppnum_t   pnum;

	/*
	 * Get a new page if we have one.
	 */
	vm_free_page_lock();
	if (vm_delayed_count == 0) {
		vm_free_page_unlock();
		return NULL;
	}

	if (!pmap_next_page(&pnum)) {
		vm_delayed_count = 0;
		vm_free_page_unlock();
		return NULL;
	}


	assert(vm_delayed_count > 0);
	--vm_delayed_count;

#if defined(__x86_64__)
	/* x86 cluster code requires increasing phys_page in vm_pages[] */
	if (vm_pages_count > 0) {
		assert(pnum > vm_pages[vm_pages_count - 1].vmp_phys_page);
	}
#endif
	p = &vm_pages[vm_pages_count];
	assert(p < vm_page_array_ending_addr);
	vm_page_init(p, pnum, FALSE);
	++vm_pages_count;
	++vm_page_pages;
	vm_free_page_unlock();

	/*
	 * These pages were initially counted as wired, undo that now.
	 */
	if (grab_options & VM_PAGE_GRAB_Q_LOCK_HELD) {
		LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);
	} else {
		LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_NOTOWNED);
		vm_page_lockspin_queues();
	}
	--vm_page_wire_count;
	--vm_page_wire_count_initial;
	if (vm_page_wire_count_on_boot != 0) {
		--vm_page_wire_count_on_boot;
	}
	if (!(grab_options & VM_PAGE_GRAB_Q_LOCK_HELD)) {
		vm_page_unlock_queues();
	}


	if (fill) {
		fillPage(pnum, fillval);
	}
	return p;
}

static void vm_page_module_init_delayed(void);

/*
 * Free all remaining delayed pages to the free lists.
 */
void
vm_free_delayed_pages(void)
{
	vm_page_t   p;
	vm_page_t   list = NULL;
	uint_t      cnt = 0;
	vm_offset_t start_free_va;
	int64_t     free_size;

	while ((p = vm_get_delayed_page(VM_PAGE_GRAB_OPTIONS_NONE)) != NULL) {
		if (vm_himemory_mode) {
			vm_page_release(p, FALSE);
		} else {
			p->vmp_snext = list;
			list = p;
		}
		++cnt;
	}

	/*
	 * Free the pages in reverse order if not himemory mode.
	 * Hence the low memory pages will be first on free lists. (LIFO)
	 */
	while (list != NULL) {
		p = list;
		list = p->vmp_snext;
		p->vmp_snext = NULL;
		vm_page_release(p, FALSE);
	}
#if DEVELOPMENT || DEBUG
	kprintf("vm_free_delayed_pages: initialized %d free pages\n", cnt);
#endif

	/*
	 * Free up any unused full pages at the end of the vm_pages[] array
	 */
	start_free_va = round_page((vm_offset_t)&vm_pages[vm_pages_count]);

#if defined(__x86_64__)
	/*
	 * Since x86 might have used large pages for vm_pages[], we can't
	 * free starting in the middle of a partially used large page.
	 */
	if (pmap_query_pagesize(kernel_pmap, start_free_va) == I386_LPGBYTES) {
		start_free_va = ((start_free_va + I386_LPGMASK) & ~I386_LPGMASK);
	}
#endif
	if (start_free_va < (vm_offset_t)vm_page_array_ending_addr) {
		free_size = trunc_page((vm_offset_t)vm_page_array_ending_addr - start_free_va);
		if (free_size > 0) {
			ml_static_mfree(start_free_va, (vm_offset_t)free_size);
			vm_page_array_ending_addr = (void *)start_free_va;

			/*
			 * Note there's no locking here, as only this thread will ever change this value.
			 * The reader, vm_page_diagnose, doesn't grab any locks for the counts it looks at.
			 */
			vm_page_stolen_count -= (free_size >> PAGE_SHIFT);

#if DEVELOPMENT || DEBUG
			kprintf("Freeing final unused %ld bytes from vm_pages[] at 0x%lx\n",
			    (long)free_size, (long)start_free_va);
#endif
		}
	}


	/*
	 * now we can create the VM page array zone
	 */
	vm_page_module_init_delayed();
}

/*
 * Try and free up enough delayed pages to match a contig memory allocation.
 */
static void
vm_free_delayed_pages_contig(
	uint_t    npages,
	ppnum_t   max_pnum,
	ppnum_t   pnum_mask)
{
	vm_page_t p;
	ppnum_t   pnum;
	uint_t    cnt = 0;

	/*
	 * Treat 0 as the absolute max page number.
	 */
	if (max_pnum == 0) {
		max_pnum = PPNUM_MAX;
	}

	/*
	 * Free till we get a properly aligned start page
	 */
	for (;;) {
		p = vm_get_delayed_page(VM_PAGE_GRAB_OPTIONS_NONE);
		if (p == NULL) {
			return;
		}
		pnum = VM_PAGE_GET_PHYS_PAGE(p);
		vm_page_release(p, FALSE);
		if (pnum >= max_pnum) {
			return;
		}
		if ((pnum & pnum_mask) == 0) {
			break;
		}
	}

	/*
	 * Having a healthy pool of free pages will help performance. We don't
	 * want to fall back to the delayed code for every page allocation.
	 */
	if (vm_page_free_count < VM_DELAY_PAGE_CHUNK) {
		npages += VM_DELAY_PAGE_CHUNK;
	}

	/*
	 * Now free up the pages
	 */
	for (cnt = 1; cnt < npages; ++cnt) {
		p = vm_get_delayed_page(VM_PAGE_GRAB_OPTIONS_NONE);
		if (p == NULL) {
			return;
		}
		vm_page_release(p, FALSE);
	}
}

#define ROUNDUP_NEXTP2(X) (1U << (32 - __builtin_clz((X) - 1)))

void
vm_page_init_local_q(unsigned int num_cpus)
{
	struct vpl *t_local_q;

	/*
	 * no point in this for a uni-processor system
	 */
	if (num_cpus >= 2) {
		ml_cpu_info_t cpu_info;

		/*
		 * Force the allocation alignment to a cacheline,
		 * because the `vpl` struct has a lock and will be taken
		 * cross CPU so we want to isolate the rest of the per-CPU
		 * data to avoid false sharing due to this lock being taken.
		 */

		ml_cpu_get_info(&cpu_info);

		t_local_q = zalloc_percpu_permanent(sizeof(struct vpl),
		    cpu_info.cache_line_size - 1);

		zpercpu_foreach(lq, t_local_q) {
			VPL_LOCK_INIT(lq, &vm_page_lck_grp_local, &vm_page_lck_attr);
			vm_page_queue_init(&lq->vpl_queue);
		}

		/* make the initialization visible to all cores */
		os_atomic_store(&vm_page_local_q, t_local_q, release);
	}
}

/*
 * vm_init_before_launchd
 *
 * This should be called right before launchd is loaded.
 */
void
vm_init_before_launchd()
{
	vm_page_lockspin_queues();
	vm_page_wire_count_on_boot = vm_page_wire_count;
	vm_page_unlock_queues();
}


/*
 *	vm_page_bootstrap:
 *
 *	Initializes the resident memory module.
 *
 *	Allocates memory for the page cells, and
 *	for the object/offset-to-page hash table headers.
 *	Each page cell is initialized and placed on the free list.
 *	Returns the range of available kernel virtual memory.
 */
__startup_func
void
vm_page_bootstrap(
	vm_offset_t             *startp,
	vm_offset_t             *endp)
{
	unsigned int            i;
	unsigned int            log1;
	unsigned int            log2;
	unsigned int            size;

	/*
	 *	Initialize the page queues.
	 */

	lck_mtx_init(&vm_page_queue_free_lock, &vm_page_lck_grp_free, &vm_page_lck_attr);
	lck_mtx_init(&vm_page_queue_lock, &vm_page_lck_grp_queue, &vm_page_lck_attr);
	lck_mtx_init(&vm_purgeable_queue_lock, &vm_page_lck_grp_purge, &vm_page_lck_attr);

	for (i = 0; i < PURGEABLE_Q_TYPE_MAX; i++) {
		int group;

		purgeable_queues[i].token_q_head = 0;
		purgeable_queues[i].token_q_tail = 0;
		for (group = 0; group < NUM_VOLATILE_GROUPS; group++) {
			queue_init(&purgeable_queues[i].objq[group]);
		}

		purgeable_queues[i].type = i;
		purgeable_queues[i].new_pages = 0;
#if MACH_ASSERT
		purgeable_queues[i].debug_count_tokens = 0;
		purgeable_queues[i].debug_count_objects = 0;
#endif
	}
	;
	purgeable_nonvolatile_count = 0;
	queue_init(&purgeable_nonvolatile_queue);

	for (i = 0; i < MAX_COLORS; i++) {
		vm_page_queue_init(&vm_page_queue_free[i].qhead);
	}

	vm_page_queue_init(&vm_lopage_queue_free);
	vm_page_queue_init(&vm_page_queue_active);
	vm_page_queue_init(&vm_page_queue_inactive);
#if CONFIG_SECLUDED_MEMORY
	vm_page_queue_init(&vm_page_queue_secluded);
#endif /* CONFIG_SECLUDED_MEMORY */
	vm_page_queue_init(&vm_page_queue_cleaned);
	vm_page_queue_init(&vm_page_queue_throttled);
	vm_page_queue_init(&vm_page_queue_anonymous);
	queue_init(&vm_objects_wired);

	for (i = 0; i <= vm_page_max_speculative_age_q; i++) {
		vm_page_queue_init(&vm_page_queue_speculative[i].age_q);

		vm_page_queue_speculative[i].age_ts.tv_sec = 0;
		vm_page_queue_speculative[i].age_ts.tv_nsec = 0;
	}

	vm_page_queue_init(&vm_page_queue_donate);
	vm_page_queue_init(&vm_page_queue_background);

	vm_page_background_count = 0;
	vm_page_background_internal_count = 0;
	vm_page_background_external_count = 0;
	vm_page_background_promoted_count = 0;

	vm_page_background_target = (unsigned int)(atop_64(max_mem) / 25);

	if (vm_page_background_target > VM_PAGE_BACKGROUND_TARGET_MAX) {
		vm_page_background_target = VM_PAGE_BACKGROUND_TARGET_MAX;
	}

#if    defined(__LP64__)
	vm_page_background_mode = VM_PAGE_BG_ENABLED;
	vm_page_donate_mode = VM_PAGE_DONATE_ENABLED;
#else
	vm_page_background_mode = VM_PAGE_BG_DISABLED;
	vm_page_donate_mode = VM_PAGE_DONATE_DISABLED;
#endif
	vm_page_background_exclude_external = 0;

	PE_parse_boot_argn("vm_page_bg_mode", &vm_page_background_mode, sizeof(vm_page_background_mode));
	PE_parse_boot_argn("vm_page_bg_exclude_external", &vm_page_background_exclude_external, sizeof(vm_page_background_exclude_external));
	PE_parse_boot_argn("vm_page_bg_target", &vm_page_background_target, sizeof(vm_page_background_target));

	if (vm_page_background_mode != VM_PAGE_BG_DISABLED && vm_page_background_mode != VM_PAGE_BG_ENABLED) {
		vm_page_background_mode = VM_PAGE_BG_DISABLED;
	}

	PE_parse_boot_argn("vm_page_donate_mode", &vm_page_donate_mode, sizeof(vm_page_donate_mode));
	if (vm_page_donate_mode != VM_PAGE_DONATE_DISABLED && vm_page_donate_mode != VM_PAGE_DONATE_ENABLED) {
		vm_page_donate_mode = VM_PAGE_DONATE_DISABLED;
	}

	vm_page_donate_target_high = VM_PAGE_DONATE_TARGET_HIGHWATER;
	vm_page_donate_target_low = VM_PAGE_DONATE_TARGET_LOWWATER;
	vm_page_donate_target = vm_page_donate_target_high;
	vm_page_donate_count = 0;

	vm_page_free_wanted = 0;
	vm_page_free_wanted_privileged = 0;
#if CONFIG_SECLUDED_MEMORY
	vm_page_free_wanted_secluded = 0;
#endif /* CONFIG_SECLUDED_MEMORY */

#if defined (__x86_64__)
	/* this must be called before vm_page_set_colors() */
	vm_page_setup_clump();
#endif

	vm_page_set_colors();

	bzero(vm_page_inactive_states, sizeof(vm_page_inactive_states));
	vm_page_inactive_states[VM_PAGE_ON_INACTIVE_INTERNAL_Q] = 1;
	vm_page_inactive_states[VM_PAGE_ON_INACTIVE_EXTERNAL_Q] = 1;
	vm_page_inactive_states[VM_PAGE_ON_INACTIVE_CLEANED_Q] = 1;

	bzero(vm_page_pageable_states, sizeof(vm_page_pageable_states));
	vm_page_pageable_states[VM_PAGE_ON_INACTIVE_INTERNAL_Q] = 1;
	vm_page_pageable_states[VM_PAGE_ON_INACTIVE_EXTERNAL_Q] = 1;
	vm_page_pageable_states[VM_PAGE_ON_INACTIVE_CLEANED_Q] = 1;
	vm_page_pageable_states[VM_PAGE_ON_ACTIVE_Q] = 1;
	vm_page_pageable_states[VM_PAGE_ON_SPECULATIVE_Q] = 1;
	vm_page_pageable_states[VM_PAGE_ON_THROTTLED_Q] = 1;
#if CONFIG_SECLUDED_MEMORY
	vm_page_pageable_states[VM_PAGE_ON_SECLUDED_Q] = 1;
#endif /* CONFIG_SECLUDED_MEMORY */

	bzero(vm_page_non_speculative_pageable_states, sizeof(vm_page_non_speculative_pageable_states));
	vm_page_non_speculative_pageable_states[VM_PAGE_ON_INACTIVE_INTERNAL_Q] = 1;
	vm_page_non_speculative_pageable_states[VM_PAGE_ON_INACTIVE_EXTERNAL_Q] = 1;
	vm_page_non_speculative_pageable_states[VM_PAGE_ON_INACTIVE_CLEANED_Q] = 1;
	vm_page_non_speculative_pageable_states[VM_PAGE_ON_ACTIVE_Q] = 1;
	vm_page_non_speculative_pageable_states[VM_PAGE_ON_THROTTLED_Q] = 1;
#if CONFIG_SECLUDED_MEMORY
	vm_page_non_speculative_pageable_states[VM_PAGE_ON_SECLUDED_Q] = 1;
#endif /* CONFIG_SECLUDED_MEMORY */

	bzero(vm_page_active_or_inactive_states, sizeof(vm_page_active_or_inactive_states));
	vm_page_active_or_inactive_states[VM_PAGE_ON_INACTIVE_INTERNAL_Q] = 1;
	vm_page_active_or_inactive_states[VM_PAGE_ON_INACTIVE_EXTERNAL_Q] = 1;
	vm_page_active_or_inactive_states[VM_PAGE_ON_INACTIVE_CLEANED_Q] = 1;
	vm_page_active_or_inactive_states[VM_PAGE_ON_ACTIVE_Q] = 1;
#if CONFIG_SECLUDED_MEMORY
	vm_page_active_or_inactive_states[VM_PAGE_ON_SECLUDED_Q] = 1;
#endif /* CONFIG_SECLUDED_MEMORY */

	for (vm_tag_t t = 0; t < VM_KERN_MEMORY_FIRST_DYNAMIC; t++) {
		vm_allocation_sites_static[t].refcount = 2;
		vm_allocation_sites_static[t].tag = t;
		vm_allocation_sites[t] = &vm_allocation_sites_static[t];
	}
	vm_allocation_sites_static[VM_KERN_MEMORY_FIRST_DYNAMIC].refcount = 2;
	vm_allocation_sites_static[VM_KERN_MEMORY_FIRST_DYNAMIC].tag = VM_KERN_MEMORY_ANY;
	vm_allocation_sites[VM_KERN_MEMORY_ANY] = &vm_allocation_sites_static[VM_KERN_MEMORY_FIRST_DYNAMIC];

	/*
	 *	Steal memory for the map and zone subsystems.
	 */
	kernel_startup_initialize_upto(STARTUP_SUB_PMAP_STEAL);

	/*
	 *	Allocate (and initialize) the virtual-to-physical
	 *	table hash buckets.
	 *
	 *	The number of buckets should be a power of two to
	 *	get a good hash function.  The following computation
	 *	chooses the first power of two that is greater
	 *	than the number of physical pages in the system.
	 */

	if (vm_page_bucket_count == 0) {
		unsigned int npages = pmap_free_pages();

		vm_page_bucket_count = 1;
		while (vm_page_bucket_count < npages) {
			vm_page_bucket_count <<= 1;
		}
	}
	vm_page_bucket_lock_count = (vm_page_bucket_count + BUCKETS_PER_LOCK - 1) / BUCKETS_PER_LOCK;

	vm_page_hash_mask = vm_page_bucket_count - 1;

	/*
	 *	Calculate object shift value for hashing algorithm:
	 *		O = log2(sizeof(struct vm_object))
	 *		B = log2(vm_page_bucket_count)
	 *	        hash shifts the object left by
	 *		B/2 - O
	 */
	size = vm_page_bucket_count;
	for (log1 = 0; size > 1; log1++) {
		size /= 2;
	}
	size = sizeof(struct vm_object);
	for (log2 = 0; size > 1; log2++) {
		size /= 2;
	}
	vm_page_hash_shift = log1 / 2 - log2 + 1;

	vm_page_bucket_hash = 1 << ((log1 + 1) >> 1);           /* Get (ceiling of sqrt of table size) */
	vm_page_bucket_hash |= 1 << ((log1 + 1) >> 2);          /* Get (ceiling of quadroot of table size) */
	vm_page_bucket_hash |= 1;                                                       /* Set bit and add 1 - always must be 1 to insure unique series */

	if (vm_page_hash_mask & vm_page_bucket_count) {
		printf("vm_page_bootstrap: WARNING -- strange page hash\n");
	}

#if VM_PAGE_BUCKETS_CHECK
#if VM_PAGE_FAKE_BUCKETS
	/*
	 * Allocate a decoy set of page buckets, to detect
	 * any stomping there.
	 */
	vm_page_fake_buckets = (vm_page_bucket_t *)
	    pmap_steal_memory(vm_page_bucket_count *
	    sizeof(vm_page_bucket_t), 0);
	vm_page_fake_buckets_start = (vm_map_offset_t) vm_page_fake_buckets;
	vm_page_fake_buckets_end =
	    vm_map_round_page((vm_page_fake_buckets_start +
	    (vm_page_bucket_count *
	    sizeof(vm_page_bucket_t))),
	    PAGE_MASK);
	char *cp;
	for (cp = (char *)vm_page_fake_buckets_start;
	    cp < (char *)vm_page_fake_buckets_end;
	    cp++) {
		*cp = 0x5a;
	}
#endif /* VM_PAGE_FAKE_BUCKETS */
#endif /* VM_PAGE_BUCKETS_CHECK */

	kernel_debug_string_early("vm_page_buckets");
	vm_page_buckets = (vm_page_bucket_t *)
	    pmap_steal_memory(vm_page_bucket_count *
	    sizeof(vm_page_bucket_t), 0);

	kernel_debug_string_early("vm_page_bucket_locks");
	vm_page_bucket_locks = (lck_spin_t *)
	    pmap_steal_memory(vm_page_bucket_lock_count *
	    sizeof(lck_spin_t), 0);

	for (i = 0; i < vm_page_bucket_count; i++) {
		vm_page_bucket_t *bucket = &vm_page_buckets[i];

		bucket->page_list = VM_PAGE_PACK_PTR(VM_PAGE_NULL);
#if     MACH_PAGE_HASH_STATS
		bucket->cur_count = 0;
		bucket->hi_count = 0;
#endif /* MACH_PAGE_HASH_STATS */
	}

	for (i = 0; i < vm_page_bucket_lock_count; i++) {
		lck_spin_init(&vm_page_bucket_locks[i], &vm_page_lck_grp_bucket, &vm_page_lck_attr);
	}

	vm_tag_init();

#if VM_PAGE_BUCKETS_CHECK
	vm_page_buckets_check_ready = TRUE;
#endif /* VM_PAGE_BUCKETS_CHECK */

	/*
	 *	Machine-dependent code allocates the resident page table.
	 *	It uses vm_page_init to initialize the page frames.
	 *	The code also returns to us the virtual space available
	 *	to the kernel.  We don't trust the pmap module
	 *	to get the alignment right.
	 */

	kernel_debug_string_early("pmap_startup");
	pmap_startup(&virtual_space_start, &virtual_space_end);
	virtual_space_start = round_page(virtual_space_start);
	virtual_space_end = trunc_page(virtual_space_end);

	*startp = virtual_space_start;
	*endp = virtual_space_end;

	/*
	 *	Compute the initial "wire" count.
	 *	Up until now, the pages which have been set aside are not under
	 *	the VM system's control, so although they aren't explicitly
	 *	wired, they nonetheless can't be moved. At this moment,
	 *	all VM managed pages are "free", courtesy of pmap_startup.
	 */
	assert((unsigned int) atop_64(max_mem) == atop_64(max_mem));
	vm_page_wire_count = ((unsigned int) atop_64(max_mem)) -
	    vm_page_free_count - vm_lopage_free_count;
#if CONFIG_SECLUDED_MEMORY
	vm_page_wire_count -= vm_page_secluded_count;
#endif
	vm_page_wire_count_initial = vm_page_wire_count;

	/* capture this for later use */
	booter_size = ml_get_booter_memory_size();

	printf("vm_page_bootstrap: %d free pages, %d wired pages, (up to %d of which are delayed free)\n",
	    vm_page_free_count, vm_page_wire_count, vm_delayed_count);

	kernel_debug_string_early("vm_page_bootstrap complete");
}

#ifndef MACHINE_PAGES
/*
 * This is the early boot time allocator for data structures needed to bootstrap the VM system.
 * On x86 it will allocate large pages if size is sufficiently large. We don't need to do this
 * on ARM yet, due to the combination of a large base page size and smaller RAM devices.
 */
static void *
pmap_steal_memory_internal(
	vm_size_t size,
	vm_size_t alignment,
	boolean_t might_free,
	unsigned int flags,
	pmap_mapping_type_t mapping_type)
{
	kern_return_t kr;
	vm_offset_t addr;
	vm_offset_t map_addr;
	ppnum_t phys_page;
	unsigned int pmap_flags;

	/*
	 * Size needs to be aligned to word size.
	 */
	size = (size + sizeof(void *) - 1) & ~(sizeof(void *) - 1);

	/*
	 * Alignment defaults to word size if not specified.
	 */
	if (alignment == 0) {
		alignment = sizeof(void*);
	}

	/*
	 * Alignment must be no greater than a page and must be a power of two.
	 */
	assert(alignment <= PAGE_SIZE);
	assert((alignment & (alignment - 1)) == 0);

	/*
	 * On the first call, get the initial values for virtual address space
	 * and page align them.
	 */
	if (virtual_space_start == virtual_space_end) {
		pmap_virtual_space(&virtual_space_start, &virtual_space_end);
		virtual_space_start = round_page(virtual_space_start);
		virtual_space_end = trunc_page(virtual_space_end);

#if defined(__x86_64__)
		/*
		 * Release remaining unused section of preallocated KVA and the 4K page tables
		 * that map it. This makes the VA available for large page mappings.
		 */
		Idle_PTs_release(virtual_space_start, virtual_space_end);
#endif
	}

	/*
	 * Allocate the virtual space for this request. On x86, we'll align to a large page
	 * address if the size is big enough to back with at least 1 large page.
	 */
#if defined(__x86_64__)
	if (size >= I386_LPGBYTES) {
		virtual_space_start = ((virtual_space_start + I386_LPGMASK) & ~I386_LPGMASK);
	}
#endif
	virtual_space_start = (virtual_space_start + (alignment - 1)) & ~(alignment - 1);
	addr = virtual_space_start;
	virtual_space_start += size;

	//kprintf("pmap_steal_memory: %08lX - %08lX; size=%08lX\n", (long)addr, (long)virtual_space_start, (long)size);	/* (TEST/DEBUG) */

	/*
	 * Allocate and map physical pages to back the new virtual space.
	 */
	map_addr = round_page(addr);
	while (map_addr < addr + size) {
#if defined(__x86_64__)
		/*
		 * Back with a large page if properly aligned on x86
		 */
		if ((map_addr & I386_LPGMASK) == 0 &&
		    map_addr + I386_LPGBYTES <= addr + size &&
		    pmap_pre_expand_large(kernel_pmap, map_addr) == KERN_SUCCESS &&
		    pmap_next_page_large(&phys_page) == KERN_SUCCESS) {
			kr = pmap_enter(kernel_pmap, map_addr, phys_page,
			    VM_PROT_READ | VM_PROT_WRITE, VM_PROT_NONE,
			    VM_WIMG_USE_DEFAULT | VM_MEM_SUPERPAGE, FALSE, mapping_type);

			if (kr != KERN_SUCCESS) {
				panic("pmap_steal_memory: pmap_enter() large failed, new_addr=%#lx, phys_page=%u",
				    (unsigned long)map_addr, phys_page);
			}
			map_addr += I386_LPGBYTES;
			vm_page_wire_count += I386_LPGBYTES >> PAGE_SHIFT;
			vm_page_stolen_count += I386_LPGBYTES >> PAGE_SHIFT;
			vm_page_kern_lpage_count++;
			continue;
		}
#endif

		if (!pmap_next_page_hi(&phys_page, might_free)) {
			panic("pmap_steal_memory() size: 0x%llx", (uint64_t)size);
		}

#if defined(__x86_64__)
		pmap_pre_expand(kernel_pmap, map_addr);
#endif
		pmap_flags = flags ? flags : VM_WIMG_USE_DEFAULT;

		kr = pmap_enter(kernel_pmap, map_addr, phys_page,
		    VM_PROT_READ | VM_PROT_WRITE, VM_PROT_NONE,
		    pmap_flags, FALSE, mapping_type);

		if (kr != KERN_SUCCESS) {
			panic("pmap_steal_memory() pmap_enter failed, map_addr=%#lx, phys_page=%u",
			    (unsigned long)map_addr, phys_page);
		}
		map_addr += PAGE_SIZE;

		/*
		 * Account for newly stolen memory
		 */
		vm_page_wire_count++;
		vm_page_stolen_count++;
	}

#if defined(__x86_64__)
	/*
	 * The call with might_free is currently the last use of pmap_steal_memory*().
	 * Notify the pmap layer to record which high pages were allocated so far.
	 */
	if (might_free) {
		pmap_hi_pages_done();
	}
#endif
#if KASAN
	kasan_notify_address(round_page(addr), size);
#endif
	return (void *) addr;
}

void *
pmap_steal_memory(
	vm_size_t size,
	vm_size_t alignment)
{
	return pmap_steal_memory_internal(size, alignment, FALSE, 0, PMAP_MAPPING_TYPE_RESTRICTED);
}

void *
pmap_steal_freeable_memory(
	vm_size_t size)
{
	return pmap_steal_memory_internal(size, 0, TRUE, 0, PMAP_MAPPING_TYPE_RESTRICTED);
}




#if CONFIG_SECLUDED_MEMORY
/* boot-args to control secluded memory */
TUNABLE_DT(unsigned int, secluded_mem_mb, "/defaults", "kern.secluded_mem_mb", "secluded_mem_mb", 0, TUNABLE_DT_NONE);
/* IOKit can use secluded memory */
TUNABLE(bool, secluded_for_iokit, "secluded_for_iokit", true);
/* apps can use secluded memory */
TUNABLE(bool, secluded_for_apps, "secluded_for_apps", true);
/* filecache can use seclude memory */
TUNABLE(secluded_filecache_mode_t, secluded_for_filecache, "secluded_for_filecache", SECLUDED_FILECACHE_RDONLY);
uint64_t secluded_shutoff_trigger = 0;
uint64_t secluded_shutoff_headroom = 150 * 1024 * 1024; /* original value from N56 */
#endif /* CONFIG_SECLUDED_MEMORY */


#if defined(__arm64__)
extern void patch_low_glo_vm_page_info(void *, void *, uint32_t);
unsigned int vm_first_phys_ppnum = 0;
#endif

void vm_page_release_startup(vm_page_t mem);
void
pmap_startup(
	vm_offset_t     *startp,
	vm_offset_t     *endp)
{
	unsigned int    i, npages;
	ppnum_t         phys_page;
	uint64_t        mem_sz;
	uint64_t        start_ns;
	uint64_t        now_ns;
	uint_t          low_page_count = 0;

#if    defined(__LP64__)
	/*
	 * make sure we are aligned on a 64 byte boundary
	 * for VM_PAGE_PACK_PTR (it clips off the low-order
	 * 6 bits of the pointer)
	 */
	if (virtual_space_start != virtual_space_end) {
		virtual_space_start = round_page(virtual_space_start);
	}
#endif

	/*
	 * We calculate how many page frames we will have
	 * and then allocate the page structures in one chunk.
	 *
	 * Note that the calculation here doesn't take into account
	 * the memory needed to map what's being allocated, i.e. the page
	 * table entries. So the actual number of pages we get will be
	 * less than this. To do someday: include that in the computation.
	 *
	 * Also for ARM, we don't use the count of free_pages, but rather the
	 * range from last page to first page (ignore holes due to retired pages).
	 */
#if defined(__arm64__)
	mem_sz = pmap_free_pages_span() * (uint64_t)PAGE_SIZE;
#else /* defined(__arm64__) */
	mem_sz = pmap_free_pages() * (uint64_t)PAGE_SIZE;
#endif /* defined(__arm64__) */
	mem_sz += round_page(virtual_space_start) - virtual_space_start;        /* Account for any slop */
	npages = (uint_t)(mem_sz / (PAGE_SIZE + sizeof(*vm_pages)));    /* scaled to include the vm_page_ts */


	vm_pages = (vm_page_t) pmap_steal_freeable_memory(npages * sizeof *vm_pages);

	/*
	 * Check if we want to initialize pages to a known value
	 */
	if (PE_parse_boot_argn("fill", &fillval, sizeof(fillval))) {
		fill = TRUE;
	}
#if     DEBUG
	/* This slows down booting the DEBUG kernel, particularly on
	 * large memory systems, but is worthwhile in deterministically
	 * trapping uninitialized memory usage.
	 */
	if (!fill) {
		fill = TRUE;
		fillval = 0xDEB8F177;
	}
#endif
	if (fill) {
		kprintf("Filling vm_pages with pattern: 0x%x\n", fillval);
	}

#if CONFIG_SECLUDED_MEMORY
	/*
	 * Figure out how much secluded memory to have before we start
	 * release pages to free lists.
	 * The default, if specified nowhere else, is no secluded mem.
	 */
	vm_page_secluded_target = (unsigned int)atop_64(secluded_mem_mb * 1024ULL * 1024ULL);

	/*
	 * Allow a really large app to effectively use secluded memory until it exits.
	 */
	if (vm_page_secluded_target != 0) {
		/*
		 * Get an amount from boot-args, else use 1/2 of max_mem.
		 * 1/2 max_mem was chosen from a Peace daemon tentpole test which
		 * used munch to induce jetsam thrashing of false idle daemons on N56.
		 */
		int secluded_shutoff_mb;
		if (PE_parse_boot_argn("secluded_shutoff_mb", &secluded_shutoff_mb,
		    sizeof(secluded_shutoff_mb))) {
			secluded_shutoff_trigger = (uint64_t)secluded_shutoff_mb * 1024 * 1024;
		} else {
			secluded_shutoff_trigger = max_mem / 2;
		}

		/* ensure the headroom value is sensible and avoid underflows */
		assert(secluded_shutoff_trigger == 0 || secluded_shutoff_trigger > secluded_shutoff_headroom);
	}

#endif /* CONFIG_SECLUDED_MEMORY */

#if defined(__x86_64__)

	/*
	 * Decide how much memory we delay freeing at boot time.
	 */
	uint32_t delay_above_gb;
	if (!PE_parse_boot_argn("delay_above_gb", &delay_above_gb, sizeof(delay_above_gb))) {
		delay_above_gb = DEFAULT_DELAY_ABOVE_PHYS_GB;
	}

	if (delay_above_gb == 0) {
		delay_above_pnum = PPNUM_MAX;
	} else {
		delay_above_pnum = delay_above_gb * (1024 * 1024 * 1024 / PAGE_SIZE);
	}

	/* make sure we have sane breathing room: 1G above low memory */
	if (delay_above_pnum <= max_valid_low_ppnum) {
		delay_above_pnum = max_valid_low_ppnum + ((1024 * 1024 * 1024) >> PAGE_SHIFT);
	}

	if (delay_above_pnum < PPNUM_MAX) {
		printf("pmap_startup() delaying init/free of page nums > 0x%x\n", delay_above_pnum);
	}

#endif /* defined(__x86_64__) */

	/*
	 * Initialize and release the page frames.
	 */
	kernel_debug_string_early("page_frame_init");

	vm_page_array_beginning_addr = &vm_pages[0];
	vm_page_array_ending_addr = &vm_pages[npages];  /* used by ptr packing/unpacking code */
#if VM_PAGE_PACKED_FROM_ARRAY
	if (npages >= VM_PAGE_PACKED_FROM_ARRAY) {
		panic("pmap_startup(): too many pages to support vm_page packing");
	}
#endif

	vm_delayed_count = 0;

	absolutetime_to_nanoseconds(mach_absolute_time(), &start_ns);
	vm_pages_count = 0;
	for (i = 0; i < npages; i++) {
		/* Did we run out of pages? */
		if (!pmap_next_page(&phys_page)) {
			break;
		}

		if (phys_page < max_valid_low_ppnum) {
			++low_page_count;
		}

		/* Are we at high enough pages to delay the rest? */
		if (low_page_count > vm_lopage_free_limit && phys_page > delay_above_pnum) {
			vm_delayed_count = pmap_free_pages();
			break;
		}

#if defined(__arm64__)
		if (i == 0) {
			vm_first_phys_ppnum = phys_page;
			patch_low_glo_vm_page_info((void *)vm_page_array_beginning_addr,
			    (void *)vm_page_array_ending_addr, vm_first_phys_ppnum);
		}
#endif /* defined(__arm64__) */

#if defined(__x86_64__)
		/* The x86 clump freeing code requires increasing ppn's to work correctly */
		if (i > 0) {
			assert(phys_page > vm_pages[i - 1].vmp_phys_page);
		}
#endif
		++vm_pages_count;
		vm_page_init(&vm_pages[i], phys_page, FALSE);
		if (fill) {
			fillPage(phys_page, fillval);
		}
		if (vm_himemory_mode) {
			vm_page_release_startup(&vm_pages[i]);
		}
	}
	vm_page_pages = vm_pages_count; /* used to report to user space */

	if (!vm_himemory_mode) {
		do {
			if (!VMP_ERROR_GET(&vm_pages[--i])) {               /* skip retired pages */
				vm_page_release_startup(&vm_pages[i]);
			}
		} while (i != 0);
	}

	absolutetime_to_nanoseconds(mach_absolute_time(), &now_ns);
	printf("pmap_startup() init/release time: %lld microsec\n", (now_ns - start_ns) / NSEC_PER_USEC);
	printf("pmap_startup() delayed init/release of %d pages\n", vm_delayed_count);

#if defined(__LP64__)
	if ((vm_page_t)(VM_PAGE_UNPACK_PTR(VM_PAGE_PACK_PTR(&vm_pages[0]))) != &vm_pages[0]) {
		panic("VM_PAGE_PACK_PTR failed on &vm_pages[0] - %p", (void *)&vm_pages[0]);
	}

	if ((vm_page_t)(VM_PAGE_UNPACK_PTR(VM_PAGE_PACK_PTR(&vm_pages[vm_pages_count - 1]))) != &vm_pages[vm_pages_count - 1]) {
		panic("VM_PAGE_PACK_PTR failed on &vm_pages[vm_pages_count-1] - %p", (void *)&vm_pages[vm_pages_count - 1]);
	}
#endif

	VM_CHECK_MEMORYSTATUS;

	/*
	 * We have to re-align virtual_space_start,
	 * because pmap_steal_memory has been using it.
	 */
	virtual_space_start = round_page(virtual_space_start);
	*startp = virtual_space_start;
	*endp = virtual_space_end;
}
#endif  /* MACHINE_PAGES */

/*
 * Create the zone that represents the vm_pages[] array. Nothing ever allocates
 * or frees to this zone. It's just here for reporting purposes via zprint command.
 * This needs to be done after all initially delayed pages are put on the free lists.
 */
static void
vm_page_module_init_delayed(void)
{
	(void)zone_create_ext("vm pages array", sizeof(struct vm_page),
	    ZC_KASAN_NOREDZONE | ZC_KASAN_NOQUARANTINE, ZONE_ID_VM_PAGES, ^(zone_t z) {
		uint64_t vm_page_zone_pages, vm_page_array_zone_data_size;

		zone_set_exhaustible(z, 0, true);
		/*
		 * Reflect size and usage information for vm_pages[].
		 */

		z->z_elems_avail = (uint32_t)(vm_page_array_ending_addr - vm_pages);
		z->z_elems_free = z->z_elems_avail - vm_pages_count;
		zpercpu_get_cpu(z->z_stats, 0)->zs_mem_allocated =
		vm_pages_count * sizeof(struct vm_page);
		vm_page_array_zone_data_size = (uint64_t)vm_page_array_ending_addr - (uint64_t)vm_pages;
		vm_page_zone_pages = atop(round_page((vm_offset_t)vm_page_array_zone_data_size));
		z->z_wired_cur += vm_page_zone_pages;
		z->z_wired_hwm = z->z_wired_cur;
		z->z_va_cur = z->z_wired_cur;
		/* since zone accounts for these, take them out of stolen */
		VM_PAGE_MOVE_STOLEN(vm_page_zone_pages);
	});
}

/*
 * Create the vm_pages zone. This is used for the vm_page structures for the pages
 * that are scavanged from other boot time usages by ml_static_mfree(). As such,
 * this needs to happen in early VM bootstrap.
 */

__startup_func
static void
vm_page_module_init(void)
{
	vm_size_t vm_page_with_ppnum_size;

	/*
	 * Since the pointers to elements in this zone will be packed, they
	 * must have appropriate size. Not strictly what sizeof() reports.
	 */
	vm_page_with_ppnum_size =
	    (sizeof(struct vm_page_with_ppnum) + (VM_PAGE_PACKED_PTR_ALIGNMENT - 1)) &
	    ~(VM_PAGE_PACKED_PTR_ALIGNMENT - 1);

	vm_page_zone = zone_create_ext("vm pages", vm_page_with_ppnum_size,
	    ZC_ALIGNMENT_REQUIRED | ZC_VM | ZC_NO_TBI_TAG,
	    ZONE_ID_ANY, ^(zone_t z) {
		/*
		 * The number "10" is a small number that is larger than the number
		 * of fictitious pages that any single caller will attempt to allocate
		 * without blocking.
		 *
		 * The largest such number at the moment is kmem_alloc()
		 * when 2 guard pages are asked. 10 is simply a somewhat larger number,
		 * taking into account the 50% hysteresis the zone allocator uses.
		 *
		 * Note: this works at all because the zone allocator
		 *       doesn't ever allocate fictitious pages.
		 */
		zone_raise_reserve(z, 10);
	});
}
STARTUP(ZALLOC, STARTUP_RANK_SECOND, vm_page_module_init);

/*
 *	Routine:	vm_page_create
 *	Purpose:
 *		After the VM system is up, machine-dependent code
 *		may stumble across more physical memory.  For example,
 *		memory that it was reserving for a frame buffer.
 *		vm_page_create turns this memory into available pages.
 */

void
vm_page_create(
	ppnum_t start,
	ppnum_t end)
{
	ppnum_t         phys_page;
	vm_page_t       m;

	for (phys_page = start;
	    phys_page < end;
	    phys_page++) {
		m = vm_page_grab_fictitious_common(phys_page, TRUE);
		m->vmp_fictitious = FALSE;
		pmap_clear_noencrypt(phys_page);


		vm_free_page_lock();
		vm_page_pages++;
		vm_free_page_unlock();
		vm_page_release(m, FALSE);
	}
}


/*
 *	vm_page_hash:
 *
 *	Distributes the object/offset key pair among hash buckets.
 *
 *	NOTE:	The bucket count must be a power of 2
 */
#define vm_page_hash(object, offset) (\
	( (natural_t)((uintptr_t)object * vm_page_bucket_hash) + ((uint32_t)atop_64(offset) ^ vm_page_bucket_hash))\
	 & vm_page_hash_mask)


/*
 *	vm_page_insert:		[ internal use only ]
 *
 *	Inserts the given mem entry into the object/object-page
 *	table and object list.
 *
 *	The object must be locked.
 */
void
vm_page_insert(
	vm_page_t               mem,
	vm_object_t             object,
	vm_object_offset_t      offset)
{
	vm_page_insert_internal(mem, object, offset, VM_KERN_MEMORY_NONE, FALSE, TRUE, FALSE, FALSE, NULL);
}

void
vm_page_insert_wired(
	vm_page_t               mem,
	vm_object_t             object,
	vm_object_offset_t      offset,
	vm_tag_t                tag)
{
	vm_page_insert_internal(mem, object, offset, tag, FALSE, TRUE, FALSE, FALSE, NULL);
}

void
vm_page_insert_internal(
	vm_page_t               mem,
	vm_object_t             object,
	vm_object_offset_t      offset,
	vm_tag_t                tag,
	boolean_t               queues_lock_held,
	boolean_t               insert_in_hash,
	boolean_t               batch_pmap_op,
	boolean_t               batch_accounting,
	uint64_t                *delayed_ledger_update)
{
	vm_page_bucket_t        *bucket;
	lck_spin_t              *bucket_lock;
	int                     hash_id;
	task_t                  owner;
	int                     ledger_idx_volatile;
	int                     ledger_idx_nonvolatile;
	int                     ledger_idx_volatile_compressed;
	int                     ledger_idx_nonvolatile_compressed;
	int                     ledger_idx_composite;
	int                     ledger_idx_external_wired;
	boolean_t               do_footprint;

#if 0
	/*
	 * we may not hold the page queue lock
	 * so this check isn't safe to make
	 */
	VM_PAGE_CHECK(mem);
#endif

	assertf(page_aligned(offset), "0x%llx\n", offset);

	assert(!VM_PAGE_WIRED(mem) || mem->vmp_private || mem->vmp_fictitious || (tag != VM_KERN_MEMORY_NONE));

	vm_object_lock_assert_exclusive(object);
	LCK_MTX_ASSERT(&vm_page_queue_lock,
	    queues_lock_held ? LCK_MTX_ASSERT_OWNED
	    : LCK_MTX_ASSERT_NOTOWNED);

	if (queues_lock_held == FALSE) {
		assert(!VM_PAGE_PAGEABLE(mem));
	}

	if (insert_in_hash == TRUE) {
#if DEBUG || VM_PAGE_BUCKETS_CHECK
		if (mem->vmp_tabled || mem->vmp_object) {
			panic("vm_page_insert: page %p for (obj=%p,off=0x%llx) "
			    "already in (obj=%p,off=0x%llx)",
			    mem, object, offset, VM_PAGE_OBJECT(mem), mem->vmp_offset);
		}
#endif
		if (object->internal && (offset >= object->vo_size)) {
			panic("vm_page_insert_internal: (page=%p,obj=%p,off=0x%llx,size=0x%llx) inserted at offset past object bounds",
			    mem, object, offset, object->vo_size);
		}

		assert(vm_page_lookup(object, offset) == VM_PAGE_NULL);

		/*
		 *	Record the object/offset pair in this page
		 */

		mem->vmp_object = VM_PAGE_PACK_OBJECT(object);
		mem->vmp_offset = offset;

#if CONFIG_SECLUDED_MEMORY
		if (object->eligible_for_secluded) {
			vm_page_secluded.eligible_for_secluded++;
		}
#endif /* CONFIG_SECLUDED_MEMORY */

		/*
		 *	Insert it into the object_object/offset hash table
		 */
		hash_id = vm_page_hash(object, offset);
		bucket = &vm_page_buckets[hash_id];
		bucket_lock = &vm_page_bucket_locks[hash_id / BUCKETS_PER_LOCK];

		lck_spin_lock_grp(bucket_lock, &vm_page_lck_grp_bucket);

		mem->vmp_next_m = bucket->page_list;
		bucket->page_list = VM_PAGE_PACK_PTR(mem);
		assert(mem == (vm_page_t)(VM_PAGE_UNPACK_PTR(bucket->page_list)));

#if     MACH_PAGE_HASH_STATS
		if (++bucket->cur_count > bucket->hi_count) {
			bucket->hi_count = bucket->cur_count;
		}
#endif /* MACH_PAGE_HASH_STATS */
		mem->vmp_hashed = TRUE;
		lck_spin_unlock(bucket_lock);
	}

	{
		unsigned int    cache_attr;

		cache_attr = object->wimg_bits & VM_WIMG_MASK;

		if (cache_attr != VM_WIMG_USE_DEFAULT) {
			PMAP_SET_CACHE_ATTR(mem, object, cache_attr, batch_pmap_op);
		}
	}
	/*
	 *	Now link into the object's list of backed pages.
	 */
	vm_page_queue_enter(&object->memq, mem, vmp_listq);
	object->memq_hint = mem;
	mem->vmp_tabled = TRUE;

	/*
	 *	Show that the object has one more resident page.
	 */

	object->resident_page_count++;
	if (VM_PAGE_WIRED(mem)) {
		assert(mem->vmp_wire_count > 0);
		VM_OBJECT_WIRED_PAGE_UPDATE_START(object);
		VM_OBJECT_WIRED_PAGE_ADD(object, mem);
		VM_OBJECT_WIRED_PAGE_UPDATE_END(object, tag);
	}
	assert(object->resident_page_count >= object->wired_page_count);

#if DEVELOPMENT || DEBUG
	if (object->object_is_shared_cache &&
	    object->pager != NULL &&
	    object->pager->mo_pager_ops == &shared_region_pager_ops) {
		int new, old;
		assert(!object->internal);
		new = OSAddAtomic(+1, &shared_region_pagers_resident_count);
		do {
			old = shared_region_pagers_resident_peak;
		} while (old < new &&
		    !OSCompareAndSwap(old, new, &shared_region_pagers_resident_peak));
	}
#endif /* DEVELOPMENT || DEBUG */

	if (batch_accounting == FALSE) {
		if (object->internal) {
			OSAddAtomic(1, &vm_page_internal_count);
		} else {
			OSAddAtomic(1, &vm_page_external_count);
		}
	}

	/*
	 * It wouldn't make sense to insert a "reusable" page in
	 * an object (the page would have been marked "reusable" only
	 * at the time of a madvise(MADV_FREE_REUSABLE) if it was already
	 * in the object at that time).
	 * But a page could be inserted in a "all_reusable" object, if
	 * something faults it in (a vm_read() from another task or a
	 * "use-after-free" issue in user space, for example).  It can
	 * also happen if we're relocating a page from that object to
	 * a different physical page during a physically-contiguous
	 * allocation.
	 */
	assert(!mem->vmp_reusable);
	if (object->all_reusable) {
		OSAddAtomic(+1, &vm_page_stats_reusable.reusable_count);
	}

	if (object->purgable == VM_PURGABLE_DENY &&
	    !object->vo_ledger_tag) {
		owner = TASK_NULL;
	} else {
		owner = VM_OBJECT_OWNER(object);
		vm_object_ledger_tag_ledgers(object,
		    &ledger_idx_volatile,
		    &ledger_idx_nonvolatile,
		    &ledger_idx_volatile_compressed,
		    &ledger_idx_nonvolatile_compressed,
		    &ledger_idx_composite,
		    &ledger_idx_external_wired,
		    &do_footprint);
	}
	if (owner &&
	    object->internal &&
	    (object->purgable == VM_PURGABLE_NONVOLATILE ||
	    object->purgable == VM_PURGABLE_DENY ||
	    VM_PAGE_WIRED(mem))) {
		if (delayed_ledger_update) {
			*delayed_ledger_update += PAGE_SIZE;
		} else {
			/* more non-volatile bytes */
			ledger_credit(owner->ledger,
			    ledger_idx_nonvolatile,
			    PAGE_SIZE);
			if (do_footprint) {
				/* more footprint */
				ledger_credit(owner->ledger,
				    task_ledgers.phys_footprint,
				    PAGE_SIZE);
			} else if (ledger_idx_composite != -1) {
				ledger_credit(owner->ledger,
				    ledger_idx_composite,
				    PAGE_SIZE);
			}
		}
	} else if (owner &&
	    object->internal &&
	    (object->purgable == VM_PURGABLE_VOLATILE ||
	    object->purgable == VM_PURGABLE_EMPTY)) {
		assert(!VM_PAGE_WIRED(mem));
		/* more volatile bytes */
		ledger_credit(owner->ledger,
		    ledger_idx_volatile,
		    PAGE_SIZE);
	}

	if (object->purgable == VM_PURGABLE_VOLATILE) {
		if (VM_PAGE_WIRED(mem)) {
			OSAddAtomic(+1, &vm_page_purgeable_wired_count);
		} else {
			OSAddAtomic(+1, &vm_page_purgeable_count);
		}
	} else if (object->purgable == VM_PURGABLE_EMPTY &&
	    mem->vmp_q_state == VM_PAGE_ON_THROTTLED_Q) {
		/*
		 * This page belongs to a purged VM object but hasn't
		 * been purged (because it was "busy").
		 * It's in the "throttled" queue and hence not
		 * visible to vm_pageout_scan().  Move it to a pageable
		 * queue, so that it can eventually be reclaimed, instead
		 * of lingering in the "empty" object.
		 */
		if (queues_lock_held == FALSE) {
			vm_page_lockspin_queues();
		}
		vm_page_deactivate(mem);
		if (queues_lock_held == FALSE) {
			vm_page_unlock_queues();
		}
	}

#if VM_OBJECT_TRACKING_OP_MODIFIED
	if (vm_object_tracking_btlog &&
	    object->internal &&
	    object->resident_page_count == 0 &&
	    object->pager == NULL &&
	    object->shadow != NULL &&
	    object->shadow->vo_copy == object) {
		btlog_record(vm_object_tracking_btlog, object,
		    VM_OBJECT_TRACKING_OP_MODIFIED,
		    btref_get(__builtin_frame_address(0), 0));
	}
#endif /* VM_OBJECT_TRACKING_OP_MODIFIED */
}

/*
 *	vm_page_replace:
 *
 *	Exactly like vm_page_insert, except that we first
 *	remove any existing page at the given offset in object.
 *
 *	The object must be locked.
 */
void
vm_page_replace(
	vm_page_t               mem,
	vm_object_t             object,
	vm_object_offset_t      offset)
{
	vm_page_bucket_t *bucket;
	vm_page_t        found_m = VM_PAGE_NULL;
	lck_spin_t      *bucket_lock;
	int             hash_id;

#if 0
	/*
	 * we don't hold the page queue lock
	 * so this check isn't safe to make
	 */
	VM_PAGE_CHECK(mem);
#endif
	vm_object_lock_assert_exclusive(object);
#if DEBUG || VM_PAGE_BUCKETS_CHECK
	if (mem->vmp_tabled || mem->vmp_object) {
		panic("vm_page_replace: page %p for (obj=%p,off=0x%llx) "
		    "already in (obj=%p,off=0x%llx)",
		    mem, object, offset, VM_PAGE_OBJECT(mem), mem->vmp_offset);
	}
#endif
	LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_NOTOWNED);

	assert(!VM_PAGE_PAGEABLE(mem));

	/*
	 *	Record the object/offset pair in this page
	 */
	mem->vmp_object = VM_PAGE_PACK_OBJECT(object);
	mem->vmp_offset = offset;

	/*
	 *	Insert it into the object_object/offset hash table,
	 *	replacing any page that might have been there.
	 */

	hash_id = vm_page_hash(object, offset);
	bucket = &vm_page_buckets[hash_id];
	bucket_lock = &vm_page_bucket_locks[hash_id / BUCKETS_PER_LOCK];

	lck_spin_lock_grp(bucket_lock, &vm_page_lck_grp_bucket);

	if (bucket->page_list) {
		vm_page_packed_t *mp = &bucket->page_list;
		vm_page_t m = (vm_page_t)(VM_PAGE_UNPACK_PTR(*mp));

		do {
			/*
			 * compare packed object pointers
			 */
			if (m->vmp_object == mem->vmp_object && m->vmp_offset == offset) {
				/*
				 * Remove old page from hash list
				 */
				*mp = m->vmp_next_m;
				m->vmp_hashed = FALSE;
				m->vmp_next_m = VM_PAGE_PACK_PTR(NULL);

				found_m = m;
				break;
			}
			mp = &m->vmp_next_m;
		} while ((m = (vm_page_t)(VM_PAGE_UNPACK_PTR(*mp))));

		mem->vmp_next_m = bucket->page_list;
	} else {
		mem->vmp_next_m = VM_PAGE_PACK_PTR(NULL);
	}
	/*
	 * insert new page at head of hash list
	 */
	bucket->page_list = VM_PAGE_PACK_PTR(mem);
	mem->vmp_hashed = TRUE;

	lck_spin_unlock(bucket_lock);

	if (found_m) {
		/*
		 * there was already a page at the specified
		 * offset for this object... remove it from
		 * the object and free it back to the free list
		 */
		vm_page_free_unlocked(found_m, FALSE);
	}
	vm_page_insert_internal(mem, object, offset, VM_KERN_MEMORY_NONE, FALSE, FALSE, FALSE, FALSE, NULL);
}

/*
 *	vm_page_remove:		[ internal use only ]
 *
 *	Removes the given mem entry from the object/offset-page
 *	table and the object page list.
 *
 *	The object must be locked.
 */

void
vm_page_remove(
	vm_page_t       mem,
	boolean_t       remove_from_hash)
{
	vm_page_bucket_t *bucket;
	vm_page_t       this;
	lck_spin_t      *bucket_lock;
	int             hash_id;
	task_t          owner;
	vm_object_t     m_object;
	int             ledger_idx_volatile;
	int             ledger_idx_nonvolatile;
	int             ledger_idx_volatile_compressed;
	int             ledger_idx_nonvolatile_compressed;
	int             ledger_idx_composite;
	int             ledger_idx_external_wired;
	int             do_footprint;

	m_object = VM_PAGE_OBJECT(mem);

	vm_object_lock_assert_exclusive(m_object);
	assert(mem->vmp_tabled);
	assert(!mem->vmp_cleaning);
	assert(!mem->vmp_laundry);

	if (VM_PAGE_PAGEABLE(mem)) {
		LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);
	}
#if 0
	/*
	 * we don't hold the page queue lock
	 * so this check isn't safe to make
	 */
	VM_PAGE_CHECK(mem);
#endif
	if (remove_from_hash == TRUE) {
		/*
		 *	Remove from the object_object/offset hash table
		 */
		hash_id = vm_page_hash(m_object, mem->vmp_offset);
		bucket = &vm_page_buckets[hash_id];
		bucket_lock = &vm_page_bucket_locks[hash_id / BUCKETS_PER_LOCK];

		lck_spin_lock_grp(bucket_lock, &vm_page_lck_grp_bucket);

		if ((this = (vm_page_t)(VM_PAGE_UNPACK_PTR(bucket->page_list))) == mem) {
			/* optimize for common case */

			bucket->page_list = mem->vmp_next_m;
		} else {
			vm_page_packed_t        *prev;

			for (prev = &this->vmp_next_m;
			    (this = (vm_page_t)(VM_PAGE_UNPACK_PTR(*prev))) != mem;
			    prev = &this->vmp_next_m) {
				continue;
			}
			*prev = this->vmp_next_m;
		}
#if     MACH_PAGE_HASH_STATS
		bucket->cur_count--;
#endif /* MACH_PAGE_HASH_STATS */
		mem->vmp_hashed = FALSE;
		this->vmp_next_m = VM_PAGE_PACK_PTR(NULL);
		lck_spin_unlock(bucket_lock);
	}
	/*
	 *	Now remove from the object's list of backed pages.
	 */

	vm_page_remove_internal(mem);

	/*
	 *	And show that the object has one fewer resident
	 *	page.
	 */

	assert(m_object->resident_page_count > 0);
	m_object->resident_page_count--;

#if DEVELOPMENT || DEBUG
	if (m_object->object_is_shared_cache &&
	    m_object->pager != NULL &&
	    m_object->pager->mo_pager_ops == &shared_region_pager_ops) {
		assert(!m_object->internal);
		OSAddAtomic(-1, &shared_region_pagers_resident_count);
	}
#endif /* DEVELOPMENT || DEBUG */

	if (m_object->internal) {
#if DEBUG
		assert(vm_page_internal_count);
#endif /* DEBUG */

		OSAddAtomic(-1, &vm_page_internal_count);
	} else {
		assert(vm_page_external_count);
		OSAddAtomic(-1, &vm_page_external_count);

		if (mem->vmp_xpmapped) {
			assert(vm_page_xpmapped_external_count);
			OSAddAtomic(-1, &vm_page_xpmapped_external_count);
		}
	}
	if (!m_object->internal &&
	    m_object->cached_list.next &&
	    m_object->cached_list.prev) {
		if (m_object->resident_page_count == 0) {
			vm_object_cache_remove(m_object);
		}
	}

	if (VM_PAGE_WIRED(mem)) {
		assert(mem->vmp_wire_count > 0);
		VM_OBJECT_WIRED_PAGE_UPDATE_START(m_object);
		VM_OBJECT_WIRED_PAGE_REMOVE(m_object, mem);
		VM_OBJECT_WIRED_PAGE_UPDATE_END(m_object, m_object->wire_tag);
	}
	assert(m_object->resident_page_count >=
	    m_object->wired_page_count);
	if (mem->vmp_reusable) {
		assert(m_object->reusable_page_count > 0);
		m_object->reusable_page_count--;
		assert(m_object->reusable_page_count <=
		    m_object->resident_page_count);
		mem->vmp_reusable = FALSE;
		OSAddAtomic(-1, &vm_page_stats_reusable.reusable_count);
		vm_page_stats_reusable.reused_remove++;
	} else if (m_object->all_reusable) {
		OSAddAtomic(-1, &vm_page_stats_reusable.reusable_count);
		vm_page_stats_reusable.reused_remove++;
	}

	if (m_object->purgable == VM_PURGABLE_DENY &&
	    !m_object->vo_ledger_tag) {
		owner = TASK_NULL;
	} else {
		owner = VM_OBJECT_OWNER(m_object);
		vm_object_ledger_tag_ledgers(m_object,
		    &ledger_idx_volatile,
		    &ledger_idx_nonvolatile,
		    &ledger_idx_volatile_compressed,
		    &ledger_idx_nonvolatile_compressed,
		    &ledger_idx_composite,
		    &ledger_idx_external_wired,
		    &do_footprint);
	}
	if (owner &&
	    m_object->internal &&
	    (m_object->purgable == VM_PURGABLE_NONVOLATILE ||
	    m_object->purgable == VM_PURGABLE_DENY ||
	    VM_PAGE_WIRED(mem))) {
		/* less non-volatile bytes */
		ledger_debit(owner->ledger,
		    ledger_idx_nonvolatile,
		    PAGE_SIZE);
		if (do_footprint) {
			/* less footprint */
			ledger_debit(owner->ledger,
			    task_ledgers.phys_footprint,
			    PAGE_SIZE);
		} else if (ledger_idx_composite != -1) {
			ledger_debit(owner->ledger,
			    ledger_idx_composite,
			    PAGE_SIZE);
		}
	} else if (owner &&
	    m_object->internal &&
	    (m_object->purgable == VM_PURGABLE_VOLATILE ||
	    m_object->purgable == VM_PURGABLE_EMPTY)) {
		assert(!VM_PAGE_WIRED(mem));
		/* less volatile bytes */
		ledger_debit(owner->ledger,
		    ledger_idx_volatile,
		    PAGE_SIZE);
	}

	if (m_object->purgable == VM_PURGABLE_VOLATILE) {
		if (VM_PAGE_WIRED(mem)) {
			assert(vm_page_purgeable_wired_count > 0);
			OSAddAtomic(-1, &vm_page_purgeable_wired_count);
		} else {
			assert(vm_page_purgeable_count > 0);
			OSAddAtomic(-1, &vm_page_purgeable_count);
		}
	}

	if (m_object->set_cache_attr == TRUE) {
		pmap_set_cache_attributes(VM_PAGE_GET_PHYS_PAGE(mem), 0);
	}

	mem->vmp_tabled = FALSE;
	mem->vmp_object = 0;
	mem->vmp_offset = (vm_object_offset_t) -1;
}


/*
 *	vm_page_lookup:
 *
 *	Returns the page associated with the object/offset
 *	pair specified; if none is found, VM_PAGE_NULL is returned.
 *
 *	The object must be locked.  No side effects.
 */

#define VM_PAGE_HASH_LOOKUP_THRESHOLD   10

#if DEBUG_VM_PAGE_LOOKUP

struct {
	uint64_t        vpl_total;
	uint64_t        vpl_empty_obj;
	uint64_t        vpl_bucket_NULL;
	uint64_t        vpl_hit_hint;
	uint64_t        vpl_hit_hint_next;
	uint64_t        vpl_hit_hint_prev;
	uint64_t        vpl_fast;
	uint64_t        vpl_slow;
	uint64_t        vpl_hit;
	uint64_t        vpl_miss;

	uint64_t        vpl_fast_elapsed;
	uint64_t        vpl_slow_elapsed;
} vm_page_lookup_stats __attribute__((aligned(8)));

#endif

#define KDP_VM_PAGE_WALK_MAX    1000

vm_page_t
kdp_vm_page_lookup(
	vm_object_t             object,
	vm_object_offset_t      offset)
{
	vm_page_t cur_page;
	int num_traversed = 0;

	if (not_in_kdp) {
		panic("panic: kdp_vm_page_lookup done outside of kernel debugger");
	}

	vm_page_queue_iterate(&object->memq, cur_page, vmp_listq) {
		if (cur_page->vmp_offset == offset) {
			return cur_page;
		}
		num_traversed++;

		if (num_traversed >= KDP_VM_PAGE_WALK_MAX) {
			return VM_PAGE_NULL;
		}
	}

	return VM_PAGE_NULL;
}

vm_page_t
vm_page_lookup(
	vm_object_t             object,
	vm_object_offset_t      offset)
{
	vm_page_t       mem;
	vm_page_bucket_t *bucket;
	vm_page_queue_entry_t   qe;
	lck_spin_t      *bucket_lock = NULL;
	int             hash_id;
#if DEBUG_VM_PAGE_LOOKUP
	uint64_t        start, elapsed;

	OSAddAtomic64(1, &vm_page_lookup_stats.vpl_total);
#endif

	if (VM_KERNEL_ADDRESS(offset)) {
		offset = VM_KERNEL_STRIP_UPTR(offset);
	}

	vm_object_lock_assert_held(object);
	assertf(page_aligned(offset), "offset 0x%llx\n", offset);

	if (object->resident_page_count == 0) {
#if DEBUG_VM_PAGE_LOOKUP
		OSAddAtomic64(1, &vm_page_lookup_stats.vpl_empty_obj);
#endif
		return VM_PAGE_NULL;
	}

	mem = object->memq_hint;

	if (mem != VM_PAGE_NULL) {
		assert(VM_PAGE_OBJECT(mem) == object);

		if (mem->vmp_offset == offset) {
#if DEBUG_VM_PAGE_LOOKUP
			OSAddAtomic64(1, &vm_page_lookup_stats.vpl_hit_hint);
#endif
			return mem;
		}
		qe = (vm_page_queue_entry_t)vm_page_queue_next(&mem->vmp_listq);

		if (!vm_page_queue_end(&object->memq, qe)) {
			vm_page_t       next_page;

			next_page = (vm_page_t)((uintptr_t)qe);
			assert(VM_PAGE_OBJECT(next_page) == object);

			if (next_page->vmp_offset == offset) {
				object->memq_hint = next_page; /* new hint */
#if DEBUG_VM_PAGE_LOOKUP
				OSAddAtomic64(1, &vm_page_lookup_stats.vpl_hit_hint_next);
#endif
				return next_page;
			}
		}
		qe = (vm_page_queue_entry_t)vm_page_queue_prev(&mem->vmp_listq);

		if (!vm_page_queue_end(&object->memq, qe)) {
			vm_page_t prev_page;

			prev_page = (vm_page_t)((uintptr_t)qe);
			assert(VM_PAGE_OBJECT(prev_page) == object);

			if (prev_page->vmp_offset == offset) {
				object->memq_hint = prev_page; /* new hint */
#if DEBUG_VM_PAGE_LOOKUP
				OSAddAtomic64(1, &vm_page_lookup_stats.vpl_hit_hint_prev);
#endif
				return prev_page;
			}
		}
	}
	/*
	 * Search the hash table for this object/offset pair
	 */
	hash_id = vm_page_hash(object, offset);
	bucket = &vm_page_buckets[hash_id];

	/*
	 * since we hold the object lock, we are guaranteed that no
	 * new pages can be inserted into this object... this in turn
	 * guarantess that the page we're looking for can't exist
	 * if the bucket it hashes to is currently NULL even when looked
	 * at outside the scope of the hash bucket lock... this is a
	 * really cheap optimiztion to avoid taking the lock
	 */
	if (!bucket->page_list) {
#if DEBUG_VM_PAGE_LOOKUP
		OSAddAtomic64(1, &vm_page_lookup_stats.vpl_bucket_NULL);
#endif
		return VM_PAGE_NULL;
	}

#if DEBUG_VM_PAGE_LOOKUP
	start = mach_absolute_time();
#endif
	if (object->resident_page_count <= VM_PAGE_HASH_LOOKUP_THRESHOLD) {
		/*
		 * on average, it's roughly 3 times faster to run a short memq list
		 * than to take the spin lock and go through the hash list
		 */
		mem = (vm_page_t)vm_page_queue_first(&object->memq);

		while (!vm_page_queue_end(&object->memq, (vm_page_queue_entry_t)mem)) {
			if (mem->vmp_offset == offset) {
				break;
			}

			mem = (vm_page_t)vm_page_queue_next(&mem->vmp_listq);
		}
		if (vm_page_queue_end(&object->memq, (vm_page_queue_entry_t)mem)) {
			mem = NULL;
		}
	} else {
		vm_page_object_t        packed_object;

		packed_object = VM_PAGE_PACK_OBJECT(object);

		bucket_lock = &vm_page_bucket_locks[hash_id / BUCKETS_PER_LOCK];

		lck_spin_lock_grp(bucket_lock, &vm_page_lck_grp_bucket);

		for (mem = (vm_page_t)(VM_PAGE_UNPACK_PTR(bucket->page_list));
		    mem != VM_PAGE_NULL;
		    mem = (vm_page_t)(VM_PAGE_UNPACK_PTR(mem->vmp_next_m))) {
#if 0
			/*
			 * we don't hold the page queue lock
			 * so this check isn't safe to make
			 */
			VM_PAGE_CHECK(mem);
#endif
			if ((mem->vmp_object == packed_object) && (mem->vmp_offset == offset)) {
				break;
			}
		}
		lck_spin_unlock(bucket_lock);
	}

#if DEBUG_VM_PAGE_LOOKUP
	elapsed = mach_absolute_time() - start;

	if (bucket_lock) {
		OSAddAtomic64(1, &vm_page_lookup_stats.vpl_slow);
		OSAddAtomic64(elapsed, &vm_page_lookup_stats.vpl_slow_elapsed);
	} else {
		OSAddAtomic64(1, &vm_page_lookup_stats.vpl_fast);
		OSAddAtomic64(elapsed, &vm_page_lookup_stats.vpl_fast_elapsed);
	}
	if (mem != VM_PAGE_NULL) {
		OSAddAtomic64(1, &vm_page_lookup_stats.vpl_hit);
	} else {
		OSAddAtomic64(1, &vm_page_lookup_stats.vpl_miss);
	}
#endif
	if (mem != VM_PAGE_NULL) {
		assert(VM_PAGE_OBJECT(mem) == object);

		object->memq_hint = mem;
	}
	return mem;
}


/*
 *	vm_page_rename:
 *
 *	Move the given memory entry from its
 *	current object to the specified target object/offset.
 *
 *	The object must be locked.
 */
void
vm_page_rename(
	vm_page_t               mem,
	vm_object_t             new_object,
	vm_object_offset_t      new_offset)
{
	boolean_t       internal_to_external, external_to_internal;
	vm_tag_t        tag;
	vm_object_t     m_object;

	m_object = VM_PAGE_OBJECT(mem);

	assert(m_object != new_object);
	assert(m_object);

	/*
	 *	Changes to mem->vmp_object require the page lock because
	 *	the pageout daemon uses that lock to get the object.
	 */
	vm_page_lockspin_queues();

	internal_to_external = FALSE;
	external_to_internal = FALSE;

	if (mem->vmp_q_state == VM_PAGE_ON_ACTIVE_LOCAL_Q) {
		/*
		 * it's much easier to get the vm_page_pageable_xxx accounting correct
		 * if we first move the page to the active queue... it's going to end
		 * up there anyway, and we don't do vm_page_rename's frequently enough
		 * for this to matter.
		 */
		vm_page_queues_remove(mem, FALSE);
		vm_page_activate(mem);
	}
	if (VM_PAGE_PAGEABLE(mem)) {
		if (m_object->internal && !new_object->internal) {
			internal_to_external = TRUE;
		}
		if (!m_object->internal && new_object->internal) {
			external_to_internal = TRUE;
		}
	}

	tag = m_object->wire_tag;
	vm_page_remove(mem, TRUE);
	vm_page_insert_internal(mem, new_object, new_offset, tag, TRUE, TRUE, FALSE, FALSE, NULL);

	if (internal_to_external) {
		vm_page_pageable_internal_count--;
		vm_page_pageable_external_count++;
	} else if (external_to_internal) {
		vm_page_pageable_external_count--;
		vm_page_pageable_internal_count++;
	}

	vm_page_unlock_queues();
}

/*
 *	vm_page_init:
 *
 *	Initialize the fields in a new page.
 *	This takes a structure with random values and initializes it
 *	so that it can be given to vm_page_release or vm_page_insert.
 */
void
vm_page_init(
	vm_page_t mem,
	ppnum_t   phys_page,
	boolean_t lopage)
{
	uint_t    i;
	uintptr_t *p;

	assert(phys_page);

#if DEBUG
	if ((phys_page != vm_page_fictitious_addr) && (phys_page != vm_page_guard_addr)) {
		if (!(pmap_valid_page(phys_page))) {
			panic("vm_page_init: non-DRAM phys_page 0x%x", phys_page);
		}
	}
#endif /* DEBUG */

	/*
	 * Initialize the fields of the vm_page. If adding any new fields to vm_page,
	 * try to use initial values which match 0. This minimizes the number of writes
	 * needed for boot-time initialization.
	 *
	 * Kernel bzero() isn't an inline yet, so do it by hand for performance.
	 */
	assert(VM_PAGE_NOT_ON_Q == 0);
	assert(sizeof(*mem) % sizeof(uintptr_t) == 0);
	for (p = (uintptr_t *)(void *)mem, i = sizeof(*mem) / sizeof(uintptr_t); i != 0; --i) {
		*p++ = 0;
	}
	mem->vmp_offset = (vm_object_offset_t)-1;
	mem->vmp_busy = TRUE;
	mem->vmp_lopage = lopage;

	VM_PAGE_SET_PHYS_PAGE(mem, phys_page);
#if 0
	/*
	 * we're leaving this turned off for now... currently pages
	 * come off the free list and are either immediately dirtied/referenced
	 * due to zero-fill or COW faults, or are used to read or write files...
	 * in the file I/O case, the UPL mechanism takes care of clearing
	 * the state of the HW ref/mod bits in a somewhat fragile way.
	 * Since we may change the way this works in the future (to toughen it up),
	 * I'm leaving this as a reminder of where these bits could get cleared
	 */

	/*
	 * make sure both the h/w referenced and modified bits are
	 * clear at this point... we are especially dependent on
	 * not finding a 'stale' h/w modified in a number of spots
	 * once this page goes back into use
	 */
	pmap_clear_refmod(phys_page, VM_MEM_MODIFIED | VM_MEM_REFERENCED);
#endif
}

/*
 *	vm_page_grab_fictitious:
 *
 *	Remove a fictitious page from the free list.
 *	Returns VM_PAGE_NULL if there are no free pages.
 */

static vm_page_t
vm_page_grab_fictitious_common(ppnum_t phys_addr, boolean_t canwait)
{
	vm_page_t m;

	m = zalloc_flags(vm_page_zone, canwait ? Z_WAITOK : Z_NOWAIT);
	if (m) {
		vm_page_init(m, phys_addr, FALSE);
		m->vmp_fictitious = TRUE;
	}
	return m;
}

vm_page_t
vm_page_grab_fictitious(boolean_t canwait)
{
	return vm_page_grab_fictitious_common(vm_page_fictitious_addr, canwait);
}

int vm_guard_count;


vm_page_t
vm_page_grab_guard(boolean_t canwait)
{
	vm_page_t page;
	page = vm_page_grab_fictitious_common(vm_page_guard_addr, canwait);
	if (page) {
		OSAddAtomic(1, &vm_guard_count);
	}
	return page;
}


/*
 *	vm_page_release_fictitious:
 *
 *	Release a fictitious page to the zone pool
 */
void
vm_page_release_fictitious(
	vm_page_t m)
{
	assert((m->vmp_q_state == VM_PAGE_NOT_ON_Q) || (m->vmp_q_state == VM_PAGE_IS_WIRED));
	assert(m->vmp_fictitious);
	assert(VM_PAGE_GET_PHYS_PAGE(m) == vm_page_fictitious_addr ||
	    VM_PAGE_GET_PHYS_PAGE(m) == vm_page_guard_addr);
	assert(!m->vmp_realtime);

	if (VM_PAGE_GET_PHYS_PAGE(m) == vm_page_guard_addr) {
		OSAddAtomic(-1, &vm_guard_count);
	}

	zfree(vm_page_zone, m);
}

/*
 *	vm_pool_low():
 *
 *	Return true if it is not likely that a non-vm_privileged thread
 *	can get memory without blocking.  Advisory only, since the
 *	situation may change under us.
 */
bool
vm_pool_low(void)
{
	/* No locking, at worst we will fib. */
	return vm_page_free_count <= vm_page_free_reserved;
}

boolean_t vm_darkwake_mode = FALSE;

/*
 * vm_update_darkwake_mode():
 *
 * Tells the VM that the system is in / out of darkwake.
 *
 * Today, the VM only lowers/raises the background queue target
 * so as to favor consuming more/less background pages when
 * darwake is ON/OFF.
 *
 * We might need to do more things in the future.
 */

void
vm_update_darkwake_mode(boolean_t darkwake_mode)
{
#if XNU_TARGET_OS_OSX && defined(__arm64__)
#pragma unused(darkwake_mode)
	assert(vm_darkwake_mode == FALSE);
	/*
	 * Darkwake mode isn't supported for AS macOS.
	 */
	return;
#else /* XNU_TARGET_OS_OSX && __arm64__ */
	LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_NOTOWNED);

	vm_page_lockspin_queues();

	if (vm_darkwake_mode == darkwake_mode) {
		/*
		 * No change.
		 */
		vm_page_unlock_queues();
		return;
	}

	vm_darkwake_mode = darkwake_mode;

	if (vm_darkwake_mode == TRUE) {
		/* save background target to restore later */
		vm_page_background_target_snapshot = vm_page_background_target;

		/* target is set to 0...no protection for background pages */
		vm_page_background_target = 0;
	} else if (vm_darkwake_mode == FALSE) {
		if (vm_page_background_target_snapshot) {
			vm_page_background_target = vm_page_background_target_snapshot;
		}
	}
	vm_page_unlock_queues();
#endif
}

void
vm_page_update_special_state(vm_page_t mem)
{
	if (mem->vmp_q_state == VM_PAGE_USED_BY_COMPRESSOR || mem->vmp_on_specialq == VM_PAGE_SPECIAL_Q_EMPTY) {
		return;
	}

	int mode = mem->vmp_on_specialq;

	switch (mode) {
	case VM_PAGE_SPECIAL_Q_BG:
	{
		task_t  my_task = current_task_early();

		if (vm_page_background_mode == VM_PAGE_BG_DISABLED) {
			return;
		}

		if (my_task) {
			if (task_get_darkwake_mode(my_task)) {
				return;
			}
		}

		if (my_task) {
			if (proc_get_effective_task_policy(my_task, TASK_POLICY_DARWIN_BG)) {
				return;
			}
		}
		vm_page_lockspin_queues();

		vm_page_background_promoted_count++;

		vm_page_remove_from_specialq(mem);
		mem->vmp_on_specialq = VM_PAGE_SPECIAL_Q_EMPTY;

		vm_page_unlock_queues();
		break;
	}

	case VM_PAGE_SPECIAL_Q_DONATE:
	{
		task_t  my_task = current_task_early();

		if (vm_page_donate_mode == VM_PAGE_DONATE_DISABLED) {
			return;
		}

		if (my_task->donates_own_pages == false) {
			vm_page_lockspin_queues();

			vm_page_remove_from_specialq(mem);
			mem->vmp_on_specialq = VM_PAGE_SPECIAL_Q_EMPTY;

			vm_page_unlock_queues();
		}
		break;
	}

	default:
	{
		assert(VM_PAGE_UNPACK_PTR(mem->vmp_specialq.next) == (uintptr_t)NULL &&
		    VM_PAGE_UNPACK_PTR(mem->vmp_specialq.prev) == (uintptr_t)NULL);
		break;
	}
	}
}


void
vm_page_assign_special_state(vm_page_t mem, int mode)
{
	if (mem->vmp_q_state == VM_PAGE_USED_BY_COMPRESSOR) {
		return;
	}

	switch (mode) {
	case VM_PAGE_SPECIAL_Q_BG:
	{
		if (vm_page_background_mode == VM_PAGE_BG_DISABLED) {
			return;
		}

		task_t  my_task = current_task_early();

		if (my_task) {
			if (task_get_darkwake_mode(my_task)) {
				mem->vmp_on_specialq = VM_PAGE_SPECIAL_Q_BG;
				return;
			}
		}

		if (my_task) {
			mem->vmp_on_specialq = (proc_get_effective_task_policy(my_task, TASK_POLICY_DARWIN_BG) ? VM_PAGE_SPECIAL_Q_BG : VM_PAGE_SPECIAL_Q_EMPTY);
		}
		break;
	}

	case VM_PAGE_SPECIAL_Q_DONATE:
	{
		if (vm_page_donate_mode == VM_PAGE_DONATE_DISABLED) {
			return;
		}
		mem->vmp_on_specialq = VM_PAGE_SPECIAL_Q_DONATE;
		break;
	}

	default:
		break;
	}
}


void
vm_page_remove_from_specialq(
	vm_page_t       mem)
{
	vm_object_t     m_object;
	unsigned short  mode;

	LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);

	mode = mem->vmp_on_specialq;

	switch (mode) {
	case VM_PAGE_SPECIAL_Q_BG:
	{
		if (mem->vmp_specialq.next && mem->vmp_specialq.prev) {
			vm_page_queue_remove(&vm_page_queue_background, mem, vmp_specialq);

			mem->vmp_specialq.next = 0;
			mem->vmp_specialq.prev = 0;

			vm_page_background_count--;

			m_object = VM_PAGE_OBJECT(mem);

			if (m_object->internal) {
				vm_page_background_internal_count--;
			} else {
				vm_page_background_external_count--;
			}
		}
		break;
	}

	case VM_PAGE_SPECIAL_Q_DONATE:
	{
		if (mem->vmp_specialq.next && mem->vmp_specialq.prev) {
			vm_page_queue_remove((vm_page_queue_head_t*)&vm_page_queue_donate, mem, vmp_specialq);
			mem->vmp_specialq.next = 0;
			mem->vmp_specialq.prev = 0;
			vm_page_donate_count--;
			if (vm_page_donate_queue_ripe && (vm_page_donate_count < vm_page_donate_target)) {
				assert(vm_page_donate_target == vm_page_donate_target_low);
				vm_page_donate_target = vm_page_donate_target_high;
				vm_page_donate_queue_ripe = false;
			}
		}

		break;
	}

	default:
	{
		assert(VM_PAGE_UNPACK_PTR(mem->vmp_specialq.next) == (uintptr_t)NULL &&
		    VM_PAGE_UNPACK_PTR(mem->vmp_specialq.prev) == (uintptr_t)NULL);
		break;
	}
	}
}


void
vm_page_add_to_specialq(
	vm_page_t       mem,
	boolean_t       first)
{
	vm_object_t     m_object;

	LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);

	if (mem->vmp_specialq.next && mem->vmp_specialq.prev) {
		return;
	}

	int mode = mem->vmp_on_specialq;

	switch (mode) {
	case VM_PAGE_SPECIAL_Q_BG:
	{
		if (vm_page_background_mode == VM_PAGE_BG_DISABLED) {
			return;
		}

		m_object = VM_PAGE_OBJECT(mem);

		if (vm_page_background_exclude_external && !m_object->internal) {
			return;
		}

		if (first == TRUE) {
			vm_page_queue_enter_first(&vm_page_queue_background, mem, vmp_specialq);
		} else {
			vm_page_queue_enter(&vm_page_queue_background, mem, vmp_specialq);
		}
		mem->vmp_on_specialq = VM_PAGE_SPECIAL_Q_BG;

		vm_page_background_count++;

		if (m_object->internal) {
			vm_page_background_internal_count++;
		} else {
			vm_page_background_external_count++;
		}
		break;
	}

	case VM_PAGE_SPECIAL_Q_DONATE:
	{
		if (first == TRUE) {
			vm_page_queue_enter_first((vm_page_queue_head_t*)&vm_page_queue_donate, mem, vmp_specialq);
		} else {
			vm_page_queue_enter((vm_page_queue_head_t*)&vm_page_queue_donate, mem, vmp_specialq);
		}
		vm_page_donate_count++;
		if (!vm_page_donate_queue_ripe && (vm_page_donate_count > vm_page_donate_target)) {
			assert(vm_page_donate_target == vm_page_donate_target_high);
			vm_page_donate_target = vm_page_donate_target_low;
			vm_page_donate_queue_ripe = true;
		}
		mem->vmp_on_specialq = VM_PAGE_SPECIAL_Q_DONATE;
		break;
	}

	default:
		break;
	}
}

/*
 * This can be switched to FALSE to help debug drivers
 * that are having problems with memory > 4G.
 */
boolean_t       vm_himemory_mode = TRUE;

/*
 * this interface exists to support hardware controllers
 * incapable of generating DMAs with more than 32 bits
 * of address on platforms with physical memory > 4G...
 */
unsigned int    vm_lopages_allocated_q = 0;
unsigned int    vm_lopages_allocated_cpm_success = 0;
unsigned int    vm_lopages_allocated_cpm_failed = 0;
vm_page_queue_head_t    vm_lopage_queue_free VM_PAGE_PACKED_ALIGNED;

vm_page_t
vm_page_grablo(void)
{
	vm_page_t       mem;

	if (vm_lopage_needed == FALSE) {
		return vm_page_grab();
	}

	vm_free_page_lock_spin();

	if (!vm_page_queue_empty(&vm_lopage_queue_free)) {
		vm_page_queue_remove_first(&vm_lopage_queue_free, mem, vmp_pageq);
		assert(vm_lopage_free_count);
		assert(mem->vmp_q_state == VM_PAGE_ON_FREE_LOPAGE_Q);
		mem->vmp_q_state = VM_PAGE_NOT_ON_Q;

		vm_lopage_free_count--;
		vm_lopages_allocated_q++;

		if (vm_lopage_free_count < vm_lopage_lowater) {
			vm_lopage_refill = TRUE;
		}

		vm_free_page_unlock();

		if (current_task()->donates_own_pages) {
			vm_page_assign_special_state(mem, VM_PAGE_SPECIAL_Q_DONATE);
		} else {
			vm_page_assign_special_state(mem, VM_PAGE_SPECIAL_Q_BG);
		}
	} else {
		vm_free_page_unlock();

		if (cpm_allocate(PAGE_SIZE, &mem, atop(PPNUM_MAX), 0, FALSE, KMA_LOMEM) != KERN_SUCCESS) {
			vm_free_page_lock_spin();
			vm_lopages_allocated_cpm_failed++;
			vm_free_page_unlock();

			return VM_PAGE_NULL;
		}
		assert(mem->vmp_q_state == VM_PAGE_NOT_ON_Q);

		mem->vmp_busy = TRUE;

		vm_page_lockspin_queues();

		mem->vmp_gobbled = FALSE;
		vm_page_gobble_count--;
		vm_page_wire_count--;

		vm_lopages_allocated_cpm_success++;
		vm_page_unlock_queues();
	}
	assert(mem->vmp_busy);
	assert(!mem->vmp_pmapped);
	assert(!mem->vmp_wpmapped);
	assert(!pmap_is_noencrypt(VM_PAGE_GET_PHYS_PAGE(mem)));

	VM_PAGE_ZERO_PAGEQ_ENTRY(mem);

	counter_inc(&vm_page_grab_count);
	VM_DEBUG_EVENT(vm_page_grab, DBG_VM_PAGE_GRAB, DBG_FUNC_NONE, 0, 1, 0, 0);

	return mem;
}

/*
 *	vm_page_grab:
 *
 *	first try to grab a page from the per-cpu free list...
 *	this must be done while pre-emption is disabled... if
 *      a page is available, we're done...
 *	if no page is available, grab the vm_page_queue_free_lock
 *	and see if current number of free pages would allow us
 *      to grab at least 1... if not, return VM_PAGE_NULL as before...
 *	if there are pages available, disable preemption and
 *      recheck the state of the per-cpu free list... we could
 *	have been preempted and moved to a different cpu, or
 *      some other thread could have re-filled it... if still
 *	empty, figure out how many pages we can steal from the
 *	global free queue and move to the per-cpu queue...
 *	return 1 of these pages when done... only wakeup the
 *      pageout_scan thread if we moved pages from the global
 *	list... no need for the wakeup if we've satisfied the
 *	request from the per-cpu queue.
 */

#if CONFIG_SECLUDED_MEMORY
vm_page_t vm_page_grab_secluded(void);
#endif /* CONFIG_SECLUDED_MEMORY */

static inline void
vm_page_grab_diags(void);

/*
 *	vm_page_validate_no_references:
 *
 *	Make sure the physical page has no refcounts.
 *
 */
static inline void
vm_page_validate_no_references(
	vm_page_t       mem)
{
	bool is_freed;

	if (mem->vmp_fictitious) {
		return;
	}

	pmap_paddr_t paddr = ptoa(VM_PAGE_GET_PHYS_PAGE(mem));

#if CONFIG_SPTM
	is_freed = pmap_is_page_free(paddr);
#else
	is_freed = pmap_verify_free(VM_PAGE_GET_PHYS_PAGE(mem));
#endif /* CONFIG_SPTM */

	if (!is_freed) {
		/*
		 * There is a redundancy here, but we are going to panic anyways,
		 * and ASSERT_PMAP_FREE traces useful information. So, we keep this
		 * behavior.
		 */
		ASSERT_PMAP_FREE(mem);
		panic("%s: page 0x%llx is referenced", __func__, paddr);
	}
}

vm_page_t
vm_page_grab(void)
{
	return vm_page_grab_options(VM_PAGE_GRAB_OPTIONS_NONE);
}

#if HIBERNATION
boolean_t       hibernate_rebuild_needed = FALSE;
#endif /* HIBERNATION */

static void
vm_page_finalize_grabed_page(vm_page_t mem)
{
	task_t cur_task = current_task_early();
	if (cur_task && cur_task != kernel_task) {
		/* tag:DONATE this is where the donate state of the page is decided according to what task grabs it */
		if (cur_task->donates_own_pages) {
			vm_page_assign_special_state(mem, VM_PAGE_SPECIAL_Q_DONATE);
		} else {
			vm_page_assign_special_state(mem, VM_PAGE_SPECIAL_Q_BG);
		}
	}
}

vm_page_t
vm_page_grab_options(
	int grab_options)
{
	vm_page_t       mem;

restart:
	disable_preemption();

	if ((mem = *PERCPU_GET(free_pages))) {
		assert(mem->vmp_q_state == VM_PAGE_ON_FREE_LOCAL_Q);

#if HIBERNATION
		if (hibernate_rebuild_needed) {
			panic("%s:%d should not modify cpu->free_pages while hibernating", __FUNCTION__, __LINE__);
		}
#endif /* HIBERNATION */

		vm_page_grab_diags();

		vm_offset_t pcpu_base = current_percpu_base();
		counter_inc_preemption_disabled(&vm_page_grab_count);
		*PERCPU_GET_WITH_BASE(pcpu_base, free_pages) = mem->vmp_snext;
		VM_DEBUG_EVENT(vm_page_grab, DBG_VM_PAGE_GRAB, DBG_FUNC_NONE, grab_options, 0, 0, 0);

		VM_PAGE_ZERO_PAGEQ_ENTRY(mem);
		mem->vmp_q_state = VM_PAGE_NOT_ON_Q;
		enable_preemption();

		assert(mem->vmp_listq.next == 0 && mem->vmp_listq.prev == 0);
		assert(mem->vmp_tabled == FALSE);
		assert(mem->vmp_object == 0);
		assert(!mem->vmp_laundry);
		assert(mem->vmp_busy);
		assert(!mem->vmp_pmapped);
		assert(!mem->vmp_wpmapped);
		assert(!pmap_is_noencrypt(VM_PAGE_GET_PHYS_PAGE(mem)));
		assert(!mem->vmp_realtime);

		vm_page_validate_no_references(mem);
		vm_page_finalize_grabed_page(mem);
		return mem;
	}
	enable_preemption();


	/*
	 *	Optionally produce warnings if the wire or gobble
	 *	counts exceed some threshold.
	 */
#if VM_PAGE_WIRE_COUNT_WARNING
	if (vm_page_wire_count >= VM_PAGE_WIRE_COUNT_WARNING) {
		printf("mk: vm_page_grab(): high wired page count of %d\n",
		    vm_page_wire_count);
	}
#endif
#if VM_PAGE_GOBBLE_COUNT_WARNING
	if (vm_page_gobble_count >= VM_PAGE_GOBBLE_COUNT_WARNING) {
		printf("mk: vm_page_grab(): high gobbled page count of %d\n",
		    vm_page_gobble_count);
	}
#endif

	/*
	 * If free count is low and we have delayed pages from early boot,
	 * get one of those instead.
	 */
	if (__improbable(vm_delayed_count > 0 &&
	    vm_page_free_count <= vm_page_free_target &&
	    (mem = vm_get_delayed_page(grab_options)) != NULL)) {
		assert(!mem->vmp_realtime);
		// TODO: missing vm_page_finalize_grabed_page()?
		return mem;
	}

	vm_free_page_lock_spin();

	/*
	 *	Only let privileged threads (involved in pageout)
	 *	dip into the reserved pool.
	 */
	if ((vm_page_free_count < vm_page_free_reserved) &&
	    !(current_thread()->options & TH_OPT_VMPRIV)) {
		/* no page for us in the free queue... */
		vm_free_page_unlock();
		mem = VM_PAGE_NULL;

#if CONFIG_SECLUDED_MEMORY
		/* ... but can we try and grab from the secluded queue? */
		if (vm_page_secluded_count > 0 &&
		    ((grab_options & VM_PAGE_GRAB_SECLUDED) ||
		    task_can_use_secluded_mem(current_task(), TRUE))) {
			mem = vm_page_grab_secluded();
			if (grab_options & VM_PAGE_GRAB_SECLUDED) {
				vm_page_secluded.grab_for_iokit++;
				if (mem) {
					vm_page_secluded.grab_for_iokit_success++;
				}
			}
			if (mem) {
				VM_CHECK_MEMORYSTATUS;

				vm_page_grab_diags();
				counter_inc(&vm_page_grab_count);
				VM_DEBUG_EVENT(vm_page_grab, DBG_VM_PAGE_GRAB, DBG_FUNC_NONE, grab_options, 0, 0, 0);

				assert(!mem->vmp_realtime);
				// TODO: missing vm_page_finalize_grabed_page()?
				return mem;
			}
		}
#else /* CONFIG_SECLUDED_MEMORY */
		(void) grab_options;
#endif /* CONFIG_SECLUDED_MEMORY */
	} else {
		vm_page_t        head;
		vm_page_t        tail;
		unsigned int     pages_to_steal;
		unsigned int     color;
		unsigned int clump_end, sub_count;

		/*
		 * Replenishing our per-CPU cache of free pages might take
		 * too long to keep holding the "free_page" lock as a spinlock,
		 * so convert to the full mutex to prevent other threads trying
		 * to acquire the "free_page" lock from timing out spinning on
		 * the mutex interlock.
		 */
		vm_free_page_lock_convert();

		while (vm_page_free_count == 0) {
			vm_free_page_unlock();
			/*
			 * must be a privileged thread to be
			 * in this state since a non-privileged
			 * thread would have bailed if we were
			 * under the vm_page_free_reserved mark
			 */
			VM_PAGE_WAIT();
			vm_free_page_lock();
		}

		/*
		 * Need to repopulate the per-CPU free list from the global free list.
		 * Note we don't do any processing of pending retirement pages here.
		 * That'll happen in the code above when the page comes off the per-CPU list.
		 */
		disable_preemption();

		/*
		 * If we got preempted the cache might now have pages.
		 */
		if ((mem = *PERCPU_GET(free_pages))) {
			vm_free_page_unlock();
			enable_preemption();
			goto restart;
		}

		if (vm_page_free_count <= vm_page_free_reserved) {
			pages_to_steal = 1;
		} else {
			if (vm_free_magazine_refill_limit <= (vm_page_free_count - vm_page_free_reserved)) {
				pages_to_steal = vm_free_magazine_refill_limit;
			} else {
				pages_to_steal = (vm_page_free_count - vm_page_free_reserved);
			}
		}
		color = *PERCPU_GET(start_color);
		head = tail = NULL;

		vm_page_free_count -= pages_to_steal;
		clump_end = sub_count = 0;

		while (pages_to_steal--) {
			while (vm_page_queue_empty(&vm_page_queue_free[color].qhead)) {
				color = (color + 1) & vm_color_mask;
			}
#if defined(__x86_64__)
			vm_page_queue_remove_first_with_clump(&vm_page_queue_free[color].qhead,
			    mem, clump_end);
#else
			vm_page_queue_remove_first(&vm_page_queue_free[color].qhead,
			    mem, vmp_pageq);
#endif

			assert(mem->vmp_q_state == VM_PAGE_ON_FREE_Q);

			VM_PAGE_ZERO_PAGEQ_ENTRY(mem);

#if defined(__arm64__)
			color = (color + 1) & vm_color_mask;
#else

#if DEVELOPMENT || DEBUG

			sub_count++;
			if (clump_end) {
				vm_clump_update_stats(sub_count);
				sub_count = 0;
				color = (color + 1) & vm_color_mask;
			}
#else
			if (clump_end) {
				color = (color + 1) & vm_color_mask;
			}

#endif /* if DEVELOPMENT || DEBUG */

#endif  /* if defined(__arm64__) */

			if (head == NULL) {
				head = mem;
			} else {
				tail->vmp_snext = mem;
			}
			tail = mem;

			assert(mem->vmp_listq.next == 0 && mem->vmp_listq.prev == 0);
			assert(mem->vmp_tabled == FALSE);
			assert(mem->vmp_object == 0);
			assert(!mem->vmp_laundry);

			mem->vmp_q_state = VM_PAGE_ON_FREE_LOCAL_Q;

			assert(mem->vmp_busy);
			assert(!mem->vmp_pmapped);
			assert(!mem->vmp_wpmapped);
			assert(!pmap_is_noencrypt(VM_PAGE_GET_PHYS_PAGE(mem)));
			assert(!mem->vmp_realtime);

			vm_page_validate_no_references(mem);
		}
#if defined (__x86_64__) && (DEVELOPMENT || DEBUG)
		vm_clump_update_stats(sub_count);
#endif

#if HIBERNATION
		if (hibernate_rebuild_needed) {
			panic("%s:%d should not modify cpu->free_pages while hibernating", __FUNCTION__, __LINE__);
		}
#endif /* HIBERNATION */
		vm_offset_t pcpu_base = current_percpu_base();
		*PERCPU_GET_WITH_BASE(pcpu_base, free_pages) = head;
		*PERCPU_GET_WITH_BASE(pcpu_base, start_color) = color;

		vm_free_page_unlock();
		enable_preemption();
		goto restart;
	}

	/*
	 *	Decide if we should poke the pageout daemon.
	 *	We do this if the free count is less than the low
	 *	water mark. VM Pageout Scan will keep running till
	 *	the free_count > free_target (& hence above free_min).
	 *	This wakeup is to catch the possibility of the counts
	 *	dropping between VM Pageout Scan parking and this check.
	 *
	 *	We don't have the counts locked ... if they change a little,
	 *	it doesn't really matter.
	 */
	if (vm_page_free_count < vm_page_free_min) {
		vm_free_page_lock();
		if (vm_pageout_running == FALSE) {
			vm_free_page_unlock();
			thread_wakeup((event_t) &vm_page_free_wanted);
		} else {
			vm_free_page_unlock();
		}
	}

	VM_CHECK_MEMORYSTATUS;

	if (mem) {
		assert(!mem->vmp_realtime);
//		dbgLog(VM_PAGE_GET_PHYS_PAGE(mem), vm_page_free_count, vm_page_wire_count, 4);	/* (TEST/DEBUG) */

		vm_page_finalize_grabed_page(mem);
	}
	return mem;
}

#if CONFIG_SECLUDED_MEMORY
vm_page_t
vm_page_grab_secluded(void)
{
	vm_page_t       mem;
	vm_object_t     object;
	int             refmod_state;

	if (vm_page_secluded_count == 0) {
		/* no secluded pages to grab... */
		return VM_PAGE_NULL;
	}

	/* secluded queue is protected by the VM page queue lock */
	vm_page_lock_queues();

	if (vm_page_secluded_count == 0) {
		/* no secluded pages to grab... */
		vm_page_unlock_queues();
		return VM_PAGE_NULL;
	}

#if 00
	/* can we grab from the secluded queue? */
	if (vm_page_secluded_count > vm_page_secluded_target ||
	    (vm_page_secluded_count > 0 &&
	    task_can_use_secluded_mem(current_task(), TRUE))) {
		/* OK */
	} else {
		/* can't grab from secluded queue... */
		vm_page_unlock_queues();
		return VM_PAGE_NULL;
	}
#endif

	/* we can grab a page from secluded queue! */
	assert((vm_page_secluded_count_free +
	    vm_page_secluded_count_inuse) ==
	    vm_page_secluded_count);
	if (current_task()->task_can_use_secluded_mem) {
		assert(num_tasks_can_use_secluded_mem > 0);
	}
	assert(!vm_page_queue_empty(&vm_page_queue_secluded));
	LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);
	mem = (vm_page_t)vm_page_queue_first(&vm_page_queue_secluded);
	assert(mem->vmp_q_state == VM_PAGE_ON_SECLUDED_Q);
	vm_page_queues_remove(mem, TRUE);

	object = VM_PAGE_OBJECT(mem);

	assert(!mem->vmp_fictitious);
	assert(!VM_PAGE_WIRED(mem));
	if (object == VM_OBJECT_NULL) {
		/* free for grab! */
		vm_page_unlock_queues();
		vm_page_secluded.grab_success_free++;

		assert(mem->vmp_busy);
		assert(mem->vmp_q_state == VM_PAGE_NOT_ON_Q);
		assert(VM_PAGE_OBJECT(mem) == VM_OBJECT_NULL);
		assert(mem->vmp_pageq.next == 0);
		assert(mem->vmp_pageq.prev == 0);
		assert(mem->vmp_listq.next == 0);
		assert(mem->vmp_listq.prev == 0);
		assert(mem->vmp_on_specialq == VM_PAGE_SPECIAL_Q_EMPTY);
		assert(mem->vmp_specialq.next == 0);
		assert(mem->vmp_specialq.prev == 0);
		return mem;
	}

	assert(!object->internal);
//	vm_page_pageable_external_count--;

	if (!vm_object_lock_try(object)) {
//		printf("SECLUDED: page %p: object %p locked\n", mem, object);
		vm_page_secluded.grab_failure_locked++;
reactivate_secluded_page:
		vm_page_activate(mem);
		vm_page_unlock_queues();
		return VM_PAGE_NULL;
	}
	if (mem->vmp_busy ||
	    mem->vmp_cleaning ||
	    mem->vmp_laundry) {
		/* can't steal page in this state... */
		vm_object_unlock(object);
		vm_page_secluded.grab_failure_state++;
		goto reactivate_secluded_page;
	}
	if (mem->vmp_realtime) {
		/* don't steal pages used by realtime threads... */
		vm_object_unlock(object);
		vm_page_secluded.grab_failure_realtime++;
		goto reactivate_secluded_page;
	}

	mem->vmp_busy = TRUE;
	refmod_state = pmap_disconnect(VM_PAGE_GET_PHYS_PAGE(mem));
	if (refmod_state & VM_MEM_REFERENCED) {
		mem->vmp_reference = TRUE;
	}
	if (refmod_state & VM_MEM_MODIFIED) {
		SET_PAGE_DIRTY(mem, FALSE);
	}
	if (mem->vmp_dirty || mem->vmp_precious) {
		/* can't grab a dirty page; re-activate */
//		printf("SECLUDED: dirty page %p\n", mem);
		vm_page_wakeup_done(object, mem);
		vm_page_secluded.grab_failure_dirty++;
		vm_object_unlock(object);
		goto reactivate_secluded_page;
	}
	if (mem->vmp_reference) {
		/* it's been used but we do need to grab a page... */
	}

	vm_page_unlock_queues();


	/* finish what vm_page_free() would have done... */
	vm_page_free_prepare_object(mem, TRUE);
	vm_object_unlock(object);
	object = VM_OBJECT_NULL;

	vm_page_validate_no_references(mem);

	pmap_clear_noencrypt(VM_PAGE_GET_PHYS_PAGE(mem));
	vm_page_secluded.grab_success_other++;

	assert(mem->vmp_busy);
	assert(mem->vmp_q_state == VM_PAGE_NOT_ON_Q);
	assert(VM_PAGE_OBJECT(mem) == VM_OBJECT_NULL);
	assert(mem->vmp_pageq.next == 0);
	assert(mem->vmp_pageq.prev == 0);
	assert(mem->vmp_listq.next == 0);
	assert(mem->vmp_listq.prev == 0);
	assert(mem->vmp_on_specialq == VM_PAGE_SPECIAL_Q_EMPTY);
	assert(mem->vmp_specialq.next == 0);
	assert(mem->vmp_specialq.prev == 0);

	return mem;
}

uint64_t
vm_page_secluded_drain(void)
{
	vm_page_t local_freeq;
	int local_freed;
	uint64_t num_reclaimed;
	unsigned int saved_secluded_count, saved_secluded_target;

	num_reclaimed = 0;
	local_freeq = NULL;
	local_freed = 0;

	vm_page_lock_queues();

	saved_secluded_count = vm_page_secluded_count;
	saved_secluded_target = vm_page_secluded_target;
	vm_page_secluded_target = 0;
	VM_PAGE_SECLUDED_COUNT_OVER_TARGET_UPDATE();
	while (vm_page_secluded_count) {
		vm_page_t secluded_page;

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
			secluded_page->vmp_snext = local_freeq;
			local_freeq = secluded_page;
			local_freed += 1;
		} else {
			/* transfer to head of active queue */
			vm_page_enqueue_active(secluded_page, FALSE);
			secluded_page = VM_PAGE_NULL;
		}
		num_reclaimed++;
	}
	vm_page_secluded_target = saved_secluded_target;
	VM_PAGE_SECLUDED_COUNT_OVER_TARGET_UPDATE();

//	printf("FBDP %s:%d secluded_count %d->%d, target %d, reclaimed %lld\n", __FUNCTION__, __LINE__, saved_secluded_count, vm_page_secluded_count, vm_page_secluded_target, num_reclaimed);

	vm_page_unlock_queues();

	if (local_freed) {
		vm_page_free_list(local_freeq, TRUE);
		local_freeq = NULL;
		local_freed = 0;
	}

	return num_reclaimed;
}
#endif /* CONFIG_SECLUDED_MEMORY */

static inline void
vm_page_grab_diags()
{
#if DEVELOPMENT || DEBUG
	task_t task = current_task_early();
	if (task == NULL) {
		return;
	}

	ledger_credit(task->ledger, task_ledgers.pages_grabbed, 1);
#endif /* DEVELOPMENT || DEBUG */
}

/*
 *	vm_page_release:
 *
 *	Return a page to the free list.
 */

void
vm_page_release(
	vm_page_t       mem,
	boolean_t       page_queues_locked)
{
	unsigned int    color;
	int     need_wakeup = 0;
	int     need_priv_wakeup = 0;
#if CONFIG_SECLUDED_MEMORY
	int     need_secluded_wakeup = 0;
#endif /* CONFIG_SECLUDED_MEMORY */
	event_t wakeup_event = NULL;

	if (page_queues_locked) {
		LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);
	} else {
		LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_NOTOWNED);
	}

	assert(!mem->vmp_private && !mem->vmp_fictitious);

#if MACH_ASSERT
	if (vm_check_refs_on_free) {
		vm_page_validate_no_references(mem);
	}
#endif /* MACH_ASSERT */

//	dbgLog(VM_PAGE_GET_PHYS_PAGE(mem), vm_page_free_count, vm_page_wire_count, 5);	/* (TEST/DEBUG) */

	pmap_clear_noencrypt(VM_PAGE_GET_PHYS_PAGE(mem));

	if (__improbable(mem->vmp_realtime)) {
		if (!page_queues_locked) {
			vm_page_lock_queues();
		}
		if (mem->vmp_realtime) {
			mem->vmp_realtime = false;
			vm_page_realtime_count--;
		}
		if (!page_queues_locked) {
			vm_page_unlock_queues();
		}
	}

	vm_free_page_lock_spin();

	assert(mem->vmp_q_state == VM_PAGE_NOT_ON_Q);
	assert(mem->vmp_busy);
	assert(!mem->vmp_laundry);
	assert(mem->vmp_object == 0);
	assert(mem->vmp_pageq.next == 0 && mem->vmp_pageq.prev == 0);
	assert(mem->vmp_listq.next == 0 && mem->vmp_listq.prev == 0);
	assert(mem->vmp_specialq.next == 0 && mem->vmp_specialq.prev == 0);

	/* Clear any specialQ hints before releasing page to the free pool*/
	mem->vmp_on_specialq = VM_PAGE_SPECIAL_Q_EMPTY;

	if ((mem->vmp_lopage == TRUE || vm_lopage_refill == TRUE) &&
	    vm_lopage_free_count < vm_lopage_free_limit &&
	    VM_PAGE_GET_PHYS_PAGE(mem) < max_valid_low_ppnum) {
		/*
		 * this exists to support hardware controllers
		 * incapable of generating DMAs with more than 32 bits
		 * of address on platforms with physical memory > 4G...
		 */
		vm_page_queue_enter_first(&vm_lopage_queue_free, mem, vmp_pageq);
		vm_lopage_free_count++;

		if (vm_lopage_free_count >= vm_lopage_free_limit) {
			vm_lopage_refill = FALSE;
		}

		mem->vmp_q_state = VM_PAGE_ON_FREE_LOPAGE_Q;
		mem->vmp_lopage = TRUE;
#if CONFIG_SECLUDED_MEMORY
	} else if (vm_page_free_count > vm_page_free_reserved &&
	    vm_page_secluded_count < vm_page_secluded_target &&
	    num_tasks_can_use_secluded_mem == 0) {
		/*
		 * XXX FBDP TODO: also avoid refilling secluded queue
		 * when some IOKit objects are already grabbing from it...
		 */
		if (!page_queues_locked) {
			if (!vm_page_trylock_queues()) {
				/* take locks in right order */
				vm_free_page_unlock();
				vm_page_lock_queues();
				vm_free_page_lock_spin();
			}
		}
		mem->vmp_lopage = FALSE;
		LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);
		vm_page_queue_enter_first(&vm_page_queue_secluded, mem, vmp_pageq);
		mem->vmp_q_state = VM_PAGE_ON_SECLUDED_Q;
		vm_page_secluded_count++;
		VM_PAGE_SECLUDED_COUNT_OVER_TARGET_UPDATE();
		vm_page_secluded_count_free++;
		if (!page_queues_locked) {
			vm_page_unlock_queues();
		}
		LCK_MTX_ASSERT(&vm_page_queue_free_lock, LCK_MTX_ASSERT_OWNED);
		if (vm_page_free_wanted_secluded > 0) {
			vm_page_free_wanted_secluded--;
			need_secluded_wakeup = 1;
		}
#endif /* CONFIG_SECLUDED_MEMORY */
	} else {
		mem->vmp_lopage = FALSE;
		mem->vmp_q_state = VM_PAGE_ON_FREE_Q;

		color = VM_PAGE_GET_COLOR(mem);
#if defined(__x86_64__)
		vm_page_queue_enter_clump(&vm_page_queue_free[color].qhead, mem);
#else
		vm_page_queue_enter(&vm_page_queue_free[color].qhead, mem, vmp_pageq);
#endif
		vm_page_free_count++;
		/*
		 *	Check if we should wake up someone waiting for page.
		 *	But don't bother waking them unless they can allocate.
		 *
		 *	We wakeup only one thread, to prevent starvation.
		 *	Because the scheduling system handles wait queues FIFO,
		 *	if we wakeup all waiting threads, one greedy thread
		 *	can starve multiple niceguy threads.  When the threads
		 *	all wakeup, the greedy threads runs first, grabs the page,
		 *	and waits for another page.  It will be the first to run
		 *	when the next page is freed.
		 *
		 *	However, there is a slight danger here.
		 *	The thread we wake might not use the free page.
		 *	Then the other threads could wait indefinitely
		 *	while the page goes unused.  To forestall this,
		 *	the pageout daemon will keep making free pages
		 *	as long as vm_page_free_wanted is non-zero.
		 */

		assert(vm_page_free_count > 0);
		if (vm_page_free_wanted_privileged > 0) {
			vm_page_free_wanted_privileged--;
			need_priv_wakeup = 1;
#if CONFIG_SECLUDED_MEMORY
		} else if (vm_page_free_wanted_secluded > 0 &&
		    vm_page_free_count > vm_page_free_reserved) {
			vm_page_free_wanted_secluded--;
			need_secluded_wakeup = 1;
#endif /* CONFIG_SECLUDED_MEMORY */
		} else if (vm_page_free_wanted > 0 &&
		    vm_page_free_count > vm_page_free_reserved) {
			vm_page_free_wanted--;
			need_wakeup = 1;
		}
	}
	vm_pageout_vminfo.vm_page_pages_freed++;

	vm_free_page_unlock();

	VM_DEBUG_CONSTANT_EVENT(vm_page_release, DBG_VM_PAGE_RELEASE, DBG_FUNC_NONE, 1, 0, 0, 0);

	if (need_priv_wakeup) {
		wakeup_event = &vm_page_free_wanted_privileged;
	}
#if CONFIG_SECLUDED_MEMORY
	else if (need_secluded_wakeup) {
		wakeup_event = &vm_page_free_wanted_secluded;
	}
#endif /* CONFIG_SECLUDED_MEMORY */
	else if (need_wakeup) {
		wakeup_event = &vm_page_free_count;
	}

	if (wakeup_event) {
		if (vps_dynamic_priority_enabled) {
			wakeup_one_with_inheritor((event_t) wakeup_event,
			    THREAD_AWAKENED, LCK_WAKE_DO_NOT_TRANSFER_PUSH,
			    NULL);
		} else {
			thread_wakeup_one((event_t) wakeup_event);
		}
	}

	VM_CHECK_MEMORYSTATUS;
}

/*
 * This version of vm_page_release() is used only at startup
 * when we are single-threaded and pages are being released
 * for the first time. Hence, no locking or unnecessary checks are made.
 * Note: VM_CHECK_MEMORYSTATUS invoked by the caller.
 */
void
vm_page_release_startup(
	vm_page_t       mem)
{
	vm_page_queue_t queue_free;

	if (vm_lopage_free_count < vm_lopage_free_limit &&
	    VM_PAGE_GET_PHYS_PAGE(mem) < max_valid_low_ppnum) {
		mem->vmp_lopage = TRUE;
		mem->vmp_q_state = VM_PAGE_ON_FREE_LOPAGE_Q;
		vm_lopage_free_count++;
		queue_free = &vm_lopage_queue_free;
#if CONFIG_SECLUDED_MEMORY
	} else if (vm_page_secluded_count < vm_page_secluded_target) {
		mem->vmp_lopage = FALSE;
		mem->vmp_q_state = VM_PAGE_ON_SECLUDED_Q;
		vm_page_secluded_count++;
		VM_PAGE_SECLUDED_COUNT_OVER_TARGET_UPDATE();
		vm_page_secluded_count_free++;
		queue_free = &vm_page_queue_secluded;
#endif /* CONFIG_SECLUDED_MEMORY */
	} else {
		mem->vmp_lopage = FALSE;
		mem->vmp_q_state = VM_PAGE_ON_FREE_Q;
		vm_page_free_count++;
		queue_free = &vm_page_queue_free[VM_PAGE_GET_COLOR(mem)].qhead;
	}
	if (mem->vmp_q_state == VM_PAGE_ON_FREE_Q) {
#if defined(__x86_64__)
		vm_page_queue_enter_clump(queue_free, mem);
#else
		vm_page_queue_enter(queue_free, mem, vmp_pageq);
#endif
	} else {
		vm_page_queue_enter_first(queue_free, mem, vmp_pageq);
	}
}

/*
 *	vm_page_wait:
 *
 *	Wait for a page to become available.
 *	If there are plenty of free pages, then we don't sleep.
 *
 *	Returns:
 *		TRUE:  There may be another page, try again
 *		FALSE: We were interrupted out of our wait, don't try again
 */

boolean_t
vm_page_wait(
	int     interruptible )
{
	/*
	 *	We can't use vm_page_free_reserved to make this
	 *	determination.  Consider: some thread might
	 *	need to allocate two pages.  The first allocation
	 *	succeeds, the second fails.  After the first page is freed,
	 *	a call to vm_page_wait must really block.
	 */
	kern_return_t   wait_result;
	int             need_wakeup = 0;
	int             is_privileged = current_thread()->options & TH_OPT_VMPRIV;
	event_t         wait_event = NULL;

	vm_free_page_lock_spin();

	if (is_privileged && vm_page_free_count) {
		vm_free_page_unlock();
		return TRUE;
	}

	if (vm_page_free_count >= vm_page_free_target) {
		vm_free_page_unlock();
		return TRUE;
	}

	if (is_privileged) {
		if (vm_page_free_wanted_privileged++ == 0) {
			need_wakeup = 1;
		}
		wait_event = (event_t)&vm_page_free_wanted_privileged;
#if CONFIG_SECLUDED_MEMORY
	} else if (secluded_for_apps &&
	    task_can_use_secluded_mem(current_task(), FALSE)) {
#if 00
		/* XXX FBDP: need pageq lock for this... */
		/* XXX FBDP: might wait even if pages available, */
		/* XXX FBDP: hopefully not for too long... */
		if (vm_page_secluded_count > 0) {
			vm_free_page_unlock();
			return TRUE;
		}
#endif
		if (vm_page_free_wanted_secluded++ == 0) {
			need_wakeup = 1;
		}
		wait_event = (event_t)&vm_page_free_wanted_secluded;
#endif /* CONFIG_SECLUDED_MEMORY */
	} else {
		if (vm_page_free_wanted++ == 0) {
			need_wakeup = 1;
		}
		wait_event = (event_t)&vm_page_free_count;
	}

	/*
	 * We don't do a vm_pageout_scan wakeup if we already have
	 * some waiters because vm_pageout_scan checks for waiters
	 * before it returns and does so behind the vm_page_queue_free_lock,
	 * which we own when we bump the waiter counts.
	 */

	if (vps_dynamic_priority_enabled) {
		/*
		 * We are waking up vm_pageout_scan here. If it needs
		 * the vm_page_queue_free_lock before we unlock it
		 * we'll end up just blocking and incur an extra
		 * context switch. Could be a perf. issue.
		 */

		if (need_wakeup) {
			thread_wakeup((event_t)&vm_page_free_wanted);
		}

		/*
		 * LD: This event is going to get recorded every time because
		 * we don't get back THREAD_WAITING from lck_mtx_sleep_with_inheritor.
		 * We just block in that routine.
		 */
		VM_DEBUG_CONSTANT_EVENT(vm_page_wait_block, DBG_VM_PAGE_WAIT_BLOCK, DBG_FUNC_START,
		    vm_page_free_wanted_privileged,
		    vm_page_free_wanted,
#if CONFIG_SECLUDED_MEMORY
		    vm_page_free_wanted_secluded,
#else /* CONFIG_SECLUDED_MEMORY */
		    0,
#endif /* CONFIG_SECLUDED_MEMORY */
		    0);
		wait_result =  lck_mtx_sleep_with_inheritor(&vm_page_queue_free_lock,
		    LCK_SLEEP_UNLOCK,
		    wait_event,
		    vm_pageout_scan_thread,
		    interruptible,
		    0);
	} else {
		wait_result = assert_wait(wait_event, interruptible);

		vm_free_page_unlock();

		if (need_wakeup) {
			thread_wakeup((event_t)&vm_page_free_wanted);
		}

		if (wait_result == THREAD_WAITING) {
			VM_DEBUG_CONSTANT_EVENT(vm_page_wait_block, DBG_VM_PAGE_WAIT_BLOCK, DBG_FUNC_START,
			    vm_page_free_wanted_privileged,
			    vm_page_free_wanted,
#if CONFIG_SECLUDED_MEMORY
			    vm_page_free_wanted_secluded,
#else /* CONFIG_SECLUDED_MEMORY */
			    0,
#endif /* CONFIG_SECLUDED_MEMORY */
			    0);
			wait_result = thread_block(THREAD_CONTINUE_NULL);
			VM_DEBUG_CONSTANT_EVENT(vm_page_wait_block,
			    DBG_VM_PAGE_WAIT_BLOCK, DBG_FUNC_END, 0, 0, 0, 0);
		}
	}

	return (wait_result == THREAD_AWAKENED) || (wait_result == THREAD_NOT_WAITING);
}

/*
 *	vm_page_alloc:
 *
 *	Allocate and return a memory cell associated
 *	with this VM object/offset pair.
 *
 *	Object must be locked.
 */

vm_page_t
vm_page_alloc(
	vm_object_t             object,
	vm_object_offset_t      offset)
{
	vm_page_t       mem;
	int             grab_options;

	vm_object_lock_assert_exclusive(object);
	grab_options = 0;
#if CONFIG_SECLUDED_MEMORY
	if (object->can_grab_secluded) {
		grab_options |= VM_PAGE_GRAB_SECLUDED;
	}
#endif /* CONFIG_SECLUDED_MEMORY */
	mem = vm_page_grab_options(grab_options);
	if (mem == VM_PAGE_NULL) {
		return VM_PAGE_NULL;
	}

	vm_page_insert(mem, object, offset);

	return mem;
}

/*
 *	vm_page_free_prepare:
 *
 *	Removes page from any queue it may be on
 *	and disassociates it from its VM object.
 *
 *	Object and page queues must be locked prior to entry.
 */
static void
vm_page_free_prepare(
	vm_page_t       mem)
{
#if CONFIG_SPTM
	/**
	 * SPTM TODO: The pmap should retype frames automatically as mappings to them are
	 *            created and destroyed. In order to catch potential cases where this
	 *            does not happen, add an appropriate assert here. This code should be
	 *            executed on every frame that is about to be released to the VM.
	 */
	const sptm_paddr_t paddr = ((uint64_t)VM_PAGE_GET_PHYS_PAGE(mem)) << PAGE_SHIFT;
	__unused const sptm_frame_type_t frame_type = sptm_get_frame_type(paddr);

	assert(frame_type == XNU_DEFAULT);
#endif /* CONFIG_SPTM */

	vm_page_free_prepare_queues(mem);
	vm_page_free_prepare_object(mem, TRUE);
}


void
vm_page_free_prepare_queues(
	vm_page_t       mem)
{
	vm_object_t     m_object;

	VM_PAGE_CHECK(mem);

	assert(mem->vmp_q_state != VM_PAGE_ON_FREE_Q);
	assert(!mem->vmp_cleaning);
	m_object = VM_PAGE_OBJECT(mem);

	LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);
	if (m_object) {
		vm_object_lock_assert_exclusive(m_object);
	}
	if (mem->vmp_laundry) {
		/*
		 * We may have to free a page while it's being laundered
		 * if we lost its pager (due to a forced unmount, for example).
		 * We need to call vm_pageout_steal_laundry() before removing
		 * the page from its VM object, so that we can remove it
		 * from its pageout queue and adjust the laundry accounting
		 */
		vm_pageout_steal_laundry(mem, TRUE);
	}

	vm_page_queues_remove(mem, TRUE);

	if (__improbable(mem->vmp_realtime)) {
		mem->vmp_realtime = false;
		vm_page_realtime_count--;
	}

	if (VM_PAGE_WIRED(mem)) {
		assert(mem->vmp_wire_count > 0);

		if (m_object) {
			task_t          owner;
			int             ledger_idx_volatile;
			int             ledger_idx_nonvolatile;
			int             ledger_idx_volatile_compressed;
			int             ledger_idx_nonvolatile_compressed;
			int             ledger_idx_composite;
			int             ledger_idx_external_wired;
			boolean_t       do_footprint;

			VM_OBJECT_WIRED_PAGE_UPDATE_START(m_object);
			VM_OBJECT_WIRED_PAGE_REMOVE(m_object, mem);
			VM_OBJECT_WIRED_PAGE_UPDATE_END(m_object, m_object->wire_tag);

			assert(m_object->resident_page_count >=
			    m_object->wired_page_count);

			if (m_object->purgable == VM_PURGABLE_VOLATILE) {
				OSAddAtomic(+1, &vm_page_purgeable_count);
				assert(vm_page_purgeable_wired_count > 0);
				OSAddAtomic(-1, &vm_page_purgeable_wired_count);
			}
			if (m_object->internal &&
			    m_object->vo_owner != TASK_NULL &&
			    (m_object->purgable == VM_PURGABLE_VOLATILE ||
			    m_object->purgable == VM_PURGABLE_EMPTY)) {
				owner = VM_OBJECT_OWNER(m_object);
				vm_object_ledger_tag_ledgers(
					m_object,
					&ledger_idx_volatile,
					&ledger_idx_nonvolatile,
					&ledger_idx_volatile_compressed,
					&ledger_idx_nonvolatile_compressed,
					&ledger_idx_composite,
					&ledger_idx_external_wired,
					&do_footprint);
				/*
				 * While wired, this page was accounted
				 * as "non-volatile" but it should now
				 * be accounted as "volatile".
				 */
				/* one less "non-volatile"... */
				ledger_debit(owner->ledger,
				    ledger_idx_nonvolatile,
				    PAGE_SIZE);
				if (do_footprint) {
					/* ... and "phys_footprint" */
					ledger_debit(owner->ledger,
					    task_ledgers.phys_footprint,
					    PAGE_SIZE);
				} else if (ledger_idx_composite != -1) {
					ledger_debit(owner->ledger,
					    ledger_idx_composite,
					    PAGE_SIZE);
				}
				/* one more "volatile" */
				ledger_credit(owner->ledger,
				    ledger_idx_volatile,
				    PAGE_SIZE);
			}
		}
		if (!mem->vmp_private && !mem->vmp_fictitious) {
			vm_page_wire_count--;
		}

		mem->vmp_q_state = VM_PAGE_NOT_ON_Q;
		mem->vmp_wire_count = 0;
		assert(!mem->vmp_gobbled);
	} else if (mem->vmp_gobbled) {
		if (!mem->vmp_private && !mem->vmp_fictitious) {
			vm_page_wire_count--;
		}
		vm_page_gobble_count--;
	}
}


void
vm_page_free_prepare_object(
	vm_page_t       mem,
	boolean_t       remove_from_hash)
{
	assert(!mem->vmp_realtime);
	if (mem->vmp_tabled) {
		vm_page_remove(mem, remove_from_hash);  /* clears tabled, object, offset */
	}
	vm_page_wakeup(VM_OBJECT_NULL, mem);               /* clears wanted */

	if (mem->vmp_private) {
		mem->vmp_private = FALSE;
		mem->vmp_fictitious = TRUE;
		VM_PAGE_SET_PHYS_PAGE(mem, vm_page_fictitious_addr);
	}
	if (!mem->vmp_fictitious) {
		assert(mem->vmp_pageq.next == 0);
		assert(mem->vmp_pageq.prev == 0);
		assert(mem->vmp_listq.next == 0);
		assert(mem->vmp_listq.prev == 0);
		assert(mem->vmp_specialq.next == 0);
		assert(mem->vmp_specialq.prev == 0);
		assert(mem->vmp_next_m == 0);

#if MACH_ASSERT
		if (vm_check_refs_on_free) {
			vm_page_validate_no_references(mem);
		}
#endif /* MACH_ASSERT */

		{
			vm_page_init(mem, VM_PAGE_GET_PHYS_PAGE(mem), mem->vmp_lopage);
		}
	}
}

/*
 *	vm_page_free:
 *
 *	Returns the given page to the free list,
 *	disassociating it with any VM object.
 *
 *	Object and page queues must be locked prior to entry.
 */
void
vm_page_free(
	vm_page_t       mem)
{
	vm_page_free_prepare(mem);

	if (mem->vmp_fictitious) {
		vm_page_release_fictitious(mem);
	} else {
		vm_page_release(mem, TRUE);  /* page queues are locked */
	}
}


void
vm_page_free_unlocked(
	vm_page_t       mem,
	boolean_t       remove_from_hash)
{
	vm_page_lockspin_queues();
	vm_page_free_prepare_queues(mem);
	vm_page_unlock_queues();

	vm_page_free_prepare_object(mem, remove_from_hash);

	if (mem->vmp_fictitious) {
		vm_page_release_fictitious(mem);
	} else {
		vm_page_release(mem, FALSE); /* page queues are not locked */
	}
}


/*
 * Free a list of pages.  The list can be up to several hundred pages,
 * as blocked up by vm_pageout_scan().
 * The big win is not having to take the free list lock once
 * per page.
 *
 * The VM page queues lock (vm_page_queue_lock) should NOT be held.
 * The VM page free queues lock (vm_page_queue_free_lock) should NOT be held.
 */
void
vm_page_free_list(
	vm_page_t       freeq,
	boolean_t       prepare_object)
{
	vm_page_t       mem;
	vm_page_t       nxt;
	vm_page_t       local_freeq;
	int             pg_count;

	LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_NOTOWNED);
	LCK_MTX_ASSERT(&vm_page_queue_free_lock, LCK_MTX_ASSERT_NOTOWNED);

	while (freeq) {
		pg_count = 0;
		local_freeq = VM_PAGE_NULL;
		mem = freeq;

		/*
		 * break up the processing into smaller chunks so
		 * that we can 'pipeline' the pages onto the
		 * free list w/o introducing too much
		 * contention on the global free queue lock
		 */
		while (mem && pg_count < 64) {
			assert((mem->vmp_q_state == VM_PAGE_NOT_ON_Q) ||
			    (mem->vmp_q_state == VM_PAGE_IS_WIRED));
			assert(mem->vmp_specialq.next == 0 &&
			    mem->vmp_specialq.prev == 0);
			/*
			 * &&
			 *   mem->vmp_on_specialq == VM_PAGE_SPECIAL_Q_EMPTY);
			 */
			nxt = mem->vmp_snext;
			mem->vmp_snext = NULL;
			assert(mem->vmp_pageq.prev == 0);

#if MACH_ASSERT
			if (vm_check_refs_on_free) {
				if (!mem->vmp_fictitious && !mem->vmp_private) {
					vm_page_validate_no_references(mem);
				}
			}
#endif /* MACH_ASSERT */

			if (__improbable(mem->vmp_realtime)) {
				vm_page_lock_queues();
				if (mem->vmp_realtime) {
					mem->vmp_realtime = false;
					vm_page_realtime_count--;
				}
				vm_page_unlock_queues();
			}

			if (prepare_object == TRUE) {
				vm_page_free_prepare_object(mem, TRUE);
			}

			if (!mem->vmp_fictitious) {
				assert(mem->vmp_busy);

				if ((mem->vmp_lopage == TRUE || vm_lopage_refill == TRUE) &&
				    vm_lopage_free_count < vm_lopage_free_limit &&
				    VM_PAGE_GET_PHYS_PAGE(mem) < max_valid_low_ppnum) {
					vm_page_release(mem, FALSE); /* page queues are not locked */
#if CONFIG_SECLUDED_MEMORY
				} else if (vm_page_secluded_count < vm_page_secluded_target &&
				    num_tasks_can_use_secluded_mem == 0) {
					vm_page_release(mem,
					    FALSE);             /* page queues are not locked */
#endif /* CONFIG_SECLUDED_MEMORY */
				} else {
					/*
					 * IMPORTANT: we can't set the page "free" here
					 * because that would make the page eligible for
					 * a physically-contiguous allocation (see
					 * vm_page_find_contiguous()) right away (we don't
					 * hold the vm_page_queue_free lock).  That would
					 * cause trouble because the page is not actually
					 * in the free queue yet...
					 */
					mem->vmp_snext = local_freeq;
					local_freeq = mem;
					pg_count++;

					pmap_clear_noencrypt(VM_PAGE_GET_PHYS_PAGE(mem));
				}
			} else {
				assert(VM_PAGE_GET_PHYS_PAGE(mem) == vm_page_fictitious_addr ||
				    VM_PAGE_GET_PHYS_PAGE(mem) == vm_page_guard_addr);
				vm_page_release_fictitious(mem);
			}
			mem = nxt;
		}
		freeq = mem;

		if ((mem = local_freeq)) {
			unsigned int    avail_free_count;
			unsigned int    need_wakeup = 0;
			unsigned int    need_priv_wakeup = 0;
#if CONFIG_SECLUDED_MEMORY
			unsigned int    need_wakeup_secluded = 0;
#endif /* CONFIG_SECLUDED_MEMORY */
			event_t         priv_wakeup_event, secluded_wakeup_event, normal_wakeup_event;
			boolean_t       priv_wakeup_all, secluded_wakeup_all, normal_wakeup_all;

			vm_free_page_lock_spin();

			while (mem) {
				int     color;

				nxt = mem->vmp_snext;

				assert(mem->vmp_q_state == VM_PAGE_NOT_ON_Q);
				assert(mem->vmp_busy);
				assert(!mem->vmp_realtime);
				mem->vmp_lopage = FALSE;
				mem->vmp_q_state = VM_PAGE_ON_FREE_Q;

				color = VM_PAGE_GET_COLOR(mem);
#if defined(__x86_64__)
				vm_page_queue_enter_clump(&vm_page_queue_free[color].qhead, mem);
#else
				vm_page_queue_enter(&vm_page_queue_free[color].qhead,
				    mem, vmp_pageq);
#endif
				mem = nxt;
			}
			vm_pageout_vminfo.vm_page_pages_freed += pg_count;
			vm_page_free_count += pg_count;
			avail_free_count = vm_page_free_count;

			VM_DEBUG_CONSTANT_EVENT(vm_page_release, DBG_VM_PAGE_RELEASE, DBG_FUNC_NONE, pg_count, 0, 0, 0);

			if (vm_page_free_wanted_privileged > 0 && avail_free_count > 0) {
				if (avail_free_count < vm_page_free_wanted_privileged) {
					need_priv_wakeup = avail_free_count;
					vm_page_free_wanted_privileged -= avail_free_count;
					avail_free_count = 0;
				} else {
					need_priv_wakeup = vm_page_free_wanted_privileged;
					avail_free_count -= vm_page_free_wanted_privileged;
					vm_page_free_wanted_privileged = 0;
				}
			}
#if CONFIG_SECLUDED_MEMORY
			if (vm_page_free_wanted_secluded > 0 &&
			    avail_free_count > vm_page_free_reserved) {
				unsigned int available_pages;
				available_pages = (avail_free_count -
				    vm_page_free_reserved);
				if (available_pages <
				    vm_page_free_wanted_secluded) {
					need_wakeup_secluded = available_pages;
					vm_page_free_wanted_secluded -=
					    available_pages;
					avail_free_count -= available_pages;
				} else {
					need_wakeup_secluded =
					    vm_page_free_wanted_secluded;
					avail_free_count -=
					    vm_page_free_wanted_secluded;
					vm_page_free_wanted_secluded = 0;
				}
			}
#endif /* CONFIG_SECLUDED_MEMORY */
			if (vm_page_free_wanted > 0 && avail_free_count > vm_page_free_reserved) {
				unsigned int  available_pages;

				available_pages = avail_free_count - vm_page_free_reserved;

				if (available_pages >= vm_page_free_wanted) {
					need_wakeup = vm_page_free_wanted;
					vm_page_free_wanted = 0;
				} else {
					need_wakeup = available_pages;
					vm_page_free_wanted -= available_pages;
				}
			}
			vm_free_page_unlock();

			priv_wakeup_event = NULL;
			secluded_wakeup_event = NULL;
			normal_wakeup_event = NULL;

			priv_wakeup_all = FALSE;
			secluded_wakeup_all = FALSE;
			normal_wakeup_all = FALSE;


			if (need_priv_wakeup != 0) {
				/*
				 * There shouldn't be that many VM-privileged threads,
				 * so let's wake them all up, even if we don't quite
				 * have enough pages to satisfy them all.
				 */
				priv_wakeup_event = (event_t)&vm_page_free_wanted_privileged;
				priv_wakeup_all = TRUE;
			}
#if CONFIG_SECLUDED_MEMORY
			if (need_wakeup_secluded != 0 &&
			    vm_page_free_wanted_secluded == 0) {
				secluded_wakeup_event = (event_t)&vm_page_free_wanted_secluded;
				secluded_wakeup_all = TRUE;
				need_wakeup_secluded = 0;
			} else {
				secluded_wakeup_event = (event_t)&vm_page_free_wanted_secluded;
			}
#endif /* CONFIG_SECLUDED_MEMORY */
			if (need_wakeup != 0 && vm_page_free_wanted == 0) {
				/*
				 * We don't expect to have any more waiters
				 * after this, so let's wake them all up at
				 * once.
				 */
				normal_wakeup_event = (event_t) &vm_page_free_count;
				normal_wakeup_all = TRUE;
				need_wakeup = 0;
			} else {
				normal_wakeup_event = (event_t) &vm_page_free_count;
			}

			if (priv_wakeup_event ||
#if CONFIG_SECLUDED_MEMORY
			    secluded_wakeup_event ||
#endif /* CONFIG_SECLUDED_MEMORY */
			    normal_wakeup_event) {
				if (vps_dynamic_priority_enabled) {
					if (priv_wakeup_all == TRUE) {
						wakeup_all_with_inheritor(priv_wakeup_event, THREAD_AWAKENED);
					}

#if CONFIG_SECLUDED_MEMORY
					if (secluded_wakeup_all == TRUE) {
						wakeup_all_with_inheritor(secluded_wakeup_event, THREAD_AWAKENED);
					}

					while (need_wakeup_secluded-- != 0) {
						/*
						 * Wake up one waiter per page we just released.
						 */
						wakeup_one_with_inheritor(secluded_wakeup_event,
						    THREAD_AWAKENED, LCK_WAKE_DO_NOT_TRANSFER_PUSH, NULL);
					}
#endif /* CONFIG_SECLUDED_MEMORY */

					if (normal_wakeup_all == TRUE) {
						wakeup_all_with_inheritor(normal_wakeup_event, THREAD_AWAKENED);
					}

					while (need_wakeup-- != 0) {
						/*
						 * Wake up one waiter per page we just released.
						 */
						wakeup_one_with_inheritor(normal_wakeup_event,
						    THREAD_AWAKENED, LCK_WAKE_DO_NOT_TRANSFER_PUSH,
						    NULL);
					}
				} else {
					/*
					 * Non-priority-aware wakeups.
					 */

					if (priv_wakeup_all == TRUE) {
						thread_wakeup(priv_wakeup_event);
					}

#if CONFIG_SECLUDED_MEMORY
					if (secluded_wakeup_all == TRUE) {
						thread_wakeup(secluded_wakeup_event);
					}

					while (need_wakeup_secluded-- != 0) {
						/*
						 * Wake up one waiter per page we just released.
						 */
						thread_wakeup_one(secluded_wakeup_event);
					}

#endif /* CONFIG_SECLUDED_MEMORY */
					if (normal_wakeup_all == TRUE) {
						thread_wakeup(normal_wakeup_event);
					}

					while (need_wakeup-- != 0) {
						/*
						 * Wake up one waiter per page we just released.
						 */
						thread_wakeup_one(normal_wakeup_event);
					}
				}
			}

			VM_CHECK_MEMORYSTATUS;
		}
	}
}


/*
 *	vm_page_wire:
 *
 *	Mark this page as wired down by yet
 *	another map, removing it from paging queues
 *	as necessary.
 *
 *	The page's object and the page queues must be locked.
 */


void
vm_page_wire(
	vm_page_t mem,
	vm_tag_t           tag,
	boolean_t          check_memorystatus)
{
	vm_object_t     m_object;

	m_object = VM_PAGE_OBJECT(mem);

//	dbgLog(current_thread(), mem->vmp_offset, m_object, 1);	/* (TEST/DEBUG) */

	VM_PAGE_CHECK(mem);
	if (m_object) {
		vm_object_lock_assert_exclusive(m_object);
	} else {
		/*
		 * In theory, the page should be in an object before it
		 * gets wired, since we need to hold the object lock
		 * to update some fields in the page structure.
		 * However, some code (i386 pmap, for example) might want
		 * to wire a page before it gets inserted into an object.
		 * That's somewhat OK, as long as nobody else can get to
		 * that page and update it at the same time.
		 */
	}
	LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);
	if (!VM_PAGE_WIRED(mem)) {
		if (mem->vmp_laundry) {
			vm_pageout_steal_laundry(mem, TRUE);
		}

		vm_page_queues_remove(mem, TRUE);

		assert(mem->vmp_wire_count == 0);
		mem->vmp_q_state = VM_PAGE_IS_WIRED;

#if CONFIG_TRACK_UNMODIFIED_ANON_PAGES
		if (mem->vmp_unmodified_ro == true) {
			/* Object and PageQ locks are held*/
			mem->vmp_unmodified_ro = false;
			os_atomic_dec(&compressor_ro_uncompressed, relaxed);
			vm_object_compressor_pager_state_clr(VM_PAGE_OBJECT(mem), mem->vmp_offset);
		}
#endif /* CONFIG_TRACK_UNMODIFIED_ANON_PAGES */

		if (m_object) {
			task_t          owner;
			int             ledger_idx_volatile;
			int             ledger_idx_nonvolatile;
			int             ledger_idx_volatile_compressed;
			int             ledger_idx_nonvolatile_compressed;
			int             ledger_idx_composite;
			int             ledger_idx_external_wired;
			boolean_t       do_footprint;

			VM_OBJECT_WIRED_PAGE_UPDATE_START(m_object);
			VM_OBJECT_WIRED_PAGE_ADD(m_object, mem);
			VM_OBJECT_WIRED_PAGE_UPDATE_END(m_object, tag);

			assert(m_object->resident_page_count >=
			    m_object->wired_page_count);
			if (m_object->purgable == VM_PURGABLE_VOLATILE) {
				assert(vm_page_purgeable_count > 0);
				OSAddAtomic(-1, &vm_page_purgeable_count);
				OSAddAtomic(1, &vm_page_purgeable_wired_count);
			}
			if (m_object->internal &&
			    m_object->vo_owner != TASK_NULL &&
			    (m_object->purgable == VM_PURGABLE_VOLATILE ||
			    m_object->purgable == VM_PURGABLE_EMPTY)) {
				owner = VM_OBJECT_OWNER(m_object);
				vm_object_ledger_tag_ledgers(
					m_object,
					&ledger_idx_volatile,
					&ledger_idx_nonvolatile,
					&ledger_idx_volatile_compressed,
					&ledger_idx_nonvolatile_compressed,
					&ledger_idx_composite,
					&ledger_idx_external_wired,
					&do_footprint);
				/* less volatile bytes */
				ledger_debit(owner->ledger,
				    ledger_idx_volatile,
				    PAGE_SIZE);
				/* more not-quite-volatile bytes */
				ledger_credit(owner->ledger,
				    ledger_idx_nonvolatile,
				    PAGE_SIZE);
				if (do_footprint) {
					/* more footprint */
					ledger_credit(owner->ledger,
					    task_ledgers.phys_footprint,
					    PAGE_SIZE);
				} else if (ledger_idx_composite != -1) {
					ledger_credit(owner->ledger,
					    ledger_idx_composite,
					    PAGE_SIZE);
				}
			}

			if (m_object->all_reusable) {
				/*
				 * Wired pages are not counted as "re-usable"
				 * in "all_reusable" VM objects, so nothing
				 * to do here.
				 */
			} else if (mem->vmp_reusable) {
				/*
				 * This page is not "re-usable" when it's
				 * wired, so adjust its state and the
				 * accounting.
				 */
				vm_page_lockconvert_queues();
				vm_object_reuse_pages(m_object,
				    mem->vmp_offset,
				    mem->vmp_offset + PAGE_SIZE_64,
				    FALSE);
			}
		}
		assert(!mem->vmp_reusable);

		if (!mem->vmp_private && !mem->vmp_fictitious && !mem->vmp_gobbled) {
			vm_page_wire_count++;
		}
		if (mem->vmp_gobbled) {
			vm_page_gobble_count--;
		}
		mem->vmp_gobbled = FALSE;

		if (check_memorystatus == TRUE) {
			VM_CHECK_MEMORYSTATUS;
		}
	}
	assert(!mem->vmp_gobbled);
	assert(mem->vmp_q_state == VM_PAGE_IS_WIRED);
	mem->vmp_wire_count++;
	if (__improbable(mem->vmp_wire_count == 0)) {
		panic("vm_page_wire(%p): wire_count overflow", mem);
	}
	VM_PAGE_CHECK(mem);
}

/*
 *	vm_page_unwire:
 *
 *	Release one wiring of this page, potentially
 *	enabling it to be paged again.
 *
 *	The page's object and the page queues must be locked.
 */
void
vm_page_unwire(
	vm_page_t       mem,
	boolean_t       queueit)
{
	vm_object_t     m_object;

	m_object = VM_PAGE_OBJECT(mem);

//	dbgLog(current_thread(), mem->vmp_offset, m_object, 0);	/* (TEST/DEBUG) */

	VM_PAGE_CHECK(mem);
	assert(VM_PAGE_WIRED(mem));
	assert(mem->vmp_wire_count > 0);
	assert(!mem->vmp_gobbled);
	assert(m_object != VM_OBJECT_NULL);
	vm_object_lock_assert_exclusive(m_object);
	LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);
	if (--mem->vmp_wire_count == 0) {
		task_t          owner;
		int             ledger_idx_volatile;
		int             ledger_idx_nonvolatile;
		int             ledger_idx_volatile_compressed;
		int             ledger_idx_nonvolatile_compressed;
		int             ledger_idx_composite;
		int             ledger_idx_external_wired;
		boolean_t       do_footprint;

		mem->vmp_q_state = VM_PAGE_NOT_ON_Q;

		VM_OBJECT_WIRED_PAGE_UPDATE_START(m_object);
		VM_OBJECT_WIRED_PAGE_REMOVE(m_object, mem);
		VM_OBJECT_WIRED_PAGE_UPDATE_END(m_object, m_object->wire_tag);
		if (!mem->vmp_private && !mem->vmp_fictitious) {
			vm_page_wire_count--;
		}

		assert(m_object->resident_page_count >=
		    m_object->wired_page_count);
		if (m_object->purgable == VM_PURGABLE_VOLATILE) {
			OSAddAtomic(+1, &vm_page_purgeable_count);
			assert(vm_page_purgeable_wired_count > 0);
			OSAddAtomic(-1, &vm_page_purgeable_wired_count);
		}
		if (m_object->internal &&
		    m_object->vo_owner != TASK_NULL &&
		    (m_object->purgable == VM_PURGABLE_VOLATILE ||
		    m_object->purgable == VM_PURGABLE_EMPTY)) {
			owner = VM_OBJECT_OWNER(m_object);
			vm_object_ledger_tag_ledgers(
				m_object,
				&ledger_idx_volatile,
				&ledger_idx_nonvolatile,
				&ledger_idx_volatile_compressed,
				&ledger_idx_nonvolatile_compressed,
				&ledger_idx_composite,
				&ledger_idx_external_wired,
				&do_footprint);
			/* more volatile bytes */
			ledger_credit(owner->ledger,
			    ledger_idx_volatile,
			    PAGE_SIZE);
			/* less not-quite-volatile bytes */
			ledger_debit(owner->ledger,
			    ledger_idx_nonvolatile,
			    PAGE_SIZE);
			if (do_footprint) {
				/* less footprint */
				ledger_debit(owner->ledger,
				    task_ledgers.phys_footprint,
				    PAGE_SIZE);
			} else if (ledger_idx_composite != -1) {
				ledger_debit(owner->ledger,
				    ledger_idx_composite,
				    PAGE_SIZE);
			}
		}
		assert(!is_kernel_object(m_object));
		assert(mem->vmp_pageq.next == 0 && mem->vmp_pageq.prev == 0);

		if (queueit == TRUE) {
			if (m_object->purgable == VM_PURGABLE_EMPTY) {
				vm_page_deactivate(mem);
			} else {
				vm_page_activate(mem);
			}
		}

		VM_CHECK_MEMORYSTATUS;
	}
	VM_PAGE_CHECK(mem);
}

/*
 *	vm_page_deactivate:
 *
 *	Returns the given page to the inactive list,
 *	indicating that no physical maps have access
 *	to this page.  [Used by the physical mapping system.]
 *
 *	The page queues must be locked.
 */
void
vm_page_deactivate(
	vm_page_t       m)
{
	vm_page_deactivate_internal(m, TRUE);
}


void
vm_page_deactivate_internal(
	vm_page_t       m,
	boolean_t       clear_hw_reference)
{
	vm_object_t     m_object;

	m_object = VM_PAGE_OBJECT(m);

	VM_PAGE_CHECK(m);
	assert(!is_kernel_object(m_object));
	assert(VM_PAGE_GET_PHYS_PAGE(m) != vm_page_guard_addr);

//	dbgLog(VM_PAGE_GET_PHYS_PAGE(m), vm_page_free_count, vm_page_wire_count, 6);	/* (TEST/DEBUG) */
	LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);
	/*
	 *	This page is no longer very interesting.  If it was
	 *	interesting (active or inactive/referenced), then we
	 *	clear the reference bit and (re)enter it in the
	 *	inactive queue.  Note wired pages should not have
	 *	their reference bit cleared.
	 */
	assert( !(m->vmp_absent && !m->vmp_unusual));

	if (m->vmp_gobbled) {           /* can this happen? */
		assert( !VM_PAGE_WIRED(m));

		if (!m->vmp_private && !m->vmp_fictitious) {
			vm_page_wire_count--;
		}
		vm_page_gobble_count--;
		m->vmp_gobbled = FALSE;
	}
	/*
	 * if this page is currently on the pageout queue, we can't do the
	 * vm_page_queues_remove (which doesn't handle the pageout queue case)
	 * and we can't remove it manually since we would need the object lock
	 * (which is not required here) to decrement the activity_in_progress
	 * reference which is held on the object while the page is in the pageout queue...
	 * just let the normal laundry processing proceed
	 */
	if (m->vmp_laundry || m->vmp_private || m->vmp_fictitious ||
	    (m->vmp_q_state == VM_PAGE_USED_BY_COMPRESSOR) ||
	    (m->vmp_q_state == VM_PAGE_ON_PAGEOUT_Q) ||
	    VM_PAGE_WIRED(m)) {
		return;
	}
	if (!m->vmp_absent && clear_hw_reference == TRUE) {
		vm_page_lockconvert_queues();
		pmap_clear_reference(VM_PAGE_GET_PHYS_PAGE(m));
	}

	m->vmp_reference = FALSE;
	m->vmp_no_cache = FALSE;

	if (!VM_PAGE_INACTIVE(m)) {
		vm_page_queues_remove(m, FALSE);

		if (!VM_DYNAMIC_PAGING_ENABLED() &&
		    m->vmp_dirty && m_object->internal &&
		    (m_object->purgable == VM_PURGABLE_DENY ||
		    m_object->purgable == VM_PURGABLE_NONVOLATILE ||
		    m_object->purgable == VM_PURGABLE_VOLATILE)) {
			vm_page_check_pageable_safe(m);
			vm_page_queue_enter(&vm_page_queue_throttled, m, vmp_pageq);
			m->vmp_q_state = VM_PAGE_ON_THROTTLED_Q;
			vm_page_throttled_count++;
		} else {
			if (m_object->named && m_object->ref_count == 1) {
				vm_page_speculate(m, FALSE);
#if DEVELOPMENT || DEBUG
				vm_page_speculative_recreated++;
#endif
			} else {
				vm_page_enqueue_inactive(m, FALSE);
			}
		}
	}
}

/*
 * vm_page_enqueue_cleaned
 *
 * Put the page on the cleaned queue, mark it cleaned, etc.
 * Being on the cleaned queue (and having m->clean_queue set)
 * does ** NOT ** guarantee that the page is clean!
 *
 * Call with the queues lock held.
 */

void
vm_page_enqueue_cleaned(vm_page_t m)
{
	vm_object_t     m_object;

	m_object = VM_PAGE_OBJECT(m);

	assert(VM_PAGE_GET_PHYS_PAGE(m) != vm_page_guard_addr);
	LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);
	assert( !(m->vmp_absent && !m->vmp_unusual));

	if (VM_PAGE_WIRED(m)) {
		return;
	}

	if (m->vmp_gobbled) {
		if (!m->vmp_private && !m->vmp_fictitious) {
			vm_page_wire_count--;
		}
		vm_page_gobble_count--;
		m->vmp_gobbled = FALSE;
	}
	/*
	 * if this page is currently on the pageout queue, we can't do the
	 * vm_page_queues_remove (which doesn't handle the pageout queue case)
	 * and we can't remove it manually since we would need the object lock
	 * (which is not required here) to decrement the activity_in_progress
	 * reference which is held on the object while the page is in the pageout queue...
	 * just let the normal laundry processing proceed
	 */
	if (m->vmp_laundry || m->vmp_private || m->vmp_fictitious ||
	    (m->vmp_q_state == VM_PAGE_ON_INACTIVE_CLEANED_Q) ||
	    (m->vmp_q_state == VM_PAGE_ON_PAGEOUT_Q)) {
		return;
	}
	vm_page_queues_remove(m, FALSE);

	vm_page_check_pageable_safe(m);
	vm_page_queue_enter(&vm_page_queue_cleaned, m, vmp_pageq);
	m->vmp_q_state = VM_PAGE_ON_INACTIVE_CLEANED_Q;
	vm_page_cleaned_count++;

	vm_page_inactive_count++;
	if (m_object->internal) {
		vm_page_pageable_internal_count++;
	} else {
		vm_page_pageable_external_count++;
	}
	vm_page_add_to_specialq(m, TRUE);
	VM_PAGEOUT_DEBUG(vm_pageout_enqueued_cleaned, 1);
}

/*
 *	vm_page_activate:
 *
 *	Put the specified page on the active list (if appropriate).
 *
 *	The page queues must be locked.
 */

void
vm_page_activate(
	vm_page_t       m)
{
	vm_object_t     m_object;

	m_object = VM_PAGE_OBJECT(m);

	VM_PAGE_CHECK(m);
#ifdef  FIXME_4778297
	assert(!is_kernel_object(m_object));
#endif
	assert(VM_PAGE_GET_PHYS_PAGE(m) != vm_page_guard_addr);
	LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);
	assert( !(m->vmp_absent && !m->vmp_unusual));

	if (m->vmp_gobbled) {
		assert( !VM_PAGE_WIRED(m));
		if (!m->vmp_private && !m->vmp_fictitious) {
			vm_page_wire_count--;
		}
		vm_page_gobble_count--;
		m->vmp_gobbled = FALSE;
	}
	/*
	 * if this page is currently on the pageout queue, we can't do the
	 * vm_page_queues_remove (which doesn't handle the pageout queue case)
	 * and we can't remove it manually since we would need the object lock
	 * (which is not required here) to decrement the activity_in_progress
	 * reference which is held on the object while the page is in the pageout queue...
	 * just let the normal laundry processing proceed
	 */
	if (m->vmp_laundry || m->vmp_private || m->vmp_fictitious ||
	    (m->vmp_q_state == VM_PAGE_USED_BY_COMPRESSOR) ||
	    (m->vmp_q_state == VM_PAGE_ON_PAGEOUT_Q)) {
		return;
	}

#if DEBUG
	if (m->vmp_q_state == VM_PAGE_ON_ACTIVE_Q) {
		panic("vm_page_activate: already active");
	}
#endif

	if (m->vmp_q_state == VM_PAGE_ON_SPECULATIVE_Q) {
		DTRACE_VM2(pgrec, int, 1, (uint64_t *), NULL);
		DTRACE_VM2(pgfrec, int, 1, (uint64_t *), NULL);
	}

	/*
	 * A freshly activated page should be promoted in the donation queue.
	 * So we remove it here while preserving its hint and we will enqueue
	 * it again in vm_page_enqueue_active.
	 */
	vm_page_queues_remove(m, ((m->vmp_on_specialq == VM_PAGE_SPECIAL_Q_DONATE) ? TRUE : FALSE));

	if (!VM_PAGE_WIRED(m)) {
		vm_page_check_pageable_safe(m);
		if (!VM_DYNAMIC_PAGING_ENABLED() &&
		    m->vmp_dirty && m_object->internal &&
		    (m_object->purgable == VM_PURGABLE_DENY ||
		    m_object->purgable == VM_PURGABLE_NONVOLATILE ||
		    m_object->purgable == VM_PURGABLE_VOLATILE)) {
			vm_page_queue_enter(&vm_page_queue_throttled, m, vmp_pageq);
			m->vmp_q_state = VM_PAGE_ON_THROTTLED_Q;
			vm_page_throttled_count++;
		} else {
#if CONFIG_SECLUDED_MEMORY
			if (secluded_for_filecache &&
			    vm_page_secluded_target != 0 &&
			    num_tasks_can_use_secluded_mem == 0 &&
			    m_object->eligible_for_secluded &&
			    !m->vmp_realtime) {
				vm_page_queue_enter(&vm_page_queue_secluded, m, vmp_pageq);
				m->vmp_q_state = VM_PAGE_ON_SECLUDED_Q;
				vm_page_secluded_count++;
				VM_PAGE_SECLUDED_COUNT_OVER_TARGET_UPDATE();
				vm_page_secluded_count_inuse++;
				assert(!m_object->internal);
//				vm_page_pageable_external_count++;
			} else
#endif /* CONFIG_SECLUDED_MEMORY */
			vm_page_enqueue_active(m, FALSE);
		}
		m->vmp_reference = TRUE;
		m->vmp_no_cache = FALSE;
	}
	VM_PAGE_CHECK(m);
}


/*
 *      vm_page_speculate:
 *
 *      Put the specified page on the speculative list (if appropriate).
 *
 *      The page queues must be locked.
 */
void
vm_page_speculate(
	vm_page_t       m,
	boolean_t       new)
{
	struct vm_speculative_age_q     *aq;
	vm_object_t     m_object;

	m_object = VM_PAGE_OBJECT(m);

	VM_PAGE_CHECK(m);
	vm_page_check_pageable_safe(m);

	assert(VM_PAGE_GET_PHYS_PAGE(m) != vm_page_guard_addr);
	LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);
	assert( !(m->vmp_absent && !m->vmp_unusual));
	assert(m_object->internal == FALSE);

	/*
	 * if this page is currently on the pageout queue, we can't do the
	 * vm_page_queues_remove (which doesn't handle the pageout queue case)
	 * and we can't remove it manually since we would need the object lock
	 * (which is not required here) to decrement the activity_in_progress
	 * reference which is held on the object while the page is in the pageout queue...
	 * just let the normal laundry processing proceed
	 */
	if (m->vmp_laundry || m->vmp_private || m->vmp_fictitious ||
	    (m->vmp_q_state == VM_PAGE_USED_BY_COMPRESSOR) ||
	    (m->vmp_q_state == VM_PAGE_ON_PAGEOUT_Q)) {
		return;
	}

	vm_page_queues_remove(m, FALSE);

	if (!VM_PAGE_WIRED(m)) {
		mach_timespec_t         ts;
		clock_sec_t sec;
		clock_nsec_t nsec;

		clock_get_system_nanotime(&sec, &nsec);
		ts.tv_sec = (unsigned int) sec;
		ts.tv_nsec = nsec;

		if (vm_page_speculative_count == 0) {
			speculative_age_index = VM_PAGE_MIN_SPECULATIVE_AGE_Q;
			speculative_steal_index = VM_PAGE_MIN_SPECULATIVE_AGE_Q;

			aq = &vm_page_queue_speculative[speculative_age_index];

			/*
			 * set the timer to begin a new group
			 */
			aq->age_ts.tv_sec = vm_pageout_state.vm_page_speculative_q_age_ms / 1000;
			aq->age_ts.tv_nsec = (vm_pageout_state.vm_page_speculative_q_age_ms % 1000) * 1000 * NSEC_PER_USEC;

			ADD_MACH_TIMESPEC(&aq->age_ts, &ts);
		} else {
			aq = &vm_page_queue_speculative[speculative_age_index];

			if (CMP_MACH_TIMESPEC(&ts, &aq->age_ts) >= 0) {
				speculative_age_index++;

				if (speculative_age_index > vm_page_max_speculative_age_q) {
					speculative_age_index = VM_PAGE_MIN_SPECULATIVE_AGE_Q;
				}
				if (speculative_age_index == speculative_steal_index) {
					speculative_steal_index = speculative_age_index + 1;

					if (speculative_steal_index > vm_page_max_speculative_age_q) {
						speculative_steal_index = VM_PAGE_MIN_SPECULATIVE_AGE_Q;
					}
				}
				aq = &vm_page_queue_speculative[speculative_age_index];

				if (!vm_page_queue_empty(&aq->age_q)) {
					vm_page_speculate_ageit(aq);
				}

				aq->age_ts.tv_sec = vm_pageout_state.vm_page_speculative_q_age_ms / 1000;
				aq->age_ts.tv_nsec = (vm_pageout_state.vm_page_speculative_q_age_ms % 1000) * 1000 * NSEC_PER_USEC;

				ADD_MACH_TIMESPEC(&aq->age_ts, &ts);
			}
		}
		vm_page_enqueue_tail(&aq->age_q, &m->vmp_pageq);
		m->vmp_q_state = VM_PAGE_ON_SPECULATIVE_Q;
		vm_page_speculative_count++;
		vm_page_pageable_external_count++;

		if (new == TRUE) {
			vm_object_lock_assert_exclusive(m_object);

			m_object->pages_created++;
#if DEVELOPMENT || DEBUG
			vm_page_speculative_created++;
#endif
		}
	}
	VM_PAGE_CHECK(m);
}


/*
 * move pages from the specified aging bin to
 * the speculative bin that pageout_scan claims from
 *
 *      The page queues must be locked.
 */
void
vm_page_speculate_ageit(struct vm_speculative_age_q *aq)
{
	struct vm_speculative_age_q     *sq;
	vm_page_t       t;

	sq = &vm_page_queue_speculative[VM_PAGE_SPECULATIVE_AGED_Q];

	if (vm_page_queue_empty(&sq->age_q)) {
		sq->age_q.next = aq->age_q.next;
		sq->age_q.prev = aq->age_q.prev;

		t = (vm_page_t)VM_PAGE_UNPACK_PTR(sq->age_q.next);
		t->vmp_pageq.prev = VM_PAGE_PACK_PTR(&sq->age_q);

		t = (vm_page_t)VM_PAGE_UNPACK_PTR(sq->age_q.prev);
		t->vmp_pageq.next = VM_PAGE_PACK_PTR(&sq->age_q);
	} else {
		t = (vm_page_t)VM_PAGE_UNPACK_PTR(sq->age_q.prev);
		t->vmp_pageq.next = aq->age_q.next;

		t = (vm_page_t)VM_PAGE_UNPACK_PTR(aq->age_q.next);
		t->vmp_pageq.prev = sq->age_q.prev;

		t = (vm_page_t)VM_PAGE_UNPACK_PTR(aq->age_q.prev);
		t->vmp_pageq.next = VM_PAGE_PACK_PTR(&sq->age_q);

		sq->age_q.prev = aq->age_q.prev;
	}
	vm_page_queue_init(&aq->age_q);
}


void
vm_page_lru(
	vm_page_t       m)
{
	VM_PAGE_CHECK(m);
	assert(!is_kernel_object(VM_PAGE_OBJECT(m)));
	assert(VM_PAGE_GET_PHYS_PAGE(m) != vm_page_guard_addr);

	LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);

	if (m->vmp_q_state == VM_PAGE_ON_INACTIVE_EXTERNAL_Q) {
		/*
		 * we don't need to do all the other work that
		 * vm_page_queues_remove and vm_page_enqueue_inactive
		 * bring along for the ride
		 */
		assert(!m->vmp_laundry);
		assert(!m->vmp_private);

		m->vmp_no_cache = FALSE;

		vm_page_queue_remove(&vm_page_queue_inactive, m, vmp_pageq);
		vm_page_queue_enter(&vm_page_queue_inactive, m, vmp_pageq);

		return;
	}
	/*
	 * if this page is currently on the pageout queue, we can't do the
	 * vm_page_queues_remove (which doesn't handle the pageout queue case)
	 * and we can't remove it manually since we would need the object lock
	 * (which is not required here) to decrement the activity_in_progress
	 * reference which is held on the object while the page is in the pageout queue...
	 * just let the normal laundry processing proceed
	 */
	if (m->vmp_laundry || m->vmp_private ||
	    (m->vmp_q_state == VM_PAGE_USED_BY_COMPRESSOR) ||
	    (m->vmp_q_state == VM_PAGE_ON_PAGEOUT_Q) ||
	    VM_PAGE_WIRED(m)) {
		return;
	}

	m->vmp_no_cache = FALSE;

	vm_page_queues_remove(m, FALSE);

	vm_page_enqueue_inactive(m, FALSE);
}


void
vm_page_reactivate_all_throttled(void)
{
	vm_page_t       first_throttled, last_throttled;
	vm_page_t       first_active;
	vm_page_t       m;
	int             extra_active_count;
	int             extra_internal_count, extra_external_count;
	vm_object_t     m_object;

	if (!VM_DYNAMIC_PAGING_ENABLED()) {
		return;
	}

	extra_active_count = 0;
	extra_internal_count = 0;
	extra_external_count = 0;
	vm_page_lock_queues();
	if (!vm_page_queue_empty(&vm_page_queue_throttled)) {
		/*
		 * Switch "throttled" pages to "active".
		 */
		vm_page_queue_iterate(&vm_page_queue_throttled, m, vmp_pageq) {
			VM_PAGE_CHECK(m);
			assert(m->vmp_q_state == VM_PAGE_ON_THROTTLED_Q);

			m_object = VM_PAGE_OBJECT(m);

			extra_active_count++;
			if (m_object->internal) {
				extra_internal_count++;
			} else {
				extra_external_count++;
			}

			m->vmp_q_state = VM_PAGE_ON_ACTIVE_Q;
			VM_PAGE_CHECK(m);
			vm_page_add_to_specialq(m, FALSE);
		}

		/*
		 * Transfer the entire throttled queue to a regular LRU page queues.
		 * We insert it at the head of the active queue, so that these pages
		 * get re-evaluated by the LRU algorithm first, since they've been
		 * completely out of it until now.
		 */
		first_throttled = (vm_page_t) vm_page_queue_first(&vm_page_queue_throttled);
		last_throttled = (vm_page_t) vm_page_queue_last(&vm_page_queue_throttled);
		first_active = (vm_page_t) vm_page_queue_first(&vm_page_queue_active);
		if (vm_page_queue_empty(&vm_page_queue_active)) {
			vm_page_queue_active.prev = VM_PAGE_CONVERT_TO_QUEUE_ENTRY(last_throttled);
		} else {
			first_active->vmp_pageq.prev = VM_PAGE_CONVERT_TO_QUEUE_ENTRY(last_throttled);
		}
		vm_page_queue_active.next = VM_PAGE_CONVERT_TO_QUEUE_ENTRY(first_throttled);
		first_throttled->vmp_pageq.prev = VM_PAGE_CONVERT_TO_QUEUE_ENTRY(&vm_page_queue_active);
		last_throttled->vmp_pageq.next = VM_PAGE_CONVERT_TO_QUEUE_ENTRY(first_active);

#if DEBUG
		printf("reactivated %d throttled pages\n", vm_page_throttled_count);
#endif
		vm_page_queue_init(&vm_page_queue_throttled);
		/*
		 * Adjust the global page counts.
		 */
		vm_page_active_count += extra_active_count;
		vm_page_pageable_internal_count += extra_internal_count;
		vm_page_pageable_external_count += extra_external_count;
		vm_page_throttled_count = 0;
	}
	assert(vm_page_throttled_count == 0);
	assert(vm_page_queue_empty(&vm_page_queue_throttled));
	vm_page_unlock_queues();
}


/*
 * move pages from the indicated local queue to the global active queue
 * its ok to fail if we're below the hard limit and force == FALSE
 * the nolocks == TRUE case is to allow this function to be run on
 * the hibernate path
 */

void
vm_page_reactivate_local(uint32_t lid, boolean_t force, boolean_t nolocks)
{
	struct vpl      *lq;
	vm_page_t       first_local, last_local;
	vm_page_t       first_active;
	vm_page_t       m;
	uint32_t        count = 0;

	if (vm_page_local_q == NULL) {
		return;
	}

	lq = zpercpu_get_cpu(vm_page_local_q, lid);

	if (nolocks == FALSE) {
		if (lq->vpl_count < vm_page_local_q_hard_limit && force == FALSE) {
			if (!vm_page_trylockspin_queues()) {
				return;
			}
		} else {
			vm_page_lockspin_queues();
		}

		VPL_LOCK(&lq->vpl_lock);
	}
	if (lq->vpl_count) {
		/*
		 * Switch "local" pages to "active".
		 */
		assert(!vm_page_queue_empty(&lq->vpl_queue));

		vm_page_queue_iterate(&lq->vpl_queue, m, vmp_pageq) {
			VM_PAGE_CHECK(m);
			vm_page_check_pageable_safe(m);
			assert(m->vmp_q_state == VM_PAGE_ON_ACTIVE_LOCAL_Q);
			assert(!m->vmp_fictitious);

			if (m->vmp_local_id != lid) {
				panic("vm_page_reactivate_local: found vm_page_t(%p) with wrong cpuid", m);
			}

			m->vmp_local_id = 0;
			m->vmp_q_state = VM_PAGE_ON_ACTIVE_Q;
			VM_PAGE_CHECK(m);
			vm_page_add_to_specialq(m, FALSE);
			count++;
		}
		if (count != lq->vpl_count) {
			panic("vm_page_reactivate_local: count = %d, vm_page_local_count = %d", count, lq->vpl_count);
		}

		/*
		 * Transfer the entire local queue to a regular LRU page queues.
		 */
		first_local = (vm_page_t) vm_page_queue_first(&lq->vpl_queue);
		last_local = (vm_page_t) vm_page_queue_last(&lq->vpl_queue);
		first_active = (vm_page_t) vm_page_queue_first(&vm_page_queue_active);

		if (vm_page_queue_empty(&vm_page_queue_active)) {
			vm_page_queue_active.prev = VM_PAGE_CONVERT_TO_QUEUE_ENTRY(last_local);
		} else {
			first_active->vmp_pageq.prev = VM_PAGE_CONVERT_TO_QUEUE_ENTRY(last_local);
		}
		vm_page_queue_active.next = VM_PAGE_CONVERT_TO_QUEUE_ENTRY(first_local);
		first_local->vmp_pageq.prev = VM_PAGE_CONVERT_TO_QUEUE_ENTRY(&vm_page_queue_active);
		last_local->vmp_pageq.next = VM_PAGE_CONVERT_TO_QUEUE_ENTRY(first_active);

		vm_page_queue_init(&lq->vpl_queue);
		/*
		 * Adjust the global page counts.
		 */
		vm_page_active_count += lq->vpl_count;
		vm_page_pageable_internal_count += lq->vpl_internal_count;
		vm_page_pageable_external_count += lq->vpl_external_count;
		lq->vpl_count = 0;
		lq->vpl_internal_count = 0;
		lq->vpl_external_count = 0;
	}
	assert(vm_page_queue_empty(&lq->vpl_queue));

	if (nolocks == FALSE) {
		VPL_UNLOCK(&lq->vpl_lock);

		vm_page_balance_inactive(count / 4);
		vm_page_unlock_queues();
	}
}

/*
 *	vm_page_part_zero_fill:
 *
 *	Zero-fill a part of the page.
 */
#define PMAP_ZERO_PART_PAGE_IMPLEMENTED
void
vm_page_part_zero_fill(
	vm_page_t       m,
	vm_offset_t     m_pa,
	vm_size_t       len)
{
#if 0
	/*
	 * we don't hold the page queue lock
	 * so this check isn't safe to make
	 */
	VM_PAGE_CHECK(m);
#endif

#ifdef PMAP_ZERO_PART_PAGE_IMPLEMENTED
	pmap_zero_part_page(VM_PAGE_GET_PHYS_PAGE(m), m_pa, len);
#else
	vm_page_t       tmp;
	while (1) {
		tmp = vm_page_grab();
		if (tmp == VM_PAGE_NULL) {
			vm_page_wait(THREAD_UNINT);
			continue;
		}
		break;
	}
	vm_page_zero_fill(tmp);
	if (m_pa != 0) {
		vm_page_part_copy(m, 0, tmp, 0, m_pa);
	}
	if ((m_pa + len) < PAGE_SIZE) {
		vm_page_part_copy(m, m_pa + len, tmp,
		    m_pa + len, PAGE_SIZE - (m_pa + len));
	}
	vm_page_copy(tmp, m);
	VM_PAGE_FREE(tmp);
#endif
}

/*
 *	vm_page_zero_fill:
 *
 *	Zero-fill the specified page.
 */
void
vm_page_zero_fill(
	vm_page_t       m)
{
#if 0
	/*
	 * we don't hold the page queue lock
	 * so this check isn't safe to make
	 */
	VM_PAGE_CHECK(m);
#endif

//	dbgTrace(0xAEAEAEAE, VM_PAGE_GET_PHYS_PAGE(m), 0);		/* (BRINGUP) */
	pmap_zero_page(VM_PAGE_GET_PHYS_PAGE(m));
}

/*
 *	vm_page_part_copy:
 *
 *	copy part of one page to another
 */

void
vm_page_part_copy(
	vm_page_t       src_m,
	vm_offset_t     src_pa,
	vm_page_t       dst_m,
	vm_offset_t     dst_pa,
	vm_size_t       len)
{
#if 0
	/*
	 * we don't hold the page queue lock
	 * so this check isn't safe to make
	 */
	VM_PAGE_CHECK(src_m);
	VM_PAGE_CHECK(dst_m);
#endif
	pmap_copy_part_page(VM_PAGE_GET_PHYS_PAGE(src_m), src_pa,
	    VM_PAGE_GET_PHYS_PAGE(dst_m), dst_pa, len);
}

/*
 *	vm_page_copy:
 *
 *	Copy one page to another
 */

int vm_page_copy_cs_validations = 0;
int vm_page_copy_cs_tainted = 0;

void
vm_page_copy(
	vm_page_t       src_m,
	vm_page_t       dest_m)
{
	vm_object_t     src_m_object;

	src_m_object = VM_PAGE_OBJECT(src_m);

#if 0
	/*
	 * we don't hold the page queue lock
	 * so this check isn't safe to make
	 */
	VM_PAGE_CHECK(src_m);
	VM_PAGE_CHECK(dest_m);
#endif
	vm_object_lock_assert_held(src_m_object);

	if (src_m_object != VM_OBJECT_NULL &&
	    src_m_object->code_signed) {
		/*
		 * We're copying a page from a code-signed object.
		 * Whoever ends up mapping the copy page might care about
		 * the original page's integrity, so let's validate the
		 * source page now.
		 */
		vm_page_copy_cs_validations++;
		vm_page_validate_cs(src_m, PAGE_SIZE, 0);
#if DEVELOPMENT || DEBUG
		DTRACE_VM4(codesigned_copy,
		    vm_object_t, src_m_object,
		    vm_object_offset_t, src_m->vmp_offset,
		    int, src_m->vmp_cs_validated,
		    int, src_m->vmp_cs_tainted);
#endif /* DEVELOPMENT || DEBUG */
	}

	/*
	 * Propagate the cs_tainted bit to the copy page. Do not propagate
	 * the cs_validated bit.
	 */
	dest_m->vmp_cs_tainted = src_m->vmp_cs_tainted;
	dest_m->vmp_cs_nx = src_m->vmp_cs_nx;
	if (dest_m->vmp_cs_tainted) {
		vm_page_copy_cs_tainted++;
	}
	dest_m->vmp_error = VMP_ERROR_GET(src_m); /* sliding src_m might have failed... */
	pmap_copy_page(VM_PAGE_GET_PHYS_PAGE(src_m), VM_PAGE_GET_PHYS_PAGE(dest_m));
}

#if MACH_ASSERT
static void
_vm_page_print(
	vm_page_t       p)
{
	printf("vm_page %p: \n", p);
	printf("  pageq: next=%p prev=%p\n",
	    (vm_page_t)VM_PAGE_UNPACK_PTR(p->vmp_pageq.next),
	    (vm_page_t)VM_PAGE_UNPACK_PTR(p->vmp_pageq.prev));
	printf("  listq: next=%p prev=%p\n",
	    (vm_page_t)(VM_PAGE_UNPACK_PTR(p->vmp_listq.next)),
	    (vm_page_t)(VM_PAGE_UNPACK_PTR(p->vmp_listq.prev)));
	printf("  next=%p\n", (vm_page_t)(VM_PAGE_UNPACK_PTR(p->vmp_next_m)));
	printf("  object=%p offset=0x%llx\n", VM_PAGE_OBJECT(p), p->vmp_offset);
	printf("  wire_count=%u\n", p->vmp_wire_count);
	printf("  q_state=%u\n", p->vmp_q_state);

	printf("  %slaundry, %sref, %sgobbled, %sprivate\n",
	    (p->vmp_laundry ? "" : "!"),
	    (p->vmp_reference ? "" : "!"),
	    (p->vmp_gobbled ? "" : "!"),
	    (p->vmp_private ? "" : "!"));
	printf("  %sbusy, %swanted, %stabled, %sfictitious, %spmapped, %swpmapped\n",
	    (p->vmp_busy ? "" : "!"),
	    (p->vmp_wanted ? "" : "!"),
	    (p->vmp_tabled ? "" : "!"),
	    (p->vmp_fictitious ? "" : "!"),
	    (p->vmp_pmapped ? "" : "!"),
	    (p->vmp_wpmapped ? "" : "!"));
	printf("  %sfree_when_done, %sabsent, %serror, %sdirty, %scleaning, %sprecious, %sclustered\n",
	    (p->vmp_free_when_done ? "" : "!"),
	    (p->vmp_absent ? "" : "!"),
	    (VMP_ERROR_GET(p) ? "" : "!"),
	    (p->vmp_dirty ? "" : "!"),
	    (p->vmp_cleaning ? "" : "!"),
	    (p->vmp_precious ? "" : "!"),
	    (p->vmp_clustered ? "" : "!"));
	printf("  %soverwriting, %srestart, %sunusual\n",
	    (p->vmp_overwriting ? "" : "!"),
	    (p->vmp_restart ? "" : "!"),
	    (p->vmp_unusual ? "" : "!"));
	printf("  cs_validated=%d, cs_tainted=%d, cs_nx=%d, %sno_cache\n",
	    p->vmp_cs_validated,
	    p->vmp_cs_tainted,
	    p->vmp_cs_nx,
	    (p->vmp_no_cache ? "" : "!"));

	printf("phys_page=0x%x\n", VM_PAGE_GET_PHYS_PAGE(p));
}

/*
 *	Check that the list of pages is ordered by
 *	ascending physical address and has no holes.
 */
static int
vm_page_verify_contiguous(
	vm_page_t       pages,
	unsigned int    npages)
{
	vm_page_t               m;
	unsigned int            page_count;
	vm_offset_t             prev_addr;

	prev_addr = VM_PAGE_GET_PHYS_PAGE(pages);
	page_count = 1;
	for (m = NEXT_PAGE(pages); m != VM_PAGE_NULL; m = NEXT_PAGE(m)) {
		if (VM_PAGE_GET_PHYS_PAGE(m) != prev_addr + 1) {
			printf("m %p prev_addr 0x%lx, current addr 0x%x\n",
			    m, (long)prev_addr, VM_PAGE_GET_PHYS_PAGE(m));
			printf("pages %p page_count %d npages %d\n", pages, page_count, npages);
			panic("vm_page_verify_contiguous:  not contiguous!");
		}
		prev_addr = VM_PAGE_GET_PHYS_PAGE(m);
		++page_count;
	}
	if (page_count != npages) {
		printf("pages %p actual count 0x%x but requested 0x%x\n",
		    pages, page_count, npages);
		panic("vm_page_verify_contiguous:  count error");
	}
	return 1;
}


/*
 *	Check the free lists for proper length etc.
 */
static boolean_t vm_page_verify_this_free_list_enabled = FALSE;
static unsigned int
vm_page_verify_free_list(
	vm_page_queue_head_t    *vm_page_queue,
	unsigned int    color,
	vm_page_t       look_for_page,
	boolean_t       expect_page)
{
	unsigned int    npages;
	vm_page_t       m;
	vm_page_t       prev_m;
	boolean_t       found_page;

	if (!vm_page_verify_this_free_list_enabled) {
		return 0;
	}

	found_page = FALSE;
	npages = 0;
	prev_m = (vm_page_t)((uintptr_t)vm_page_queue);

	vm_page_queue_iterate(vm_page_queue, m, vmp_pageq) {
		if (m == look_for_page) {
			found_page = TRUE;
		}
		if ((vm_page_t)VM_PAGE_UNPACK_PTR(m->vmp_pageq.prev) != prev_m) {
			panic("vm_page_verify_free_list(color=%u, npages=%u): page %p corrupted prev ptr %p instead of %p",
			    color, npages, m, (vm_page_t)VM_PAGE_UNPACK_PTR(m->vmp_pageq.prev), prev_m);
		}
		if (!m->vmp_busy) {
			panic("vm_page_verify_free_list(color=%u, npages=%u): page %p not busy",
			    color, npages, m);
		}
		if (color != (unsigned int) -1) {
			if (VM_PAGE_GET_COLOR(m) != color) {
				panic("vm_page_verify_free_list(color=%u, npages=%u): page %p wrong color %u instead of %u",
				    color, npages, m, VM_PAGE_GET_COLOR(m), color);
			}
			if (m->vmp_q_state != VM_PAGE_ON_FREE_Q) {
				panic("vm_page_verify_free_list(color=%u, npages=%u): page %p - expecting q_state == VM_PAGE_ON_FREE_Q, found %d",
				    color, npages, m, m->vmp_q_state);
			}
		} else {
			if (m->vmp_q_state != VM_PAGE_ON_FREE_LOCAL_Q) {
				panic("vm_page_verify_free_list(npages=%u): local page %p - expecting q_state == VM_PAGE_ON_FREE_LOCAL_Q, found %d",
				    npages, m, m->vmp_q_state);
			}
		}
		++npages;
		prev_m = m;
	}
	if (look_for_page != VM_PAGE_NULL) {
		unsigned int other_color;

		if (expect_page && !found_page) {
			printf("vm_page_verify_free_list(color=%u, npages=%u): page %p not found phys=%u\n",
			    color, npages, look_for_page, VM_PAGE_GET_PHYS_PAGE(look_for_page));
			_vm_page_print(look_for_page);
			for (other_color = 0;
			    other_color < vm_colors;
			    other_color++) {
				if (other_color == color) {
					continue;
				}
				vm_page_verify_free_list(&vm_page_queue_free[other_color].qhead,
				    other_color, look_for_page, FALSE);
			}
			if (color == (unsigned int) -1) {
				vm_page_verify_free_list(&vm_lopage_queue_free,
				    (unsigned int) -1, look_for_page, FALSE);
			}
			panic("vm_page_verify_free_list(color=%u)", color);
		}
		if (!expect_page && found_page) {
			printf("vm_page_verify_free_list(color=%u, npages=%u): page %p found phys=%u\n",
			    color, npages, look_for_page, VM_PAGE_GET_PHYS_PAGE(look_for_page));
		}
	}
	return npages;
}

static boolean_t vm_page_verify_all_free_lists_enabled = FALSE;
static void
vm_page_verify_free_lists( void )
{
	unsigned int    color, npages, nlopages;
	boolean_t       toggle = TRUE;

	if (!vm_page_verify_all_free_lists_enabled) {
		return;
	}

	npages = 0;

	vm_free_page_lock();

	if (vm_page_verify_this_free_list_enabled == TRUE) {
		/*
		 * This variable has been set globally for extra checking of
		 * each free list Q. Since we didn't set it, we don't own it
		 * and we shouldn't toggle it.
		 */
		toggle = FALSE;
	}

	if (toggle == TRUE) {
		vm_page_verify_this_free_list_enabled = TRUE;
	}

	for (color = 0; color < vm_colors; color++) {
		npages += vm_page_verify_free_list(&vm_page_queue_free[color].qhead,
		    color, VM_PAGE_NULL, FALSE);
	}
	nlopages = vm_page_verify_free_list(&vm_lopage_queue_free,
	    (unsigned int) -1,
	    VM_PAGE_NULL, FALSE);
	if (npages != vm_page_free_count || nlopages != vm_lopage_free_count) {
		panic("vm_page_verify_free_lists:  "
		    "npages %u free_count %d nlopages %u lo_free_count %u",
		    npages, vm_page_free_count, nlopages, vm_lopage_free_count);
	}

	if (toggle == TRUE) {
		vm_page_verify_this_free_list_enabled = FALSE;
	}

	vm_free_page_unlock();
}

#endif  /* MACH_ASSERT */

/*
 * wrapper for pmap_enter()
 */
kern_return_t
pmap_enter_check(
	pmap_t           pmap,
	vm_map_address_t virtual_address,
	vm_page_t        page,
	vm_prot_t        protection,
	vm_prot_t        fault_type,
	unsigned int     flags,
	boolean_t        wired)
{
	int             options = 0;
	vm_object_t     obj;

	if (VMP_ERROR_GET(page)) {
		return KERN_MEMORY_FAILURE;
	}
	obj = VM_PAGE_OBJECT(page);
	if (obj->internal) {
		options |= PMAP_OPTIONS_INTERNAL;
	}
	if (page->vmp_reusable || obj->all_reusable) {
		options |= PMAP_OPTIONS_REUSABLE;
	}
	return pmap_enter_options(pmap,
	           virtual_address,
	           VM_PAGE_GET_PHYS_PAGE(page),
	           protection,
	           fault_type,
	           flags,
	           wired,
	           options,
	           NULL,
	           PMAP_MAPPING_TYPE_INFER);
}


extern boolean_t(*volatile consider_buffer_cache_collect)(int);

/*
 *	CONTIGUOUS PAGE ALLOCATION AND HELPER FUNCTIONS
 */

/*
 * Helper function used to determine if a page can be relocated
 * A page is relocatable if it is in a stable non-transient state
 */
static inline boolean_t
vm_page_is_relocatable(vm_page_t m)
{

	if (VM_PAGE_WIRED(m) || m->vmp_gobbled || m->vmp_laundry || m->vmp_wanted ||
	    m->vmp_cleaning || m->vmp_overwriting || m->vmp_free_when_done) {
		/*
		 * Page is in a transient state
		 * or a state we don't want to deal with.
		 */
		return FALSE;
	} else if ((m->vmp_q_state == VM_PAGE_NOT_ON_Q) ||
	    (m->vmp_q_state == VM_PAGE_ON_FREE_LOCAL_Q) ||
	    (m->vmp_q_state == VM_PAGE_ON_FREE_LOPAGE_Q) ||
	    (m->vmp_q_state == VM_PAGE_ON_PAGEOUT_Q)) {
		/*
		 * Page needs to be on one of our queues (other then the pageout or special
		 * free queues) or it needs to belong to the compressor pool (which is now
		 * indicated by vmp_q_state == VM_PAGE_USED_BY_COMPRESSOR and falls out from
		 * the check for VM_PAGE_NOT_ON_Q) in order for it to be stable behind the
		 * locks we hold at this point...
		 */
		return FALSE;
	} else if ((m->vmp_q_state != VM_PAGE_ON_FREE_Q) &&
	    (!m->vmp_tabled || m->vmp_busy)) {
		/*
		 * pages on the free list are always 'busy'
		 * so we couldn't test for 'busy' in the check
		 * for the transient states... pages that are
		 * 'free' are never 'tabled', so we also couldn't
		 * test for 'tabled'.  So we check here to make
		 * sure that a non-free page is not busy and is
		 * tabled on an object...
		 */
		return FALSE;
	}
	return TRUE;
}

/*
 * Free up the given page by possibily relocating its contents to a new page
 * If the page is on an object the object lock must be held.
 */
static kern_return_t
vm_page_relocate(vm_page_t m1, int *compressed_pages)
{
	int refmod = 0;
	vm_object_t object = VM_PAGE_OBJECT(m1);
	kern_return_t kr;

	if (object == VM_OBJECT_NULL) {
		return KERN_FAILURE;
	}

	vm_object_lock_assert_held(object);

	if (VM_PAGE_WIRED(m1) ||
	    m1->vmp_gobbled ||
	    m1->vmp_laundry ||
	    m1->vmp_wanted ||
	    m1->vmp_cleaning ||
	    m1->vmp_overwriting ||
	    m1->vmp_free_when_done ||
	    m1->vmp_busy ||
	    m1->vmp_q_state == VM_PAGE_ON_PAGEOUT_Q) {
		return KERN_FAILURE;
	}

	boolean_t disconnected = FALSE;
	boolean_t reusable = FALSE;

	/*
	 * Pages from reusable objects can be reclaimed directly.
	 */
	if ((m1->vmp_reusable || object->all_reusable) &&
	    m1->vmp_q_state == VM_PAGE_ON_INACTIVE_INTERNAL_Q && !m1->vmp_dirty &&
	    !m1->vmp_reference) {
		/*
		 * reusable page...
		 */

		refmod = pmap_disconnect(VM_PAGE_GET_PHYS_PAGE(m1));
		disconnected = TRUE;
		if (refmod == 0) {
			/*
			 * ... not reused: can steal without relocating contents.
			 */
			reusable = TRUE;
		}
	}

	if ((m1->vmp_pmapped && !reusable) || m1->vmp_dirty || m1->vmp_precious) {
		vm_object_offset_t offset;

		/* page is not reusable, we need to allocate a new page
		 * and move its contents there.
		 */
		vm_page_t m2 = vm_page_grab_options(VM_PAGE_GRAB_Q_LOCK_HELD);

		if (m2 == VM_PAGE_NULL) {
			return KERN_RESOURCE_SHORTAGE;
		}

		if (!disconnected) {
			if (m1->vmp_pmapped) {
				refmod = pmap_disconnect(VM_PAGE_GET_PHYS_PAGE(m1));
			} else {
				refmod = 0;
			}
		}

		/* copy the page's contents */
		pmap_copy_page(VM_PAGE_GET_PHYS_PAGE(m1), VM_PAGE_GET_PHYS_PAGE(m2));

		/* copy the page's state */
		assert(!VM_PAGE_WIRED(m1));
		assert(m1->vmp_q_state != VM_PAGE_ON_FREE_Q);
		assert(m1->vmp_q_state != VM_PAGE_ON_PAGEOUT_Q);
		assert(!m1->vmp_laundry);
		m2->vmp_reference = m1->vmp_reference;
		assert(!m1->vmp_gobbled);
		assert(!m1->vmp_private);
		m2->vmp_no_cache = m1->vmp_no_cache;
		m2->vmp_xpmapped = 0;
		assert(!m1->vmp_busy);
		assert(!m1->vmp_wanted);
		assert(!m1->vmp_fictitious);
		m2->vmp_pmapped = m1->vmp_pmapped; /* should flush cache ? */
		m2->vmp_wpmapped = m1->vmp_wpmapped;
		assert(!m1->vmp_free_when_done);
		m2->vmp_absent = m1->vmp_absent;
		m2->vmp_error = VMP_ERROR_GET(m1);
		m2->vmp_dirty = m1->vmp_dirty;
		assert(!m1->vmp_cleaning);
		m2->vmp_precious = m1->vmp_precious;
		m2->vmp_clustered = m1->vmp_clustered;
		assert(!m1->vmp_overwriting);
		m2->vmp_restart = m1->vmp_restart;
		m2->vmp_unusual = m1->vmp_unusual;
		m2->vmp_cs_validated = m1->vmp_cs_validated;
		m2->vmp_cs_tainted = m1->vmp_cs_tainted;
		m2->vmp_cs_nx = m1->vmp_cs_nx;

		m2->vmp_realtime = m1->vmp_realtime;
		m1->vmp_realtime = false;

		/*
		 * If m1 had really been reusable,
		 * we would have just stolen it, so
		 * let's not propagate its "reusable"
		 * bit and assert that m2 is not
		 * marked as "reusable".
		 */
		// m2->vmp_reusable	= m1->vmp_reusable;
		assert(!m2->vmp_reusable);

		// assert(!m1->vmp_lopage);

		if (m1->vmp_q_state == VM_PAGE_USED_BY_COMPRESSOR) {
			m2->vmp_q_state = VM_PAGE_USED_BY_COMPRESSOR;
			/*
			 * We just grabbed m2 up above and so it isn't
			 * going to be on any special Q as yet and so
			 * we don't need to 'remove' it from the special
			 * queues. Just resetting the state should be enough.
			 */
			m2->vmp_on_specialq = VM_PAGE_SPECIAL_Q_EMPTY;
		}

		/*
		 * page may need to be flushed if
		 * it is marshalled into a UPL
		 * that is going to be used by a device
		 * that doesn't support coherency
		 */
		m2->vmp_written_by_kernel = TRUE;

		/*
		 * make sure we clear the ref/mod state
		 * from the pmap layer... else we risk
		 * inheriting state from the last time
		 * this page was used...
		 */
		pmap_clear_refmod(VM_PAGE_GET_PHYS_PAGE(m2),
		    VM_MEM_MODIFIED | VM_MEM_REFERENCED);

		if (refmod & VM_MEM_REFERENCED) {
			m2->vmp_reference = TRUE;
		}
		if (refmod & VM_MEM_MODIFIED) {
			SET_PAGE_DIRTY(m2, TRUE);
		}
		offset = m1->vmp_offset;

		/*
		 * completely cleans up the state
		 * of the page so that it is ready
		 * to be put onto the free list, or
		 * for this purpose it looks like it
		 * just came off of the free list
		 */
		vm_page_free_prepare(m1);

		/*
		 * now put the substitute page on the object
		 */
		vm_page_insert_internal(m2, object, offset, VM_KERN_MEMORY_NONE, TRUE,
		    TRUE, FALSE, FALSE, NULL);

		if (m2->vmp_q_state == VM_PAGE_USED_BY_COMPRESSOR) {
			m2->vmp_pmapped = TRUE;
			m2->vmp_wpmapped = TRUE;

			kr = pmap_enter_check(kernel_pmap, (vm_map_offset_t)m2->vmp_offset, m2,
			    VM_PROT_READ | VM_PROT_WRITE, VM_PROT_NONE, 0, TRUE);

			assert(kr == KERN_SUCCESS);

			if (compressed_pages) {
				++*compressed_pages;
			}
		} else {
			/* relocated page was not used by the compressor
			 * put it on either the active or inactive lists */
			if (m2->vmp_reference) {
				vm_page_activate(m2);
			} else {
				vm_page_deactivate(m2);
			}
		}

		/* unset the busy flag (pages on the free queue are busy) and notify if wanted */
		vm_page_wakeup_done(object, m2);

		return KERN_SUCCESS;
	} else {
		assert(m1->vmp_q_state != VM_PAGE_USED_BY_COMPRESSOR);

		/*
		 * completely cleans up the state
		 * of the page so that it is ready
		 * to be put onto the free list, or
		 * for this purpose it looks like it
		 * just came off of the free list
		 */
		vm_page_free_prepare(m1);

		/* we're done here */
		return KERN_SUCCESS;
	}

	return KERN_FAILURE;
}

/*
 *	CONTIGUOUS PAGE ALLOCATION
 *
 *	Find a region large enough to contain at least n pages
 *	of contiguous physical memory.
 *
 *	This is done by traversing the vm_page_t array in a linear fashion
 *	we assume that the vm_page_t array has the avaiable physical pages in an
 *	ordered, ascending list... this is currently true of all our implementations
 *      and must remain so... there can be 'holes' in the array...  we also can
 *	no longer tolerate the vm_page_t's in the list being 'freed' and reclaimed
 *      which use to happen via 'vm_page_convert'... that function was no longer
 *      being called and was removed...
 *
 *	The basic flow consists of stabilizing some of the interesting state of
 *	a vm_page_t behind the vm_page_queue and vm_page_free locks... we start our
 *	sweep at the beginning of the array looking for pages that meet our criterea
 *	for a 'stealable' page... currently we are pretty conservative... if the page
 *	meets this criterea and is physically contiguous to the previous page in the 'run'
 *      we keep developing it.  If we hit a page that doesn't fit, we reset our state
 *	and start to develop a new run... if at this point we've already considered
 *      at least MAX_CONSIDERED_BEFORE_YIELD pages, we'll drop the 2 locks we hold,
 *	and mutex_pause (which will yield the processor), to keep the latency low w/r
 *	to other threads trying to acquire free pages (or move pages from q to q),
 *	and then continue from the spot we left off... we only make 1 pass through the
 *	array.  Once we have a 'run' that is long enough, we'll go into the loop which
 *      which steals the pages from the queues they're currently on... pages on the free
 *	queue can be stolen directly... pages that are on any of the other queues
 *	must be removed from the object they are tabled on... this requires taking the
 *      object lock... we do this as a 'try' to prevent deadlocks... if the 'try' fails
 *	or if the state of the page behind the vm_object lock is no longer viable, we'll
 *	dump the pages we've currently stolen back to the free list, and pick up our
 *	scan from the point where we aborted the 'current' run.
 *
 *
 *	Requirements:
 *		- neither vm_page_queue nor vm_free_list lock can be held on entry
 *
 *	Returns a pointer to a list of gobbled/wired pages or VM_PAGE_NULL.
 *
 * Algorithm:
 */

#define MAX_CONSIDERED_BEFORE_YIELD     1000


#define RESET_STATE_OF_RUN()    \
	MACRO_BEGIN             \
	prevcontaddr = -2;      \
	start_pnum = -1;        \
	free_considered = 0;    \
	substitute_needed = 0;  \
	npages = 0;             \
	MACRO_END

/*
 * Can we steal in-use (i.e. not free) pages when searching for
 * physically-contiguous pages ?
 */
#define VM_PAGE_FIND_CONTIGUOUS_CAN_STEAL 1

static unsigned int vm_page_find_contiguous_last_idx = 0, vm_page_lomem_find_contiguous_last_idx = 0;
#if DEBUG
int vm_page_find_contig_debug = 0;
#endif

static vm_page_t
vm_page_find_contiguous(
	unsigned int    contig_pages,
	ppnum_t         max_pnum,
	ppnum_t         pnum_mask,
	boolean_t       wire,
	int             flags)
{
	vm_page_t       m = NULL;
	ppnum_t         prevcontaddr = 0;
	ppnum_t         start_pnum = 0;
	unsigned int    npages = 0, considered = 0, scanned = 0;
	unsigned int    page_idx = 0, start_idx = 0, last_idx = 0, orig_last_idx = 0;
	unsigned int    idx_last_contig_page_found = 0;
	int             free_considered = 0, free_available = 0;
	int             substitute_needed = 0;
	int             zone_gc_called = 0;
	boolean_t       wrapped;
	kern_return_t   kr;
#if DEBUG
	clock_sec_t     tv_start_sec = 0, tv_end_sec = 0;
	clock_usec_t    tv_start_usec = 0, tv_end_usec = 0;
#endif

	int             yielded = 0;
	int             dumped_run = 0;
	int             stolen_pages = 0;
	int             compressed_pages = 0;


	if (contig_pages == 0) {
		return VM_PAGE_NULL;
	}

full_scan_again:

#if MACH_ASSERT
	vm_page_verify_free_lists();
#endif
#if DEBUG
	clock_get_system_microtime(&tv_start_sec, &tv_start_usec);
#endif
	PAGE_REPLACEMENT_ALLOWED(TRUE);

	/*
	 * If there are still delayed pages, try to free up some that match.
	 */
	if (__improbable(vm_delayed_count != 0 && contig_pages != 0)) {
		vm_free_delayed_pages_contig(contig_pages, max_pnum, pnum_mask);
	}

	vm_page_lock_queues();
	vm_free_page_lock();

	RESET_STATE_OF_RUN();

	scanned = 0;
	considered = 0;
	free_available = vm_page_free_count - vm_page_free_reserved;

	wrapped = FALSE;

	if (flags & KMA_LOMEM) {
		idx_last_contig_page_found = vm_page_lomem_find_contiguous_last_idx;
	} else {
		idx_last_contig_page_found =  vm_page_find_contiguous_last_idx;
	}

	orig_last_idx = idx_last_contig_page_found;
	last_idx = orig_last_idx;

	for (page_idx = last_idx, start_idx = last_idx;
	    npages < contig_pages && page_idx < vm_pages_count;
	    page_idx++) {
retry:
		if (wrapped &&
		    npages == 0 &&
		    page_idx >= orig_last_idx) {
			/*
			 * We're back where we started and we haven't
			 * found any suitable contiguous range.  Let's
			 * give up.
			 */
			break;
		}
		scanned++;
		m = &vm_pages[page_idx];

		assert(!m->vmp_fictitious);
		assert(!m->vmp_private);

		if (max_pnum && VM_PAGE_GET_PHYS_PAGE(m) > max_pnum) {
			/* no more low pages... */
			break;
		}
		if (!npages & ((VM_PAGE_GET_PHYS_PAGE(m) & pnum_mask) != 0)) {
			/*
			 * not aligned
			 */
			RESET_STATE_OF_RUN();
		} else if (!vm_page_is_relocatable(m)) {
			/*
			 * page is not relocatable */
			RESET_STATE_OF_RUN();
		} else {
			if (VM_PAGE_GET_PHYS_PAGE(m) != prevcontaddr + 1) {
				if ((VM_PAGE_GET_PHYS_PAGE(m) & pnum_mask) != 0) {
					RESET_STATE_OF_RUN();
					goto did_consider;
				} else {
					npages = 1;
					start_idx = page_idx;
					start_pnum = VM_PAGE_GET_PHYS_PAGE(m);
				}
			} else {
				npages++;
			}
			prevcontaddr = VM_PAGE_GET_PHYS_PAGE(m);

			VM_PAGE_CHECK(m);
			if (m->vmp_q_state == VM_PAGE_ON_FREE_Q) {
				free_considered++;
			} else {
				/*
				 * This page is not free.
				 * If we can't steal used pages,
				 * we have to give up this run
				 * and keep looking.
				 * Otherwise, we might need to
				 * move the contents of this page
				 * into a substitute page.
				 */
#if VM_PAGE_FIND_CONTIGUOUS_CAN_STEAL
				if (m->vmp_pmapped || m->vmp_dirty || m->vmp_precious) {
					substitute_needed++;
				}
#else
				RESET_STATE_OF_RUN();
#endif
			}

			if ((free_considered + substitute_needed) > free_available) {
				/*
				 * if we let this run continue
				 * we will end up dropping the vm_page_free_count
				 * below the reserve limit... we need to abort
				 * this run, but we can at least re-consider this
				 * page... thus the jump back to 'retry'
				 */
				RESET_STATE_OF_RUN();

				if (free_available && considered <= MAX_CONSIDERED_BEFORE_YIELD) {
					considered++;
					goto retry;
				}
				/*
				 * free_available == 0
				 * so can't consider any free pages... if
				 * we went to retry in this case, we'd
				 * get stuck looking at the same page
				 * w/o making any forward progress
				 * we also want to take this path if we've already
				 * reached our limit that controls the lock latency
				 */
			}
		}
did_consider:
		if (considered > MAX_CONSIDERED_BEFORE_YIELD && npages <= 1) {
			PAGE_REPLACEMENT_ALLOWED(FALSE);

			vm_free_page_unlock();
			vm_page_unlock_queues();

			mutex_pause(0);

			PAGE_REPLACEMENT_ALLOWED(TRUE);

			vm_page_lock_queues();
			vm_free_page_lock();

			RESET_STATE_OF_RUN();
			/*
			 * reset our free page limit since we
			 * dropped the lock protecting the vm_page_free_queue
			 */
			free_available = vm_page_free_count - vm_page_free_reserved;
			considered = 0;

			yielded++;

			goto retry;
		}
		considered++;
	} /* main for-loop end */

	m = VM_PAGE_NULL;

	if (npages != contig_pages) {
		if (!wrapped) {
			/*
			 * We didn't find a contiguous range but we didn't
			 * start from the very first page.
			 * Start again from the very first page.
			 */
			RESET_STATE_OF_RUN();
			if (flags & KMA_LOMEM) {
				idx_last_contig_page_found  = vm_page_lomem_find_contiguous_last_idx = 0;
			} else {
				idx_last_contig_page_found = vm_page_find_contiguous_last_idx = 0;
			}
			last_idx = 0;
			page_idx = last_idx;
			wrapped = TRUE;
			goto retry;
		}
		vm_free_page_unlock();
	} else {
		vm_page_t m1;
		unsigned int cur_idx;
		unsigned int tmp_start_idx;
		vm_object_t locked_object = VM_OBJECT_NULL;
		boolean_t abort_run = FALSE;

		assert(page_idx - start_idx == contig_pages);

		tmp_start_idx = start_idx;

		/*
		 * first pass through to pull the free pages
		 * off of the free queue so that in case we
		 * need substitute pages, we won't grab any
		 * of the free pages in the run... we'll clear
		 * the 'free' bit in the 2nd pass, and even in
		 * an abort_run case, we'll collect all of the
		 * free pages in this run and return them to the free list
		 */
		while (start_idx < page_idx) {
			m1 = &vm_pages[start_idx++];

#if !VM_PAGE_FIND_CONTIGUOUS_CAN_STEAL
			assert(m1->vmp_q_state == VM_PAGE_ON_FREE_Q);
#endif

			if (m1->vmp_q_state == VM_PAGE_ON_FREE_Q) {
				unsigned int color;

				color = VM_PAGE_GET_COLOR(m1);
#if MACH_ASSERT
				vm_page_verify_free_list(&vm_page_queue_free[color].qhead, color, m1, TRUE);
#endif
				vm_page_queue_remove(&vm_page_queue_free[color].qhead, m1, vmp_pageq);

				VM_PAGE_ZERO_PAGEQ_ENTRY(m1);
#if MACH_ASSERT
				vm_page_verify_free_list(&vm_page_queue_free[color].qhead, color, VM_PAGE_NULL, FALSE);
#endif
				/*
				 * Clear the "free" bit so that this page
				 * does not get considered for another
				 * concurrent physically-contiguous allocation.
				 */
				m1->vmp_q_state = VM_PAGE_NOT_ON_Q;
				assert(m1->vmp_busy);

				vm_page_free_count--;
			}
		}
		if (flags & KMA_LOMEM) {
			vm_page_lomem_find_contiguous_last_idx = page_idx;
		} else {
			vm_page_find_contiguous_last_idx = page_idx;
		}

		/*
		 * we can drop the free queue lock at this point since
		 * we've pulled any 'free' candidates off of the list
		 * we need it dropped so that we can do a vm_page_grab
		 * when substituing for pmapped/dirty pages
		 */
		vm_free_page_unlock();

		start_idx = tmp_start_idx;
		cur_idx = page_idx - 1;

		while (start_idx++ < page_idx) {
			/*
			 * must go through the list from back to front
			 * so that the page list is created in the
			 * correct order - low -> high phys addresses
			 */
			m1 = &vm_pages[cur_idx--];

			if (m1->vmp_object == 0) {
				/*
				 * page has already been removed from
				 * the free list in the 1st pass
				 */
				assert(m1->vmp_q_state == VM_PAGE_NOT_ON_Q);
				assert(m1->vmp_offset == (vm_object_offset_t) -1);
				assert(m1->vmp_busy);
				assert(!m1->vmp_wanted);
				assert(!m1->vmp_laundry);
			} else {
				/*
				 * try to relocate/steal the page
				 */
				if (abort_run == TRUE) {
					continue;
				}

				assert(m1->vmp_q_state != VM_PAGE_NOT_ON_Q);

				vm_object_t object = VM_PAGE_OBJECT(m1);

				if (object != locked_object) {
					if (locked_object) {
						vm_object_unlock(locked_object);
						locked_object = VM_OBJECT_NULL;
					}
					if (vm_object_lock_try(object)) {
						locked_object = object;
					} else {
						/* object must be locked to relocate its pages */
						tmp_start_idx = cur_idx;
						abort_run = TRUE;
						continue;
					}
				}

				kr = vm_page_relocate(m1, &compressed_pages);
				if (kr != KERN_SUCCESS) {
					if (locked_object) {
						vm_object_unlock(locked_object);
						locked_object = VM_OBJECT_NULL;
					}
					tmp_start_idx = cur_idx;
					abort_run = TRUE;
					continue;
				}

				stolen_pages++;
			}

			/* m1 is ours at this point ... */

			if (m1->vmp_q_state != VM_PAGE_USED_BY_COMPRESSOR) {
				/*
				 * The Q state is preserved on m1 because vm_page_queues_remove doesn't
				 * change it for pages marked as used-by-compressor.
				 */
				vm_page_assign_special_state(m1, VM_PAGE_SPECIAL_Q_BG);
			}
			VM_PAGE_ZERO_PAGEQ_ENTRY(m1);
			m1->vmp_snext = m;
			m = m1;
		}

		if (locked_object) {
			vm_object_unlock(locked_object);
			locked_object = VM_OBJECT_NULL;
		}

		if (abort_run == TRUE) {
			/*
			 * want the index of the last
			 * page in this run that was
			 * successfully 'stolen', so back
			 * it up 1 for the auto-decrement on use
			 * and 1 more to bump back over this page
			 */
			page_idx = tmp_start_idx + 2;
			if (page_idx >= vm_pages_count) {
				if (wrapped) {
					if (m != VM_PAGE_NULL) {
						vm_page_unlock_queues();
						vm_page_free_list(m, FALSE);
						vm_page_lock_queues();
						m = VM_PAGE_NULL;
					}
					dumped_run++;
					goto done_scanning;
				}
				page_idx = last_idx = 0;
				wrapped = TRUE;
			}
			abort_run = FALSE;

			/*
			 * We didn't find a contiguous range but we didn't
			 * start from the very first page.
			 * Start again from the very first page.
			 */
			RESET_STATE_OF_RUN();

			if (flags & KMA_LOMEM) {
				idx_last_contig_page_found  = vm_page_lomem_find_contiguous_last_idx = page_idx;
			} else {
				idx_last_contig_page_found = vm_page_find_contiguous_last_idx = page_idx;
			}

			last_idx = page_idx;

			if (m != VM_PAGE_NULL) {
				vm_page_unlock_queues();
				vm_page_free_list(m, FALSE);
				vm_page_lock_queues();
				m = VM_PAGE_NULL;
			}
			dumped_run++;

			vm_free_page_lock();
			/*
			 * reset our free page limit since we
			 * dropped the lock protecting the vm_page_free_queue
			 */
			free_available = vm_page_free_count - vm_page_free_reserved;
			goto retry;
		}

		for (m1 = m; m1 != VM_PAGE_NULL; m1 = NEXT_PAGE(m1)) {
			assert(m1->vmp_q_state == VM_PAGE_NOT_ON_Q);
			assert(m1->vmp_wire_count == 0);

			if (wire == TRUE) {
				m1->vmp_wire_count++;
				m1->vmp_q_state = VM_PAGE_IS_WIRED;
			} else {
				m1->vmp_gobbled = TRUE;
			}
		}
		if (wire == FALSE) {
			vm_page_gobble_count += npages;
		}

		/*
		 * gobbled pages are also counted as wired pages
		 */
		vm_page_wire_count += npages;

		assert(vm_page_verify_contiguous(m, npages));
	}
done_scanning:
	PAGE_REPLACEMENT_ALLOWED(FALSE);

	vm_page_unlock_queues();

#if DEBUG
	clock_get_system_microtime(&tv_end_sec, &tv_end_usec);

	tv_end_sec -= tv_start_sec;
	if (tv_end_usec < tv_start_usec) {
		tv_end_sec--;
		tv_end_usec += 1000000;
	}
	tv_end_usec -= tv_start_usec;
	if (tv_end_usec >= 1000000) {
		tv_end_sec++;
		tv_end_sec -= 1000000;
	}
	if (vm_page_find_contig_debug) {
		printf("%s(num=%d,low=%d): found %d pages at 0x%llx in %ld.%06ds...  started at %d...  scanned %d pages...  yielded %d times...  dumped run %d times... stole %d pages... stole %d compressed pages\n",
		    __func__, contig_pages, max_pnum, npages, (vm_object_offset_t)start_pnum << PAGE_SHIFT,
		    (long)tv_end_sec, tv_end_usec, orig_last_idx,
		        scanned, yielded, dumped_run, stolen_pages, compressed_pages);
	}

#endif
#if MACH_ASSERT
	vm_page_verify_free_lists();
#endif
	if (m == NULL && zone_gc_called < 2) {
		printf("%s(num=%d,low=%d): found %d pages at 0x%llx...scanned %d pages...  yielded %d times...  dumped run %d times... stole %d pages... stole %d compressed pages... wired count is %d\n",
		    __func__, contig_pages, max_pnum, npages, (vm_object_offset_t)start_pnum << PAGE_SHIFT,
		        scanned, yielded, dumped_run, stolen_pages, compressed_pages, vm_page_wire_count);

		if (consider_buffer_cache_collect != NULL) {
			(void)(*consider_buffer_cache_collect)(1);
		}

		zone_gc(zone_gc_called ? ZONE_GC_DRAIN : ZONE_GC_TRIM);

		zone_gc_called++;

		printf("vm_page_find_contiguous: zone_gc called... wired count is %d\n", vm_page_wire_count);
		goto full_scan_again;
	}

	return m;
}

/*
 *	Allocate a list of contiguous, wired pages.
 */
kern_return_t
cpm_allocate(
	vm_size_t       size,
	vm_page_t       *list,
	ppnum_t         max_pnum,
	ppnum_t         pnum_mask,
	boolean_t       wire,
	int             flags)
{
	vm_page_t               pages;
	unsigned int            npages;

	if (size % PAGE_SIZE != 0) {
		return KERN_INVALID_ARGUMENT;
	}

	npages = (unsigned int) (size / PAGE_SIZE);
	if (npages != size / PAGE_SIZE) {
		/* 32-bit overflow */
		return KERN_INVALID_ARGUMENT;
	}

	/*
	 *	Obtain a pointer to a subset of the free
	 *	list large enough to satisfy the request;
	 *	the region will be physically contiguous.
	 */
	pages = vm_page_find_contiguous(npages, max_pnum, pnum_mask, wire, flags);

	if (pages == VM_PAGE_NULL) {
		return KERN_NO_SPACE;
	}
	/*
	 * determine need for wakeups
	 */
	if (vm_page_free_count < vm_page_free_min) {
		vm_free_page_lock();
		if (vm_pageout_running == FALSE) {
			vm_free_page_unlock();
			thread_wakeup((event_t) &vm_page_free_wanted);
		} else {
			vm_free_page_unlock();
		}
	}

	VM_CHECK_MEMORYSTATUS;

	/*
	 *	The CPM pages should now be available and
	 *	ordered by ascending physical address.
	 */
	assert(vm_page_verify_contiguous(pages, npages));

	if (flags & KMA_ZERO) {
		for (vm_page_t m = pages; m; m = NEXT_PAGE(m)) {
			vm_page_zero_fill(m);
		}
	}

	*list = pages;
	return KERN_SUCCESS;
}


unsigned int vm_max_delayed_work_limit = DEFAULT_DELAYED_WORK_LIMIT;

/*
 * when working on a 'run' of pages, it is necessary to hold
 * the vm_page_queue_lock (a hot global lock) for certain operations
 * on the page... however, the majority of the work can be done
 * while merely holding the object lock... in fact there are certain
 * collections of pages that don't require any work brokered by the
 * vm_page_queue_lock... to mitigate the time spent behind the global
 * lock, go to a 2 pass algorithm... collect pages up to DELAYED_WORK_LIMIT
 * while doing all of the work that doesn't require the vm_page_queue_lock...
 * then call vm_page_do_delayed_work to acquire the vm_page_queue_lock and do the
 * necessary work for each page... we will grab the busy bit on the page
 * if it's not already held so that vm_page_do_delayed_work can drop the object lock
 * if it can't immediately take the vm_page_queue_lock in order to compete
 * for the locks in the same order that vm_pageout_scan takes them.
 * the operation names are modeled after the names of the routines that
 * need to be called in order to make the changes very obvious in the
 * original loop
 */

void
vm_page_do_delayed_work(
	vm_object_t     object,
	vm_tag_t        tag,
	struct vm_page_delayed_work *dwp,
	int             dw_count)
{
	int             j;
	vm_page_t       m;
	vm_page_t       local_free_q = VM_PAGE_NULL;

	/*
	 * pageout_scan takes the vm_page_lock_queues first
	 * then tries for the object lock... to avoid what
	 * is effectively a lock inversion, we'll go to the
	 * trouble of taking them in that same order... otherwise
	 * if this object contains the majority of the pages resident
	 * in the UBC (or a small set of large objects actively being
	 * worked on contain the majority of the pages), we could
	 * cause the pageout_scan thread to 'starve' in its attempt
	 * to find pages to move to the free queue, since it has to
	 * successfully acquire the object lock of any candidate page
	 * before it can steal/clean it.
	 */
	if (!vm_page_trylock_queues()) {
		vm_object_unlock(object);

		/*
		 * "Turnstile enabled vm_pageout_scan" can be runnable
		 * for a very long time without getting on a core.
		 * If this is a higher priority thread it could be
		 * waiting here for a very long time respecting the fact
		 * that pageout_scan would like its object after VPS does
		 * a mutex_pause(0).
		 * So we cap the number of yields in the vm_object_lock_avoid()
		 * case to a single mutex_pause(0) which will give vm_pageout_scan
		 * 10us to run and grab the object if needed.
		 */
		vm_page_lock_queues();

		for (j = 0;; j++) {
			if ((!vm_object_lock_avoid(object) ||
			    (vps_dynamic_priority_enabled && (j > 0))) &&
			    _vm_object_lock_try(object)) {
				break;
			}
			vm_page_unlock_queues();
			mutex_pause(j);
			vm_page_lock_queues();
		}
	}
	for (j = 0; j < dw_count; j++, dwp++) {
		m = dwp->dw_m;

		if (dwp->dw_mask & DW_vm_pageout_throttle_up) {
			vm_pageout_throttle_up(m);
		}
#if CONFIG_PHANTOM_CACHE
		if (dwp->dw_mask & DW_vm_phantom_cache_update) {
			vm_phantom_cache_update(m);
		}
#endif
		if (dwp->dw_mask & DW_vm_page_wire) {
			vm_page_wire(m, tag, FALSE);
		} else if (dwp->dw_mask & DW_vm_page_unwire) {
			boolean_t       queueit;

			queueit = (dwp->dw_mask & (DW_vm_page_free | DW_vm_page_deactivate_internal)) ? FALSE : TRUE;

			vm_page_unwire(m, queueit);
		}
		if (dwp->dw_mask & DW_vm_page_free) {
			vm_page_free_prepare_queues(m);

			assert(m->vmp_pageq.next == 0 && m->vmp_pageq.prev == 0);
			/*
			 * Add this page to our list of reclaimed pages,
			 * to be freed later.
			 */
			m->vmp_snext = local_free_q;
			local_free_q = m;
		} else {
			if (dwp->dw_mask & DW_vm_page_deactivate_internal) {
				vm_page_deactivate_internal(m, FALSE);
			} else if (dwp->dw_mask & DW_vm_page_activate) {
				if (m->vmp_q_state != VM_PAGE_ON_ACTIVE_Q) {
					vm_page_activate(m);
				}
			} else if (dwp->dw_mask & DW_vm_page_speculate) {
				vm_page_speculate(m, TRUE);
			} else if (dwp->dw_mask & DW_enqueue_cleaned) {
				/*
				 * if we didn't hold the object lock and did this,
				 * we might disconnect the page, then someone might
				 * soft fault it back in, then we would put it on the
				 * cleaned queue, and so we would have a referenced (maybe even dirty)
				 * page on that queue, which we don't want
				 */
				int refmod_state = pmap_disconnect(VM_PAGE_GET_PHYS_PAGE(m));

				if ((refmod_state & VM_MEM_REFERENCED)) {
					/*
					 * this page has been touched since it got cleaned; let's activate it
					 * if it hasn't already been
					 */
					VM_PAGEOUT_DEBUG(vm_pageout_enqueued_cleaned, 1);
					VM_PAGEOUT_DEBUG(vm_pageout_cleaned_reactivated, 1);

					if (m->vmp_q_state != VM_PAGE_ON_ACTIVE_Q) {
						vm_page_activate(m);
					}
				} else {
					m->vmp_reference = FALSE;
					vm_page_enqueue_cleaned(m);
				}
			} else if (dwp->dw_mask & DW_vm_page_lru) {
				vm_page_lru(m);
			} else if (dwp->dw_mask & DW_VM_PAGE_QUEUES_REMOVE) {
				if (m->vmp_q_state != VM_PAGE_ON_PAGEOUT_Q) {
					vm_page_queues_remove(m, TRUE);
				}
			}
			if (dwp->dw_mask & DW_set_reference) {
				m->vmp_reference = TRUE;
			} else if (dwp->dw_mask & DW_clear_reference) {
				m->vmp_reference = FALSE;
			}

			if (dwp->dw_mask & DW_move_page) {
				if (m->vmp_q_state != VM_PAGE_ON_PAGEOUT_Q) {
					vm_page_queues_remove(m, FALSE);

					assert(!is_kernel_object(VM_PAGE_OBJECT(m)));

					vm_page_enqueue_inactive(m, FALSE);
				}
			}
			if (dwp->dw_mask & DW_clear_busy) {
				m->vmp_busy = FALSE;
			}

			if (dwp->dw_mask & DW_PAGE_WAKEUP) {
				vm_page_wakeup(object, m);
			}
		}
	}
	vm_page_unlock_queues();

	if (local_free_q) {
		vm_page_free_list(local_free_q, TRUE);
	}

	VM_CHECK_MEMORYSTATUS;
}

__abortlike
static void
__vm_page_alloc_list_failed_panic(
	vm_size_t       page_count,
	kma_flags_t     flags,
	kern_return_t   kr)
{
	panic("vm_page_alloc_list(%zd, 0x%x) failed unexpectedly with %d",
	    (size_t)page_count, flags, kr);
}

kern_return_t
vm_page_alloc_list(
	vm_size_t   page_count,
	kma_flags_t flags,
	vm_page_t  *list)
{
	vm_page_t       page_list = VM_PAGE_NULL;
	vm_page_t       mem;
	kern_return_t   kr = KERN_SUCCESS;
	int             page_grab_count = 0;
#if DEVELOPMENT || DEBUG
	task_t          task;
#endif /* DEVELOPMENT || DEBUG */

	for (vm_size_t i = 0; i < page_count; i++) {
		for (;;) {
			if (flags & KMA_LOMEM) {
				mem = vm_page_grablo();
			} else {
				uint_t options = VM_PAGE_GRAB_OPTIONS_NONE;
				mem = vm_page_grab_options(options);
			}

			if (mem != VM_PAGE_NULL) {
				break;
			}

			if (flags & KMA_NOPAGEWAIT) {
				kr = KERN_RESOURCE_SHORTAGE;
				goto out;
			}
			if ((flags & KMA_LOMEM) && (vm_lopage_needed == TRUE)) {
				kr = KERN_RESOURCE_SHORTAGE;
				goto out;
			}

			/* VM privileged threads should have waited in vm_page_grab() and not get here. */
			assert(!(current_thread()->options & TH_OPT_VMPRIV));

			if ((flags & KMA_NOFAIL) == 0 && ptoa_64(page_size) > max_mem / 4) {
				uint64_t unavailable = ptoa_64(vm_page_wire_count + vm_page_free_target);
				if (unavailable > max_mem || ptoa_64(page_count) > (max_mem - unavailable)) {
					kr = KERN_RESOURCE_SHORTAGE;
					goto out;
				}
			}
			VM_PAGE_WAIT();
		}

		page_grab_count++;
		mem->vmp_snext = page_list;
		page_list = mem;
	}

	if ((KMA_ZERO | KMA_NOENCRYPT) & flags) {
		for (mem = page_list; mem; mem = mem->vmp_snext) {
			vm_page_zero_fill(mem);
		}
	}

out:
#if DEBUG || DEVELOPMENT
	task = current_task_early();
	if (task != NULL) {
		ledger_credit(task->ledger, task_ledgers.pages_grabbed_kern, page_grab_count);
	}
#endif

	if (kr == KERN_SUCCESS) {
		*list = page_list;
	} else if (flags & KMA_NOFAIL) {
		__vm_page_alloc_list_failed_panic(page_count, flags, kr);
	} else {
		vm_page_free_list(page_list, FALSE);
	}

	return kr;
}

void
vm_page_set_offset(vm_page_t page, vm_object_offset_t offset)
{
	page->vmp_offset = offset;
}

vm_page_t
vm_page_get_next(vm_page_t page)
{
	return page->vmp_snext;
}

vm_object_offset_t
vm_page_get_offset(vm_page_t page)
{
	return page->vmp_offset;
}

ppnum_t
vm_page_get_phys_page(vm_page_t page)
{
	return VM_PAGE_GET_PHYS_PAGE(page);
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if HIBERNATION

static vm_page_t hibernate_gobble_queue;

static int  hibernate_drain_pageout_queue(struct vm_pageout_queue *);
static int  hibernate_flush_dirty_pages(int);
static int  hibernate_flush_queue(vm_page_queue_head_t *, int);

void hibernate_flush_wait(void);
void hibernate_mark_in_progress(void);
void hibernate_clear_in_progress(void);

void            hibernate_free_range(int, int);
void            hibernate_hash_insert_page(vm_page_t);
uint32_t        hibernate_mark_as_unneeded(addr64_t, addr64_t, hibernate_page_list_t *, hibernate_page_list_t *);
uint32_t        hibernate_teardown_vm_structs(hibernate_page_list_t *, hibernate_page_list_t *);
ppnum_t         hibernate_lookup_paddr(unsigned int);

struct hibernate_statistics {
	int hibernate_considered;
	int hibernate_reentered_on_q;
	int hibernate_found_dirty;
	int hibernate_skipped_cleaning;
	int hibernate_skipped_transient;
	int hibernate_skipped_precious;
	int hibernate_skipped_external;
	int hibernate_queue_nolock;
	int hibernate_queue_paused;
	int hibernate_throttled;
	int hibernate_throttle_timeout;
	int hibernate_drained;
	int hibernate_drain_timeout;
	int cd_lock_failed;
	int cd_found_precious;
	int cd_found_wired;
	int cd_found_busy;
	int cd_found_unusual;
	int cd_found_cleaning;
	int cd_found_laundry;
	int cd_found_dirty;
	int cd_found_xpmapped;
	int cd_skipped_xpmapped;
	int cd_local_free;
	int cd_total_free;
	int cd_vm_page_wire_count;
	int cd_vm_struct_pages_unneeded;
	int cd_pages;
	int cd_discarded;
	int cd_count_wire;
} hibernate_stats;

#if CONFIG_SPTM
/**
 * On SPTM-based systems don't save any executable pages into the hibernation
 * image. The SPTM has stronger guarantees around not allowing write access to
 * the executable pages than on older systems, which prevents XNU from being
 * able to restore any pages mapped as executable.
 */
#define HIBERNATE_XPMAPPED_LIMIT        0ULL
#else /* CONFIG_SPTM */
/*
 * clamp the number of 'xpmapped' pages we'll sweep into the hibernation image
 * so that we don't overrun the estimated image size, which would
 * result in a hibernation failure.
 *
 * We use a size value instead of pages because we don't want to take up more space
 * on disk if the system has a 16K page size vs 4K. Also, we are not guaranteed
 * to have that additional space available.
 *
 * Since this was set at 40000 pages on X86 we are going to use 160MB as our
 * xpmapped size.
 */
#define HIBERNATE_XPMAPPED_LIMIT        ((160 * 1024 * 1024ULL) / PAGE_SIZE)
#endif /* CONFIG_SPTM */

static int
hibernate_drain_pageout_queue(struct vm_pageout_queue *q)
{
	wait_result_t   wait_result;

	vm_page_lock_queues();

	while (!vm_page_queue_empty(&q->pgo_pending)) {
		q->pgo_draining = TRUE;

		assert_wait_timeout((event_t) (&q->pgo_laundry + 1), THREAD_INTERRUPTIBLE, 5000, 1000 * NSEC_PER_USEC);

		vm_page_unlock_queues();

		wait_result = thread_block(THREAD_CONTINUE_NULL);

		if (wait_result == THREAD_TIMED_OUT && !vm_page_queue_empty(&q->pgo_pending)) {
			hibernate_stats.hibernate_drain_timeout++;

			if (q == &vm_pageout_queue_external) {
				return 0;
			}

			return 1;
		}
		vm_page_lock_queues();

		hibernate_stats.hibernate_drained++;
	}
	vm_page_unlock_queues();

	return 0;
}


boolean_t hibernate_skip_external = FALSE;

static int
hibernate_flush_queue(vm_page_queue_head_t *q, int qcount)
{
	vm_page_t       m;
	vm_object_t     l_object = NULL;
	vm_object_t     m_object = NULL;
	int             refmod_state = 0;
	int             try_failed_count = 0;
	int             retval = 0;
	int             current_run = 0;
	struct  vm_pageout_queue *iq;
	struct  vm_pageout_queue *eq;
	struct  vm_pageout_queue *tq;

	KDBG(IOKDBG_CODE(DBG_HIBERNATE, 4) | DBG_FUNC_START,
	    VM_KERNEL_UNSLIDE_OR_PERM(q), qcount);

	iq = &vm_pageout_queue_internal;
	eq = &vm_pageout_queue_external;

	vm_page_lock_queues();

	while (qcount && !vm_page_queue_empty(q)) {
		if (current_run++ == 1000) {
			if (hibernate_should_abort()) {
				retval = 1;
				break;
			}
			current_run = 0;
		}

		m = (vm_page_t) vm_page_queue_first(q);
		m_object = VM_PAGE_OBJECT(m);

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
			/*
			 * Try to lock object; since we've alread got the
			 * page queues lock, we can only 'try' for this one.
			 * if the 'try' fails, we need to do a mutex_pause
			 * to allow the owner of the object lock a chance to
			 * run...
			 */
			if (!vm_object_lock_try_scan(m_object)) {
				if (try_failed_count > 20) {
					hibernate_stats.hibernate_queue_nolock++;

					goto reenter_pg_on_q;
				}

				vm_page_unlock_queues();
				mutex_pause(try_failed_count++);
				vm_page_lock_queues();

				hibernate_stats.hibernate_queue_paused++;
				continue;
			} else {
				l_object = m_object;
			}
		}
		if (!m_object->alive || m->vmp_cleaning || m->vmp_laundry || m->vmp_busy || m->vmp_absent || VMP_ERROR_GET(m)) {
			/*
			 * page is not to be cleaned
			 * put it back on the head of its queue
			 */
			if (m->vmp_cleaning) {
				hibernate_stats.hibernate_skipped_cleaning++;
			} else {
				hibernate_stats.hibernate_skipped_transient++;
			}

			goto reenter_pg_on_q;
		}
		if (m_object->vo_copy == VM_OBJECT_NULL) {
			if (m_object->purgable == VM_PURGABLE_VOLATILE || m_object->purgable == VM_PURGABLE_EMPTY) {
				/*
				 * let the normal hibernate image path
				 * deal with these
				 */
				goto reenter_pg_on_q;
			}
		}
		if (!m->vmp_dirty && m->vmp_pmapped) {
			refmod_state = pmap_get_refmod(VM_PAGE_GET_PHYS_PAGE(m));

			if ((refmod_state & VM_MEM_MODIFIED)) {
				SET_PAGE_DIRTY(m, FALSE);
			}
		} else {
			refmod_state = 0;
		}

		if (!m->vmp_dirty) {
			/*
			 * page is not to be cleaned
			 * put it back on the head of its queue
			 */
			if (m->vmp_precious) {
				hibernate_stats.hibernate_skipped_precious++;
			}

			goto reenter_pg_on_q;
		}

		if (hibernate_skip_external == TRUE && !m_object->internal) {
			hibernate_stats.hibernate_skipped_external++;

			goto reenter_pg_on_q;
		}
		tq = NULL;

		if (m_object->internal) {
			if (VM_PAGE_Q_THROTTLED(iq)) {
				tq = iq;
			}
		} else if (VM_PAGE_Q_THROTTLED(eq)) {
			tq = eq;
		}

		if (tq != NULL) {
			wait_result_t   wait_result;
			int             wait_count = 5;

			if (l_object != NULL) {
				vm_object_unlock(l_object);
				l_object = NULL;
			}

			while (retval == 0) {
				tq->pgo_throttled = TRUE;

				assert_wait_timeout((event_t) &tq->pgo_laundry, THREAD_INTERRUPTIBLE, 1000, 1000 * NSEC_PER_USEC);

				vm_page_unlock_queues();

				wait_result = thread_block(THREAD_CONTINUE_NULL);

				vm_page_lock_queues();

				if (wait_result != THREAD_TIMED_OUT) {
					break;
				}
				if (!VM_PAGE_Q_THROTTLED(tq)) {
					break;
				}

				if (hibernate_should_abort()) {
					retval = 1;
				}

				if (--wait_count == 0) {
					hibernate_stats.hibernate_throttle_timeout++;

					if (tq == eq) {
						hibernate_skip_external = TRUE;
						break;
					}
					retval = 1;
				}
			}
			if (retval) {
				break;
			}

			hibernate_stats.hibernate_throttled++;

			continue;
		}
		/*
		 * we've already factored out pages in the laundry which
		 * means this page can't be on the pageout queue so it's
		 * safe to do the vm_page_queues_remove
		 */
		vm_page_queues_remove(m, TRUE);

		if (m_object->internal == TRUE) {
			pmap_disconnect_options(VM_PAGE_GET_PHYS_PAGE(m), PMAP_OPTIONS_COMPRESSOR, NULL);
		}

		vm_pageout_cluster(m);

		hibernate_stats.hibernate_found_dirty++;

		goto next_pg;

reenter_pg_on_q:
		vm_page_queue_remove(q, m, vmp_pageq);
		vm_page_queue_enter(q, m, vmp_pageq);

		hibernate_stats.hibernate_reentered_on_q++;
next_pg:
		hibernate_stats.hibernate_considered++;

		qcount--;
		try_failed_count = 0;
	}
	if (l_object != NULL) {
		vm_object_unlock(l_object);
		l_object = NULL;
	}

	vm_page_unlock_queues();

	KERNEL_DEBUG_CONSTANT(IOKDBG_CODE(DBG_HIBERNATE, 4) | DBG_FUNC_END, hibernate_stats.hibernate_found_dirty, retval, 0, 0, 0);

	return retval;
}


static int
hibernate_flush_dirty_pages(int pass)
{
	struct vm_speculative_age_q     *aq;
	uint32_t        i;

	if (vm_page_local_q) {
		zpercpu_foreach_cpu(lid) {
			vm_page_reactivate_local(lid, TRUE, FALSE);
		}
	}

	for (i = 0; i <= vm_page_max_speculative_age_q; i++) {
		int             qcount;
		vm_page_t       m;

		aq = &vm_page_queue_speculative[i];

		if (vm_page_queue_empty(&aq->age_q)) {
			continue;
		}
		qcount = 0;

		vm_page_lockspin_queues();

		vm_page_queue_iterate(&aq->age_q, m, vmp_pageq) {
			qcount++;
		}
		vm_page_unlock_queues();

		if (qcount) {
			if (hibernate_flush_queue(&aq->age_q, qcount)) {
				return 1;
			}
		}
	}
	if (hibernate_flush_queue(&vm_page_queue_inactive, vm_page_inactive_count - vm_page_anonymous_count - vm_page_cleaned_count)) {
		return 1;
	}
	/* XXX FBDP TODO: flush secluded queue */
	if (hibernate_flush_queue(&vm_page_queue_anonymous, vm_page_anonymous_count)) {
		return 1;
	}
	if (hibernate_flush_queue(&vm_page_queue_cleaned, vm_page_cleaned_count)) {
		return 1;
	}
	if (hibernate_drain_pageout_queue(&vm_pageout_queue_internal)) {
		return 1;
	}

	if (pass == 1) {
		vm_compressor_record_warmup_start();
	}

	if (hibernate_flush_queue(&vm_page_queue_active, vm_page_active_count)) {
		if (pass == 1) {
			vm_compressor_record_warmup_end();
		}
		return 1;
	}
	if (hibernate_drain_pageout_queue(&vm_pageout_queue_internal)) {
		if (pass == 1) {
			vm_compressor_record_warmup_end();
		}
		return 1;
	}
	if (pass == 1) {
		vm_compressor_record_warmup_end();
	}

	if (hibernate_skip_external == FALSE && hibernate_drain_pageout_queue(&vm_pageout_queue_external)) {
		return 1;
	}

	return 0;
}


void
hibernate_reset_stats()
{
	bzero(&hibernate_stats, sizeof(struct hibernate_statistics));
}


int
hibernate_flush_memory()
{
	int     retval;

	assert(VM_CONFIG_COMPRESSOR_IS_PRESENT);

	KERNEL_DEBUG_CONSTANT(IOKDBG_CODE(DBG_HIBERNATE, 3) | DBG_FUNC_START, vm_page_free_count, 0, 0, 0, 0);

	hibernate_cleaning_in_progress = TRUE;
	hibernate_skip_external = FALSE;

	if ((retval = hibernate_flush_dirty_pages(1)) == 0) {
		KERNEL_DEBUG_CONSTANT(IOKDBG_CODE(DBG_HIBERNATE, 10) | DBG_FUNC_START, VM_PAGE_COMPRESSOR_COUNT, 0, 0, 0, 0);

		vm_compressor_flush();

		KERNEL_DEBUG_CONSTANT(IOKDBG_CODE(DBG_HIBERNATE, 10) | DBG_FUNC_END, VM_PAGE_COMPRESSOR_COUNT, 0, 0, 0, 0);

		if (consider_buffer_cache_collect != NULL) {
			unsigned int orig_wire_count;

			KERNEL_DEBUG_CONSTANT(IOKDBG_CODE(DBG_HIBERNATE, 7) | DBG_FUNC_START, 0, 0, 0, 0, 0);
			orig_wire_count = vm_page_wire_count;

			(void)(*consider_buffer_cache_collect)(1);
			zone_gc(ZONE_GC_DRAIN);

			HIBLOG("hibernate_flush_memory: buffer_cache_gc freed up %d wired pages\n", orig_wire_count - vm_page_wire_count);

			KERNEL_DEBUG_CONSTANT(IOKDBG_CODE(DBG_HIBERNATE, 7) | DBG_FUNC_END, orig_wire_count - vm_page_wire_count, 0, 0, 0, 0);
		}
	}
	hibernate_cleaning_in_progress = FALSE;

	KERNEL_DEBUG_CONSTANT(IOKDBG_CODE(DBG_HIBERNATE, 3) | DBG_FUNC_END, vm_page_free_count, hibernate_stats.hibernate_found_dirty, retval, 0, 0);

	if (retval) {
		HIBLOG("hibernate_flush_memory() failed to finish - vm_page_compressor_count(%d)\n", VM_PAGE_COMPRESSOR_COUNT);
	}


	HIBPRINT("hibernate_flush_memory() considered(%d) reentered_on_q(%d) found_dirty(%d)\n",
	    hibernate_stats.hibernate_considered,
	    hibernate_stats.hibernate_reentered_on_q,
	    hibernate_stats.hibernate_found_dirty);
	HIBPRINT("   skipped_cleaning(%d) skipped_transient(%d) skipped_precious(%d) skipped_external(%d) queue_nolock(%d)\n",
	    hibernate_stats.hibernate_skipped_cleaning,
	    hibernate_stats.hibernate_skipped_transient,
	    hibernate_stats.hibernate_skipped_precious,
	    hibernate_stats.hibernate_skipped_external,
	    hibernate_stats.hibernate_queue_nolock);
	HIBPRINT("   queue_paused(%d) throttled(%d) throttle_timeout(%d) drained(%d) drain_timeout(%d)\n",
	    hibernate_stats.hibernate_queue_paused,
	    hibernate_stats.hibernate_throttled,
	    hibernate_stats.hibernate_throttle_timeout,
	    hibernate_stats.hibernate_drained,
	    hibernate_stats.hibernate_drain_timeout);

	return retval;
}


static void
hibernate_page_list_zero(hibernate_page_list_t *list)
{
	uint32_t             bank;
	hibernate_bitmap_t * bitmap;

	bitmap = &list->bank_bitmap[0];
	for (bank = 0; bank < list->bank_count; bank++) {
		uint32_t last_bit;

		bzero((void *) &bitmap->bitmap[0], bitmap->bitmapwords << 2);
		// set out-of-bound bits at end of bitmap.
		last_bit = ((bitmap->last_page - bitmap->first_page + 1) & 31);
		if (last_bit) {
			bitmap->bitmap[bitmap->bitmapwords - 1] = (0xFFFFFFFF >> last_bit);
		}

		bitmap = (hibernate_bitmap_t *) &bitmap->bitmap[bitmap->bitmapwords];
	}
}

void
hibernate_free_gobble_pages(void)
{
	vm_page_t m, next;
	uint32_t  count = 0;

	m = (vm_page_t) hibernate_gobble_queue;
	while (m) {
		next = m->vmp_snext;
		vm_page_free(m);
		count++;
		m = next;
	}
	hibernate_gobble_queue = VM_PAGE_NULL;

	if (count) {
		HIBLOG("Freed %d pages\n", count);
	}
}

static boolean_t
hibernate_consider_discard(vm_page_t m, boolean_t preflight)
{
	vm_object_t object = NULL;
	int                  refmod_state;
	boolean_t            discard = FALSE;

	do{
		if (m->vmp_private) {
			panic("hibernate_consider_discard: private");
		}

		object = VM_PAGE_OBJECT(m);

		if (!vm_object_lock_try(object)) {
			object = NULL;
			if (!preflight) {
				hibernate_stats.cd_lock_failed++;
			}
			break;
		}
		if (VM_PAGE_WIRED(m)) {
			if (!preflight) {
				hibernate_stats.cd_found_wired++;
			}
			break;
		}
		if (m->vmp_precious) {
			if (!preflight) {
				hibernate_stats.cd_found_precious++;
			}
			break;
		}
		if (m->vmp_busy || !object->alive) {
			/*
			 *	Somebody is playing with this page.
			 */
			if (!preflight) {
				hibernate_stats.cd_found_busy++;
			}
			break;
		}
		if (m->vmp_absent || m->vmp_unusual || VMP_ERROR_GET(m)) {
			/*
			 * If it's unusual in anyway, ignore it
			 */
			if (!preflight) {
				hibernate_stats.cd_found_unusual++;
			}
			break;
		}
		if (m->vmp_cleaning) {
			if (!preflight) {
				hibernate_stats.cd_found_cleaning++;
			}
			break;
		}
		if (m->vmp_laundry) {
			if (!preflight) {
				hibernate_stats.cd_found_laundry++;
			}
			break;
		}
		if (!m->vmp_dirty) {
			refmod_state = pmap_get_refmod(VM_PAGE_GET_PHYS_PAGE(m));

			if (refmod_state & VM_MEM_REFERENCED) {
				m->vmp_reference = TRUE;
			}
			if (refmod_state & VM_MEM_MODIFIED) {
				SET_PAGE_DIRTY(m, FALSE);
			}
		}

		/*
		 * If it's clean or purgeable we can discard the page on wakeup.
		 */
		discard = (!m->vmp_dirty)
		    || (VM_PURGABLE_VOLATILE == object->purgable)
		    || (VM_PURGABLE_EMPTY == object->purgable);


		if (discard == FALSE) {
			if (!preflight) {
				hibernate_stats.cd_found_dirty++;
			}
		} else if (m->vmp_xpmapped && m->vmp_reference && !object->internal) {
			if (hibernate_stats.cd_found_xpmapped < HIBERNATE_XPMAPPED_LIMIT) {
				if (!preflight) {
					hibernate_stats.cd_found_xpmapped++;
				}
				discard = FALSE;
			} else {
				if (!preflight) {
					hibernate_stats.cd_skipped_xpmapped++;
				}
			}
		}
	}while (FALSE);

	if (object) {
		vm_object_unlock(object);
	}

	return discard;
}


static void
hibernate_discard_page(vm_page_t m)
{
	vm_object_t m_object;

	if (m->vmp_absent || m->vmp_unusual || VMP_ERROR_GET(m)) {
		/*
		 * If it's unusual in anyway, ignore
		 */
		return;
	}

	m_object = VM_PAGE_OBJECT(m);

#if MACH_ASSERT || DEBUG
	if (!vm_object_lock_try(m_object)) {
		panic("hibernate_discard_page(%p) !vm_object_lock_try", m);
	}
#else
	/* No need to lock page queue for token delete, hibernate_vm_unlock()
	 *  makes sure these locks are uncontended before sleep */
#endif /* MACH_ASSERT || DEBUG */

	if (m->vmp_pmapped == TRUE) {
		__unused int refmod_state = pmap_disconnect(VM_PAGE_GET_PHYS_PAGE(m));
	}

	if (m->vmp_laundry) {
		panic("hibernate_discard_page(%p) laundry", m);
	}
	if (m->vmp_private) {
		panic("hibernate_discard_page(%p) private", m);
	}
	if (m->vmp_fictitious) {
		panic("hibernate_discard_page(%p) fictitious", m);
	}

	if (VM_PURGABLE_VOLATILE == m_object->purgable) {
		/* object should be on a queue */
		assert((m_object->objq.next != NULL) && (m_object->objq.prev != NULL));
		purgeable_q_t old_queue = vm_purgeable_object_remove(m_object);
		assert(old_queue);
		if (m_object->purgeable_when_ripe) {
			vm_purgeable_token_delete_first(old_queue);
		}
		vm_object_lock_assert_exclusive(m_object);
		VM_OBJECT_SET_PURGABLE(m_object, VM_PURGABLE_EMPTY);

		/*
		 * Purgeable ledgers:  pages of VOLATILE and EMPTY objects are
		 * accounted in the "volatile" ledger, so no change here.
		 * We have to update vm_page_purgeable_count, though, since we're
		 * effectively purging this object.
		 */
		unsigned int delta;
		assert(m_object->resident_page_count >= m_object->wired_page_count);
		delta = (m_object->resident_page_count - m_object->wired_page_count);
		assert(vm_page_purgeable_count >= delta);
		assert(delta > 0);
		OSAddAtomic(-delta, (SInt32 *)&vm_page_purgeable_count);
	}

	vm_page_free(m);

#if MACH_ASSERT || DEBUG
	vm_object_unlock(m_object);
#endif  /* MACH_ASSERT || DEBUG */
}

/*
 *  Grab locks for hibernate_page_list_setall()
 */
void
hibernate_vm_lock_queues(void)
{
	vm_object_lock(compressor_object);
	vm_page_lock_queues();
	vm_free_page_lock();
	lck_mtx_lock(&vm_purgeable_queue_lock);

	if (vm_page_local_q) {
		zpercpu_foreach(lq, vm_page_local_q) {
			VPL_LOCK(&lq->vpl_lock);
		}
	}
}

void
hibernate_vm_unlock_queues(void)
{
	if (vm_page_local_q) {
		zpercpu_foreach(lq, vm_page_local_q) {
			VPL_UNLOCK(&lq->vpl_lock);
		}
	}
	lck_mtx_unlock(&vm_purgeable_queue_lock);
	vm_free_page_unlock();
	vm_page_unlock_queues();
	vm_object_unlock(compressor_object);
}

#if CONFIG_SPTM
static bool
hibernate_sptm_should_force_page_to_wired_pagelist(sptm_paddr_t paddr)
{
	const sptm_frame_type_t frame_type = sptm_get_frame_type(paddr);

	return frame_type == XNU_USER_JIT || frame_type == XNU_USER_DEBUG;
}
#endif

/*
 *  Bits zero in the bitmaps => page needs to be saved. All pages default to be saved,
 *  pages known to VM to not need saving are subtracted.
 *  Wired pages to be saved are present in page_list_wired, pageable in page_list.
 */

void
hibernate_page_list_setall(hibernate_page_list_t * page_list,
    hibernate_page_list_t * page_list_wired,
    hibernate_page_list_t * page_list_pal,
    boolean_t preflight,
    boolean_t will_discard,
    uint32_t * pagesOut)
{
	uint64_t start, end, nsec;
	vm_page_t m;
	vm_page_t next;
	uint32_t pages = page_list->page_count;
	uint32_t count_anonymous = 0, count_throttled = 0, count_compressor = 0;
	uint32_t count_inactive = 0, count_active = 0, count_speculative = 0, count_cleaned = 0;
	uint32_t count_wire = pages;
	uint32_t count_discard_active    = 0;
	uint32_t count_discard_inactive  = 0;
	uint32_t count_retired = 0;
	uint32_t count_discard_cleaned   = 0;
	uint32_t count_discard_purgeable = 0;
	uint32_t count_discard_speculative = 0;
	uint32_t count_discard_vm_struct_pages = 0;
	uint32_t i;
	uint32_t             bank;
	hibernate_bitmap_t * bitmap;
	hibernate_bitmap_t * bitmap_wired;
	boolean_t                    discard_all;
	boolean_t            discard = FALSE;

	HIBLOG("hibernate_page_list_setall(preflight %d) start\n", preflight);

	if (preflight) {
		page_list       = NULL;
		page_list_wired = NULL;
		page_list_pal   = NULL;
		discard_all     = FALSE;
	} else {
		discard_all     = will_discard;
	}

#if MACH_ASSERT || DEBUG
	if (!preflight) {
		assert(hibernate_vm_locks_are_safe());
		vm_page_lock_queues();
		if (vm_page_local_q) {
			zpercpu_foreach(lq, vm_page_local_q) {
				VPL_LOCK(&lq->vpl_lock);
			}
		}
	}
#endif  /* MACH_ASSERT || DEBUG */


	KERNEL_DEBUG_CONSTANT(IOKDBG_CODE(DBG_HIBERNATE, 8) | DBG_FUNC_START, count_wire, 0, 0, 0, 0);

	clock_get_uptime(&start);

	if (!preflight) {
		hibernate_page_list_zero(page_list);
		hibernate_page_list_zero(page_list_wired);
		hibernate_page_list_zero(page_list_pal);

		hibernate_stats.cd_vm_page_wire_count = vm_page_wire_count;
		hibernate_stats.cd_pages = pages;
	}

	if (vm_page_local_q) {
		zpercpu_foreach_cpu(lid) {
			vm_page_reactivate_local(lid, TRUE, !preflight);
		}
	}

	if (preflight) {
		vm_object_lock(compressor_object);
		vm_page_lock_queues();
		vm_free_page_lock();
	}

	LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);

	hibernation_vmqueues_inspection = TRUE;

	m = (vm_page_t) hibernate_gobble_queue;
	while (m) {
		pages--;
		count_wire--;
		if (!preflight) {
			hibernate_page_bitset(page_list, TRUE, VM_PAGE_GET_PHYS_PAGE(m));
			hibernate_page_bitset(page_list_wired, TRUE, VM_PAGE_GET_PHYS_PAGE(m));
		}
		m = m->vmp_snext;
	}

	if (!preflight) {
		percpu_foreach(free_pages_head, free_pages) {
			for (m = *free_pages_head; m; m = m->vmp_snext) {
				assert(m->vmp_q_state == VM_PAGE_ON_FREE_LOCAL_Q);

				pages--;
				count_wire--;
				hibernate_page_bitset(page_list, TRUE, VM_PAGE_GET_PHYS_PAGE(m));
				hibernate_page_bitset(page_list_wired, TRUE, VM_PAGE_GET_PHYS_PAGE(m));

				hibernate_stats.cd_local_free++;
				hibernate_stats.cd_total_free++;
			}
		}
	}

	for (i = 0; i < vm_colors; i++) {
		vm_page_queue_iterate(&vm_page_queue_free[i].qhead, m, vmp_pageq) {
			assert(m->vmp_q_state == VM_PAGE_ON_FREE_Q);

			pages--;
			count_wire--;
			if (!preflight) {
				hibernate_page_bitset(page_list, TRUE, VM_PAGE_GET_PHYS_PAGE(m));
				hibernate_page_bitset(page_list_wired, TRUE, VM_PAGE_GET_PHYS_PAGE(m));

				hibernate_stats.cd_total_free++;
			}
		}
	}

	vm_page_queue_iterate(&vm_lopage_queue_free, m, vmp_pageq) {
		assert(m->vmp_q_state == VM_PAGE_ON_FREE_LOPAGE_Q);

		pages--;
		count_wire--;
		if (!preflight) {
			hibernate_page_bitset(page_list, TRUE, VM_PAGE_GET_PHYS_PAGE(m));
			hibernate_page_bitset(page_list_wired, TRUE, VM_PAGE_GET_PHYS_PAGE(m));

			hibernate_stats.cd_total_free++;
		}
	}

	m = (vm_page_t) vm_page_queue_first(&vm_page_queue_throttled);
	while (m && !vm_page_queue_end(&vm_page_queue_throttled, (vm_page_queue_entry_t)m)) {
		assert(m->vmp_q_state == VM_PAGE_ON_THROTTLED_Q);

		next = (vm_page_t)VM_PAGE_UNPACK_PTR(m->vmp_pageq.next);
		discard = FALSE;
		if ((kIOHibernateModeDiscardCleanInactive & gIOHibernateMode)
		    && hibernate_consider_discard(m, preflight)) {
			if (!preflight) {
				hibernate_page_bitset(page_list, TRUE, VM_PAGE_GET_PHYS_PAGE(m));
			}
			count_discard_inactive++;
			discard = discard_all;
		} else {
			count_throttled++;
		}
		count_wire--;
		if (!preflight) {
			hibernate_page_bitset(page_list_wired, TRUE, VM_PAGE_GET_PHYS_PAGE(m));
		}

		if (discard) {
			hibernate_discard_page(m);
		}
		m = next;
	}

	m = (vm_page_t)vm_page_queue_first(&vm_page_queue_anonymous);
	while (m && !vm_page_queue_end(&vm_page_queue_anonymous, (vm_page_queue_entry_t)m)) {
		assert(m->vmp_q_state == VM_PAGE_ON_INACTIVE_INTERNAL_Q);
		bool force_to_wired_list = false;       /* Default to NOT forcing page into the wired page list */
#if CONFIG_SPTM
		force_to_wired_list = hibernate_sptm_should_force_page_to_wired_pagelist(ptoa_64(VM_PAGE_GET_PHYS_PAGE(m)));
#endif
		next = (vm_page_t)VM_PAGE_UNPACK_PTR(m->vmp_pageq.next);
		discard = FALSE;
		if ((kIOHibernateModeDiscardCleanInactive & gIOHibernateMode) &&
		    hibernate_consider_discard(m, preflight)) {
			if (!preflight) {
				hibernate_page_bitset(page_list, TRUE, VM_PAGE_GET_PHYS_PAGE(m));
			}
			if (m->vmp_dirty) {
				count_discard_purgeable++;
			} else {
				count_discard_inactive++;
			}
			discard = discard_all;
		} else {
			/*
			 * If the page must be force-added to the wired page list, prevent it from appearing
			 * in the unwired page list.
			 */
			if (force_to_wired_list) {
				if (!preflight) {
					hibernate_page_bitset(page_list, TRUE, VM_PAGE_GET_PHYS_PAGE(m));
				}
			} else {
				count_anonymous++;
			}
		}
		/*
		 * If the page is NOT being forced into the wired page list, remove it from the
		 * wired page list here.
		 */
		if (!force_to_wired_list) {
			count_wire--;
			if (!preflight) {
				hibernate_page_bitset(page_list_wired, TRUE, VM_PAGE_GET_PHYS_PAGE(m));
			}
		}
		if (discard) {
			hibernate_discard_page(m);
		}
		m = next;
	}

	m = (vm_page_t) vm_page_queue_first(&vm_page_queue_cleaned);
	while (m && !vm_page_queue_end(&vm_page_queue_cleaned, (vm_page_queue_entry_t)m)) {
		assert(m->vmp_q_state == VM_PAGE_ON_INACTIVE_CLEANED_Q);

		next = (vm_page_t)VM_PAGE_UNPACK_PTR(m->vmp_pageq.next);
		discard = FALSE;
		if ((kIOHibernateModeDiscardCleanInactive & gIOHibernateMode) &&
		    hibernate_consider_discard(m, preflight)) {
			if (!preflight) {
				hibernate_page_bitset(page_list, TRUE, VM_PAGE_GET_PHYS_PAGE(m));
			}
			if (m->vmp_dirty) {
				count_discard_purgeable++;
			} else {
				count_discard_cleaned++;
			}
			discard = discard_all;
		} else {
			count_cleaned++;
		}
		count_wire--;
		if (!preflight) {
			hibernate_page_bitset(page_list_wired, TRUE, VM_PAGE_GET_PHYS_PAGE(m));
		}
		if (discard) {
			hibernate_discard_page(m);
		}
		m = next;
	}

	m = (vm_page_t) vm_page_queue_first(&vm_page_queue_active);
	while (m && !vm_page_queue_end(&vm_page_queue_active, (vm_page_queue_entry_t)m)) {
		assert(m->vmp_q_state == VM_PAGE_ON_ACTIVE_Q);
		bool force_to_wired_list = false;       /* Default to NOT forcing page into the wired page list */
#if CONFIG_SPTM
		force_to_wired_list = hibernate_sptm_should_force_page_to_wired_pagelist(ptoa_64(VM_PAGE_GET_PHYS_PAGE(m)));
#endif
		next = (vm_page_t)VM_PAGE_UNPACK_PTR(m->vmp_pageq.next);
		discard = FALSE;
		if ((kIOHibernateModeDiscardCleanActive & gIOHibernateMode) &&
		    hibernate_consider_discard(m, preflight)) {
			if (!preflight) {
				hibernate_page_bitset(page_list, TRUE, VM_PAGE_GET_PHYS_PAGE(m));
			}
			if (m->vmp_dirty) {
				count_discard_purgeable++;
			} else {
				count_discard_active++;
			}
			discard = discard_all;
		} else {
			/*
			 * If the page must be force-added to the wired page list, prevent it from appearing
			 * in the unwired page list.
			 */
			if (force_to_wired_list) {
				if (!preflight) {
					hibernate_page_bitset(page_list, TRUE, VM_PAGE_GET_PHYS_PAGE(m));
				}
			} else {
				count_active++;
			}
		}
		/*
		 * If the page is NOT being forced into the wired page list, remove it from the
		 * wired page list here.
		 */
		if (!force_to_wired_list) {
			count_wire--;
			if (!preflight) {
				hibernate_page_bitset(page_list_wired, TRUE, VM_PAGE_GET_PHYS_PAGE(m));
			}
		}
		if (discard) {
			hibernate_discard_page(m);
		}
		m = next;
	}

	m = (vm_page_t) vm_page_queue_first(&vm_page_queue_inactive);
	while (m && !vm_page_queue_end(&vm_page_queue_inactive, (vm_page_queue_entry_t)m)) {
		assert(m->vmp_q_state == VM_PAGE_ON_INACTIVE_EXTERNAL_Q);
		bool force_to_wired_list = false;        /* Default to NOT forcing page into the wired page list */
#if CONFIG_SPTM
		force_to_wired_list = hibernate_sptm_should_force_page_to_wired_pagelist(ptoa_64(VM_PAGE_GET_PHYS_PAGE(m)));
#endif
		next = (vm_page_t)VM_PAGE_UNPACK_PTR(m->vmp_pageq.next);
		discard = FALSE;
		if ((kIOHibernateModeDiscardCleanInactive & gIOHibernateMode) &&
		    hibernate_consider_discard(m, preflight)) {
			if (!preflight) {
				hibernate_page_bitset(page_list, TRUE, VM_PAGE_GET_PHYS_PAGE(m));
			}
			if (m->vmp_dirty) {
				count_discard_purgeable++;
			} else {
				count_discard_inactive++;
			}
			discard = discard_all;
		} else {
			/*
			 * If the page must be force-added to the wired page list, prevent it from appearing
			 * in the unwired page list.
			 */
			if (force_to_wired_list) {
				if (!preflight) {
					hibernate_page_bitset(page_list, TRUE, VM_PAGE_GET_PHYS_PAGE(m));
				}
			} else {
				count_inactive++;
			}
		}
		/*
		 * If the page is NOT being forced into the wired page list, remove it from the
		 * wired page list here.
		 */
		if (!force_to_wired_list) {
			count_wire--;
			if (!preflight) {
				hibernate_page_bitset(page_list_wired, TRUE, VM_PAGE_GET_PHYS_PAGE(m));
			}
		}
		if (discard) {
			hibernate_discard_page(m);
		}
		m = next;
	}
	/* XXX FBDP TODO: secluded queue */

	for (i = 0; i <= vm_page_max_speculative_age_q; i++) {
		m = (vm_page_t) vm_page_queue_first(&vm_page_queue_speculative[i].age_q);
		while (m && !vm_page_queue_end(&vm_page_queue_speculative[i].age_q, (vm_page_queue_entry_t)m)) {
			assertf(m->vmp_q_state == VM_PAGE_ON_SPECULATIVE_Q,
			    "Bad page: %p (0x%x:0x%x) on queue %d has state: %d (Discard: %d, Preflight: %d)",
			    m, m->vmp_pageq.next, m->vmp_pageq.prev, i, m->vmp_q_state, discard, preflight);

			next = (vm_page_t)VM_PAGE_UNPACK_PTR(m->vmp_pageq.next);
			discard = FALSE;
			if ((kIOHibernateModeDiscardCleanInactive & gIOHibernateMode) &&
			    hibernate_consider_discard(m, preflight)) {
				if (!preflight) {
					hibernate_page_bitset(page_list, TRUE, VM_PAGE_GET_PHYS_PAGE(m));
				}
				count_discard_speculative++;
				discard = discard_all;
			} else {
				count_speculative++;
			}
			count_wire--;
			if (!preflight) {
				hibernate_page_bitset(page_list_wired, TRUE, VM_PAGE_GET_PHYS_PAGE(m));
			}
			if (discard) {
				hibernate_discard_page(m);
			}
			m = next;
		}
	}

	vm_page_queue_iterate(&compressor_object->memq, m, vmp_listq) {
		assert(m->vmp_q_state == VM_PAGE_USED_BY_COMPRESSOR);

		count_compressor++;
		count_wire--;
		if (!preflight) {
			hibernate_page_bitset(page_list_wired, TRUE, VM_PAGE_GET_PHYS_PAGE(m));
		}
	}


	if (preflight == FALSE && discard_all == TRUE) {
		KDBG(IOKDBG_CODE(DBG_HIBERNATE, 12) | DBG_FUNC_START);

		HIBLOG("hibernate_teardown started\n");
		count_discard_vm_struct_pages = hibernate_teardown_vm_structs(page_list, page_list_wired);
		HIBLOG("hibernate_teardown completed - discarded %d\n", count_discard_vm_struct_pages);

		pages -= count_discard_vm_struct_pages;
		count_wire -= count_discard_vm_struct_pages;

		hibernate_stats.cd_vm_struct_pages_unneeded = count_discard_vm_struct_pages;

		KDBG(IOKDBG_CODE(DBG_HIBERNATE, 12) | DBG_FUNC_END);
	}

	if (!preflight) {
		// pull wired from hibernate_bitmap
		bitmap = &page_list->bank_bitmap[0];
		bitmap_wired = &page_list_wired->bank_bitmap[0];
		for (bank = 0; bank < page_list->bank_count; bank++) {
			for (i = 0; i < bitmap->bitmapwords; i++) {
				bitmap->bitmap[i] = bitmap->bitmap[i] | ~bitmap_wired->bitmap[i];
			}
			bitmap = (hibernate_bitmap_t *)&bitmap->bitmap[bitmap->bitmapwords];
			bitmap_wired = (hibernate_bitmap_t *) &bitmap_wired->bitmap[bitmap_wired->bitmapwords];
		}
	}

	// machine dependent adjustments
	hibernate_page_list_setall_machine(page_list, page_list_wired, preflight, &pages);

	if (!preflight) {
		hibernate_stats.cd_count_wire = count_wire;
		hibernate_stats.cd_discarded = count_discard_active + count_discard_inactive + count_discard_purgeable +
		    count_discard_speculative + count_discard_cleaned + count_discard_vm_struct_pages;
	}

	clock_get_uptime(&end);
	absolutetime_to_nanoseconds(end - start, &nsec);
	HIBLOG("hibernate_page_list_setall time: %qd ms\n", nsec / 1000000ULL);

	HIBLOG("pages %d, wire %d, act %d, inact %d, cleaned %d spec %d, zf %d, throt %d, compr %d, xpmapped %d\n  %s discard act %d inact %d purgeable %d spec %d cleaned %d retired %d\n",
	    pages, count_wire, count_active, count_inactive, count_cleaned, count_speculative, count_anonymous, count_throttled, count_compressor, hibernate_stats.cd_found_xpmapped,
	    discard_all ? "did" : "could",
	    count_discard_active, count_discard_inactive, count_discard_purgeable, count_discard_speculative, count_discard_cleaned, count_retired);

	if (hibernate_stats.cd_skipped_xpmapped) {
		HIBLOG("WARNING: hibernate_page_list_setall skipped %d xpmapped pages\n", hibernate_stats.cd_skipped_xpmapped);
	}

	*pagesOut = pages - count_discard_active - count_discard_inactive - count_discard_purgeable - count_discard_speculative - count_discard_cleaned - count_retired;

	if (preflight && will_discard) {
		*pagesOut -= count_compressor + count_throttled + count_anonymous + count_inactive + count_cleaned + count_speculative + count_active;
		/*
		 * We try to keep max HIBERNATE_XPMAPPED_LIMIT pages around in the hibernation image
		 * even if these are clean and so we need to size the hibernation image accordingly.
		 *
		 * NB: We have to assume all HIBERNATE_XPMAPPED_LIMIT pages might show up because 'dirty'
		 * xpmapped pages aren't distinguishable from other 'dirty' pages in preflight. So we might
		 * only see part of the xpmapped pages if we look at 'cd_found_xpmapped' which solely tracks
		 * clean xpmapped pages.
		 *
		 * Since these pages are all cleaned by the time we are in the post-preflight phase, we might
		 * see a much larger number in 'cd_found_xpmapped' now than we did in the preflight phase
		 */
		*pagesOut +=  HIBERNATE_XPMAPPED_LIMIT;
	}

	hibernation_vmqueues_inspection = FALSE;

#if MACH_ASSERT || DEBUG
	if (!preflight) {
		if (vm_page_local_q) {
			zpercpu_foreach(lq, vm_page_local_q) {
				VPL_UNLOCK(&lq->vpl_lock);
			}
		}
		vm_page_unlock_queues();
	}
#endif  /* MACH_ASSERT || DEBUG */

	if (preflight) {
		vm_free_page_unlock();
		vm_page_unlock_queues();
		vm_object_unlock(compressor_object);
	}

	KERNEL_DEBUG_CONSTANT(IOKDBG_CODE(DBG_HIBERNATE, 8) | DBG_FUNC_END, count_wire, *pagesOut, 0, 0, 0);
}

void
hibernate_page_list_discard(hibernate_page_list_t * page_list)
{
	uint64_t  start, end, nsec;
	vm_page_t m;
	vm_page_t next;
	uint32_t  i;
	uint32_t  count_discard_active    = 0;
	uint32_t  count_discard_inactive  = 0;
	uint32_t  count_discard_purgeable = 0;
	uint32_t  count_discard_cleaned   = 0;
	uint32_t  count_discard_speculative = 0;


#if MACH_ASSERT || DEBUG
	vm_page_lock_queues();
	if (vm_page_local_q) {
		zpercpu_foreach(lq, vm_page_local_q) {
			VPL_LOCK(&lq->vpl_lock);
		}
	}
#endif  /* MACH_ASSERT || DEBUG */

	clock_get_uptime(&start);

	m = (vm_page_t) vm_page_queue_first(&vm_page_queue_anonymous);
	while (m && !vm_page_queue_end(&vm_page_queue_anonymous, (vm_page_queue_entry_t)m)) {
		assert(m->vmp_q_state == VM_PAGE_ON_INACTIVE_INTERNAL_Q);

		next = (vm_page_t) VM_PAGE_UNPACK_PTR(m->vmp_pageq.next);
		if (hibernate_page_bittst(page_list, VM_PAGE_GET_PHYS_PAGE(m))) {
			if (m->vmp_dirty) {
				count_discard_purgeable++;
			} else {
				count_discard_inactive++;
			}
			hibernate_discard_page(m);
		}
		m = next;
	}

	for (i = 0; i <= vm_page_max_speculative_age_q; i++) {
		m = (vm_page_t) vm_page_queue_first(&vm_page_queue_speculative[i].age_q);
		while (m && !vm_page_queue_end(&vm_page_queue_speculative[i].age_q, (vm_page_queue_entry_t)m)) {
			assert(m->vmp_q_state == VM_PAGE_ON_SPECULATIVE_Q);

			next = (vm_page_t) VM_PAGE_UNPACK_PTR(m->vmp_pageq.next);
			if (hibernate_page_bittst(page_list, VM_PAGE_GET_PHYS_PAGE(m))) {
				count_discard_speculative++;
				hibernate_discard_page(m);
			}
			m = next;
		}
	}

	m = (vm_page_t) vm_page_queue_first(&vm_page_queue_inactive);
	while (m && !vm_page_queue_end(&vm_page_queue_inactive, (vm_page_queue_entry_t)m)) {
		assert(m->vmp_q_state == VM_PAGE_ON_INACTIVE_EXTERNAL_Q);

		next = (vm_page_t) VM_PAGE_UNPACK_PTR(m->vmp_pageq.next);
		if (hibernate_page_bittst(page_list, VM_PAGE_GET_PHYS_PAGE(m))) {
			if (m->vmp_dirty) {
				count_discard_purgeable++;
			} else {
				count_discard_inactive++;
			}
			hibernate_discard_page(m);
		}
		m = next;
	}
	/* XXX FBDP TODO: secluded queue */

	m = (vm_page_t) vm_page_queue_first(&vm_page_queue_active);
	while (m && !vm_page_queue_end(&vm_page_queue_active, (vm_page_queue_entry_t)m)) {
		assert(m->vmp_q_state == VM_PAGE_ON_ACTIVE_Q);

		next = (vm_page_t) VM_PAGE_UNPACK_PTR(m->vmp_pageq.next);
		if (hibernate_page_bittst(page_list, VM_PAGE_GET_PHYS_PAGE(m))) {
			if (m->vmp_dirty) {
				count_discard_purgeable++;
			} else {
				count_discard_active++;
			}
			hibernate_discard_page(m);
		}
		m = next;
	}

	m = (vm_page_t) vm_page_queue_first(&vm_page_queue_cleaned);
	while (m && !vm_page_queue_end(&vm_page_queue_cleaned, (vm_page_queue_entry_t)m)) {
		assert(m->vmp_q_state == VM_PAGE_ON_INACTIVE_CLEANED_Q);

		next = (vm_page_t) VM_PAGE_UNPACK_PTR(m->vmp_pageq.next);
		if (hibernate_page_bittst(page_list, VM_PAGE_GET_PHYS_PAGE(m))) {
			if (m->vmp_dirty) {
				count_discard_purgeable++;
			} else {
				count_discard_cleaned++;
			}
			hibernate_discard_page(m);
		}
		m = next;
	}

#if MACH_ASSERT || DEBUG
	if (vm_page_local_q) {
		zpercpu_foreach(lq, vm_page_local_q) {
			VPL_UNLOCK(&lq->vpl_lock);
		}
	}
	vm_page_unlock_queues();
#endif  /* MACH_ASSERT || DEBUG */

	clock_get_uptime(&end);
	absolutetime_to_nanoseconds(end - start, &nsec);
	HIBLOG("hibernate_page_list_discard time: %qd ms, discarded act %d inact %d purgeable %d spec %d cleaned %d\n",
	    nsec / 1000000ULL,
	    count_discard_active, count_discard_inactive, count_discard_purgeable, count_discard_speculative, count_discard_cleaned);
}

boolean_t       hibernate_paddr_map_inited = FALSE;
unsigned int    hibernate_teardown_last_valid_compact_indx = -1;
vm_page_t       hibernate_rebuild_hash_list = NULL;

unsigned int    hibernate_teardown_found_tabled_pages = 0;
unsigned int    hibernate_teardown_found_created_pages = 0;
unsigned int    hibernate_teardown_found_free_pages = 0;
unsigned int    hibernate_teardown_vm_page_free_count;


struct ppnum_mapping {
	struct ppnum_mapping    *ppnm_next;
	ppnum_t                 ppnm_base_paddr;
	unsigned int            ppnm_sindx;
	unsigned int            ppnm_eindx;
};

struct ppnum_mapping    *ppnm_head;
struct ppnum_mapping    *ppnm_last_found = NULL;


void
hibernate_create_paddr_map(void)
{
	unsigned int    i;
	ppnum_t         next_ppnum_in_run = 0;
	struct ppnum_mapping *ppnm = NULL;

	if (hibernate_paddr_map_inited == FALSE) {
		for (i = 0; i < vm_pages_count; i++) {
			if (ppnm) {
				ppnm->ppnm_eindx = i;
			}

			if (ppnm == NULL || VM_PAGE_GET_PHYS_PAGE(&vm_pages[i]) != next_ppnum_in_run) {
				ppnm = zalloc_permanent_type(struct ppnum_mapping);

				ppnm->ppnm_next = ppnm_head;
				ppnm_head = ppnm;

				ppnm->ppnm_sindx = i;
				ppnm->ppnm_base_paddr = VM_PAGE_GET_PHYS_PAGE(&vm_pages[i]);
			}
			next_ppnum_in_run = VM_PAGE_GET_PHYS_PAGE(&vm_pages[i]) + 1;
		}
		ppnm->ppnm_eindx = vm_pages_count;

		hibernate_paddr_map_inited = TRUE;
	}
}

ppnum_t
hibernate_lookup_paddr(unsigned int indx)
{
	struct ppnum_mapping *ppnm = NULL;

	ppnm = ppnm_last_found;

	if (ppnm) {
		if (indx >= ppnm->ppnm_sindx && indx < ppnm->ppnm_eindx) {
			goto done;
		}
	}
	for (ppnm = ppnm_head; ppnm; ppnm = ppnm->ppnm_next) {
		if (indx >= ppnm->ppnm_sindx && indx < ppnm->ppnm_eindx) {
			ppnm_last_found = ppnm;
			break;
		}
	}
	if (ppnm == NULL) {
		panic("hibernate_lookup_paddr of %d failed", indx);
	}
done:
	return ppnm->ppnm_base_paddr + (indx - ppnm->ppnm_sindx);
}


uint32_t
hibernate_mark_as_unneeded(addr64_t saddr, addr64_t eaddr, hibernate_page_list_t *page_list, hibernate_page_list_t *page_list_wired)
{
	addr64_t        saddr_aligned;
	addr64_t        eaddr_aligned;
	addr64_t        addr;
	ppnum_t         paddr;
	unsigned int    mark_as_unneeded_pages = 0;

	saddr_aligned = (saddr + PAGE_MASK_64) & ~PAGE_MASK_64;
	eaddr_aligned = eaddr & ~PAGE_MASK_64;

	for (addr = saddr_aligned; addr < eaddr_aligned; addr += PAGE_SIZE_64) {
		paddr = pmap_find_phys(kernel_pmap, addr);

		assert(paddr);

		hibernate_page_bitset(page_list, TRUE, paddr);
		hibernate_page_bitset(page_list_wired, TRUE, paddr);

		mark_as_unneeded_pages++;
	}
	return mark_as_unneeded_pages;
}


void
hibernate_hash_insert_page(vm_page_t mem)
{
	vm_page_bucket_t *bucket;
	int             hash_id;
	vm_object_t     m_object;

	m_object = VM_PAGE_OBJECT(mem);

	assert(mem->vmp_hashed);
	assert(m_object);
	assert(mem->vmp_offset != (vm_object_offset_t) -1);

	/*
	 *	Insert it into the object_object/offset hash table
	 */
	hash_id = vm_page_hash(m_object, mem->vmp_offset);
	bucket = &vm_page_buckets[hash_id];

	mem->vmp_next_m = bucket->page_list;
	bucket->page_list = VM_PAGE_PACK_PTR(mem);
}


void
hibernate_free_range(int sindx, int eindx)
{
	vm_page_t       mem;
	unsigned int    color;

	while (sindx < eindx) {
		mem = &vm_pages[sindx];

		vm_page_init(mem, hibernate_lookup_paddr(sindx), FALSE);

		mem->vmp_lopage = FALSE;
		mem->vmp_q_state = VM_PAGE_ON_FREE_Q;

		color = VM_PAGE_GET_COLOR(mem);
#if defined(__x86_64__)
		vm_page_queue_enter_clump(&vm_page_queue_free[color].qhead, mem);
#else
		vm_page_queue_enter(&vm_page_queue_free[color].qhead, mem, vmp_pageq);
#endif
		vm_page_free_count++;

		sindx++;
	}
}

void
hibernate_rebuild_vm_structs(void)
{
	int             i, cindx, sindx, eindx;
	vm_page_t       mem, tmem, mem_next;
	AbsoluteTime    startTime, endTime;
	uint64_t        nsec;

	if (hibernate_rebuild_needed == FALSE) {
		return;
	}

	KDBG(IOKDBG_CODE(DBG_HIBERNATE, 13) | DBG_FUNC_START);
	HIBLOG("hibernate_rebuild started\n");

	clock_get_uptime(&startTime);

	pal_hib_rebuild_pmap_structs();

	bzero(&vm_page_buckets[0], vm_page_bucket_count * sizeof(vm_page_bucket_t));
	eindx = vm_pages_count;

	/*
	 * Mark all the vm_pages[] that have not been initialized yet as being
	 * transient. This is needed to ensure that buddy page search is corrrect.
	 * Without this random data in these vm_pages[] can trip the buddy search
	 */
	for (i = hibernate_teardown_last_valid_compact_indx + 1; i < eindx; ++i) {
		vm_pages[i].vmp_q_state = VM_PAGE_NOT_ON_Q;
	}

	for (cindx = hibernate_teardown_last_valid_compact_indx; cindx >= 0; cindx--) {
		mem = &vm_pages[cindx];
		assert(mem->vmp_q_state != VM_PAGE_ON_FREE_Q);
		/*
		 * hibernate_teardown_vm_structs leaves the location where
		 * this vm_page_t must be located in "next".
		 */
		tmem = (vm_page_t)(VM_PAGE_UNPACK_PTR(mem->vmp_next_m));
		mem->vmp_next_m = VM_PAGE_PACK_PTR(NULL);

		sindx = (int)(tmem - &vm_pages[0]);

		if (mem != tmem) {
			/*
			 * this vm_page_t was moved by hibernate_teardown_vm_structs,
			 * so move it back to its real location
			 */
			*tmem = *mem;
			mem = tmem;
		}
		if (mem->vmp_hashed) {
			hibernate_hash_insert_page(mem);
		}
		/*
		 * the 'hole' between this vm_page_t and the previous
		 * vm_page_t we moved needs to be initialized as
		 * a range of free vm_page_t's
		 */
		hibernate_free_range(sindx + 1, eindx);

		eindx = sindx;
	}
	if (sindx) {
		hibernate_free_range(0, sindx);
	}

	assert(vm_page_free_count == hibernate_teardown_vm_page_free_count);

	/*
	 * process the list of vm_page_t's that were entered in the hash,
	 * but were not located in the vm_pages arrary... these are
	 * vm_page_t's that were created on the fly (i.e. fictitious)
	 */
	for (mem = hibernate_rebuild_hash_list; mem; mem = mem_next) {
		mem_next = (vm_page_t)(VM_PAGE_UNPACK_PTR(mem->vmp_next_m));

		mem->vmp_next_m = 0;
		hibernate_hash_insert_page(mem);
	}
	hibernate_rebuild_hash_list = NULL;

	clock_get_uptime(&endTime);
	SUB_ABSOLUTETIME(&endTime, &startTime);
	absolutetime_to_nanoseconds(endTime, &nsec);

	HIBLOG("hibernate_rebuild completed - took %qd msecs\n", nsec / 1000000ULL);

	hibernate_rebuild_needed = FALSE;

	KDBG(IOKDBG_CODE(DBG_HIBERNATE, 13) | DBG_FUNC_END);
}

uint32_t
hibernate_teardown_vm_structs(hibernate_page_list_t *page_list, hibernate_page_list_t *page_list_wired)
{
	unsigned int    i;
	unsigned int    compact_target_indx;
	vm_page_t       mem, mem_next;
	vm_page_bucket_t *bucket;
	unsigned int    mark_as_unneeded_pages = 0;
	unsigned int    unneeded_vm_page_bucket_pages = 0;
	unsigned int    unneeded_vm_pages_pages = 0;
	unsigned int    unneeded_pmap_pages = 0;
	addr64_t        start_of_unneeded = 0;
	addr64_t        end_of_unneeded = 0;


	if (hibernate_should_abort()) {
		return 0;
	}

	hibernate_rebuild_needed = TRUE;

	HIBLOG("hibernate_teardown: wired_pages %d, free_pages %d, active_pages %d, inactive_pages %d, speculative_pages %d, cleaned_pages %d, compressor_pages %d\n",
	    vm_page_wire_count, vm_page_free_count, vm_page_active_count, vm_page_inactive_count, vm_page_speculative_count,
	    vm_page_cleaned_count, compressor_object->resident_page_count);

	for (i = 0; i < vm_page_bucket_count; i++) {
		bucket = &vm_page_buckets[i];

		for (mem = (vm_page_t)(VM_PAGE_UNPACK_PTR(bucket->page_list)); mem != VM_PAGE_NULL; mem = mem_next) {
			assert(mem->vmp_hashed);

			mem_next = (vm_page_t)(VM_PAGE_UNPACK_PTR(mem->vmp_next_m));

			if (mem < &vm_pages[0] || mem >= &vm_pages[vm_pages_count]) {
				mem->vmp_next_m = VM_PAGE_PACK_PTR(hibernate_rebuild_hash_list);
				hibernate_rebuild_hash_list = mem;
			}
		}
	}
	unneeded_vm_page_bucket_pages = hibernate_mark_as_unneeded((addr64_t)&vm_page_buckets[0], (addr64_t)&vm_page_buckets[vm_page_bucket_count], page_list, page_list_wired);
	mark_as_unneeded_pages += unneeded_vm_page_bucket_pages;

	hibernate_teardown_vm_page_free_count = vm_page_free_count;

	compact_target_indx = 0;

	for (i = 0; i < vm_pages_count; i++) {
		mem = &vm_pages[i];

		if (mem->vmp_q_state == VM_PAGE_ON_FREE_Q) {
			unsigned int color;

			assert(mem->vmp_busy);
			assert(!mem->vmp_lopage);

			color = VM_PAGE_GET_COLOR(mem);

			vm_page_queue_remove(&vm_page_queue_free[color].qhead, mem, vmp_pageq);

			VM_PAGE_ZERO_PAGEQ_ENTRY(mem);

			vm_page_free_count--;

			hibernate_teardown_found_free_pages++;

			if (vm_pages[compact_target_indx].vmp_q_state != VM_PAGE_ON_FREE_Q) {
				compact_target_indx = i;
			}
		} else {
			/*
			 * record this vm_page_t's original location
			 * we need this even if it doesn't get moved
			 * as an indicator to the rebuild function that
			 * we don't have to move it
			 */
			mem->vmp_next_m = VM_PAGE_PACK_PTR(mem);

			if (vm_pages[compact_target_indx].vmp_q_state == VM_PAGE_ON_FREE_Q) {
				/*
				 * we've got a hole to fill, so
				 * move this vm_page_t to it's new home
				 */
				vm_pages[compact_target_indx] = *mem;
				mem->vmp_q_state = VM_PAGE_ON_FREE_Q;

				hibernate_teardown_last_valid_compact_indx = compact_target_indx;
				compact_target_indx++;
			} else {
				hibernate_teardown_last_valid_compact_indx = i;
			}
		}
	}
	unneeded_vm_pages_pages = hibernate_mark_as_unneeded((addr64_t)&vm_pages[hibernate_teardown_last_valid_compact_indx + 1],
	    (addr64_t)&vm_pages[vm_pages_count - 1], page_list, page_list_wired);
	mark_as_unneeded_pages += unneeded_vm_pages_pages;

	pal_hib_teardown_pmap_structs(&start_of_unneeded, &end_of_unneeded);

	if (start_of_unneeded) {
		unneeded_pmap_pages = hibernate_mark_as_unneeded(start_of_unneeded, end_of_unneeded, page_list, page_list_wired);
		mark_as_unneeded_pages += unneeded_pmap_pages;
	}
	HIBLOG("hibernate_teardown: mark_as_unneeded_pages %d, %d, %d\n", unneeded_vm_page_bucket_pages, unneeded_vm_pages_pages, unneeded_pmap_pages);

	return mark_as_unneeded_pages;
}


#endif /* HIBERNATION */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <mach_vm_debug.h>
#if     MACH_VM_DEBUG

#include <mach_debug/hash_info.h>
#include <vm/vm_debug_internal.h>

/*
 *	Routine:	vm_page_info
 *	Purpose:
 *		Return information about the global VP table.
 *		Fills the buffer with as much information as possible
 *		and returns the desired size of the buffer.
 *	Conditions:
 *		Nothing locked.  The caller should provide
 *		possibly-pageable memory.
 */

unsigned int
vm_page_info(
	hash_info_bucket_t *info,
	unsigned int count)
{
	unsigned int i;
	lck_spin_t      *bucket_lock;

	if (vm_page_bucket_count < count) {
		count = vm_page_bucket_count;
	}

	for (i = 0; i < count; i++) {
		vm_page_bucket_t *bucket = &vm_page_buckets[i];
		unsigned int bucket_count = 0;
		vm_page_t m;

		bucket_lock = &vm_page_bucket_locks[i / BUCKETS_PER_LOCK];
		lck_spin_lock_grp(bucket_lock, &vm_page_lck_grp_bucket);

		for (m = (vm_page_t)(VM_PAGE_UNPACK_PTR(bucket->page_list));
		    m != VM_PAGE_NULL;
		    m = (vm_page_t)(VM_PAGE_UNPACK_PTR(m->vmp_next_m))) {
			bucket_count++;
		}

		lck_spin_unlock(bucket_lock);

		/* don't touch pageable memory while holding locks */
		info[i].hib_count = bucket_count;
	}

	return vm_page_bucket_count;
}
#endif  /* MACH_VM_DEBUG */

#if VM_PAGE_BUCKETS_CHECK
void
vm_page_buckets_check(void)
{
	unsigned int i;
	vm_page_t p;
	unsigned int p_hash;
	vm_page_bucket_t *bucket;
	lck_spin_t      *bucket_lock;

	if (!vm_page_buckets_check_ready) {
		return;
	}

#if HIBERNATION
	if (hibernate_rebuild_needed ||
	    hibernate_rebuild_hash_list) {
		panic("BUCKET_CHECK: hibernation in progress: "
		    "rebuild_needed=%d rebuild_hash_list=%p\n",
		    hibernate_rebuild_needed,
		    hibernate_rebuild_hash_list);
	}
#endif /* HIBERNATION */

#if VM_PAGE_FAKE_BUCKETS
	char *cp;
	for (cp = (char *) vm_page_fake_buckets_start;
	    cp < (char *) vm_page_fake_buckets_end;
	    cp++) {
		if (*cp != 0x5a) {
			panic("BUCKET_CHECK: corruption at %p in fake buckets "
			    "[0x%llx:0x%llx]\n",
			    cp,
			    (uint64_t) vm_page_fake_buckets_start,
			    (uint64_t) vm_page_fake_buckets_end);
		}
	}
#endif /* VM_PAGE_FAKE_BUCKETS */

	for (i = 0; i < vm_page_bucket_count; i++) {
		vm_object_t     p_object;

		bucket = &vm_page_buckets[i];
		if (!bucket->page_list) {
			continue;
		}

		bucket_lock = &vm_page_bucket_locks[i / BUCKETS_PER_LOCK];
		lck_spin_lock_grp(bucket_lock, &vm_page_lck_grp_bucket);
		p = (vm_page_t)(VM_PAGE_UNPACK_PTR(bucket->page_list));

		while (p != VM_PAGE_NULL) {
			p_object = VM_PAGE_OBJECT(p);

			if (!p->vmp_hashed) {
				panic("BUCKET_CHECK: page %p (%p,0x%llx) "
				    "hash %d in bucket %d at %p "
				    "is not hashed\n",
				    p, p_object, p->vmp_offset,
				    p_hash, i, bucket);
			}
			p_hash = vm_page_hash(p_object, p->vmp_offset);
			if (p_hash != i) {
				panic("BUCKET_CHECK: corruption in bucket %d "
				    "at %p: page %p object %p offset 0x%llx "
				    "hash %d\n",
				    i, bucket, p, p_object, p->vmp_offset,
				    p_hash);
			}
			p = (vm_page_t)(VM_PAGE_UNPACK_PTR(p->vmp_next_m));
		}
		lck_spin_unlock(bucket_lock);
	}

//	printf("BUCKET_CHECK: checked buckets\n");
}
#endif /* VM_PAGE_BUCKETS_CHECK */

/*
 * 'vm_fault_enter' will place newly created pages (zero-fill and COW) onto the
 * local queues if they exist... its the only spot in the system where we add pages
 * to those queues...  once on those queues, those pages can only move to one of the
 * global page queues or the free queues... they NEVER move from local q to local q.
 * the 'local' state is stable when vm_page_queues_remove is called since we're behind
 * the global vm_page_queue_lock at this point...  we still need to take the local lock
 * in case this operation is being run on a different CPU then the local queue's identity,
 * but we don't have to worry about the page moving to a global queue or becoming wired
 * while we're grabbing the local lock since those operations would require the global
 * vm_page_queue_lock to be held, and we already own it.
 *
 * this is why its safe to utilze the wire_count field in the vm_page_t as the local_id...
 * 'wired' and local are ALWAYS mutually exclusive conditions.
 */

void
vm_page_queues_remove(vm_page_t mem, boolean_t remove_from_specialq)
{
	boolean_t       was_pageable = TRUE;
	vm_object_t     m_object;

	m_object = VM_PAGE_OBJECT(mem);

	LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);

	if (mem->vmp_q_state == VM_PAGE_NOT_ON_Q) {
		assert(mem->vmp_pageq.next == 0 && mem->vmp_pageq.prev == 0);
		if (remove_from_specialq == TRUE) {
			vm_page_remove_from_specialq(mem);
		}
		/*if (mem->vmp_on_specialq != VM_PAGE_SPECIAL_Q_EMPTY) {
		 *       assert(mem->vmp_specialq.next != 0);
		 *       assert(mem->vmp_specialq.prev != 0);
		 *  } else {*/
		if (mem->vmp_on_specialq == VM_PAGE_SPECIAL_Q_EMPTY) {
			assert(mem->vmp_specialq.next == 0);
			assert(mem->vmp_specialq.prev == 0);
		}
		return;
	}

	if (mem->vmp_q_state == VM_PAGE_USED_BY_COMPRESSOR) {
		assert(mem->vmp_pageq.next == 0 && mem->vmp_pageq.prev == 0);
		assert(mem->vmp_specialq.next == 0 &&
		    mem->vmp_specialq.prev == 0 &&
		    mem->vmp_on_specialq == VM_PAGE_SPECIAL_Q_EMPTY);
		return;
	}
	if (mem->vmp_q_state == VM_PAGE_IS_WIRED) {
		/*
		 * might put these guys on a list for debugging purposes
		 * if we do, we'll need to remove this assert
		 */
		assert(mem->vmp_pageq.next == 0 && mem->vmp_pageq.prev == 0);
		assert(mem->vmp_specialq.next == 0 &&
		    mem->vmp_specialq.prev == 0);
		/*
		 * Recall that vmp_on_specialq also means a request to put
		 * it on the special Q. So we don't want to reset that bit
		 * just because a wiring request came in. We might want to
		 * put it on the special queue post-unwiring.
		 *
		 * &&
		 * mem->vmp_on_specialq == VM_PAGE_SPECIAL_Q_EMPTY);
		 */
		return;
	}

	assert(m_object != compressor_object);
	assert(!is_kernel_object(m_object));
	assert(!mem->vmp_fictitious);

	switch (mem->vmp_q_state) {
	case VM_PAGE_ON_ACTIVE_LOCAL_Q:
	{
		struct vpl      *lq;

		lq = zpercpu_get_cpu(vm_page_local_q, mem->vmp_local_id);
		VPL_LOCK(&lq->vpl_lock);
		vm_page_queue_remove(&lq->vpl_queue, mem, vmp_pageq);
		mem->vmp_local_id = 0;
		lq->vpl_count--;
		if (m_object->internal) {
			lq->vpl_internal_count--;
		} else {
			lq->vpl_external_count--;
		}
		VPL_UNLOCK(&lq->vpl_lock);
		was_pageable = FALSE;
		break;
	}
	case VM_PAGE_ON_ACTIVE_Q:
	{
		vm_page_queue_remove(&vm_page_queue_active, mem, vmp_pageq);
		vm_page_active_count--;
		break;
	}

	case VM_PAGE_ON_INACTIVE_INTERNAL_Q:
	{
		assert(m_object->internal == TRUE);

		vm_page_inactive_count--;
		vm_page_queue_remove(&vm_page_queue_anonymous, mem, vmp_pageq);
		vm_page_anonymous_count--;

		vm_purgeable_q_advance_all();
		vm_page_balance_inactive(3);
		break;
	}

	case VM_PAGE_ON_INACTIVE_EXTERNAL_Q:
	{
		assert(m_object->internal == FALSE);

		vm_page_inactive_count--;
		vm_page_queue_remove(&vm_page_queue_inactive, mem, vmp_pageq);
		vm_purgeable_q_advance_all();
		vm_page_balance_inactive(3);
		break;
	}

	case VM_PAGE_ON_INACTIVE_CLEANED_Q:
	{
		assert(m_object->internal == FALSE);

		vm_page_inactive_count--;
		vm_page_queue_remove(&vm_page_queue_cleaned, mem, vmp_pageq);
		vm_page_cleaned_count--;
		vm_page_balance_inactive(3);
		break;
	}

	case VM_PAGE_ON_THROTTLED_Q:
	{
		assert(m_object->internal == TRUE);

		vm_page_queue_remove(&vm_page_queue_throttled, mem, vmp_pageq);
		vm_page_throttled_count--;
		was_pageable = FALSE;
		break;
	}

	case VM_PAGE_ON_SPECULATIVE_Q:
	{
		assert(m_object->internal == FALSE);

		vm_page_remque(&mem->vmp_pageq);
		vm_page_speculative_count--;
		vm_page_balance_inactive(3);
		break;
	}

#if CONFIG_SECLUDED_MEMORY
	case VM_PAGE_ON_SECLUDED_Q:
	{
		vm_page_queue_remove(&vm_page_queue_secluded, mem, vmp_pageq);
		vm_page_secluded_count--;
		VM_PAGE_SECLUDED_COUNT_OVER_TARGET_UPDATE();
		if (m_object == VM_OBJECT_NULL) {
			vm_page_secluded_count_free--;
			was_pageable = FALSE;
		} else {
			assert(!m_object->internal);
			vm_page_secluded_count_inuse--;
			was_pageable = FALSE;
//			was_pageable = TRUE;
		}
		break;
	}
#endif /* CONFIG_SECLUDED_MEMORY */

	default:
	{
		/*
		 *	if (mem->vmp_q_state == VM_PAGE_ON_PAGEOUT_Q)
		 *              NOTE: vm_page_queues_remove does not deal with removing pages from the pageout queue...
		 *              the caller is responsible for determing if the page is on that queue, and if so, must
		 *              either first remove it (it needs both the page queues lock and the object lock to do
		 *              this via vm_pageout_steal_laundry), or avoid the call to vm_page_queues_remove
		 *
		 *	we also don't expect to encounter VM_PAGE_ON_FREE_Q, VM_PAGE_ON_FREE_LOCAL_Q, VM_PAGE_ON_FREE_LOPAGE_Q
		 *	or any of the undefined states
		 */
		panic("vm_page_queues_remove - bad page q_state (%p, %d)", mem, mem->vmp_q_state);
		break;
	}
	}
	VM_PAGE_ZERO_PAGEQ_ENTRY(mem);
	mem->vmp_q_state = VM_PAGE_NOT_ON_Q;

	if (remove_from_specialq == TRUE) {
		vm_page_remove_from_specialq(mem);
	}
	if (was_pageable) {
		if (m_object->internal) {
			vm_page_pageable_internal_count--;
		} else {
			vm_page_pageable_external_count--;
		}
	}
}

void
vm_page_remove_internal(vm_page_t page)
{
	vm_object_t __object = VM_PAGE_OBJECT(page);
	if (page == __object->memq_hint) {
		vm_page_t       __new_hint;
		vm_page_queue_entry_t   __qe;
		__qe = (vm_page_queue_entry_t)vm_page_queue_next(&page->vmp_listq);
		if (vm_page_queue_end(&__object->memq, __qe)) {
			__qe = (vm_page_queue_entry_t)vm_page_queue_prev(&page->vmp_listq);
			if (vm_page_queue_end(&__object->memq, __qe)) {
				__qe = NULL;
			}
		}
		__new_hint = (vm_page_t)((uintptr_t) __qe);
		__object->memq_hint = __new_hint;
	}
	vm_page_queue_remove(&__object->memq, page, vmp_listq);
#if CONFIG_SECLUDED_MEMORY
	if (__object->eligible_for_secluded) {
		vm_page_secluded.eligible_for_secluded--;
	}
#endif /* CONFIG_SECLUDED_MEMORY */
}

void
vm_page_enqueue_inactive(vm_page_t mem, boolean_t first)
{
	vm_object_t     m_object;

	m_object = VM_PAGE_OBJECT(mem);

	LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);
	assert(!mem->vmp_fictitious);
	assert(!mem->vmp_laundry);
	assert(mem->vmp_q_state == VM_PAGE_NOT_ON_Q);
	vm_page_check_pageable_safe(mem);

	if (m_object->internal) {
		mem->vmp_q_state = VM_PAGE_ON_INACTIVE_INTERNAL_Q;

		if (first == TRUE) {
			vm_page_queue_enter_first(&vm_page_queue_anonymous, mem, vmp_pageq);
		} else {
			vm_page_queue_enter(&vm_page_queue_anonymous, mem, vmp_pageq);
		}

		vm_page_anonymous_count++;
		vm_page_pageable_internal_count++;
	} else {
		mem->vmp_q_state = VM_PAGE_ON_INACTIVE_EXTERNAL_Q;

		if (first == TRUE) {
			vm_page_queue_enter_first(&vm_page_queue_inactive, mem, vmp_pageq);
		} else {
			vm_page_queue_enter(&vm_page_queue_inactive, mem, vmp_pageq);
		}

		vm_page_pageable_external_count++;
	}
	vm_page_inactive_count++;
	token_new_pagecount++;

	vm_page_add_to_specialq(mem, FALSE);
}

void
vm_page_enqueue_active(vm_page_t mem, boolean_t first)
{
	vm_object_t     m_object;

	m_object = VM_PAGE_OBJECT(mem);

	LCK_MTX_ASSERT(&vm_page_queue_lock, LCK_MTX_ASSERT_OWNED);
	assert(!mem->vmp_fictitious);
	assert(!mem->vmp_laundry);
	assert(mem->vmp_q_state == VM_PAGE_NOT_ON_Q);
	vm_page_check_pageable_safe(mem);

	mem->vmp_q_state = VM_PAGE_ON_ACTIVE_Q;
	if (first == TRUE) {
		vm_page_queue_enter_first(&vm_page_queue_active, mem, vmp_pageq);
	} else {
		vm_page_queue_enter(&vm_page_queue_active, mem, vmp_pageq);
	}
	vm_page_active_count++;

	if (m_object->internal) {
		vm_page_pageable_internal_count++;
	} else {
		vm_page_pageable_external_count++;
	}

	vm_page_add_to_specialq(mem, FALSE);
	vm_page_balance_inactive(3);
}

/*
 * Pages from special kernel objects shouldn't
 * be placed on pageable queues.
 */
void
vm_page_check_pageable_safe(vm_page_t page)
{
	vm_object_t     page_object;

	page_object = VM_PAGE_OBJECT(page);

	if (is_kernel_object(page_object)) {
		panic("vm_page_check_pageable_safe: trying to add page"
		    "from a kernel object to pageable queue");
	}

	if (page_object == compressor_object) {
		panic("vm_page_check_pageable_safe: trying to add page"
		    "from compressor object (%p) to pageable queue", compressor_object);
	}
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
* wired page diagnose
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <libkern/OSKextLibPrivate.h>

#define KA_SIZE(namelen, subtotalscount)        \
	(sizeof(struct vm_allocation_site) + (namelen) + 1 + ((subtotalscount) * sizeof(struct vm_allocation_total)))

#define KA_NAME(alloc)  \
	((char *)(&(alloc)->subtotals[(alloc->subtotalscount)]))

#define KA_NAME_LEN(alloc)      \
    (VM_TAG_NAME_LEN_MAX & (alloc->flags >> VM_TAG_NAME_LEN_SHIFT))

vm_tag_t
vm_tag_bt(void)
{
	uintptr_t* frameptr;
	uintptr_t* frameptr_next;
	uintptr_t retaddr;
	uintptr_t kstackb, kstackt;
	const vm_allocation_site_t * site;
	thread_t cthread;
	kern_allocation_name_t name;

	cthread = current_thread();
	if (__improbable(cthread == NULL)) {
		return VM_KERN_MEMORY_OSFMK;
	}

	if ((name = thread_get_kernel_state(cthread)->allocation_name)) {
		if (!name->tag) {
			vm_tag_alloc(name);
		}
		return name->tag;
	}

	kstackb = cthread->kernel_stack;
	kstackt = kstackb + kernel_stack_size;

	/* Load stack frame pointer (EBP on x86) into frameptr */
	frameptr = __builtin_frame_address(0);
	site = NULL;
	while (frameptr != NULL) {
		/* Verify thread stack bounds */
		if (((uintptr_t)(frameptr + 2) > kstackt) || ((uintptr_t)frameptr < kstackb)) {
			break;
		}

		/* Next frame pointer is pointed to by the previous one */
		frameptr_next = (uintptr_t*) *frameptr;
#if defined(HAS_APPLE_PAC)
		frameptr_next = ptrauth_strip(frameptr_next, ptrauth_key_frame_pointer);
#endif

		/* Pull return address from one spot above the frame pointer */
		retaddr = *(frameptr + 1);

#if defined(HAS_APPLE_PAC)
		retaddr = (uintptr_t) ptrauth_strip((void *)retaddr, ptrauth_key_return_address);
#endif

		if (((retaddr < vm_kernel_builtinkmod_text_end) && (retaddr >= vm_kernel_builtinkmod_text))
		    || (retaddr < vm_kernel_stext) || (retaddr > vm_kernel_top)) {
			site = OSKextGetAllocationSiteForCaller(retaddr);
			break;
		}
		frameptr = frameptr_next;
	}

	return site ? site->tag : VM_KERN_MEMORY_NONE;
}

static uint64_t free_tag_bits[VM_MAX_TAG_VALUE / 64];

void
vm_tag_alloc_locked(vm_allocation_site_t * site, vm_allocation_site_t ** releasesiteP)
{
	vm_tag_t tag;
	uint64_t avail;
	uint32_t idx;
	vm_allocation_site_t * prev;

	if (site->tag) {
		return;
	}

	idx = 0;
	while (TRUE) {
		avail = free_tag_bits[idx];
		if (avail) {
			tag = (vm_tag_t)__builtin_clzll(avail);
			avail &= ~(1ULL << (63 - tag));
			free_tag_bits[idx] = avail;
			tag += (idx << 6);
			break;
		}
		idx++;
		if (idx >= ARRAY_COUNT(free_tag_bits)) {
			for (idx = 0; idx < ARRAY_COUNT(vm_allocation_sites); idx++) {
				prev = vm_allocation_sites[idx];
				if (!prev) {
					continue;
				}
				if (!KA_NAME_LEN(prev)) {
					continue;
				}
				if (!prev->tag) {
					continue;
				}
				if (prev->total) {
					continue;
				}
				if (1 != prev->refcount) {
					continue;
				}

				assert(idx == prev->tag);
				tag = (vm_tag_t)idx;
				prev->tag = VM_KERN_MEMORY_NONE;
				*releasesiteP = prev;
				break;
			}
			if (idx >= ARRAY_COUNT(vm_allocation_sites)) {
				tag = VM_KERN_MEMORY_ANY;
			}
			break;
		}
	}
	site->tag = tag;

	OSAddAtomic16(1, &site->refcount);

	if (VM_KERN_MEMORY_ANY != tag) {
		vm_allocation_sites[tag] = site;
	}

	if (tag > vm_allocation_tag_highest) {
		vm_allocation_tag_highest = tag;
	}
}

static void
vm_tag_free_locked(vm_tag_t tag)
{
	uint64_t avail;
	uint32_t idx;
	uint64_t bit;

	if (VM_KERN_MEMORY_ANY == tag) {
		return;
	}

	idx = (tag >> 6);
	avail = free_tag_bits[idx];
	tag &= 63;
	bit = (1ULL << (63 - tag));
	assert(!(avail & bit));
	free_tag_bits[idx] = (avail | bit);
}

static void
vm_tag_init(void)
{
	vm_tag_t tag;
	for (tag = VM_KERN_MEMORY_FIRST_DYNAMIC; tag < VM_KERN_MEMORY_ANY; tag++) {
		vm_tag_free_locked(tag);
	}

	for (tag = VM_KERN_MEMORY_ANY + 1; tag < VM_MAX_TAG_VALUE; tag++) {
		vm_tag_free_locked(tag);
	}
}

vm_tag_t
vm_tag_alloc(vm_allocation_site_t * site)
{
	vm_allocation_site_t * releasesite;

	if (!site->tag) {
		releasesite = NULL;
		lck_ticket_lock(&vm_allocation_sites_lock, &vm_page_lck_grp_bucket);
		vm_tag_alloc_locked(site, &releasesite);
		lck_ticket_unlock(&vm_allocation_sites_lock);
		if (releasesite) {
			kern_allocation_name_release(releasesite);
		}
	}

	return site->tag;
}

#if VM_BTLOG_TAGS
#define VM_KERN_MEMORY_STR_MAX_LEN (32)
TUNABLE_STR(vmtaglog, VM_KERN_MEMORY_STR_MAX_LEN, "vmtaglog", "");
#define VM_TAG_BTLOG_SIZE (16u << 10)

btlog_t vmtaglog_btlog;
vm_tag_t vmtaglog_tag;

static void
vm_tag_log(vm_object_t object, int64_t delta, void *fp)
{
	if (is_kernel_object(object)) {
		/* kernel object backtraces are tracked in vm entries */
		return;
	}
	if (delta > 0) {
		btref_t ref = btref_get(fp, BTREF_GET_NOWAIT);
		btlog_record(vmtaglog_btlog, object, 0, ref);
	} else if (object->wired_page_count == 0) {
		btlog_erase(vmtaglog_btlog, object);
	}
}

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif /* ARRAY_SIZE */
#define VM_KERN_MEMORY_ELEM(name) [VM_KERN_MEMORY_##name] = #name
const char *vm_kern_memory_strs[] = {
	VM_KERN_MEMORY_ELEM(OSFMK),
	VM_KERN_MEMORY_ELEM(BSD),
	VM_KERN_MEMORY_ELEM(IOKIT),
	VM_KERN_MEMORY_ELEM(LIBKERN),
	VM_KERN_MEMORY_ELEM(OSKEXT),
	VM_KERN_MEMORY_ELEM(KEXT),
	VM_KERN_MEMORY_ELEM(IPC),
	VM_KERN_MEMORY_ELEM(STACK),
	VM_KERN_MEMORY_ELEM(CPU),
	VM_KERN_MEMORY_ELEM(PMAP),
	VM_KERN_MEMORY_ELEM(PTE),
	VM_KERN_MEMORY_ELEM(ZONE),
	VM_KERN_MEMORY_ELEM(KALLOC),
	VM_KERN_MEMORY_ELEM(COMPRESSOR),
	VM_KERN_MEMORY_ELEM(COMPRESSED_DATA),
	VM_KERN_MEMORY_ELEM(PHANTOM_CACHE),
	VM_KERN_MEMORY_ELEM(WAITQ),
	VM_KERN_MEMORY_ELEM(DIAG),
	VM_KERN_MEMORY_ELEM(LOG),
	VM_KERN_MEMORY_ELEM(FILE),
	VM_KERN_MEMORY_ELEM(MBUF),
	VM_KERN_MEMORY_ELEM(UBC),
	VM_KERN_MEMORY_ELEM(SECURITY),
	VM_KERN_MEMORY_ELEM(MLOCK),
	VM_KERN_MEMORY_ELEM(REASON),
	VM_KERN_MEMORY_ELEM(SKYWALK),
	VM_KERN_MEMORY_ELEM(LTABLE),
	VM_KERN_MEMORY_ELEM(HV),
	VM_KERN_MEMORY_ELEM(KALLOC_DATA),
	VM_KERN_MEMORY_ELEM(RETIRED),
	VM_KERN_MEMORY_ELEM(KALLOC_TYPE),
	VM_KERN_MEMORY_ELEM(TRIAGE),
	VM_KERN_MEMORY_ELEM(RECOUNT),
};

static vm_tag_t
vm_tag_str_to_idx(char tagstr[VM_KERN_MEMORY_STR_MAX_LEN])
{
	for (vm_tag_t i = VM_KERN_MEMORY_OSFMK; i < ARRAY_SIZE(vm_kern_memory_strs); i++) {
		if (!strncmp(vm_kern_memory_strs[i], tagstr, VM_KERN_MEMORY_STR_MAX_LEN)) {
			return i;
		}
	}

	printf("Unable to find vm tag %s for btlog\n", tagstr);
	return VM_KERN_MEMORY_NONE;
}

__startup_func
static void
vm_btlog_init(void)
{
	vmtaglog_tag = vm_tag_str_to_idx(vmtaglog);

	if (vmtaglog_tag != VM_KERN_MEMORY_NONE) {
		vmtaglog_btlog = btlog_create(BTLOG_HASH, VM_TAG_BTLOG_SIZE, 0);
	}
}
STARTUP(ZALLOC, STARTUP_RANK_FIRST, vm_btlog_init);
#endif /* VM_BTLOG_TAGS */

void
vm_tag_update_size(vm_tag_t tag, int64_t delta, vm_object_t object)
{
	assert(VM_KERN_MEMORY_NONE != tag && tag < VM_MAX_TAG_VALUE);

	kern_allocation_update_size(vm_allocation_sites[tag], delta, object);
}

uint64_t
vm_tag_get_size(vm_tag_t tag)
{
	vm_allocation_site_t *allocation;

	assert(VM_KERN_MEMORY_NONE != tag && tag < VM_MAX_TAG_VALUE);

	allocation = vm_allocation_sites[tag];
	return allocation ? os_atomic_load(&allocation->total, relaxed) : 0;
}

void
kern_allocation_update_size(kern_allocation_name_t allocation, int64_t delta, __unused vm_object_t object)
{
	uint64_t value;

	value = os_atomic_add(&allocation->total, delta, relaxed);
	if (delta < 0) {
		assertf(value + (uint64_t)-delta > value,
		    "tag %d, site %p", allocation->tag, allocation);
	}

#if DEBUG || DEVELOPMENT
	if (value > allocation->peak) {
		os_atomic_max(&allocation->peak, value, relaxed);
	}
#endif /* DEBUG || DEVELOPMENT */

	if (value == (uint64_t)delta && !allocation->tag) {
		vm_tag_alloc(allocation);
	}

#if VM_BTLOG_TAGS
	if (vmtaglog_tag && (allocation->tag == vmtaglog_tag) && object) {
		vm_tag_log(object, delta, __builtin_frame_address(0));
	}
#endif /* VM_BTLOG_TAGS */
}

#if VM_TAG_SIZECLASSES

void
vm_allocation_zones_init(void)
{
	vm_offset_t   addr;
	vm_size_t     size;

	const vm_tag_t early_tags[] = {
		VM_KERN_MEMORY_DIAG,
		VM_KERN_MEMORY_KALLOC,
		VM_KERN_MEMORY_KALLOC_DATA,
		VM_KERN_MEMORY_KALLOC_TYPE,
		VM_KERN_MEMORY_LIBKERN,
		VM_KERN_MEMORY_OSFMK,
		VM_KERN_MEMORY_RECOUNT,
	};

	size = VM_MAX_TAG_VALUE * sizeof(vm_allocation_zone_total_t * *)
	    + ARRAY_COUNT(early_tags) * VM_TAG_SIZECLASSES * sizeof(vm_allocation_zone_total_t);

	kmem_alloc(kernel_map, &addr, round_page(size),
	    KMA_NOFAIL | KMA_KOBJECT | KMA_ZERO | KMA_PERMANENT,
	    VM_KERN_MEMORY_DIAG);

	vm_allocation_zone_totals = (vm_allocation_zone_total_t **) addr;
	addr += VM_MAX_TAG_VALUE * sizeof(vm_allocation_zone_total_t * *);

	// prepopulate early tag ranges so allocations
	// in vm_tag_update_zone_size() and early boot won't recurse
	for (size_t i = 0; i < ARRAY_COUNT(early_tags); i++) {
		vm_allocation_zone_totals[early_tags[i]] = (vm_allocation_zone_total_t *)addr;
		addr += VM_TAG_SIZECLASSES * sizeof(vm_allocation_zone_total_t);
	}
}

__attribute__((noinline))
static vm_tag_t
vm_tag_zone_stats_alloc(vm_tag_t tag, zalloc_flags_t flags)
{
	vm_allocation_zone_total_t *stats;
	vm_size_t size = sizeof(*stats) * VM_TAG_SIZECLASSES;

	flags = Z_VM_TAG(Z_ZERO | flags, VM_KERN_MEMORY_DIAG);
	stats = kalloc_data(size, flags);
	if (!stats) {
		return VM_KERN_MEMORY_NONE;
	}
	if (!os_atomic_cmpxchg(&vm_allocation_zone_totals[tag], NULL, stats, release)) {
		kfree_data(stats, size);
	}
	return tag;
}

vm_tag_t
vm_tag_will_update_zone(vm_tag_t tag, uint32_t zflags)
{
	assert(VM_KERN_MEMORY_NONE != tag);
	assert(tag < VM_MAX_TAG_VALUE);

	if (__probable(vm_allocation_zone_totals[tag])) {
		return tag;
	}
	return vm_tag_zone_stats_alloc(tag, zflags);
}

void
vm_tag_update_zone_size(vm_tag_t tag, uint32_t zidx, long delta)
{
	vm_allocation_zone_total_t *stats;
	vm_size_t value;

	assert(VM_KERN_MEMORY_NONE != tag);
	assert(tag < VM_MAX_TAG_VALUE);

	if (zidx >= VM_TAG_SIZECLASSES) {
		return;
	}

	stats = vm_allocation_zone_totals[tag];
	assert(stats);
	stats += zidx;

	value = os_atomic_add(&stats->vazt_total, delta, relaxed);
	if (delta < 0) {
		assertf((long)value >= 0, "zidx %d, tag %d, %p", zidx, tag, stats);
		return;
	} else if (os_atomic_load(&stats->vazt_peak, relaxed) < value) {
		os_atomic_max(&stats->vazt_peak, value, relaxed);
	}
}

#endif /* VM_TAG_SIZECLASSES */

void
kern_allocation_update_subtotal(kern_allocation_name_t allocation, vm_tag_t subtag, int64_t delta)
{
	kern_allocation_name_t other;
	struct vm_allocation_total * total;
	uint32_t subidx;

	assert(VM_KERN_MEMORY_NONE != subtag);
	lck_ticket_lock(&vm_allocation_sites_lock, &vm_page_lck_grp_bucket);
	for (subidx = 0; subidx < allocation->subtotalscount; subidx++) {
		total = &allocation->subtotals[subidx];
		if (subtag == total->tag) {
			break;
		}
	}
	if (subidx >= allocation->subtotalscount) {
		for (subidx = 0; subidx < allocation->subtotalscount; subidx++) {
			total = &allocation->subtotals[subidx];
			if ((VM_KERN_MEMORY_NONE == total->tag)
			    || !total->total) {
				total->tag = (vm_tag_t)subtag;
				break;
			}
		}
	}
	assert(subidx < allocation->subtotalscount);
	if (subidx >= allocation->subtotalscount) {
		lck_ticket_unlock(&vm_allocation_sites_lock);
		return;
	}
	if (delta < 0) {
		assertf(total->total >= ((uint64_t)-delta), "name %p", allocation);
	}
	OSAddAtomic64(delta, &total->total);
	lck_ticket_unlock(&vm_allocation_sites_lock);

	other = vm_allocation_sites[subtag];
	assert(other);
	if (delta < 0) {
		assertf(other->mapped >= ((uint64_t)-delta), "other %p", other);
	}
	OSAddAtomic64(delta, &other->mapped);
}

const char *
kern_allocation_get_name(kern_allocation_name_t allocation)
{
	return KA_NAME(allocation);
}

kern_allocation_name_t
kern_allocation_name_allocate(const char * name, uint16_t subtotalscount)
{
	kern_allocation_name_t allocation;
	uint16_t namelen;

	namelen = (uint16_t)strnlen(name, MACH_MEMORY_INFO_NAME_MAX_LEN - 1);

	allocation = kalloc_data(KA_SIZE(namelen, subtotalscount), Z_WAITOK | Z_ZERO);
	allocation->refcount       = 1;
	allocation->subtotalscount = subtotalscount;
	allocation->flags          = (uint16_t)(namelen << VM_TAG_NAME_LEN_SHIFT);
	strlcpy(KA_NAME(allocation), name, namelen + 1);

	vm_tag_alloc(allocation);
	return allocation;
}

void
kern_allocation_name_release(kern_allocation_name_t allocation)
{
	assert(allocation->refcount > 0);
	if (1 == OSAddAtomic16(-1, &allocation->refcount)) {
		kfree_data(allocation,
		    KA_SIZE(KA_NAME_LEN(allocation), allocation->subtotalscount));
	}
}

#if !VM_TAG_ACTIVE_UPDATE
static void
vm_page_count_object(mach_memory_info_t * info, unsigned int __unused num_info, vm_object_t object)
{
	if (!object->wired_page_count) {
		return;
	}
	if (!is_kernel_object(object)) {
		assert(object->wire_tag < num_info);
		info[object->wire_tag].size += ptoa_64(object->wired_page_count);
	}
}

typedef void (*vm_page_iterate_proc)(mach_memory_info_t * info,
    unsigned int num_info, vm_object_t object);

static void
vm_page_iterate_purgeable_objects(mach_memory_info_t * info, unsigned int num_info,
    vm_page_iterate_proc proc, purgeable_q_t queue,
    int group)
{
	vm_object_t object;

	for (object = (vm_object_t) queue_first(&queue->objq[group]);
	    !queue_end(&queue->objq[group], (queue_entry_t) object);
	    object = (vm_object_t) queue_next(&object->objq)) {
		proc(info, num_info, object);
	}
}

static void
vm_page_iterate_objects(mach_memory_info_t * info, unsigned int num_info,
    vm_page_iterate_proc proc)
{
	vm_object_t     object;

	lck_spin_lock_grp(&vm_objects_wired_lock, &vm_page_lck_grp_bucket);
	queue_iterate(&vm_objects_wired,
	    object,
	    vm_object_t,
	    wired_objq)
	{
		proc(info, num_info, object);
	}
	lck_spin_unlock(&vm_objects_wired_lock);
}
#endif /* ! VM_TAG_ACTIVE_UPDATE */

static uint64_t
process_account(mach_memory_info_t * info, unsigned int num_info,
    uint64_t zones_collectable_bytes, boolean_t iterated, bool redact_info __unused)
{
	size_t                 namelen;
	unsigned int           idx, count, nextinfo;
	vm_allocation_site_t * site;
	lck_ticket_lock(&vm_allocation_sites_lock, &vm_page_lck_grp_bucket);

	for (idx = 0; idx <= vm_allocation_tag_highest; idx++) {
		site = vm_allocation_sites[idx];
		if (!site) {
			continue;
		}
		info[idx].mapped = site->mapped;
		info[idx].tag    = site->tag;
		if (!iterated) {
			info[idx].size = site->total;
#if DEBUG || DEVELOPMENT
			info[idx].peak = site->peak;
#endif /* DEBUG || DEVELOPMENT */
		} else {
			if (!site->subtotalscount && (site->total != info[idx].size)) {
				printf("tag mismatch[%d] 0x%qx, iter 0x%qx\n", idx, site->total, info[idx].size);
				info[idx].size = site->total;
			}
		}
		info[idx].flags |= VM_KERN_SITE_WIRED;
		if (idx < VM_KERN_MEMORY_FIRST_DYNAMIC) {
			info[idx].site   = idx;
			info[idx].flags |= VM_KERN_SITE_TAG;
			if (VM_KERN_MEMORY_ZONE == idx) {
				info[idx].flags |= VM_KERN_SITE_HIDE;
				info[idx].flags &= ~VM_KERN_SITE_WIRED;
				info[idx].collectable_bytes = zones_collectable_bytes;
			}
		} else if ((namelen = (VM_TAG_NAME_LEN_MAX & (site->flags >> VM_TAG_NAME_LEN_SHIFT)))) {
			info[idx].site   = 0;
			info[idx].flags |= VM_KERN_SITE_NAMED;
			if (namelen > sizeof(info[idx].name)) {
				namelen = sizeof(info[idx].name);
			}
			strncpy(&info[idx].name[0], KA_NAME(site), namelen);
		} else if (VM_TAG_KMOD & site->flags) {
			info[idx].site   = OSKextGetKmodIDForSite(site, NULL, 0);
			info[idx].flags |= VM_KERN_SITE_KMOD;
		} else {
			info[idx].site   = VM_KERNEL_UNSLIDE(site);
			info[idx].flags |= VM_KERN_SITE_KERNEL;
		}
	}

	nextinfo = (vm_allocation_tag_highest + 1);
	count    = nextinfo;
	if (count >= num_info) {
		count = num_info;
	}

	for (idx = 0; idx < count; idx++) {
		site = vm_allocation_sites[idx];
		if (!site) {
			continue;
		}
#if VM_TAG_SIZECLASSES
		vm_allocation_zone_total_t * zone;
		unsigned int                 zidx;

		if (!redact_info
		    && vm_allocation_zone_totals
		    && (zone = vm_allocation_zone_totals[idx])
		    && (nextinfo < num_info)) {
			for (zidx = 0; zidx < VM_TAG_SIZECLASSES; zidx++) {
				if (!zone[zidx].vazt_peak) {
					continue;
				}
				info[nextinfo]        = info[idx];
				info[nextinfo].zone   = zone_index_from_tag_index(zidx);
				info[nextinfo].flags  &= ~VM_KERN_SITE_WIRED;
				info[nextinfo].flags  |= VM_KERN_SITE_ZONE;
				info[nextinfo].flags  |= VM_KERN_SITE_KALLOC;
				info[nextinfo].size   = zone[zidx].vazt_total;
				info[nextinfo].peak   = zone[zidx].vazt_peak;
				info[nextinfo].mapped = 0;
				nextinfo++;
			}
		}
#endif /* VM_TAG_SIZECLASSES */
		if (site->subtotalscount) {
			uint64_t mapped, mapcost, take;
			uint32_t sub;
			vm_tag_t alloctag;

			info[idx].size = site->total;
			mapped = info[idx].size;
			info[idx].mapped = mapped;
			mapcost = 0;
			for (sub = 0; sub < site->subtotalscount; sub++) {
				alloctag = site->subtotals[sub].tag;
				assert(alloctag < num_info);
				if (info[alloctag].name[0]) {
					continue;
				}
				take = site->subtotals[sub].total;
				if (take > info[alloctag].size) {
					take = info[alloctag].size;
				}
				if (take > mapped) {
					take = mapped;
				}
				info[alloctag].mapped  -= take;
				info[alloctag].size    -= take;
				mapped                 -= take;
				mapcost                += take;
			}
			info[idx].size = mapcost;
		}
	}
	lck_ticket_unlock(&vm_allocation_sites_lock);

	return 0;
}

uint32_t
vm_page_diagnose_estimate(void)
{
	vm_allocation_site_t * site;
	uint32_t               count = zone_view_count;
	uint32_t               idx;

	lck_ticket_lock(&vm_allocation_sites_lock, &vm_page_lck_grp_bucket);
	for (idx = 0; idx < VM_MAX_TAG_VALUE; idx++) {
		site = vm_allocation_sites[idx];
		if (!site) {
			continue;
		}
		count++;
#if VM_TAG_SIZECLASSES
		if (vm_allocation_zone_totals) {
			vm_allocation_zone_total_t * zone;
			zone = vm_allocation_zone_totals[idx];
			if (!zone) {
				continue;
			}
			for (uint32_t zidx = 0; zidx < VM_TAG_SIZECLASSES; zidx++) {
				count += (zone[zidx].vazt_peak != 0);
			}
		}
#endif
	}
	lck_ticket_unlock(&vm_allocation_sites_lock);

	/* some slop for new tags created */
	count += 8;
	count += VM_KERN_COUNTER_COUNT;

	return count;
}

static void
vm_page_diagnose_zone_stats(mach_memory_info_t *info, zone_stats_t zstats,
    bool percpu)
{
	zpercpu_foreach(zs, zstats) {
		info->size += zs->zs_mem_allocated - zs->zs_mem_freed;
	}
	if (percpu) {
		info->size *= zpercpu_count();
	}
	info->flags |= VM_KERN_SITE_NAMED | VM_KERN_SITE_ZONE_VIEW;
}

static void
vm_page_add_info(
	mach_memory_info_t     *info,
	zone_stats_t            stats,
	bool                    per_cpu,
	const char             *parent_heap_name,
	const char             *parent_zone_name,
	const char             *view_name)
{
	vm_page_diagnose_zone_stats(info, stats, per_cpu);
	snprintf(info->name, sizeof(info->name),
	    "%s%s[%s]", parent_heap_name, parent_zone_name, view_name);
}

static void
vm_page_diagnose_zone(mach_memory_info_t *info, zone_t z)
{
	vm_page_add_info(info, z->z_stats, z->z_percpu, zone_heap_name(z),
	    z->z_name, "raw");
}

static void
vm_page_add_view(
	mach_memory_info_t     *info,
	zone_stats_t            stats,
	const char             *parent_heap_name,
	const char             *parent_zone_name,
	const char             *view_name)
{
	vm_page_add_info(info, stats, false, parent_heap_name, parent_zone_name,
	    view_name);
}

static uint32_t
vm_page_diagnose_heap_views(
	mach_memory_info_t     *info,
	kalloc_heap_t           kh,
	const char             *parent_heap_name,
	const char             *parent_zone_name)
{
	uint32_t i = 0;

	while (kh) {
		vm_page_add_view(info + i, kh->kh_stats, parent_heap_name,
		    parent_zone_name, kh->kh_name);
		kh = kh->kh_views;
		i++;
	}
	return i;
}

static uint32_t
vm_page_diagnose_heap(mach_memory_info_t *info, kalloc_heap_t kheap)
{
	uint32_t i = 0;

	for (; i < KHEAP_NUM_ZONES; i++) {
		vm_page_diagnose_zone(info + i, zone_by_id(kheap->kh_zstart + i));
	}

	i += vm_page_diagnose_heap_views(info + i, kheap->kh_views, kheap->kh_name,
	    NULL);
	return i;
}

static int
vm_page_diagnose_kt_heaps(mach_memory_info_t *info)
{
	uint32_t idx = 0;
	vm_page_add_view(info + idx, KHEAP_KT_VAR->kh_stats, KHEAP_KT_VAR->kh_name,
	    "", "raw");
	idx++;

	for (uint32_t i = 0; i < KT_VAR_MAX_HEAPS; i++) {
		struct kheap_info heap = kalloc_type_heap_array[i];
		char heap_num_tmp[MAX_ZONE_NAME] = "";
		const char *heap_num;

		snprintf(&heap_num_tmp[0], MAX_ZONE_NAME, "%u", i);
		heap_num = &heap_num_tmp[0];

		for (kalloc_type_var_view_t ktv = heap.kt_views; ktv;
		    ktv = (kalloc_type_var_view_t) ktv->kt_next) {
			if (ktv->kt_stats && ktv->kt_stats != KHEAP_KT_VAR->kh_stats) {
				vm_page_add_view(info + idx, ktv->kt_stats, KHEAP_KT_VAR->kh_name,
				    heap_num, ktv->kt_name);
				idx++;
			}
		}

		idx += vm_page_diagnose_heap_views(info + idx, heap.kh_views,
		    KHEAP_KT_VAR->kh_name, heap_num);
	}

	return idx;
}

kern_return_t
vm_page_diagnose(mach_memory_info_t * info, unsigned int num_info, uint64_t zones_collectable_bytes, bool redact_info)
{
	uint64_t                 wired_size;
	uint64_t                 wired_managed_size;
	uint64_t                 wired_reserved_size;
	boolean_t                iterate;
	mach_memory_info_t     * counts;
	uint32_t                 i;

	bzero(info, num_info * sizeof(mach_memory_info_t));

	if (!vm_page_wire_count_initial) {
		return KERN_ABORTED;
	}

#if !XNU_TARGET_OS_OSX
	wired_size          = ptoa_64(vm_page_wire_count);
	wired_reserved_size = ptoa_64(vm_page_wire_count_initial - vm_page_stolen_count);
#else /* !XNU_TARGET_OS_OSX */
	wired_size          = ptoa_64(vm_page_wire_count + vm_lopage_free_count + vm_page_throttled_count);
	wired_reserved_size = ptoa_64(vm_page_wire_count_initial - vm_page_stolen_count + vm_page_throttled_count);
#endif /* !XNU_TARGET_OS_OSX */
	wired_managed_size  = ptoa_64(vm_page_wire_count - vm_page_wire_count_initial);

	wired_size += booter_size;

	assert(num_info >= VM_KERN_COUNTER_COUNT);
	num_info -= VM_KERN_COUNTER_COUNT;
	counts = &info[num_info];

#define SET_COUNT(xcount, xsize, xflags)                        \
    counts[xcount].tag   = VM_MAX_TAG_VALUE + xcount;   \
    counts[xcount].site  = (xcount);                            \
    counts[xcount].size  = (xsize);                                 \
    counts[xcount].mapped  = (xsize);                           \
    counts[xcount].flags = VM_KERN_SITE_COUNTER | xflags;

	SET_COUNT(VM_KERN_COUNT_MANAGED, ptoa_64(vm_page_pages), 0);
	SET_COUNT(VM_KERN_COUNT_WIRED, wired_size, 0);
	SET_COUNT(VM_KERN_COUNT_WIRED_MANAGED, wired_managed_size, 0);
	SET_COUNT(VM_KERN_COUNT_RESERVED, wired_reserved_size, VM_KERN_SITE_WIRED);
	SET_COUNT(VM_KERN_COUNT_STOLEN, ptoa_64(vm_page_stolen_count), VM_KERN_SITE_WIRED);
	SET_COUNT(VM_KERN_COUNT_LOPAGE, ptoa_64(vm_lopage_free_count), VM_KERN_SITE_WIRED);
	SET_COUNT(VM_KERN_COUNT_WIRED_BOOT, ptoa_64(vm_page_wire_count_on_boot), 0);
	SET_COUNT(VM_KERN_COUNT_BOOT_STOLEN, booter_size, VM_KERN_SITE_WIRED);
	SET_COUNT(VM_KERN_COUNT_WIRED_STATIC_KERNELCACHE, ptoa_64(vm_page_kernelcache_count), 0);

#define SET_MAP(xcount, xsize, xfree, xlargest) \
    counts[xcount].site    = (xcount);                  \
    counts[xcount].size    = (xsize);                   \
    counts[xcount].mapped  = (xsize);                   \
    counts[xcount].free    = (xfree);                   \
    counts[xcount].largest = (xlargest);                \
    counts[xcount].flags   = VM_KERN_SITE_COUNTER;

	vm_map_size_t map_size, map_free, map_largest;

	vm_map_sizes(kernel_map, &map_size, &map_free, &map_largest);
	SET_MAP(VM_KERN_COUNT_MAP_KERNEL, map_size, map_free, map_largest);

	zone_map_sizes(&map_size, &map_free, &map_largest);
	SET_MAP(VM_KERN_COUNT_MAP_ZONE, map_size, map_free, map_largest);

	assert(num_info >= zone_view_count);
	num_info -= zone_view_count;
	counts = &info[num_info];
	i = 0;

	if (!redact_info) {
		if (KHEAP_DATA_BUFFERS->kh_heap_id == KHEAP_ID_DATA_BUFFERS) {
			i += vm_page_diagnose_heap(counts + i, KHEAP_DATA_BUFFERS);
		}
		if (KHEAP_KT_VAR->kh_heap_id == KHEAP_ID_KT_VAR) {
			i += vm_page_diagnose_kt_heaps(counts + i);
		}
		assert(i <= zone_view_count);

		zone_index_foreach(zidx) {
			zone_t z = &zone_array[zidx];
			zone_security_flags_t zsflags = zone_security_array[zidx];
			zone_view_t zv = z->z_views;

			if (zv == NULL) {
				continue;
			}

			zone_stats_t zv_stats_head = z->z_stats;
			bool has_raw_view = false;

			for (; zv; zv = zv->zv_next) {
				/*
				 * kalloc_types that allocate from the same zone are linked
				 * as views. Only print the ones that have their own stats.
				 */
				if (zv->zv_stats == zv_stats_head) {
					continue;
				}
				has_raw_view = true;
				vm_page_diagnose_zone_stats(counts + i, zv->zv_stats,
				    z->z_percpu);
				snprintf(counts[i].name, sizeof(counts[i].name), "%s%s[%s]",
				    zone_heap_name(z), z->z_name, zv->zv_name);
				i++;
				assert(i <= zone_view_count);
			}

			/*
			 * Print raw views for non kalloc or kalloc_type zones
			 */
			bool kalloc_type = zsflags.z_kalloc_type;
			if ((zsflags.z_kheap_id == KHEAP_ID_NONE && !kalloc_type) ||
			    (kalloc_type && has_raw_view)) {
				vm_page_diagnose_zone(counts + i, z);
				i++;
				assert(i <= zone_view_count);
			}
		}
	}

	iterate = !VM_TAG_ACTIVE_UPDATE;
	if (iterate) {
		enum                       { kMaxKernelDepth = 1 };
		vm_map_t                     maps[kMaxKernelDepth];
		vm_map_entry_t               entries[kMaxKernelDepth];
		vm_map_t                     map;
		vm_map_entry_t               entry;
		vm_object_offset_t           offset;
		vm_page_t                    page;
		int                          stackIdx, count;

#if !VM_TAG_ACTIVE_UPDATE
		vm_page_iterate_objects(info, num_info, &vm_page_count_object);
#endif /* ! VM_TAG_ACTIVE_UPDATE */

		map = kernel_map;
		stackIdx = 0;
		while (map) {
			vm_map_lock(map);
			for (entry = map->hdr.links.next; map; entry = entry->vme_next) {
				if (entry->is_sub_map) {
					assert(stackIdx < kMaxKernelDepth);
					maps[stackIdx] = map;
					entries[stackIdx] = entry;
					stackIdx++;
					map = VME_SUBMAP(entry);
					entry = NULL;
					break;
				}
				if (is_kernel_object(VME_OBJECT(entry))) {
					count = 0;
					vm_object_lock(VME_OBJECT(entry));
					for (offset = entry->vme_start; offset < entry->vme_end; offset += page_size) {
						page = vm_page_lookup(VME_OBJECT(entry), offset);
						if (page && VM_PAGE_WIRED(page)) {
							count++;
						}
					}
					vm_object_unlock(VME_OBJECT(entry));

					if (count) {
						assert(VME_ALIAS(entry) != VM_KERN_MEMORY_NONE);
						assert(VME_ALIAS(entry) < num_info);
						info[VME_ALIAS(entry)].size += ptoa_64(count);
					}
				}
				while (map && (entry == vm_map_last_entry(map))) {
					vm_map_unlock(map);
					if (!stackIdx) {
						map = NULL;
					} else {
						--stackIdx;
						map = maps[stackIdx];
						entry = entries[stackIdx];
					}
				}
			}
		}
	}

	process_account(info, num_info, zones_collectable_bytes, iterate, redact_info);

	return KERN_SUCCESS;
}

#if DEBUG || DEVELOPMENT

kern_return_t
vm_kern_allocation_info(uintptr_t addr, vm_size_t * size, vm_tag_t * tag, vm_size_t * zone_size)
{
	kern_return_t  ret;
	vm_size_t      zsize;
	vm_map_t       map;
	vm_map_entry_t entry;

	zsize = zone_element_info((void *) addr, tag);
	if (zsize) {
		*zone_size = *size = zsize;
		return KERN_SUCCESS;
	}

	*zone_size = 0;
	ret = KERN_INVALID_ADDRESS;
	for (map = kernel_map; map;) {
		vm_map_lock(map);
		if (!vm_map_lookup_entry_allow_pgz(map, addr, &entry)) {
			break;
		}
		if (entry->is_sub_map) {
			if (map != kernel_map) {
				break;
			}
			map = VME_SUBMAP(entry);
			continue;
		}
		if (entry->vme_start != addr) {
			break;
		}
		*tag = (vm_tag_t)VME_ALIAS(entry);
		*size = (entry->vme_end - addr);
		ret = KERN_SUCCESS;
		break;
	}
	if (map != kernel_map) {
		vm_map_unlock(map);
	}
	vm_map_unlock(kernel_map);

	return ret;
}

#endif /* DEBUG || DEVELOPMENT */

uint32_t
vm_tag_get_kext(vm_tag_t tag, char * name, vm_size_t namelen)
{
	vm_allocation_site_t * site;
	uint32_t               kmodId;

	kmodId = 0;
	lck_ticket_lock(&vm_allocation_sites_lock, &vm_page_lck_grp_bucket);
	if ((site = vm_allocation_sites[tag])) {
		if (VM_TAG_KMOD & site->flags) {
			kmodId = OSKextGetKmodIDForSite(site, name, namelen);
		}
	}
	lck_ticket_unlock(&vm_allocation_sites_lock);

	return kmodId;
}


#if CONFIG_SECLUDED_MEMORY
/*
 * Note that there's no locking around other accesses to vm_page_secluded_target.
 * That should be OK, since these are the only place where it can be changed after
 * initialization. Other users (like vm_pageout) may see the wrong value briefly,
 * but will eventually get the correct value. This brief mismatch is OK as pageout
 * and page freeing will auto-adjust the vm_page_secluded_count to match the target
 * over time.
 */
unsigned int vm_page_secluded_suppress_cnt = 0;
unsigned int vm_page_secluded_save_target;

LCK_GRP_DECLARE(secluded_suppress_slock_grp, "secluded_suppress_slock");
LCK_SPIN_DECLARE(secluded_suppress_slock, &secluded_suppress_slock_grp);

void
start_secluded_suppression(task_t task)
{
	if (task->task_suppressed_secluded) {
		return;
	}
	lck_spin_lock(&secluded_suppress_slock);
	if (!task->task_suppressed_secluded && vm_page_secluded_suppress_cnt++ == 0) {
		task->task_suppressed_secluded = TRUE;
		vm_page_secluded_save_target = vm_page_secluded_target;
		vm_page_secluded_target = 0;
		VM_PAGE_SECLUDED_COUNT_OVER_TARGET_UPDATE();
	}
	lck_spin_unlock(&secluded_suppress_slock);
}

void
stop_secluded_suppression(task_t task)
{
	lck_spin_lock(&secluded_suppress_slock);
	if (task->task_suppressed_secluded && --vm_page_secluded_suppress_cnt == 0) {
		task->task_suppressed_secluded = FALSE;
		vm_page_secluded_target = vm_page_secluded_save_target;
		VM_PAGE_SECLUDED_COUNT_OVER_TARGET_UPDATE();
	}
	lck_spin_unlock(&secluded_suppress_slock);
}

#endif /* CONFIG_SECLUDED_MEMORY */

/*
 * Move the list of retired pages on the vm_page_queue_retired to
 * their final resting place on retired_pages_object.
 */
void
vm_retire_boot_pages(void)
{
}

/*
 * This holds the reported physical address if an ECC error leads to a panic.
 * SMC will store it in PMU SRAM under the 'sECC' key.
 */
uint64_t ecc_panic_physical_address = 0;


boolean_t
vm_page_created(vm_page_t page)
{
	return (page < &vm_pages[0]) || (page >= &vm_pages[vm_pages_count]);
}
