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
 *	File:	vm/vm_map.h
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *	Date:	1985
 *
 *	Virtual memory map module definitions.
 *
 * Contributors:
 *	avie, dlb, mwyoung
 */

#ifndef _VM_VM_MAP_H_
#define _VM_VM_MAP_H_

#include <sys/cdefs.h>

#include <mach/mach_types.h>
#include <mach/kern_return.h>
#include <mach/boolean.h>
#include <mach/vm_types.h>
#include <mach/vm_prot.h>
#include <mach/vm_inherit.h>
#include <mach/vm_behavior.h>
#include <mach/vm_param.h>
#include <mach/sdt.h>
#include <vm/pmap.h>
#include <os/overflow.h>
#ifdef XNU_KERNEL_PRIVATE
#include <vm/vm_protos.h>
#endif /* XNU_KERNEL_PRIVATE */
#ifdef  MACH_KERNEL_PRIVATE
#include <mach_assert.h>
#include <vm/vm_map_store.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <kern/locks.h>
#include <kern/zalloc.h>
#include <kern/macro_help.h>

#include <kern/thread.h>
#include <os/refcnt.h>
#endif /* MACH_KERNEL_PRIVATE */

__BEGIN_DECLS

#ifdef  KERNEL_PRIVATE

extern void     vm_map_reference(vm_map_t       map);
extern vm_map_t current_map(void);

/* Setup reserved areas in a new VM map */
extern kern_return_t    vm_map_exec(
	vm_map_t                new_map,
	task_t                  task,
	boolean_t               is64bit,
	void                    *fsroot,
	cpu_type_t              cpu,
	cpu_subtype_t           cpu_subtype,
	boolean_t               reslide,
	boolean_t               is_driverkit,
	uint32_t                rsr_version);

#ifdef  MACH_KERNEL_PRIVATE

#define current_map_fast()      (current_thread()->map)
#define current_map()           (current_map_fast())

/*
 *	Types defined:
 *
 *	vm_map_t		the high-level address map data structure.
 *	vm_map_entry_t		an entry in an address map.
 *	vm_map_version_t	a timestamp of a map, for use with vm_map_lookup
 *	vm_map_copy_t		represents memory copied from an address map,
 *				 used for inter-map copy operations
 */
typedef struct vm_map_entry     *vm_map_entry_t;
#define VM_MAP_ENTRY_NULL       ((vm_map_entry_t) NULL)


#define named_entry_lock_init(object)   lck_mtx_init(&(object)->Lock, &vm_object_lck_grp, &vm_object_lck_attr)
#define named_entry_lock_destroy(object)        lck_mtx_destroy(&(object)->Lock, &vm_object_lck_grp)
#define named_entry_lock(object)                lck_mtx_lock(&(object)->Lock)
#define named_entry_unlock(object)              lck_mtx_unlock(&(object)->Lock)

/*
 *	Type:		vm_named_entry_t [internal use only]
 *
 *	Description:
 *		Description of a mapping to a memory cache object.
 *
 *	Implementation:
 *		While the handle to this object is used as a means to map
 *              and pass around the right to map regions backed by pagers
 *		of all sorts, the named_entry itself is only manipulated
 *		by the kernel.  Named entries hold information on the
 *		right to map a region of a cached object.  Namely,
 *		the target cache object, the beginning and ending of the
 *		region to be mapped, and the permissions, (read, write)
 *		with which it can be mapped.
 *
 */

struct vm_named_entry {
	decl_lck_mtx_data(, Lock);              /* Synchronization */
	union {
		vm_map_t        map;            /* map backing submap */
		vm_map_copy_t   copy;           /* a VM map copy */
	} backing;
	vm_object_offset_t      offset;         /* offset into object */
	vm_object_size_t        size;           /* size of region */
	vm_object_offset_t      data_offset;    /* offset to first byte of data */
	unsigned int                            /* Is backing.xxx : */
	/* unsigned  */ access:8,               /* MAP_MEM_* */
	/* vm_prot_t */ protection:4,           /* access permissions */
	/* boolean_t */ is_object:1,            /* ... a VM object (wrapped in a VM map copy) */
	/* boolean_t */ internal:1,             /* ... an internal object */
	/* boolean_t */ is_sub_map:1,           /* ... a submap? */
	/* boolean_t */ is_copy:1,              /* ... a VM map copy */
	/* boolean_t */ is_fully_owned:1;       /* ... all objects are owned */
#if VM_NAMED_ENTRY_DEBUG
	uint32_t                named_entry_bt; /* btref_t */
#endif /* VM_NAMED_ENTRY_DEBUG */
};

/*
 * Bit 3 of the protection and max_protection bitfields in a vm_map_entry
 * does not correspond to bit 3 of a vm_prot_t, so these macros provide a means
 * to convert between the "packed" representation in the vm_map_entry's fields
 * and the equivalent bits defined in vm_prot_t.
 */
#if defined(__x86_64__)
#define VM_VALID_VMPROTECT_FLAGS        (VM_PROT_ALL | VM_PROT_COPY | VM_PROT_UEXEC)
#else
#define VM_VALID_VMPROTECT_FLAGS        (VM_PROT_ALL | VM_PROT_COPY)
#endif

/*
 * FOOTPRINT ACCOUNTING:
 * The "memory footprint" is better described in the pmap layer.
 *
 * At the VM level, these 2 vm_map_entry_t fields are relevant:
 * iokit_mapped:
 *	For an "iokit_mapped" entry, we add the size of the entry to the
 *	footprint when the entry is entered into the map and we subtract that
 *	size when the entry is removed.  No other accounting should take place.
 *	"use_pmap" should be FALSE but is not taken into account.
 * use_pmap: (only when is_sub_map is FALSE)
 *	This indicates if we should ask the pmap layer to account for pages
 *	in this mapping.  If FALSE, we expect that another form of accounting
 *	is being used (e.g. "iokit_mapped" or the explicit accounting of
 *	non-volatile purgable memory).
 *
 * So the logic is mostly:
 * if entry->is_sub_map == TRUE
 *	anything in a submap does not count for the footprint
 * else if entry->iokit_mapped == TRUE
 *	footprint includes the entire virtual size of this entry
 * else if entry->use_pmap == FALSE
 *	tell pmap NOT to account for pages being pmap_enter()'d from this
 *	mapping (i.e. use "alternate accounting")
 * else
 *	pmap will account for pages being pmap_enter()'d from this mapping
 *	as it sees fit (only if anonymous, etc...)
 */

#define VME_ALIAS_BITS          12
#define VME_ALIAS_MASK          ((1u << VME_ALIAS_BITS) - 1)
#define VME_OFFSET_SHIFT        VME_ALIAS_BITS
#define VME_OFFSET_BITS         (64 - VME_ALIAS_BITS)
#define VME_SUBMAP_SHIFT        2
#define VME_SUBMAP_BITS         (sizeof(vm_offset_t) * 8 - VME_SUBMAP_SHIFT)

struct vm_map_entry {
	struct vm_map_links     links;                      /* links to other entries */
#define vme_prev                links.prev
#define vme_next                links.next
#define vme_start               links.start
#define vme_end                 links.end

	struct vm_map_store     store;

	union {
		vm_offset_t     vme_object_value;
		struct {
			vm_offset_t vme_atomic:1;           /* entry cannot be split/coalesced */
			vm_offset_t is_sub_map:1;           /* Is "object" a submap? */
			vm_offset_t vme_submap:VME_SUBMAP_BITS;
		};
		struct {
			uint32_t    vme_ctx_atomic : 1;
			uint32_t    vme_ctx_is_sub_map : 1;
			uint32_t    vme_context : 30;

			/**
			 * If vme_kernel_object==1 && KASAN,
			 * vme_object_or_delta holds the delta.
			 *
			 * If vme_kernel_object==1 && !KASAN,
			 * vme_tag_btref holds a btref when vme_alias is equal to the "vmtaglog"
			 * boot-arg.
			 *
			 * If vme_kernel_object==0,
			 * vme_object_or_delta holds the packed vm object.
			 */
			union {
				vm_page_object_t vme_object_or_delta;
				btref_t vme_tag_btref;
			};
		};
	};

