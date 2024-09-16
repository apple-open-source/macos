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
 *	File:	mach/vm_statistics.h
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young, David Golub
 *
 *	Virtual memory statistics structure.
 *
 */

#ifndef _MACH_VM_STATISTICS_H_
#define _MACH_VM_STATISTICS_H_

#include <sys/cdefs.h>

#include <mach/machine/vm_types.h>
#include <mach/machine/kern_return.h>

__BEGIN_DECLS

/*
 * vm_statistics
 *
 * History:
 *	rev0 -  original structure.
 *	rev1 -  added purgable info (purgable_count and purges).
 *	rev2 -  added speculative_count.
 *
 * Note: you cannot add any new fields to this structure. Add them below in
 *       vm_statistics64.
 */

struct vm_statistics {
	natural_t       free_count;             /* # of pages free */
	natural_t       active_count;           /* # of pages active */
	natural_t       inactive_count;         /* # of pages inactive */
	natural_t       wire_count;             /* # of pages wired down */
	natural_t       zero_fill_count;        /* # of zero fill pages */
	natural_t       reactivations;          /* # of pages reactivated */
	natural_t       pageins;                /* # of pageins */
	natural_t       pageouts;               /* # of pageouts */
	natural_t       faults;                 /* # of faults */
	natural_t       cow_faults;             /* # of copy-on-writes */
	natural_t       lookups;                /* object cache lookups */
	natural_t       hits;                   /* object cache hits */

	/* added for rev1 */
	natural_t       purgeable_count;        /* # of pages purgeable */
	natural_t       purges;                 /* # of pages purged */

	/* added for rev2 */
	/*
	 * NB: speculative pages are already accounted for in "free_count",
	 * so "speculative_count" is the number of "free" pages that are
	 * used to hold data that was read speculatively from disk but
	 * haven't actually been used by anyone so far.
	 */
	natural_t       speculative_count;      /* # of pages speculative */
};

/* Used by all architectures */
typedef struct vm_statistics    *vm_statistics_t;
typedef struct vm_statistics    vm_statistics_data_t;

/*
 * vm_statistics64
 *
 * History:
 *	rev0 -  original structure.
 *	rev1 -  added purgable info (purgable_count and purges).
 *	rev2 -  added speculative_count.
 *	   ----
 *	rev3 -  changed name to vm_statistics64.
 *		changed some fields in structure to 64-bit on
 *		arm, i386 and x86_64 architectures.
 *	rev4 -  require 64-bit alignment for efficient access
 *		in the kernel. No change to reported data.
 *
 */

struct vm_statistics64 {
	natural_t       free_count;             /* # of pages free */
	natural_t       active_count;           /* # of pages active */
	natural_t       inactive_count;         /* # of pages inactive */
	natural_t       wire_count;             /* # of pages wired down */
	uint64_t        zero_fill_count;        /* # of zero fill pages */
	uint64_t        reactivations;          /* # of pages reactivated */
	uint64_t        pageins;                /* # of pageins */
	uint64_t        pageouts;               /* # of pageouts */
	uint64_t        faults;                 /* # of faults */
	uint64_t        cow_faults;             /* # of copy-on-writes */
	uint64_t        lookups;                /* object cache lookups */
	uint64_t        hits;                   /* object cache hits */
	uint64_t        purges;                 /* # of pages purged */
	natural_t       purgeable_count;        /* # of pages purgeable */
	/*
	 * NB: speculative pages are already accounted for in "free_count",
	 * so "speculative_count" is the number of "free" pages that are
	 * used to hold data that was read speculatively from disk but
	 * haven't actually been used by anyone so far.
	 */
	natural_t       speculative_count;      /* # of pages speculative */

	/* added for rev1 */
	uint64_t        decompressions;         /* # of pages decompressed */
	uint64_t        compressions;           /* # of pages compressed */
	uint64_t        swapins;                /* # of pages swapped in (via compression segments) */
	uint64_t        swapouts;               /* # of pages swapped out (via compression segments) */
	natural_t       compressor_page_count;  /* # of pages used by the compressed pager to hold all the compressed data */
	natural_t       throttled_count;        /* # of pages throttled */
	natural_t       external_page_count;    /* # of pages that are file-backed (non-swap) */
	natural_t       internal_page_count;    /* # of pages that are anonymous */
	uint64_t        total_uncompressed_pages_in_compressor; /* # of pages (uncompressed) held within the compressor. */
} __attribute__((aligned(8)));

typedef struct vm_statistics64  *vm_statistics64_t;
typedef struct vm_statistics64  vm_statistics64_data_t;

kern_return_t vm_stats(void *info, unsigned int *count);