	unsigned long long
	/* vm_tag_t          */ vme_alias:VME_ALIAS_BITS,   /* entry VM tag */
	/* vm_object_offset_t*/ vme_offset:VME_OFFSET_BITS, /* offset into object */

	/* boolean_t         */ is_shared:1,                /* region is shared */
	/* boolean_t         */ __unused1:1,
	/* boolean_t         */ in_transition:1,            /* Entry being changed */
	/* boolean_t         */ needs_wakeup:1,             /* Waiters on in_transition */
	/* behavior is not defined for submap type */
	/* vm_behavior_t     */ behavior:2,                 /* user paging behavior hint */
	/* boolean_t         */ needs_copy:1,               /* object need to be copied? */

	/* Only in task maps: */
#if defined(__arm64e__)
	/*
	 * On ARM, the fourth protection bit is unused (UEXEC is x86_64 only).
	 * We reuse it here to keep track of mappings that have hardware support
	 * for read-only/read-write trusted paths.
	 */
	/* vm_prot_t-like    */ protection:3,               /* protection code */
	/* boolean_t         */ used_for_tpro:1,
#else /* __arm64e__ */
	/* vm_prot_t-like    */protection:4,                /* protection code, bit3=UEXEC */
#endif /* __arm64e__ */

	/* vm_prot_t-like    */ max_protection:4,           /* maximum protection, bit3=UEXEC */
	/* vm_inherit_t      */ inheritance:2,              /* inheritance */

	/*
	 * use_pmap is overloaded:
	 * if "is_sub_map":
	 *      use a nested pmap?
	 * else (i.e. if object):
	 *      use pmap accounting
	 *      for footprint?
	 */
	/* boolean_t         */ use_pmap:1,
	/* boolean_t         */ no_cache:1,                 /* should new pages be cached? */
	/* boolean_t         */ vme_permanent:1,            /* mapping can not be removed */
	/* boolean_t         */ superpage_size:1,           /* use superpages of a certain size */
	/* boolean_t         */ map_aligned:1,              /* align to map's page size */
	/*
	 * zero out the wired pages of this entry
	 * if is being deleted without unwiring them
	 */
	/* boolean_t         */ zero_wired_pages:1,
	/* boolean_t         */ used_for_jit:1,
	/* boolean_t         */ csm_associated:1,       /* code signing monitor will validate */

	/* iokit accounting: use the virtual size rather than resident size: */
	/* boolean_t         */ iokit_acct:1,
	/* boolean_t         */ vme_resilient_codesign:1,
	/* boolean_t         */ vme_resilient_media:1,
	/* boolean_t         */ vme_xnu_user_debug:1,
	/* boolean_t         */ vme_no_copy_on_read:1,
	/* boolean_t         */ translated_allow_execute:1, /* execute in translated processes */
	/* boolean_t         */ vme_kernel_object:1;        /* vme_object is kernel_object */

	unsigned short          wired_count;                /* can be paged if = 0 */
	unsigned short          user_wired_count;           /* for vm_wire */

#if     DEBUG
#define MAP_ENTRY_CREATION_DEBUG (1)
#define MAP_ENTRY_INSERTION_DEBUG (1)
#endif /* DEBUG */
#if     MAP_ENTRY_CREATION_DEBUG
	struct vm_map_header    *vme_creation_maphdr;
	uint32_t                vme_creation_bt;            /* btref_t */
#endif /* MAP_ENTRY_CREATION_DEBUG */
#if     MAP_ENTRY_INSERTION_DEBUG
	uint32_t                vme_insertion_bt;           /* btref_t */
	vm_map_offset_t         vme_start_original;
	vm_map_offset_t         vme_end_original;
#endif /* MAP_ENTRY_INSERTION_DEBUG */
};

#define VME_ALIAS(entry) \
	((entry)->vme_alias)

static inline vm_map_t
_VME_SUBMAP(
	vm_map_entry_t entry)
{
	__builtin_assume(entry->vme_submap);
	return (vm_map_t)(entry->vme_submap << VME_SUBMAP_SHIFT);
}
#define VME_SUBMAP(entry) ({ assert((entry)->is_sub_map); _VME_SUBMAP(entry); })

static inline void
VME_SUBMAP_SET(
	vm_map_entry_t entry,
	vm_map_t submap)
{
	__builtin_assume(((vm_offset_t)submap & 3) == 0);

	entry->is_sub_map = true;
	entry->vme_submap = (vm_offset_t)submap >> VME_SUBMAP_SHIFT;
}

static inline vm_object_t
_VME_OBJECT(
	vm_map_entry_t entry)
{
	vm_object_t object;

	if (!entry->vme_kernel_object) {
		object = VM_OBJECT_UNPACK(entry->vme_object_or_delta);
		__builtin_assume(!is_kernel_object(object));
	} else {
		object = kernel_object_default;
	}
	return object;
}
#define VME_OBJECT(entry) ({ assert(!(entry)->is_sub_map); _VME_OBJECT(entry); })

static inline void
VME_OBJECT_SET(
	vm_map_entry_t entry,
	vm_object_t    object,
	bool           atomic,
	uint32_t       context)
{
	__builtin_assume(((vm_offset_t)object & 3) == 0);

	entry->vme_atomic = atomic;
	entry->is_sub_map = false;
	if (atomic) {
		entry->vme_context = context;
	} else {
		entry->vme_context = 0;
	}

	if (!object) {
		entry->vme_object_or_delta = 0;
	} else if (is_kernel_object(object)) {
#if VM_BTLOG_TAGS
		if (!(entry->vme_kernel_object && entry->vme_tag_btref))
#endif /* VM_BTLOG_TAGS */
		{
			entry->vme_object_or_delta = 0;
		}
	} else {
#if VM_BTLOG_TAGS
		if (entry->vme_kernel_object && entry->vme_tag_btref) {
			btref_put(entry->vme_tag_btref);
		}
#endif /* VM_BTLOG_TAGS */
		entry->vme_object_or_delta = VM_OBJECT_PACK(object);
	}

	entry->vme_kernel_object = is_kernel_object(object);
	entry->vme_resilient_codesign = false;
	entry->used_for_jit = false;
}

static inline vm_object_offset_t
VME_OFFSET(
	vm_map_entry_t entry)
{
	return entry->vme_offset << VME_OFFSET_SHIFT;
}

static inline void
VME_OFFSET_SET(
	vm_map_entry_t entry,
	vm_object_offset_t offset)
{
	entry->vme_offset = offset >> VME_OFFSET_SHIFT;
	assert3u(VME_OFFSET(entry), ==, offset);
}

/*
 * IMPORTANT:
 * The "alias" field can be updated while holding the VM map lock
 * "shared".  It's OK as along as it's the only field that can be
 * updated without the VM map "exclusive" lock.
 */
static inline void
VME_ALIAS_SET(
	vm_map_entry_t entry,
	unsigned int alias)
{
	assert3u(alias & VME_ALIAS_MASK, ==, alias);
	entry->vme_alias = alias;
}

static inline void
VME_OBJECT_SHADOW(
	vm_map_entry_t entry,
	vm_object_size_t length,
	bool always)
{
	vm_object_t object;
	vm_object_offset_t offset;

	object = VME_OBJECT(entry);
	offset = VME_OFFSET(entry);
	vm_object_shadow(&object, &offset, length, always);
	if (object != VME_OBJECT(entry)) {
		entry->vme_object_or_delta = VM_OBJECT_PACK(object);
		entry->use_pmap = true;
	}
	if (offset != VME_OFFSET(entry)) {
		VME_OFFSET_SET(entry, offset);
	}
}

#if (DEBUG || DEVELOPMENT) && !KASAN
#define VM_BTLOG_TAGS 1
#else
#define VM_BTLOG_TAGS 0
#endif