/*
 * VM_STATISTICS_TRUNCATE_TO_32_BIT
 *
 * This is used by host_statistics() to truncate and peg the 64-bit in-kernel values from
 * vm_statistics64 to the 32-bit values of the older structure above (vm_statistics).
 */
#define VM_STATISTICS_TRUNCATE_TO_32_BIT(value) ((uint32_t)(((value) > UINT32_MAX ) ? UINT32_MAX : (value)))

/*
 * vm_extmod_statistics
 *
 * Structure to record modifications to a task by an
 * external agent.
 *
 * History:
 *	rev0 -  original structure.
 */

struct vm_extmod_statistics {
	int64_t task_for_pid_count;                     /* # of times task port was looked up */
	int64_t task_for_pid_caller_count;      /* # of times this task called task_for_pid */
	int64_t thread_creation_count;          /* # of threads created in task */
	int64_t thread_creation_caller_count;   /* # of threads created by task */
	int64_t thread_set_state_count;         /* # of register state sets in task */
	int64_t thread_set_state_caller_count;  /* # of register state sets by task */
} __attribute__((aligned(8)));

typedef struct vm_extmod_statistics *vm_extmod_statistics_t;
typedef struct vm_extmod_statistics vm_extmod_statistics_data_t;

typedef struct vm_purgeable_stat {
	uint64_t        count;
	uint64_t        size;
}vm_purgeable_stat_t;

struct vm_purgeable_info {
	vm_purgeable_stat_t fifo_data[8];
	vm_purgeable_stat_t obsolete_data;
	vm_purgeable_stat_t lifo_data[8];
};

typedef struct vm_purgeable_info        *vm_purgeable_info_t;

/* included for the vm_map_page_query call */

#define VM_PAGE_QUERY_PAGE_PRESENT      0x1
#define VM_PAGE_QUERY_PAGE_FICTITIOUS   0x2
#define VM_PAGE_QUERY_PAGE_REF          0x4
#define VM_PAGE_QUERY_PAGE_DIRTY        0x8
#define VM_PAGE_QUERY_PAGE_PAGED_OUT    0x10
#define VM_PAGE_QUERY_PAGE_COPIED       0x20
#define VM_PAGE_QUERY_PAGE_SPECULATIVE  0x40
#define VM_PAGE_QUERY_PAGE_EXTERNAL     0x80
#define VM_PAGE_QUERY_PAGE_CS_VALIDATED 0x100
#define VM_PAGE_QUERY_PAGE_CS_TAINTED   0x200
#define VM_PAGE_QUERY_PAGE_CS_NX        0x400
#define VM_PAGE_QUERY_PAGE_REUSABLE     0x800

/*
 * VM allocation flags:
 *
 * VM_FLAGS_FIXED
 *      (really the absence of VM_FLAGS_ANYWHERE)
 *	Allocate new VM region at the specified virtual address, if possible.
 *
 * VM_FLAGS_ANYWHERE
 *	Allocate new VM region anywhere it would fit in the address space.
 *
 * VM_FLAGS_PURGABLE
 *	Create a purgable VM object for that new VM region.
 *
 * VM_FLAGS_4GB_CHUNK
 *	The new VM region will be chunked up into 4GB sized pieces.
 *
 * VM_FLAGS_NO_PMAP_CHECK
 *	(for DEBUG kernel config only, ignored for other configs)
 *	Do not check that there is no stale pmap mapping for the new VM region.
 *	This is useful for kernel memory allocations at bootstrap when building
 *	the initial kernel address space while some memory is already in use.
 *
 * VM_FLAGS_OVERWRITE
 *	The new VM region can replace existing VM regions if necessary
 *	(to be used in combination with VM_FLAGS_FIXED).
 *
 * VM_FLAGS_NO_CACHE
 *	Pages brought in to this VM region are placed on the speculative
 *	queue instead of the active queue.  In other words, they are not
 *	cached so that they will be stolen first if memory runs low.
 */

#define VM_FLAGS_FIXED                  0x00000000
#define VM_FLAGS_ANYWHERE               0x00000001
#define VM_FLAGS_PURGABLE               0x00000002
#define VM_FLAGS_4GB_CHUNK              0x00000004
#define VM_FLAGS_RANDOM_ADDR            0x00000008
#define VM_FLAGS_NO_CACHE               0x00000010
#define VM_FLAGS_RESILIENT_CODESIGN     0x00000020
#define VM_FLAGS_RESILIENT_MEDIA        0x00000040
#define VM_FLAGS_PERMANENT              0x00000080
#define VM_FLAGS_TPRO                   0x00001000
#define VM_FLAGS_OVERWRITE              0x00004000  /* delete any existing mappings first */
/*
 * VM_FLAGS_SUPERPAGE_MASK
 *	3 bits that specify whether large pages should be used instead of
 *	base pages (!=0), as well as the requested page size.
 */
#define VM_FLAGS_SUPERPAGE_MASK         0x00070000 /* bits 0x10000, 0x20000, 0x40000 */
#define VM_FLAGS_RETURN_DATA_ADDR       0x00100000 /* Return address of target data, rather than base of page */
#define VM_FLAGS_RETURN_4K_DATA_ADDR    0x00800000 /* Return 4K aligned address of target data */
#define VM_FLAGS_ALIAS_MASK             0xFF000000
#define VM_GET_FLAGS_ALIAS(flags, alias)                        \
	        (alias) = (((flags) >> 24) & 0xff)
#if !XNU_KERNEL_PRIVATE
#define VM_SET_FLAGS_ALIAS(flags, alias)                        \
	        (flags) = (((flags) & ~VM_FLAGS_ALIAS_MASK) |   \
	        (((alias) & ~VM_FLAGS_ALIAS_MASK) << 24))
#endif /* !XNU_KERNEL_PRIVATE */

#if XNU_KERNEL_PRIVATE
/*
 * When making a new VM_FLAG_*:
 * - add it to this mask
 * - add a vmf_* field to vm_map_kernel_flags_t in the right spot
 * - add a check in vm_map_kernel_flags_check_vmflags()
 * - update tests vm_parameter_validation_[user|kern] and their expected
 *     results; they deliberately call VM functions with invalid flag values
 *     and you may be turning one of those invalid flags valid.
 */
#define VM_FLAGS_ANY_MASK       (VM_FLAGS_FIXED |               \
	                         VM_FLAGS_ANYWHERE |            \
	                         VM_FLAGS_PURGABLE |            \
	                         VM_FLAGS_4GB_CHUNK |           \
	                         VM_FLAGS_RANDOM_ADDR |         \
	                         VM_FLAGS_NO_CACHE |            \
	                         VM_FLAGS_RESILIENT_CODESIGN |  \
	                         VM_FLAGS_RESILIENT_MEDIA |     \
	                         VM_FLAGS_PERMANENT |           \
	                         VM_FLAGS_TPRO |                \
	                         VM_FLAGS_OVERWRITE |           \
	                         VM_FLAGS_SUPERPAGE_MASK |      \
	                         VM_FLAGS_RETURN_DATA_ADDR |    \
	                         VM_FLAGS_RETURN_4K_DATA_ADDR | \
	                         VM_FLAGS_ALIAS_MASK)
#endif /* XNU_KERNEL_PRIVATE */
#define VM_FLAGS_HW     (VM_FLAGS_TPRO)

/* These are the flags that we accept from user-space */
#define VM_FLAGS_USER_ALLOCATE  (VM_FLAGS_FIXED |               \
	                         VM_FLAGS_ANYWHERE |            \
	                         VM_FLAGS_PURGABLE |            \
	                         VM_FLAGS_4GB_CHUNK |           \
	                         VM_FLAGS_RANDOM_ADDR |         \
	                         VM_FLAGS_NO_CACHE |            \
	                         VM_FLAGS_PERMANENT |           \
	                         VM_FLAGS_OVERWRITE |           \
	                         VM_FLAGS_SUPERPAGE_MASK |      \
	                         VM_FLAGS_HW |                  \
	                         VM_FLAGS_ALIAS_MASK)

#define VM_FLAGS_USER_MAP       (VM_FLAGS_USER_ALLOCATE |       \
	                         VM_FLAGS_RETURN_4K_DATA_ADDR | \
	                         VM_FLAGS_RETURN_DATA_ADDR)

#define VM_FLAGS_USER_REMAP     (VM_FLAGS_FIXED |               \
	                         VM_FLAGS_ANYWHERE |            \
	                         VM_FLAGS_RANDOM_ADDR |         \
	                         VM_FLAGS_OVERWRITE|            \
	                         VM_FLAGS_RETURN_DATA_ADDR |    \
	                         VM_FLAGS_RESILIENT_CODESIGN |  \
	                         VM_FLAGS_RESILIENT_MEDIA)

#define VM_FLAGS_SUPERPAGE_SHIFT 16
#define SUPERPAGE_NONE                  0       /* no superpages, if all bits are 0 */
#define SUPERPAGE_SIZE_ANY              1
#define VM_FLAGS_SUPERPAGE_NONE     (SUPERPAGE_NONE     << VM_FLAGS_SUPERPAGE_SHIFT)
#define VM_FLAGS_SUPERPAGE_SIZE_ANY (SUPERPAGE_SIZE_ANY << VM_FLAGS_SUPERPAGE_SHIFT)
#if defined(__x86_64__) || !defined(KERNEL)
#define SUPERPAGE_SIZE_2MB              2
#define VM_FLAGS_SUPERPAGE_SIZE_2MB (SUPERPAGE_SIZE_2MB<<VM_FLAGS_SUPERPAGE_SHIFT)
#endif