extern vm_tag_t vmtaglog_tag; /* Collected from a tunable in vm_resident.c */
static inline void
vme_btref_consider_and_set(__unused vm_map_entry_t entry, __unused void *fp)
{
#if VM_BTLOG_TAGS
	if (vmtaglog_tag && (VME_ALIAS(entry) == vmtaglog_tag) && entry->vme_kernel_object && entry->wired_count) {
		assert(!entry->vme_tag_btref); /* We should have already zeroed and freed the btref if we're here. */
		entry->vme_tag_btref = btref_get(fp, BTREF_GET_NOWAIT);
	}
#endif /* VM_BTLOG_TAGS */
}

static inline void
vme_btref_consider_and_put(__unused vm_map_entry_t entry)
{
#if VM_BTLOG_TAGS
	if (entry->vme_tag_btref && entry->vme_kernel_object && (entry->wired_count == 0) && (entry->user_wired_count == 0)) {
		btref_put(entry->vme_tag_btref);
		entry->vme_tag_btref = 0;
	}
#endif /* VM_BTLOG_TAGS */
}


/*
 * Convenience macros for dealing with superpages
 * SUPERPAGE_NBASEPAGES is architecture dependent and defined in pmap.h
 */
#define SUPERPAGE_SIZE (PAGE_SIZE*SUPERPAGE_NBASEPAGES)
#define SUPERPAGE_MASK (-SUPERPAGE_SIZE)
#define SUPERPAGE_ROUND_DOWN(a) (a & SUPERPAGE_MASK)
#define SUPERPAGE_ROUND_UP(a) ((a + SUPERPAGE_SIZE-1) & SUPERPAGE_MASK)

/*
 * wired_counts are unsigned short.  This value is used to safeguard
 * against any mishaps due to runaway user programs.
 */
#define MAX_WIRE_COUNT          65535

typedef struct vm_map_user_range {
	vm_map_address_t        vmur_min_address __kernel_data_semantics;

	vm_map_address_t        vmur_max_address : 56 __kernel_data_semantics;
	vm_map_range_id_t       vmur_range_id : 8;
} *vm_map_user_range_t;

/*
 *	Type:		vm_map_t [exported; contents invisible]
 *
 *	Description:
 *		An address map -- a directory relating valid
 *		regions of a task's address space to the corresponding
 *		virtual memory objects.
 *
 *	Implementation:
 *		Maps are doubly-linked lists of map entries, sorted
 *		by address.  One hint is used to start
 *		searches again from the last successful search,
 *		insertion, or removal.  Another hint is used to
 *		quickly find free space.
 *
 *	Note:
 *		vm_map_relocate_early_elem() knows about this layout,
 *		and needs to be kept in sync.
 */
struct _vm_map {
	lck_rw_t                lock;           /* map lock */
	struct vm_map_header    hdr;            /* Map entry header */
#define min_offset              hdr.links.start /* start of range */
#define max_offset              hdr.links.end   /* end of range */
	pmap_t                  XNU_PTRAUTH_SIGNED_PTR("_vm_map.pmap") pmap;           /* Physical map */
	vm_map_size_t           size;           /* virtual size */
	uint64_t                size_limit;     /* rlimit on address space size */
	uint64_t                data_limit;     /* rlimit on data size */
	vm_map_size_t           user_wire_limit;/* rlimit on user locked memory */
	vm_map_size_t           user_wire_size; /* current size of user locked memory in this map */
#if __x86_64__
	vm_map_offset_t         vmmap_high_start;
#endif /* __x86_64__ */

	os_ref_atomic_t         map_refcnt;       /* Reference count */

#if CONFIG_MAP_RANGES
#define VM_MAP_EXTRA_RANGES_MAX 1024
	struct mach_vm_range    default_range;
	struct mach_vm_range    data_range;

	uint16_t                extra_ranges_count;
	vm_map_user_range_t     extra_ranges;
#endif /* CONFIG_MAP_RANGES */

	union {
		/*
		 * If map->disable_vmentry_reuse == TRUE:
		 * the end address of the highest allocated vm_map_entry_t.
		 */
		vm_map_offset_t         vmu1_highest_entry_end;
		/*
		 * For a nested VM map:
		 * the lowest address in this nested VM map that we would
		 * expect to be unnested under normal operation (i.e. for
		 * regular copy-on-write on DATA section).
		 */
		vm_map_offset_t         vmu1_lowest_unnestable_start;
	} vmu1;
#define highest_entry_end       vmu1.vmu1_highest_entry_end
#define lowest_unnestable_start vmu1.vmu1_lowest_unnestable_start
	vm_map_entry_t          hint;           /* hint for quick lookups */
	union {
		struct vm_map_links* vmmap_hole_hint;   /* hint for quick hole lookups */
		struct vm_map_corpse_footprint_header *vmmap_corpse_footprint;
	} vmmap_u_1;
#define hole_hint vmmap_u_1.vmmap_hole_hint
#define vmmap_corpse_footprint vmmap_u_1.vmmap_corpse_footprint
	union {
		vm_map_entry_t          _first_free;    /* First free space hint */
		struct vm_map_links*    _holes;         /* links all holes between entries */
	} f_s;                                          /* Union for free space data structures being used */

#define first_free              f_s._first_free
#define holes_list              f_s._holes

	unsigned int
	/* boolean_t */ wait_for_space:1,         /* Should callers wait for space? */
	/* boolean_t */ wiring_required:1,        /* All memory wired? */
	/* boolean_t */ no_zero_fill:1,           /* No zero fill absent pages */
	/* boolean_t */ mapped_in_other_pmaps:1,  /* has this submap been mapped in maps that use a different pmap */
	/* boolean_t */ switch_protect:1,         /* Protect map from write faults while switched */
	/* boolean_t */ disable_vmentry_reuse:1,  /* All vm entries should keep using newer and higher addresses in the map */
	/* boolean_t */ map_disallow_data_exec:1, /* Disallow execution from data pages on exec-permissive architectures */
	/* boolean_t */ holelistenabled:1,
	/* boolean_t */ is_nested_map:1,
	/* boolean_t */ map_disallow_new_exec:1,  /* Disallow new executable code */
	/* boolean_t */ jit_entry_exists:1,
	/* boolean_t */ has_corpse_footprint:1,
	/* boolean_t */ terminated:1,
	/* boolean_t */ is_alien:1,               /* for platform simulation, i.e. PLATFORM_IOS on OSX */
	/* boolean_t */ cs_enforcement:1,         /* code-signing enforcement */
	/* boolean_t */ cs_debugged:1,            /* code-signed but debugged */
	/* boolean_t */ reserved_regions:1,       /* has reserved regions. The map size that userspace sees should ignore these. */
	/* boolean_t */ single_jit:1,             /* only allow one JIT mapping */
	/* boolean_t */ never_faults:1,           /* this map should never cause faults */
	/* boolean_t */ uses_user_ranges:1,       /* has the map been configured to use user VM ranges */
	/* boolean_t */ tpro_enforcement:1,       /* enforce TPRO propagation */
	/* boolean_t */ corpse_source:1,          /* map is being used to create a corpse for diagnostics.*/
	/* reserved */ res0:1,
	/* reserved  */pad:9;
	unsigned int            timestamp;        /* Version number */
};

#define CAST_TO_VM_MAP_ENTRY(x) ((struct vm_map_entry *)(uintptr_t)(x))
#define vm_map_to_entry(map) CAST_TO_VM_MAP_ENTRY(&(map)->hdr.links)
#define vm_map_first_entry(map) ((map)->hdr.links.next)
#define vm_map_last_entry(map)  ((map)->hdr.links.prev)

/*
 *	Type:		vm_map_version_t [exported; contents invisible]
 *
 *	Description:
 *		Map versions may be used to quickly validate a previous
 *		lookup operation.
 *
 *	Usage note:
 *		Because they are bulky objects, map versions are usually
 *		passed by reference.
 *
 *	Implementation:
 *		Just a timestamp for the main map.
 */
typedef struct vm_map_version {
	unsigned int    main_timestamp;
} vm_map_version_t;