/*
 * EXC_GUARD definitions for virtual memory.
 */
#define GUARD_TYPE_VIRT_MEMORY  0x5

/* Reasons for exception for virtual memory */
enum virtual_memory_guard_exception_codes {
	kGUARD_EXC_DEALLOC_GAP  = 1u << 0,
	kGUARD_EXC_RECLAIM_COPYIO_FAILURE = 1u << 1,
	kGUARD_EXC_RECLAIM_INDEX_FAILURE = 1u << 2,
	kGUARD_EXC_RECLAIM_DEALLOCATE_FAILURE = 1u << 3,
};

#ifdef XNU_KERNEL_PRIVATE

/*!
 * @enum vm_map_range_id_t
 *
 * @brief
 * Enumerate a particular vm_map range.
 *
 * @discussion
 * The kernel_map VA has been split into the following ranges. Userspace
 * VA for any given process can also optionally be split by the following user
 * ranges.
 *
 * @const KMEM_RANGE_ID_NONE
 * This range is only used for early initialization.
 *
 * @const KMEM_RANGE_ID_PTR_*
 * Range containing general purpose allocations from kalloc, etc that
 * contain pointers.
 *
 * @const KMEM_RANGE_ID_SPRAYQTN
 * The spray quarantine range contains allocations that have the following
 * properties:
 * - An attacker could control the size, lifetime and number of allocations
 *   of this type (or from this callsite).
 * - The pointer to the allocation is zeroed to ensure that it isn't left
 *   dangling limiting the use of UaFs.
 * - OOBs on the allocation is carefully considered and sufficiently
 *   addressed.
 *
 * @const KMEM_RANGE_ID_DATA
 * Range containing allocations that are bags of bytes and contain no
 * pointers.
 */
__enum_decl(vm_map_range_id_t, uint8_t, {
	KMEM_RANGE_ID_NONE,
	KMEM_RANGE_ID_PTR_0,
	KMEM_RANGE_ID_PTR_1,
	KMEM_RANGE_ID_PTR_2,
	KMEM_RANGE_ID_SPRAYQTN,
	KMEM_RANGE_ID_DATA,

	KMEM_RANGE_ID_FIRST   = KMEM_RANGE_ID_PTR_0,
	KMEM_RANGE_ID_NUM_PTR = KMEM_RANGE_ID_PTR_2,
	KMEM_RANGE_ID_MAX     = KMEM_RANGE_ID_DATA,

	/* these UMEM_* correspond to the MACH_VM_RANGE_* tags and are ABI */
	UMEM_RANGE_ID_DEFAULT = 0, /* same as MACH_VM_RANGE_DEFAULT */
	UMEM_RANGE_ID_HEAP,        /* same as MACH_VM_RANGE_DATA    */
	UMEM_RANGE_ID_FIXED,       /* same as MACH_VM_RANGE_FIXED   */
	UMEM_RANGE_ID_LARGE_FILE,

	/* these UMEM_* are XNU internal only range IDs, and aren't ABI */
	UMEM_RANGE_ID_MAX     = UMEM_RANGE_ID_LARGE_FILE,

#define KMEM_RANGE_COUNT        (KMEM_RANGE_ID_MAX + 1)
});

typedef vm_map_range_id_t       kmem_range_id_t;

#define kmem_log2down(mask)     (31 - __builtin_clz(mask))
#define KMEM_RANGE_MAX          (UMEM_RANGE_ID_MAX < KMEM_RANGE_ID_MAX \
	                        ? KMEM_RANGE_ID_MAX : UMEM_RANGE_ID_MAX)
#define KMEM_RANGE_BITS         kmem_log2down(2 * KMEM_RANGE_MAX - 1)

typedef union {
	struct {
		unsigned long long
		/*
		 * VM_FLAG_* flags
		 */
		    vmf_fixed:1,
		    vmf_purgeable:1,
		    vmf_4gb_chunk:1,
		    vmf_random_addr:1,
		    vmf_no_cache:1,
		    vmf_resilient_codesign:1,
		    vmf_resilient_media:1,
		    vmf_permanent:1,

		    __unused_bit_8:1,
		    __unused_bit_9:1,
		    __unused_bit_10:1,
		    __unused_bit_11:1,
		    vmf_tpro:1,
		__unused_bit_13:1,
		vmf_overwrite:1,
		    __unused_bit_15:1,

		    vmf_superpage_size:3,
		    __unused_bit_19:1,
		    vmf_return_data_addr:1,
		    __unused_bit_21:1,
		    __unused_bit_22:1,
		    vmf_return_4k_data_addr:1,

		/*
		 * VM tag (user or kernel)
		 *
		 * User tags are limited to 8 bits,
		 * kernel tags can use up to 12 bits
		 * with -zt or similar features.
		 */
		    vm_tag : 12, /* same as VME_ALIAS_BITS */

		/*
		 * General kernel flags
		 */
		    vmkf_already:1,             /* OK if same mapping already exists */
		    vmkf_beyond_max:1,          /* map beyond the map's max offset */
		    vmkf_no_pmap_check:1,       /* do not check that pmap is empty */
		    vmkf_map_jit:1,             /* mark entry as JIT region */
		    vmkf_iokit_acct:1,          /* IOKit accounting */
		    vmkf_keep_map_locked:1,     /* keep map locked when returning from vm_map_enter() */
		    vmkf_overwrite_immutable:1, /* can overwrite immutable mappings */
		    vmkf_remap_prot_copy:1,     /* vm_remap for VM_PROT_COPY */
		    vmkf_cs_enforcement_override:1,     /* override CS_ENFORCEMENT */
		    vmkf_cs_enforcement:1,      /* new value for CS_ENFORCEMENT */
		    vmkf_nested_pmap:1,         /* use a nested pmap */
		    vmkf_no_copy_on_read:1,     /* do not use copy_on_read */
		    vmkf_copy_single_object:1,  /* vm_map_copy only 1 VM object */
		    vmkf_copy_pageable:1,       /* vm_map_copy with pageable entries */
		    vmkf_copy_same_map:1,       /* vm_map_copy to remap in original map */
		    vmkf_translated_allow_execute:1,    /* allow execute in translated processes */
		    vmkf_tpro_enforcement_override:1,   /* override TPRO propagation */

		/*
		 * Submap creation, altering vm_map_enter() only
		 */
		    vmkf_submap:1,              /* mapping a VM submap */
		    vmkf_submap_atomic:1,       /* keep entry atomic (no splitting/coalescing) */
		    vmkf_submap_adjust:1,       /* the submap needs to be adjusted */

		/*
		 * Flags altering the behavior of vm_map_locate_space_anywhere()
		 */
		    vmkf_32bit_map_va:1,        /* allocate in low 32-bits range */
		    vmkf_guard_before:1,        /* guard page before the mapping */
		    vmkf_last_free:1,           /* find space from the end */
		    vmkf_range_id:KMEM_RANGE_BITS,      /* kmem range to allocate in */

		    __vmkf_unused:2;
	};

	/*
	 * do not access these directly,
	 * use vm_map_kernel_flags_check_vmflags*()
	 */
	uint32_t __vm_flags : 24;
} vm_map_kernel_flags_t;

/*
 * using this means that vmf_* flags can't be used
 * until vm_map_kernel_flags_set_vmflags() is set,
 * or some manual careful init is done.
 *
 * Prefer VM_MAP_KERNEL_FLAGS_(FIXED,ANYWHERE) instead.
 */
#define VM_MAP_KERNEL_FLAGS_NONE \
	(vm_map_kernel_flags_t){ }

#define VM_MAP_KERNEL_FLAGS_FIXED(...) \
	(vm_map_kernel_flags_t){ .vmf_fixed = true, __VA_ARGS__ }

#define VM_MAP_KERNEL_FLAGS_ANYWHERE(...) \
	(vm_map_kernel_flags_t){ .vmf_fixed = false, __VA_ARGS__ }

#define VM_MAP_KERNEL_FLAGS_FIXED_PERMANENT(...) \
	VM_MAP_KERNEL_FLAGS_FIXED(.vmf_permanent = true, __VA_ARGS__)

#define VM_MAP_KERNEL_FLAGS_ANYWHERE_PERMANENT(...) \
	VM_MAP_KERNEL_FLAGS_ANYWHERE(.vmf_permanent = true, __VA_ARGS__)

#define VM_MAP_KERNEL_FLAGS_DATA_ANYWHERE(...) \
	VM_MAP_KERNEL_FLAGS_ANYWHERE(.vmkf_range_id = KMEM_RANGE_ID_DATA, __VA_ARGS__)

typedef struct {
	unsigned int
	    vmnekf_ledger_tag:3,
	    vmnekf_ledger_no_footprint:1,
	    __vmnekf_unused:28;
} vm_named_entry_kernel_flags_t;
#define VM_NAMED_ENTRY_KERNEL_FLAGS_NONE (vm_named_entry_kernel_flags_t) {    \
	.vmnekf_ledger_tag = 0,                                                \
	.vmnekf_ledger_no_footprint = 0,                                       \
	.__vmnekf_unused = 0                                                   \
}

#endif /* XNU_KERNEL_PRIVATE */