/*
 *	Type:		vm_map_copy_t [exported; contents invisible]
 *
 *	Description:
 *		A map copy object represents a region of virtual memory
 *		that has been copied from an address map but is still
 *		in transit.
 *
 *		A map copy object may only be used by a single thread
 *		at a time.
 *
 *	Implementation:
 *              There are two formats for map copy objects.
 *		The first is very similar to the main
 *		address map in structure, and as a result, some
 *		of the internal maintenance functions/macros can
 *		be used with either address maps or map copy objects.
 *
 *		The map copy object contains a header links
 *		entry onto which the other entries that represent
 *		the region are chained.
 *
 *		The second format is a kernel buffer copy object - for data
 *              small enough that physical copies were the most efficient
 *		method. This method uses a zero-sized array unioned with
 *		other format-specific data in the 'c_u' member. This unsized
 *		array overlaps the other elements and allows us to use this
 *		extra structure space for physical memory copies. On 64-bit
 *		systems this saves ~64 bytes per vm_map_copy.
 */

struct vm_map_copy {
#define VM_MAP_COPY_ENTRY_LIST          1
#define VM_MAP_COPY_KERNEL_BUFFER       2
	uint16_t                type;
	bool                    is_kernel_range;
	bool                    is_user_range;
	vm_map_range_id_t       orig_range;
	vm_object_offset_t      offset;
	vm_map_size_t           size;
	union {
		struct vm_map_header                  hdr;    /* ENTRY_LIST */
		void *XNU_PTRAUTH_SIGNED_PTR("vm_map_copy.kdata") kdata;  /* KERNEL_BUFFER */
	} c_u;
};


ZONE_DECLARE_ID(ZONE_ID_VM_MAP_ENTRY, struct vm_map_entry);
#define vm_map_entry_zone       (&zone_array[ZONE_ID_VM_MAP_ENTRY])

ZONE_DECLARE_ID(ZONE_ID_VM_MAP_HOLES, struct vm_map_links);
#define vm_map_holes_zone       (&zone_array[ZONE_ID_VM_MAP_HOLES])

ZONE_DECLARE_ID(ZONE_ID_VM_MAP, struct _vm_map);
#define vm_map_zone             (&zone_array[ZONE_ID_VM_MAP])


#define cpy_hdr                 c_u.hdr
#define cpy_kdata               c_u.kdata

#define VM_MAP_COPY_PAGE_SHIFT(copy) ((copy)->cpy_hdr.page_shift)
#define VM_MAP_COPY_PAGE_SIZE(copy) (1 << VM_MAP_COPY_PAGE_SHIFT((copy)))
#define VM_MAP_COPY_PAGE_MASK(copy) (VM_MAP_COPY_PAGE_SIZE((copy)) - 1)

/*
 *	Useful macros for entry list copy objects
 */

#define vm_map_copy_to_entry(copy) CAST_TO_VM_MAP_ENTRY(&(copy)->cpy_hdr.links)
#define vm_map_copy_first_entry(copy)           \
	        ((copy)->cpy_hdr.links.next)
#define vm_map_copy_last_entry(copy)            \
	        ((copy)->cpy_hdr.links.prev)

extern kern_return_t
vm_map_copy_adjust_to_target(
	vm_map_copy_t           copy_map,
	vm_map_offset_t         offset,
	vm_map_size_t           size,
	vm_map_t                target_map,
	boolean_t               copy,
	vm_map_copy_t           *target_copy_map_p,
	vm_map_offset_t         *overmap_start_p,
	vm_map_offset_t         *overmap_end_p,
	vm_map_offset_t         *trimmed_start_p);

/*
 *	Macros:		vm_map_lock, etc. [internal use only]
 *	Description:
 *		Perform locking on the data portion of a map.
 *	When multiple maps are to be locked, order by map address.
 *	(See vm_map.c::vm_remap())
 */

#define vm_map_lock_init(map)                                           \
	((map)->timestamp = 0 ,                                         \
	lck_rw_init(&(map)->lock, &vm_map_lck_grp, &vm_map_lck_rw_attr))

#define vm_map_lock(map)                     \
	MACRO_BEGIN                          \
	DTRACE_VM(vm_map_lock_w);            \
	lck_rw_lock_exclusive(&(map)->lock); \
	MACRO_END

#define vm_map_unlock(map)          \
	MACRO_BEGIN                 \
	DTRACE_VM(vm_map_unlock_w); \
	(map)->timestamp++;         \
	lck_rw_done(&(map)->lock);  \
	MACRO_END

#define vm_map_lock_read(map)             \
	MACRO_BEGIN                       \
	DTRACE_VM(vm_map_lock_r);         \
	lck_rw_lock_shared(&(map)->lock); \
	MACRO_END

#define vm_map_unlock_read(map)     \
	MACRO_BEGIN                 \
	DTRACE_VM(vm_map_unlock_r); \
	lck_rw_done(&(map)->lock);  \
	MACRO_END

#define vm_map_lock_write_to_read(map)                 \
	MACRO_BEGIN                                    \
	DTRACE_VM(vm_map_lock_downgrade);              \
	(map)->timestamp++;                            \
	lck_rw_lock_exclusive_to_shared(&(map)->lock); \
	MACRO_END

__attribute__((always_inline))
int vm_map_lock_read_to_write(vm_map_t map);

__attribute__((always_inline))
boolean_t vm_map_try_lock(vm_map_t map);

__attribute__((always_inline))
boolean_t vm_map_try_lock_read(vm_map_t map);

int vm_self_region_page_shift(vm_map_t target_map);
int vm_self_region_page_shift_safely(vm_map_t target_map);

#define vm_map_lock_assert_held(map) \
	LCK_RW_ASSERT(&(map)->lock, LCK_RW_ASSERT_HELD)
#define vm_map_lock_assert_shared(map)  \
	LCK_RW_ASSERT(&(map)->lock, LCK_RW_ASSERT_SHARED)
#define vm_map_lock_assert_exclusive(map) \
	LCK_RW_ASSERT(&(map)->lock, LCK_RW_ASSERT_EXCLUSIVE)
#define vm_map_lock_assert_notheld(map) \
	LCK_RW_ASSERT(&(map)->lock, LCK_RW_ASSERT_NOTHELD)

/*
 *	Exported procedures that operate on vm_map_t.
 */

/* Lookup map entry containing or the specified address in the given map */
extern boolean_t        vm_map_lookup_entry(
	vm_map_t                map,
	vm_map_address_t        address,
	vm_map_entry_t          *entry);                                /* OUT */

/* Lookup map entry containing or the specified address in the given map */
extern boolean_t        vm_map_lookup_entry_or_next(
	vm_map_t                map,
	vm_map_address_t        address,
	vm_map_entry_t          *entry);                                /* OUT */

/* like vm_map_lookup_entry without the PGZ bear trap */
#if CONFIG_PROB_GZALLOC
extern boolean_t        vm_map_lookup_entry_allow_pgz(
	vm_map_t                map,
	vm_map_address_t        address,
	vm_map_entry_t          *entry);                                /* OUT */
#else /* !CONFIG_PROB_GZALLOC */
#define vm_map_lookup_entry_allow_pgz vm_map_lookup_entry
#endif /* !CONFIG_PROB_GZALLOC */

extern void             vm_map_copy_remap(
	vm_map_t                map,
	vm_map_entry_t          where,
	vm_map_copy_t           copy,
	vm_map_offset_t         adjustment,
	vm_prot_t               cur_prot,
	vm_prot_t               max_prot,
	vm_inherit_t            inheritance);

/* Find the VM object, offset, and protection for a given virtual address
 * in the specified map, assuming a page fault of the	type specified. */
extern kern_return_t    vm_map_lookup_and_lock_object(
	vm_map_t                *var_map,                               /* IN/OUT */
	vm_map_address_t        vaddr,
	vm_prot_t               fault_type,
	int                     object_lock_type,
	vm_map_version_t        *out_version,                           /* OUT */
	vm_object_t             *object,                                /* OUT */
	vm_object_offset_t      *offset,                                /* OUT */
	vm_prot_t               *out_prot,                              /* OUT */
	boolean_t               *wired,                                 /* OUT */
	vm_object_fault_info_t  fault_info,                             /* OUT */
	vm_map_t                *real_map,                              /* OUT */
	bool                    *contended);                            /* OUT */