/* current accounting postmark */
#define __VM_LEDGER_ACCOUNTING_POSTMARK 2019032600

/* discrete values: */
#define VM_LEDGER_TAG_NONE      0x00000000
#define VM_LEDGER_TAG_DEFAULT   0x00000001
#define VM_LEDGER_TAG_NETWORK   0x00000002
#define VM_LEDGER_TAG_MEDIA     0x00000003
#define VM_LEDGER_TAG_GRAPHICS  0x00000004
#define VM_LEDGER_TAG_NEURAL    0x00000005
#define VM_LEDGER_TAG_MAX       0x00000005
#define VM_LEDGER_TAG_UNCHANGED ((int)-1)

/* individual bits: */
#define VM_LEDGER_FLAG_NO_FOOTPRINT              (1 << 0)
#define VM_LEDGER_FLAG_NO_FOOTPRINT_FOR_DEBUG    (1 << 1)
#define VM_LEDGER_FLAG_FROM_KERNEL               (1 << 2)

#define VM_LEDGER_FLAGS_USER (VM_LEDGER_FLAG_NO_FOOTPRINT | VM_LEDGER_FLAG_NO_FOOTPRINT_FOR_DEBUG)
#define VM_LEDGER_FLAGS_ALL (VM_LEDGER_FLAGS_USER | VM_LEDGER_FLAG_FROM_KERNEL)

#define VM_MEMORY_MALLOC 1
#define VM_MEMORY_MALLOC_SMALL 2
#define VM_MEMORY_MALLOC_LARGE 3
#define VM_MEMORY_MALLOC_HUGE 4
#define VM_MEMORY_SBRK 5// uninteresting -- no one should call
#define VM_MEMORY_REALLOC 6
#define VM_MEMORY_MALLOC_TINY 7
#define VM_MEMORY_MALLOC_LARGE_REUSABLE 8
#define VM_MEMORY_MALLOC_LARGE_REUSED 9

#define VM_MEMORY_ANALYSIS_TOOL 10

#define VM_MEMORY_MALLOC_NANO 11
#define VM_MEMORY_MALLOC_MEDIUM 12
#define VM_MEMORY_MALLOC_PROB_GUARD 13

#define VM_MEMORY_MACH_MSG 20
#define VM_MEMORY_IOKIT 21
#define VM_MEMORY_STACK  30
#define VM_MEMORY_GUARD  31
#define VM_MEMORY_SHARED_PMAP 32
/* memory containing a dylib */
#define VM_MEMORY_DYLIB 33
#define VM_MEMORY_OBJC_DISPATCHERS 34

/* Was a nested pmap (VM_MEMORY_SHARED_PMAP) which has now been unnested */
#define VM_MEMORY_UNSHARED_PMAP 35


// Placeholders for now -- as we analyze the libraries and find how they
// use memory, we can make these labels more specific.
#define VM_MEMORY_APPKIT 40
#define VM_MEMORY_FOUNDATION 41
#define VM_MEMORY_COREGRAPHICS 42
#define VM_MEMORY_CORESERVICES 43
#define VM_MEMORY_CARBON VM_MEMORY_CORESERVICES
#define VM_MEMORY_JAVA 44
#define VM_MEMORY_COREDATA 45
#define VM_MEMORY_COREDATA_OBJECTIDS 46
#define VM_MEMORY_ATS 50
#define VM_MEMORY_LAYERKIT 51
#define VM_MEMORY_CGIMAGE 52
#define VM_MEMORY_TCMALLOC 53

/* private raster data (i.e. layers, some images, QGL allocator) */
#define VM_MEMORY_COREGRAPHICS_DATA     54

/* shared image and font caches */
#define VM_MEMORY_COREGRAPHICS_SHARED   55

/* Memory used for virtual framebuffers, shadowing buffers, etc... */
#define VM_MEMORY_COREGRAPHICS_FRAMEBUFFERS     56

/* Window backing stores, custom shadow data, and compressed backing stores */
#define VM_MEMORY_COREGRAPHICS_BACKINGSTORES    57

/* x-alloc'd memory */
#define VM_MEMORY_COREGRAPHICS_XALLOC 58

/* catch-all for other uses, such as the read-only shared data page */
#define VM_MEMORY_COREGRAPHICS_MISC VM_MEMORY_COREGRAPHICS

/* memory allocated by the dynamic loader for itself */
#define VM_MEMORY_DYLD 60
/* malloc'd memory created by dyld */
#define VM_MEMORY_DYLD_MALLOC 61

/* Used for sqlite page cache */
#define VM_MEMORY_SQLITE 62