/* Verifies that the map has not changed since the given version. */
extern boolean_t        vm_map_verify(
	vm_map_t                map,
	vm_map_version_t        *version);                              /* REF */


/*
 *	Functions implemented as macros
 */
#define         vm_map_min(map) ((map)->min_offset)
/* Lowest valid address in
 * a map */

#define         vm_map_max(map) ((map)->max_offset)
/* Highest valid address */

#define         vm_map_pmap(map)        ((map)->pmap)
/* Physical map associated
* with this address map */

/* Gain a reference to an existing map */
extern void             vm_map_reference(
	vm_map_t        map);

/*
 *	Wait and wakeup macros for in_transition map entries.
 */
#define vm_map_entry_wait(map, interruptible)           \
	((map)->timestamp++ ,                           \
	 lck_rw_sleep(&(map)->lock, LCK_SLEEP_EXCLUSIVE|LCK_SLEEP_PROMOTED_PRI, \
	                          (event_t)&(map)->hdr,	interruptible))


#define vm_map_entry_wakeup(map)        \
	thread_wakeup((event_t)(&(map)->hdr))


/* simplify map entries */
extern void             vm_map_simplify_entry(
	vm_map_t        map,
	vm_map_entry_t  this_entry);
extern void             vm_map_simplify(
	vm_map_t                map,
	vm_map_offset_t         start);

#if XNU_PLATFORM_MacOSX

/* Move the information in a map copy object to a new map copy object */
extern vm_map_copy_t    vm_map_copy_copy(
	vm_map_copy_t           copy);

#endif /* XNU_PLATFORM_MacOSX */

/* Enter a mapping */
extern kern_return_t    vm_map_enter(
	vm_map_t                map,
	vm_map_offset_t         *address,
	vm_map_size_t           size,
	vm_map_offset_t         mask,
	vm_map_kernel_flags_t   vmk_flags,
	vm_object_t             object,
	vm_object_offset_t      offset,
	boolean_t               needs_copy,
	vm_prot_t               cur_protection,
	vm_prot_t               max_protection,
	vm_inherit_t            inheritance);

#if __arm64__
extern kern_return_t    vm_map_enter_fourk(
	vm_map_t                map,
	vm_map_offset_t         *address,
	vm_map_size_t           size,
	vm_map_offset_t         mask,
	vm_map_kernel_flags_t   vmk_flags,
	vm_object_t             object,
	vm_object_offset_t      offset,
	boolean_t               needs_copy,
	vm_prot_t               cur_protection,
	vm_prot_t               max_protection,
	vm_inherit_t            inheritance);
#endif /* __arm64__ */

/* XXX should go away - replaced with regular enter of contig object */
extern  kern_return_t   vm_map_enter_cpm(
	vm_map_t                map,
	vm_map_address_t        *addr,
	vm_map_size_t           size,
	vm_map_kernel_flags_t   vmk_flags);

extern kern_return_t vm_map_remap(
	vm_map_t                target_map,
	vm_map_offset_t         *address,
	vm_map_size_t           size,
	vm_map_offset_t         mask,
	vm_map_kernel_flags_t   vmk_flags,
	vm_map_t                src_map,
	vm_map_offset_t         memory_address,
	boolean_t               copy,
	vm_prot_t               *cur_protection,
	vm_prot_t               *max_protection,
	vm_inherit_t            inheritance);


/*
 * Read and write from a kernel buffer to a specified map.
 */
extern  kern_return_t   vm_map_write_user(
	vm_map_t                map,
	void                    *src_p,
	vm_map_offset_t         dst_addr,
	vm_size_t               size);

extern  kern_return_t   vm_map_read_user(
	vm_map_t                map,
	vm_map_offset_t         src_addr,
	void                    *dst_p,
	vm_size_t               size);

extern void             vm_map_inherit_limits(
	vm_map_t                new_map,
	const struct _vm_map   *old_map);

/* Create a new task map using an existing task map as a template. */
extern vm_map_t         vm_map_fork(
	ledger_t                ledger,
	vm_map_t                old_map,
	int                     options);
#define VM_MAP_FORK_SHARE_IF_INHERIT_NONE       0x00000001
#define VM_MAP_FORK_PRESERVE_PURGEABLE          0x00000002
#define VM_MAP_FORK_CORPSE_FOOTPRINT            0x00000004

/* Change inheritance */
extern kern_return_t    vm_map_inherit(
	vm_map_t                map,
	vm_map_offset_t         start,
	vm_map_offset_t         end,
	vm_inherit_t            new_inheritance);

/* Add or remove machine-dependent attributes from map regions */
extern kern_return_t    vm_map_machine_attribute(
	vm_map_t                map,
	vm_map_offset_t         start,
	vm_map_offset_t         end,
	vm_machine_attribute_t  attribute,
	vm_machine_attribute_val_t* value);                         /* IN/OUT */

extern kern_return_t    vm_map_msync(
	vm_map_t                map,
	vm_map_address_t        address,
	vm_map_size_t           size,
	vm_sync_t               sync_flags);

/* Set paging behavior */
extern kern_return_t    vm_map_behavior_set(
	vm_map_t                map,
	vm_map_offset_t         start,
	vm_map_offset_t         end,
	vm_behavior_t           new_behavior);

extern kern_return_t vm_map_region(
	vm_map_t                 map,
	vm_map_offset_t         *address,
	vm_map_size_t           *size,
	vm_region_flavor_t       flavor,
	vm_region_info_t         info,
	mach_msg_type_number_t  *count,
	mach_port_t             *object_name);

extern kern_return_t vm_map_region_recurse_64(
	vm_map_t                 map,
	vm_map_offset_t         *address,
	vm_map_size_t           *size,
	natural_t               *nesting_depth,
	vm_region_submap_info_64_t info,
	mach_msg_type_number_t  *count);

extern kern_return_t vm_map_page_query_internal(
	vm_map_t                map,
	vm_map_offset_t         offset,
	int                     *disposition,
	int                     *ref_count);

extern kern_return_t vm_map_query_volatile(
	vm_map_t        map,
	mach_vm_size_t  *volatile_virtual_size_p,
	mach_vm_size_t  *volatile_resident_size_p,
	mach_vm_size_t  *volatile_compressed_size_p,
	mach_vm_size_t  *volatile_pmap_size_p,
	mach_vm_size_t  *volatile_compressed_pmap_size_p);

/* Convert from a map entry port to a map */
extern vm_map_t convert_port_entry_to_map(
	ipc_port_t      port);


extern kern_return_t vm_map_set_cache_attr(
	vm_map_t        map,
	vm_map_offset_t va);


/* definitions related to overriding the NX behavior */

#define VM_ABI_32       0x1
#define VM_ABI_64       0x2

extern int override_nx(vm_map_t map, uint32_t user_tag);

extern void vm_map_region_top_walk(
	vm_map_entry_t entry,
	vm_region_top_info_t top);
extern void vm_map_region_walk(
	vm_map_t map,
	vm_map_offset_t va,
	vm_map_entry_t entry,
	vm_object_offset_t offset,
	vm_object_size_t range,
	vm_region_extended_info_t extended,
	boolean_t look_for_pages,
	mach_msg_type_number_t count);



extern void vm_map_copy_footprint_ledgers(
	task_t  old_task,
	task_t  new_task);
extern void vm_map_copy_ledger(
	task_t  old_task,
	task_t  new_task,
	int     ledger_entry);

/**
 * Represents a single region of virtual address space that should be reserved
 * (pre-mapped) in a user address space.
 */
struct vm_reserved_region {
	const char             *vmrr_name;
	vm_map_offset_t         vmrr_addr;
	vm_map_size_t           vmrr_size;
};

/**
 * Return back a machine-dependent array of address space regions that should be
 * reserved by the VM. This function is defined in the machine-dependent
 * machine_routines.c files.
 */
extern size_t ml_get_vm_reserved_regions(
	bool                    vm_is64bit,
	const struct vm_reserved_region **regions);

/**
 * Explicitly preallocates a floating point save area. This function is defined
 * in the machine-dependent machine_routines.c files.
 */
extern void ml_fp_save_area_prealloc(void);

#endif /* MACH_KERNEL_PRIVATE */

/* Create an empty map */
extern vm_map_t         vm_map_create(
	pmap_t                  pmap,
	vm_map_offset_t         min_off,
	vm_map_offset_t         max_off,
	boolean_t               pageable);

extern vm_map_size_t    vm_map_adjusted_size(vm_map_t map);

extern void             vm_map_disable_hole_optimization(vm_map_t map);

/* Get rid of a map */
extern void             vm_map_destroy(
	vm_map_t                map);

/* Lose a reference */
extern void             vm_map_deallocate(
	vm_map_t                map);

/* Lose a reference */
extern void             vm_map_inspect_deallocate(
	vm_map_inspect_t        map);

/* Lose a reference */
extern void             vm_map_read_deallocate(
	vm_map_read_t        map);

extern vm_map_t         vm_map_switch(
	vm_map_t                map);

/* Change protection */
extern kern_return_t    vm_map_protect(
	vm_map_t                map,
	vm_map_offset_t         start,
	vm_map_offset_t         end,
	vm_prot_t               new_prot,
	boolean_t               set_max);

/* Check protection */
extern boolean_t vm_map_check_protection(
	vm_map_t                map,
	vm_map_offset_t         start,
	vm_map_offset_t         end,
	vm_prot_t               protection);

extern boolean_t vm_map_cs_enforcement(
	vm_map_t                map);
extern void vm_map_cs_enforcement_set(
	vm_map_t                map,
	boolean_t               val);

extern void vm_map_cs_debugged_set(
	vm_map_t map,
	boolean_t val);

extern kern_return_t vm_map_cs_wx_enable(vm_map_t map);
extern kern_return_t vm_map_csm_allow_jit(vm_map_t map);

/* wire down a region */

#ifdef XNU_KERNEL_PRIVATE

extern void vm_map_will_allocate_early_map(
	vm_map_t               *map_owner);

extern void vm_map_relocate_early_maps(
	vm_offset_t             delta);

extern void vm_map_relocate_early_elem(
	uint32_t                zone_id,
	vm_offset_t             new_addr,
	vm_offset_t             delta);

/* never fails */
extern vm_map_t vm_map_create_options(
	pmap_t                  pmap,
	vm_map_offset_t         min_off,
	vm_map_offset_t         max_off,
	vm_map_create_options_t options);

extern kern_return_t    vm_map_wire_kernel(
	vm_map_t                map,
	vm_map_offset_t         start,
	vm_map_offset_t         end,
	vm_prot_t               access_type,
	vm_tag_t                tag,
	boolean_t               user_wire);

extern kern_return_t    vm_map_wire_and_extract_kernel(
	vm_map_t                map,
	vm_map_offset_t         start,
	vm_prot_t               access_type,
	vm_tag_t                tag,
	boolean_t               user_wire,
	ppnum_t                 *physpage_p);

/* kext exported versions */

extern kern_return_t    vm_map_wire_external(
	vm_map_t                map,
	vm_map_offset_t         start,
	vm_map_offset_t         end,
	vm_prot_t               access_type,
	boolean_t               user_wire);

extern kern_return_t    vm_map_wire_and_extract_external(
	vm_map_t                map,
	vm_map_offset_t         start,
	vm_prot_t               access_type,
	boolean_t               user_wire,
	ppnum_t                 *physpage_p);

#else /* XNU_KERNEL_PRIVATE */

extern kern_return_t    vm_map_wire(
	vm_map_t                map,
	vm_map_offset_t         start,
	vm_map_offset_t         end,
	vm_prot_t               access_type,
	boolean_t               user_wire);

extern kern_return_t    vm_map_wire_and_extract(
	vm_map_t                map,
	vm_map_offset_t         start,
	vm_prot_t               access_type,
	boolean_t               user_wire,
	ppnum_t                 *physpage_p);

#endif /* !XNU_KERNEL_PRIVATE */

/* unwire a region */
extern kern_return_t    vm_map_unwire(
	vm_map_t                map,
	vm_map_offset_t         start,
	vm_map_offset_t         end,
	boolean_t               user_wire);

#ifdef XNU_KERNEL_PRIVATE

/* Enter a mapping of a memory object */
extern kern_return_t    vm_map_enter_mem_object(
	vm_map_t                map,
	vm_map_offset_t         *address,
	vm_map_size_t           size,
	vm_map_offset_t         mask,
	vm_map_kernel_flags_t   vmk_flags,
	ipc_port_t              port,
	vm_object_offset_t      offset,
	boolean_t               needs_copy,
	vm_prot_t               cur_protection,
	vm_prot_t               max_protection,
	vm_inherit_t            inheritance);

/* Enter a mapping of a memory object */
extern kern_return_t    vm_map_enter_mem_object_prefault(
	vm_map_t                map,
	vm_map_offset_t         *address,
	vm_map_size_t           size,
	vm_map_offset_t         mask,
	vm_map_kernel_flags_t   vmk_flags,
	ipc_port_t              port,
	vm_object_offset_t      offset,
	vm_prot_t               cur_protection,
	vm_prot_t               max_protection,
	upl_page_list_ptr_t     page_list,
	unsigned int            page_list_count);

/* Enter a mapping of a memory object */
extern kern_return_t    vm_map_enter_mem_object_control(
	vm_map_t                map,
	vm_map_offset_t         *address,
	vm_map_size_t           size,
	vm_map_offset_t         mask,
	vm_map_kernel_flags_t   vmk_flags,
	memory_object_control_t control,
	vm_object_offset_t      offset,
	boolean_t               needs_copy,
	vm_prot_t               cur_protection,
	vm_prot_t               max_protection,
	vm_inherit_t            inheritance);

extern kern_return_t    vm_map_terminate(
	vm_map_t                map);

extern void             vm_map_require(
	vm_map_t                map);

extern void             vm_map_copy_require(
	vm_map_copy_t           copy);

extern kern_return_t    vm_map_copy_extract(
	vm_map_t                src_map,
	vm_map_address_t        src_addr,
	vm_map_size_t           len,
	boolean_t               copy,
	vm_map_copy_t           *copy_result,   /* OUT */
	vm_prot_t               *cur_prot,      /* OUT */
	vm_prot_t               *max_prot,      /* OUT */
	vm_inherit_t            inheritance,
	vm_map_kernel_flags_t   vmk_flags);

#endif /* !XNU_KERNEL_PRIVATE */

/* Discard a copy without using it */
extern void             vm_map_copy_discard(
	vm_map_copy_t           copy);

/* Overwrite existing memory with a copy */
extern kern_return_t    vm_map_copy_overwrite(
	vm_map_t                dst_map,
	vm_map_address_t        dst_addr,
	vm_map_copy_t           copy,
	vm_map_size_t           copy_size,
	boolean_t               interruptible);

#define VM_MAP_COPY_OVERWRITE_OPTIMIZATION_THRESHOLD_PAGES      (3)


/* returns TRUE if size of vm_map_copy == size parameter FALSE otherwise */
extern boolean_t        vm_map_copy_validate_size(
	vm_map_t                dst_map,
	vm_map_copy_t           copy,
	vm_map_size_t           *size);

/* Place a copy into a map */
extern kern_return_t    vm_map_copyout(
	vm_map_t                dst_map,
	vm_map_address_t        *dst_addr,                              /* OUT */
	vm_map_copy_t           copy);

extern kern_return_t vm_map_copyout_size(
	vm_map_t                dst_map,
	vm_map_address_t        *dst_addr,                              /* OUT */
	vm_map_copy_t           copy,
	vm_map_size_t           copy_size);

extern kern_return_t    vm_map_copyout_internal(
	vm_map_t                dst_map,
	vm_map_address_t        *dst_addr,      /* OUT */
	vm_map_copy_t           copy,
	vm_map_size_t           copy_size,
	boolean_t               consume_on_success,
	vm_prot_t               cur_protection,
	vm_prot_t               max_protection,
	vm_inherit_t            inheritance);