/* JavaScriptCore heaps */
#define VM_MEMORY_JAVASCRIPT_CORE 63
#define VM_MEMORY_WEBASSEMBLY VM_MEMORY_JAVASCRIPT_CORE
/* memory allocated for the JIT */
#define VM_MEMORY_JAVASCRIPT_JIT_EXECUTABLE_ALLOCATOR 64
#define VM_MEMORY_JAVASCRIPT_JIT_REGISTER_FILE 65

/* memory allocated for GLSL */
#define VM_MEMORY_GLSL  66

/* memory allocated for OpenCL.framework */
#define VM_MEMORY_OPENCL    67

/* memory allocated for QuartzCore.framework */
#define VM_MEMORY_COREIMAGE 68

/* memory allocated for WebCore Purgeable Buffers */
#define VM_MEMORY_WEBCORE_PURGEABLE_BUFFERS 69

/* ImageIO memory */
#define VM_MEMORY_IMAGEIO       70

/* CoreProfile memory */
#define VM_MEMORY_COREPROFILE   71

/* assetsd / MobileSlideShow memory */
#define VM_MEMORY_ASSETSD       72

/* libsystem_kernel os_once_alloc */
#define VM_MEMORY_OS_ALLOC_ONCE 73

/* libdispatch internal allocator */
#define VM_MEMORY_LIBDISPATCH 74

/* Accelerate.framework image backing stores */
#define VM_MEMORY_ACCELERATE 75

/* CoreUI image block data */
#define VM_MEMORY_COREUI 76

/* CoreUI image file */
#define VM_MEMORY_COREUIFILE 77

/* Genealogy buffers */
#define VM_MEMORY_GENEALOGY 78

/* RawCamera VM allocated memory */
#define VM_MEMORY_RAWCAMERA 79

/* corpse info for dead process */
#define VM_MEMORY_CORPSEINFO 80

/* Apple System Logger (ASL) messages */
#define VM_MEMORY_ASL 81

/* Swift runtime */
#define VM_MEMORY_SWIFT_RUNTIME 82

/* Swift metadata */
#define VM_MEMORY_SWIFT_METADATA 83

/* DHMM data */
#define VM_MEMORY_DHMM 84


/* memory allocated by SceneKit.framework */
#define VM_MEMORY_SCENEKIT 86

/* memory allocated by skywalk networking */
#define VM_MEMORY_SKYWALK 87

#define VM_MEMORY_IOSURFACE 88

#define VM_MEMORY_LIBNETWORK 89

#define VM_MEMORY_AUDIO 90

#define VM_MEMORY_VIDEOBITSTREAM 91

/* memory allocated by CoreMedia */
#define VM_MEMORY_CM_XPC 92

#define VM_MEMORY_CM_RPC 93

#define VM_MEMORY_CM_MEMORYPOOL 94

#define VM_MEMORY_CM_READCACHE 95

#define VM_MEMORY_CM_CRABS 96

/* memory allocated for QuickLookThumbnailing */
#define VM_MEMORY_QUICKLOOK_THUMBNAILS 97

/* memory allocated by Accounts framework */
#define VM_MEMORY_ACCOUNTS 98

/* memory allocated by Sanitizer runtime libraries */
#define VM_MEMORY_SANITIZER 99

/* Differentiate memory needed by GPU drivers and frameworks from generic IOKit allocations */
#define VM_MEMORY_IOACCELERATOR 100

/* memory allocated by CoreMedia for global image registration of frames */
#define VM_MEMORY_CM_REGWARP 101

/* memory allocated by EmbeddedAcousticRecognition for speech decoder */
#define VM_MEMORY_EAR_DECODER 102

/* CoreUI cached image data */
#define VM_MEMORY_COREUI_CACHED_IMAGE_DATA 103

/* ColorSync is using mmap for read-only copies of ICC profile data */
#define VM_MEMORY_COLORSYNC 104

/* backtrace info for simulated crashes */
#define VM_MEMORY_BTINFO 105

/* memory allocated by CoreMedia */
#define VM_MEMORY_CM_HLS 106

/* Reserve 230-239 for Rosetta */
#define VM_MEMORY_ROSETTA 230
#define VM_MEMORY_ROSETTA_THREAD_CONTEXT 231
#define VM_MEMORY_ROSETTA_INDIRECT_BRANCH_MAP 232
#define VM_MEMORY_ROSETTA_RETURN_STACK 233
#define VM_MEMORY_ROSETTA_EXECUTABLE_HEAP 234
#define VM_MEMORY_ROSETTA_USER_LDT 235
#define VM_MEMORY_ROSETTA_ARENA 236
#define VM_MEMORY_ROSETTA_10 239

/* Reserve 240-255 for application */
#define VM_MEMORY_APPLICATION_SPECIFIC_1 240
#define VM_MEMORY_APPLICATION_SPECIFIC_16 255