extern kern_return_t    vm_map_copyin(
	vm_map_t                src_map,
	vm_map_address_t        src_addr,
	vm_map_size_t           len,
	boolean_t               src_destroy,
	vm_map_copy_t           *copy_result);                          /* OUT */

extern kern_return_t    vm_map_copyin_common(
	vm_map_t                src_map,
	vm_map_address_t        src_addr,
	vm_map_size_t           len,
	boolean_t               src_destroy,
	boolean_t               src_volatile,
	vm_map_copy_t           *copy_result,                           /* OUT */
	boolean_t               use_maxprot);

#define VM_MAP_COPYIN_SRC_DESTROY       0x00000001
#define VM_MAP_COPYIN_USE_MAXPROT       0x00000002
#define VM_MAP_COPYIN_ENTRY_LIST        0x00000004
#define VM_MAP_COPYIN_PRESERVE_PURGEABLE 0x00000008
#define VM_MAP_COPYIN_FORK              0x00000010
#define VM_MAP_COPYIN_ALL_FLAGS         0x0000001F
extern kern_return_t    vm_map_copyin_internal(
	vm_map_t                src_map,
	vm_map_address_t        src_addr,
	vm_map_size_t           len,
	int                     flags,
	vm_map_copy_t           *copy_result);                         /* OUT */


extern void             vm_map_disable_NX(
	vm_map_t                map);

extern void             vm_map_disallow_data_exec(
	vm_map_t                map);

extern void             vm_map_set_64bit(
	vm_map_t                map);

extern void             vm_map_set_32bit(
	vm_map_t                map);

extern void             vm_map_set_jumbo(
	vm_map_t                map);

extern void             vm_map_set_jit_entitled(
	vm_map_t                map);

extern void             vm_map_set_max_addr(
	vm_map_t                map, vm_map_offset_t new_max_offset);

extern boolean_t        vm_map_has_hard_pagezero(
	vm_map_t                map,
	vm_map_offset_t         pagezero_size);
extern void             vm_commit_pagezero_status(vm_map_t      tmap);

extern boolean_t        vm_map_tpro(
	vm_map_t                map);

extern void             vm_map_set_tpro(
	vm_map_t                map);

extern boolean_t        vm_map_tpro_enforcement(
	vm_map_t                map);

extern void             vm_map_set_tpro_enforcement(
	vm_map_t                map);

extern boolean_t        vm_map_set_tpro_range(
	vm_map_t                map,
	vm_map_address_t        start,
	vm_map_address_t        end);

extern boolean_t        vm_map_is_64bit(
	vm_map_t                map);

extern kern_return_t    vm_map_raise_max_offset(
	vm_map_t        map,
	vm_map_offset_t new_max_offset);

extern kern_return_t    vm_map_raise_min_offset(
	vm_map_t        map,
	vm_map_offset_t new_min_offset);

#if XNU_TARGET_OS_OSX
extern void vm_map_set_high_start(
	vm_map_t        map,
	vm_map_offset_t high_start);
#endif /* XNU_TARGET_OS_OSX */

extern vm_map_offset_t  vm_compute_max_offset(
	boolean_t               is64);

extern void             vm_map_get_max_aslr_slide_section(
	vm_map_t                map,
	int64_t                 *max_sections,
	int64_t                 *section_size);

extern uint64_t         vm_map_get_max_aslr_slide_pages(
	vm_map_t map);

extern uint64_t         vm_map_get_max_loader_aslr_slide_pages(
	vm_map_t map);

extern kern_return_t    vm_map_set_size_limit(
	vm_map_t                map,
	uint64_t                limit);

extern kern_return_t    vm_map_set_data_limit(
	vm_map_t                map,
	uint64_t                limit);

extern void             vm_map_set_user_wire_limit(
	vm_map_t                map,
	vm_size_t               limit);

extern void vm_map_switch_protect(
	vm_map_t                map,
	boolean_t               val);

extern void vm_map_iokit_mapped_region(
	vm_map_t                map,
	vm_size_t               bytes);

extern void vm_map_iokit_unmapped_region(
	vm_map_t                map,
	vm_size_t               bytes);


extern boolean_t first_free_is_valid(vm_map_t);

extern int              vm_map_page_shift(
	vm_map_t                map);

extern vm_map_offset_t  vm_map_page_mask(
	vm_map_t                map);

extern int              vm_map_page_size(
	vm_map_t                map);

extern vm_map_offset_t  vm_map_round_page_mask(
	vm_map_offset_t         offset,
	vm_map_offset_t         mask);

extern vm_map_offset_t  vm_map_trunc_page_mask(
	vm_map_offset_t         offset,
	vm_map_offset_t         mask);

extern boolean_t        vm_map_page_aligned(
	vm_map_offset_t         offset,
	vm_map_offset_t         mask);

extern bool vm_map_range_overflows(
	vm_map_t                map,
	vm_map_offset_t         addr,
	vm_map_size_t           size);
#ifdef XNU_KERNEL_PRIVATE

/* Support for vm_map ranges */
extern kern_return_t    vm_map_range_configure(
	vm_map_t                map);

extern void             vm_map_range_fork(
	vm_map_t                new_map,
	vm_map_t                old_map);

extern int              vm_map_get_user_range(
	vm_map_t                map,
	vm_map_range_id_t       range_id,
	mach_vm_range_t         range);

/*!
 * @function vm_map_kernel_flags_update_range_id()
 *
 * @brief
 * Updates the @c vmkf_range_id field with the adequate value
 * according to the policy for specified map and tag set in @c vmk_flags.
 *
 * @discussion
 * This function is meant to be called by Mach VM entry points,
 * which matters for the kernel: allocations with pointers _MUST_
 * be allocated with @c kmem_*() functions.
 *
 * If the range ID is already set, it is preserved.
 */
extern void             vm_map_kernel_flags_update_range_id(
	vm_map_kernel_flags_t  *flags,
	vm_map_t                map);

#if XNU_TARGET_OS_OSX
extern void vm_map_mark_alien(vm_map_t map);
extern void vm_map_single_jit(vm_map_t map);
#endif /* XNU_TARGET_OS_OSX */

extern kern_return_t vm_map_page_info(
	vm_map_t                map,
	vm_map_offset_t         offset,
	vm_page_info_flavor_t   flavor,
	vm_page_info_t          info,
	mach_msg_type_number_t  *count);
extern kern_return_t vm_map_page_range_info_internal(
	vm_map_t                map,
	vm_map_offset_t         start_offset,
	vm_map_offset_t         end_offset,
	int                     effective_page_shift,
	vm_page_info_flavor_t   flavor,
	vm_page_info_t          info,
	mach_msg_type_number_t  *count);

#endif /* XNU_KERNEL_PRIVATE */
#ifdef  MACH_KERNEL_PRIVATE


/*
 * Internal macros for rounding and truncation of vm_map offsets and sizes
 */
#define VM_MAP_ROUND_PAGE(x, pgmask) (((vm_map_offset_t)(x) + (pgmask)) & ~((signed)(pgmask)))
#define VM_MAP_TRUNC_PAGE(x, pgmask) ((vm_map_offset_t)(x) & ~((signed)(pgmask)))

/*
 * Macros for rounding and truncation of vm_map offsets and sizes
 */
static inline int
VM_MAP_PAGE_SHIFT(
	vm_map_t map)
{
	int shift = map ? map->hdr.page_shift : PAGE_SHIFT;
	/*
	 * help ubsan and codegen in general,
	 * cannot use PAGE_{MIN,MAX}_SHIFT
	 * because of testing code which
	 * tests 16k aligned maps on 4k only systems.
	 */
	__builtin_assume(shift >= 12 && shift <= 14);
	return shift;
}

#define VM_MAP_PAGE_SIZE(map) (1 << VM_MAP_PAGE_SHIFT((map)))
#define VM_MAP_PAGE_MASK(map) (VM_MAP_PAGE_SIZE((map)) - 1)
#define VM_MAP_PAGE_ALIGNED(x, pgmask) (((x) & (pgmask)) == 0)

static inline bool
VM_MAP_IS_EXOTIC(
	vm_map_t map __unused)
{
#if __arm64__
	if (VM_MAP_PAGE_SHIFT(map) < PAGE_SHIFT ||
	    pmap_is_exotic(map->pmap)) {
		return true;
	}
#endif /* __arm64__ */
	return false;
}

static inline bool
VM_MAP_IS_ALIEN(
	vm_map_t map __unused)
{
	/*
	 * An "alien" process/task/map/pmap should mostly behave
	 * as it currently would on iOS.
	 */
#if XNU_TARGET_OS_OSX
	if (map->is_alien) {
		return true;
	}
	return false;
#else /* XNU_TARGET_OS_OSX */
	return true;
#endif /* XNU_TARGET_OS_OSX */
}

static inline bool
VM_MAP_POLICY_WX_FAIL(
	vm_map_t map __unused)
{
	if (VM_MAP_IS_ALIEN(map)) {
		return false;
	}
	return true;
}

static inline bool
VM_MAP_POLICY_WX_STRIP_X(
	vm_map_t map __unused)
{
	if (VM_MAP_IS_ALIEN(map)) {
		return true;
	}
	return false;
}

static inline bool
VM_MAP_POLICY_ALLOW_MULTIPLE_JIT(
	vm_map_t map __unused)
{
	if (VM_MAP_IS_ALIEN(map) || map->single_jit) {
		return false;
	}
	return true;
}

static inline bool
VM_MAP_POLICY_ALLOW_JIT_RANDOM_ADDRESS(
	vm_map_t map)
{
	return VM_MAP_IS_ALIEN(map);
}

static inline bool
VM_MAP_POLICY_ALLOW_JIT_INHERIT(
	vm_map_t map __unused)
{
	if (VM_MAP_IS_ALIEN(map)) {
		return false;
	}
	return true;
}

static inline bool
VM_MAP_POLICY_ALLOW_JIT_SHARING(
	vm_map_t map __unused)
{
	if (VM_MAP_IS_ALIEN(map)) {
		return false;
	}
	return true;
}

static inline bool
VM_MAP_POLICY_ALLOW_JIT_COPY(
	vm_map_t map __unused)
{
	if (VM_MAP_IS_ALIEN(map)) {
		return false;
	}
	return true;
}

static inline bool
VM_MAP_POLICY_WRITABLE_SHARED_REGION(
	vm_map_t map __unused)
{
#if __x86_64__
	return true;
#else /* __x86_64__ */
	if (VM_MAP_IS_EXOTIC(map)) {
		return true;
	}
	return false;
#endif /* __x86_64__ */
}

static inline void
vm_prot_to_wimg(unsigned int prot, unsigned int *wimg)
{
	switch (prot) {
	case MAP_MEM_NOOP:                      break;
	case MAP_MEM_IO:                        *wimg = VM_WIMG_IO; break;
	case MAP_MEM_COPYBACK:                  *wimg = VM_WIMG_USE_DEFAULT; break;
	case MAP_MEM_INNERWBACK:                *wimg = VM_WIMG_INNERWBACK; break;
	case MAP_MEM_POSTED:                    *wimg = VM_WIMG_POSTED; break;
	case MAP_MEM_POSTED_REORDERED:          *wimg = VM_WIMG_POSTED_REORDERED; break;
	case MAP_MEM_POSTED_COMBINED_REORDERED: *wimg = VM_WIMG_POSTED_COMBINED_REORDERED; break;
	case MAP_MEM_WTHRU:                     *wimg = VM_WIMG_WTHRU; break;
	case MAP_MEM_WCOMB:                     *wimg = VM_WIMG_WCOMB; break;
	case MAP_MEM_RT:                        *wimg = VM_WIMG_RT; break;
	default:                                break;
	}
}

static inline boolean_t
vm_map_always_shadow(vm_map_t map)
{
	if (map->mapped_in_other_pmaps) {
		/*
		 * This is a submap, mapped in other maps.
		 * Even if a VM object is mapped only once in this submap,
		 * the submap itself could be mapped multiple times,
		 * so vm_object_shadow() should always create a shadow
		 * object, even if the object has only 1 reference.
		 */
		return TRUE;
	}
	return FALSE;
}

#endif /* MACH_KERNEL_PRIVATE */
#ifdef XNU_KERNEL_PRIVATE

extern kern_return_t vm_map_set_page_shift(vm_map_t map, int pageshift);
extern bool vm_map_is_exotic(vm_map_t map);
extern bool vm_map_is_alien(vm_map_t map);
extern pmap_t vm_map_get_pmap(vm_map_t map);

extern bool vm_map_is_corpse_source(vm_map_t map);
extern void vm_map_set_corpse_source(vm_map_t map);
extern void vm_map_unset_corpse_source(vm_map_t map);
#endif /* XNU_KERNEL_PRIVATE */

#define vm_map_round_page(x, pgmask) (((vm_map_offset_t)(x) + (pgmask)) & ~((signed)(pgmask)))
#define vm_map_trunc_page(x, pgmask) ((vm_map_offset_t)(x) & ~((signed)(pgmask)))

/* Support for UPLs from vm_maps */

#ifdef XNU_KERNEL_PRIVATE

extern kern_return_t vm_map_get_upl(
	vm_map_t                target_map,
	vm_map_offset_t         map_offset,
	upl_size_t              *size,
	upl_t                   *upl,
	upl_page_info_array_t   page_info,
	unsigned int            *page_infoCnt,
	upl_control_flags_t     *flags,
	vm_tag_t                tag,
	int                     force_data_sync);

#endif /* XNU_KERNEL_PRIVATE */

extern void
vm_map_sizes(vm_map_t map,
    vm_map_size_t * psize,
    vm_map_size_t * pfree,
    vm_map_size_t * plargest_free);

#if CONFIG_DYNAMIC_CODE_SIGNING

extern kern_return_t vm_map_sign(vm_map_t map,
    vm_map_offset_t start,
    vm_map_offset_t end);

#endif /* CONFIG_DYNAMIC_CODE_SIGNING */

extern kern_return_t vm_map_partial_reap(
	vm_map_t map,
	unsigned int *reclaimed_resident,
	unsigned int *reclaimed_compressed);


#if DEVELOPMENT || DEBUG

extern int vm_map_disconnect_page_mappings(
	vm_map_t map,
	boolean_t);

extern kern_return_t vm_map_inject_error(vm_map_t map, vm_map_offset_t vaddr);

#endif /* DEVELOPMENT || DEBUG */

#if CONFIG_FREEZE

extern kern_return_t vm_map_freeze(
	task_t       task,
	unsigned int *purgeable_count,
	unsigned int *wired_count,
	unsigned int *clean_count,
	unsigned int *dirty_count,
	unsigned int dirty_budget,
	unsigned int *shared_count,
	int          *freezer_error_code,
	boolean_t    eval_only);

__enum_decl(freezer_error_code_t, int, {
	FREEZER_ERROR_GENERIC = -1,
	FREEZER_ERROR_EXCESS_SHARED_MEMORY = -2,
	FREEZER_ERROR_LOW_PRIVATE_SHARED_RATIO = -3,
	FREEZER_ERROR_NO_COMPRESSOR_SPACE = -4,
	FREEZER_ERROR_NO_SWAP_SPACE = -5,
	FREEZER_ERROR_NO_SLOTS = -6,
});

#endif /* CONFIG_FREEZE */
#if XNU_KERNEL_PRIVATE

boolean_t        kdp_vm_map_is_acquired_exclusive(vm_map_t map);

boolean_t        vm_map_entry_has_device_pager(vm_map_t, vm_map_offset_t vaddr);

#endif /* XNU_KERNEL_PRIVATE */

/*
 * In some cases, we don't have a real VM object but still want to return a
 * unique ID (to avoid a memory region looking like shared memory), so build
 * a fake pointer based on the map's ledger and the index of the ledger being
 * reported.
 */
#define VM_OBJECT_ID_FAKE(map, ledger_id) ((uint32_t)(uintptr_t)VM_KERNEL_ADDRHASH((int*)((map)->pmap->ledger)+(ledger_id)))

#endif  /* KERNEL_PRIVATE */

__END_DECLS

#endif  /* _VM_VM_MAP_H_ */