#define VM_MEMORY_COUNT 256

#if !XNU_KERNEL_PRIVATE
#define VM_MAKE_TAG(tag) ((tag) << 24)
#endif /* XNU_KERNEL_PRIVATE */


#if KERNEL_PRIVATE

/* kernel map tags */
/* please add new definition strings to zprint */

#define VM_KERN_MEMORY_NONE             0

#define VM_KERN_MEMORY_OSFMK            1
#define VM_KERN_MEMORY_BSD              2
#define VM_KERN_MEMORY_IOKIT            3
#define VM_KERN_MEMORY_LIBKERN          4
#define VM_KERN_MEMORY_OSKEXT           5
#define VM_KERN_MEMORY_KEXT             6
#define VM_KERN_MEMORY_IPC              7
#define VM_KERN_MEMORY_STACK            8
#define VM_KERN_MEMORY_CPU              9
#define VM_KERN_MEMORY_PMAP             10
#define VM_KERN_MEMORY_PTE              11
#define VM_KERN_MEMORY_ZONE             12
#define VM_KERN_MEMORY_KALLOC           13
#define VM_KERN_MEMORY_COMPRESSOR       14
#define VM_KERN_MEMORY_COMPRESSED_DATA  15
#define VM_KERN_MEMORY_PHANTOM_CACHE    16
#define VM_KERN_MEMORY_WAITQ            17
#define VM_KERN_MEMORY_DIAG             18
#define VM_KERN_MEMORY_LOG              19
#define VM_KERN_MEMORY_FILE             20
#define VM_KERN_MEMORY_MBUF             21
#define VM_KERN_MEMORY_UBC              22
#define VM_KERN_MEMORY_SECURITY         23
#define VM_KERN_MEMORY_MLOCK            24
#define VM_KERN_MEMORY_REASON           25
#define VM_KERN_MEMORY_SKYWALK          26
#define VM_KERN_MEMORY_LTABLE           27
#define VM_KERN_MEMORY_HV               28
#define VM_KERN_MEMORY_KALLOC_DATA      29
#define VM_KERN_MEMORY_RETIRED          30
#define VM_KERN_MEMORY_KALLOC_TYPE      31
#define VM_KERN_MEMORY_TRIAGE           32
#define VM_KERN_MEMORY_RECOUNT          33
#define VM_KERN_MEMORY_EXCLAVES         35
/* add new tags here and adjust first-dynamic value */
#define VM_KERN_MEMORY_FIRST_DYNAMIC    36

/* out of tags: */
#define VM_KERN_MEMORY_ANY              255
#define VM_KERN_MEMORY_COUNT            256

/* end kernel map tags */

// mach_memory_info.flags
#define VM_KERN_SITE_TYPE               0x000000FF
#define VM_KERN_SITE_TAG                0x00000000
#define VM_KERN_SITE_KMOD               0x00000001
#define VM_KERN_SITE_KERNEL             0x00000002
#define VM_KERN_SITE_COUNTER            0x00000003
#define VM_KERN_SITE_WIRED              0x00000100      /* add to wired count */
#define VM_KERN_SITE_HIDE               0x00000200      /* no zprint */
#define VM_KERN_SITE_NAMED              0x00000400
#define VM_KERN_SITE_ZONE               0x00000800
#define VM_KERN_SITE_ZONE_VIEW          0x00001000
#define VM_KERN_SITE_KALLOC             0x00002000      /* zone field is size class */

#define VM_KERN_COUNT_MANAGED           0
#define VM_KERN_COUNT_RESERVED          1
#define VM_KERN_COUNT_WIRED             2
#define VM_KERN_COUNT_WIRED_MANAGED     3
#define VM_KERN_COUNT_STOLEN            4
#define VM_KERN_COUNT_LOPAGE            5
#define VM_KERN_COUNT_MAP_KERNEL        6
#define VM_KERN_COUNT_MAP_ZONE          7
#define VM_KERN_COUNT_MAP_KALLOC        8

#define VM_KERN_COUNT_WIRED_BOOT    9

#define VM_KERN_COUNT_BOOT_STOLEN       10

/* The number of bytes from the kernel cache that are wired in memory */
#define VM_KERN_COUNT_WIRED_STATIC_KERNELCACHE 11

#define VM_KERN_COUNT_MAP_KALLOC_LARGE  VM_KERN_COUNT_MAP_KALLOC
#define VM_KERN_COUNT_MAP_KALLOC_LARGE_DATA     12
#define VM_KERN_COUNT_MAP_KERNEL_DATA   13

#define VM_KERN_COUNTER_COUNT           14

#endif /* KERNEL_PRIVATE */

__END_DECLS

#endif  /* _MACH_VM_STATISTICS_H_ */
