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
 *	File:	vm/vm_map.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *	Date:	1985
 *
 *	Virtual memory mapping module.
 */

#include <mach/vm_types.h>
#include <mach_assert.h>

#include <vm/vm_options.h>

#include <libkern/OSAtomic.h>

#include <mach/kern_return.h>
#include <mach/port.h>
#include <mach/vm_attributes.h>
#include <mach/vm_param.h>
#include <mach/vm_behavior.h>
#include <mach/vm_statistics.h>
#include <mach/memory_object.h>
#include <mach/mach_vm.h>
#include <machine/cpu_capabilities.h>
#include <mach/sdt.h>

#include <kern/assert.h>
#include <kern/backtrace.h>
#include <kern/counter.h>
#include <kern/exc_guard.h>
#include <kern/kalloc.h>
#include <kern/zalloc_internal.h>

#include <vm/cpm.h>
#include <vm/vm_compressor.h>
#include <vm/vm_compressor_pager.h>
#include <vm/vm_init.h>
#include <vm/vm_fault.h>
#include <vm/vm_map_internal.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <ipc/ipc_port.h>
#include <kern/sched_prim.h>
#include <kern/misc_protos.h>

#include <mach/vm_map_server.h>
#include <mach/mach_host_server.h>
#include <vm/vm_memtag.h>
#include <vm/vm_protos.h>
#include <vm/vm_purgeable_internal.h>
#include <vm/vm_reclaim_internal.h>

#include <vm/vm_protos.h>
#include <vm/vm_shared_region.h>
#include <vm/vm_map_store.h>

#include <san/kasan.h>

#include <sys/resource.h>
#include <sys/random.h>
#include <sys/codesign.h>
#include <sys/code_signing.h>
#include <sys/mman.h>
#include <sys/reboot.h>
#include <sys/kdebug_triage.h>

#include <libkern/section_keywords.h>

#if DEVELOPMENT || DEBUG
extern int proc_selfcsflags(void);
int vm_log_xnu_user_debug = 0;
int panic_on_unsigned_execute = 0;
int panic_on_mlock_failure = 0;
#endif /* DEVELOPMENT || DEBUG */

#if MACH_ASSERT
int debug4k_filter = 0;
char debug4k_proc_name[1024] = "";
int debug4k_proc_filter = (int)-1 & ~(1 << __DEBUG4K_FAULT);
int debug4k_panic_on_misaligned_sharing = 0;
const char *debug4k_category_name[] = {
	"error",        /* 0 */
	"life",         /* 1 */
	"load",         /* 2 */
	"fault",        /* 3 */
	"copy",         /* 4 */
	"share",        /* 5 */
	"adjust",       /* 6 */
	"pmap",         /* 7 */
	"mementry",     /* 8 */
	"iokit",        /* 9 */
	"upl",          /* 10 */
	"exc",          /* 11 */
	"vfs"           /* 12 */
};
#endif /* MACH_ASSERT */
int debug4k_no_cow_copyin = 0;


#if __arm64__
extern const int fourk_binary_compatibility_unsafe;
extern const int fourk_binary_compatibility_allow_wx;
#endif /* __arm64__ */
extern void qsort(void *a, size_t n, size_t es, int (*cmp)(const void *, const void *));
extern int proc_selfpid(void);
extern char *proc_name_address(void *p);
extern char *proc_best_name(struct proc *p);

#if VM_MAP_DEBUG_APPLE_PROTECT
int vm_map_debug_apple_protect = 0;
#endif /* VM_MAP_DEBUG_APPLE_PROTECT */
#if VM_MAP_DEBUG_FOURK
int vm_map_debug_fourk = 0;
#endif /* VM_MAP_DEBUG_FOURK */

#if DEBUG || DEVELOPMENT
static TUNABLE(bool, vm_map_executable_immutable,
    "vm_map_executable_immutable", true);
#else
#define vm_map_executable_immutable true
#endif

os_refgrp_decl(static, map_refgrp, "vm_map", NULL);

extern u_int32_t random(void);  /* from <libkern/libkern.h> */
/* Internal prototypes
 */

typedef struct vm_map_zap {
	vm_map_entry_t          vmz_head;
	vm_map_entry_t         *vmz_tail;
} *vm_map_zap_t;

#define VM_MAP_ZAP_DECLARE(zap) \
	struct vm_map_zap zap = { .vmz_tail = &zap.vmz_head }

static vm_map_entry_t   vm_map_entry_insert(
	vm_map_t                map,
	vm_map_entry_t          insp_entry,
	vm_map_offset_t         start,
	vm_map_offset_t         end,
	vm_object_t             object,
	vm_object_offset_t      offset,
	vm_map_kernel_flags_t   vmk_flags,
	boolean_t               needs_copy,
	vm_prot_t               cur_protection,
	vm_prot_t               max_protection,
	vm_inherit_t            inheritance,
	boolean_t               clear_map_aligned);

static void vm_map_simplify_range(
	vm_map_t        map,
	vm_map_offset_t start,
	vm_map_offset_t end);   /* forward */

static boolean_t        vm_map_range_check(
	vm_map_t        map,
	vm_map_offset_t start,
	vm_map_offset_t end,
	vm_map_entry_t  *entry);

static void vm_map_submap_pmap_clean(
	vm_map_t        map,
	vm_map_offset_t start,
	vm_map_offset_t end,
	vm_map_t        sub_map,
	vm_map_offset_t offset);

static void             vm_map_pmap_enter(
	vm_map_t                map,
	vm_map_offset_t         addr,
	vm_map_offset_t         end_addr,
	vm_object_t             object,
	vm_object_offset_t      offset,
	vm_prot_t               protection);

static void             _vm_map_clip_end(
	struct vm_map_header    *map_header,
	vm_map_entry_t          entry,
	vm_map_offset_t         end);

static void             _vm_map_clip_start(
	struct vm_map_header    *map_header,
	vm_map_entry_t          entry,
	vm_map_offset_t         start);

static kmem_return_t vm_map_delete(
	vm_map_t        map,
	vm_map_offset_t start,
	vm_map_offset_t end,
	vmr_flags_t     flags,
	kmem_guard_t    guard,
	vm_map_zap_t    zap);

static void             vm_map_copy_insert(
	vm_map_t        map,
	vm_map_entry_t  after_where,
	vm_map_copy_t   copy);

static kern_return_t    vm_map_copy_overwrite_unaligned(
	vm_map_t        dst_map,
	vm_map_entry_t  entry,
	vm_map_copy_t   copy,
	vm_map_address_t start,
	boolean_t       discard_on_success);

static kern_return_t    vm_map_copy_overwrite_aligned(
	vm_map_t        dst_map,
	vm_map_entry_t  tmp_entry,
	vm_map_copy_t   copy,
	vm_map_offset_t start,
	pmap_t          pmap);

static kern_return_t    vm_map_copyin_kernel_buffer(
	vm_map_t        src_map,
	vm_map_address_t src_addr,
	vm_map_size_t   len,
	boolean_t       src_destroy,
	vm_map_copy_t   *copy_result);  /* OUT */

static kern_return_t    vm_map_copyout_kernel_buffer(
	vm_map_t        map,
	vm_map_address_t *addr, /* IN/OUT */
	vm_map_copy_t   copy,
	vm_map_size_t   copy_size,
	boolean_t       overwrite,
	boolean_t       consume_on_success);

static void             vm_map_fork_share(
	vm_map_t        old_map,
	vm_map_entry_t  old_entry,
	vm_map_t        new_map);

static boolean_t        vm_map_fork_copy(
	vm_map_t        old_map,
	vm_map_entry_t  *old_entry_p,
	vm_map_t        new_map,
	int             vm_map_copyin_flags);

static kern_return_t    vm_map_wire_nested(
	vm_map_t                   map,
	vm_map_offset_t            start,
	vm_map_offset_t            end,
	vm_prot_t                  caller_prot,
	vm_tag_t                   tag,
	boolean_t                  user_wire,
	pmap_t                     map_pmap,
	vm_map_offset_t            pmap_addr,
	ppnum_t                    *physpage_p);

static kern_return_t    vm_map_unwire_nested(
	vm_map_t                   map,
	vm_map_offset_t            start,
	vm_map_offset_t            end,
	boolean_t                  user_wire,
	pmap_t                     map_pmap,
	vm_map_offset_t            pmap_addr);

static kern_return_t    vm_map_overwrite_submap_recurse(
	vm_map_t                   dst_map,
	vm_map_offset_t            dst_addr,
	vm_map_size_t              dst_size);

static kern_return_t    vm_map_copy_overwrite_nested(
	vm_map_t                   dst_map,
	vm_map_offset_t            dst_addr,
	vm_map_copy_t              copy,
	boolean_t                  interruptible,
	pmap_t                     pmap,
	boolean_t                  discard_on_success);

static kern_return_t    vm_map_remap_extract(
	vm_map_t                map,
	vm_map_offset_t         addr,
	vm_map_size_t           size,
	boolean_t               copy,
	vm_map_copy_t           map_copy,
	vm_prot_t               *cur_protection,
	vm_prot_t               *max_protection,
	vm_inherit_t            inheritance,
	vm_map_kernel_flags_t   vmk_flags);

static kern_return_t    vm_map_remap_range_allocate(
	vm_map_t                map,
	vm_map_address_t        *address,
	vm_map_size_t           size,
	vm_map_offset_t         mask,
	vm_map_kernel_flags_t   vmk_flags,
	vm_map_entry_t          *map_entry,
	vm_map_zap_t            zap_list);

static void             vm_map_region_look_for_page(
	vm_map_t                   map,
	vm_map_offset_t            va,
	vm_object_t                object,
	vm_object_offset_t         offset,
	int                        max_refcnt,
	unsigned short             depth,
	vm_region_extended_info_t  extended,
	mach_msg_type_number_t count);

static int              vm_map_region_count_obj_refs(
	vm_map_entry_t             entry,
	vm_object_t                object);


static kern_return_t    vm_map_willneed(
	vm_map_t        map,
	vm_map_offset_t start,
	vm_map_offset_t end);

static kern_return_t    vm_map_reuse_pages(
	vm_map_t        map,
	vm_map_offset_t start,
	vm_map_offset_t end);

static kern_return_t    vm_map_reusable_pages(
	vm_map_t        map,
	vm_map_offset_t start,
	vm_map_offset_t end);

static kern_return_t    vm_map_can_reuse(
	vm_map_t        map,
	vm_map_offset_t start,
	vm_map_offset_t end);

static kern_return_t    vm_map_zero(
	vm_map_t        map,
	vm_map_offset_t start,
	vm_map_offset_t end);

static kern_return_t    vm_map_random_address_for_size(
	vm_map_t                map,
	vm_map_offset_t        *address,
	vm_map_size_t           size,
	vm_map_kernel_flags_t   vmk_flags);


#if CONFIG_MAP_RANGES

static vm_map_range_id_t vm_map_user_range_resolve(
	vm_map_t                map,
	mach_vm_address_t       addr,
	mach_vm_address_t       size,
	mach_vm_range_t         range);

#endif /* CONFIG_MAP_RANGES */
#if MACH_ASSERT
static kern_return_t    vm_map_pageout(
	vm_map_t        map,
	vm_map_offset_t start,
	vm_map_offset_t end);
#endif /* MACH_ASSERT */

kern_return_t vm_map_corpse_footprint_collect(
	vm_map_t        old_map,
	vm_map_entry_t  old_entry,
	vm_map_t        new_map);
void vm_map_corpse_footprint_collect_done(
	vm_map_t        new_map);
void vm_map_corpse_footprint_destroy(
	vm_map_t        map);
kern_return_t vm_map_corpse_footprint_query_page_info(
	vm_map_t        map,
	vm_map_offset_t va,
	int             *disposition_p);
void vm_map_footprint_query_page_info(
	vm_map_t        map,
	vm_map_entry_t  map_entry,
	vm_map_offset_t curr_s_offset,
	int             *disposition_p);

#if CONFIG_MAP_RANGES
static void vm_map_range_map_init(void);
#endif /* CONFIG_MAP_RANGES */

pid_t find_largest_process_vm_map_entries(void);

extern int exit_with_guard_exception(void *p, mach_exception_data_type_t code,
    mach_exception_data_type_t subcode);

/*
 * Macros to copy a vm_map_entry. We must be careful to correctly
 * manage the wired page count. vm_map_entry_copy() creates a new
 * map entry to the same memory - the wired count in the new entry
 * must be set to zero. vm_map_entry_copy_full() creates a new
 * entry that is identical to the old entry.  This preserves the
 * wire count; it's used for map splitting and zone changing in
 * vm_map_copyout.
 */

static inline void
vm_map_entry_copy_csm_assoc(
	vm_map_t map __unused,
	vm_map_entry_t new __unused,
	vm_map_entry_t old __unused)
{
#if CODE_SIGNING_MONITOR
	/* when code signing monitor is enabled, we want to reset on copy */
	new->csm_associated = FALSE;
#else
	/* when code signing monitor is not enabled, assert as a sanity check */
	assert(new->csm_associated == FALSE);
#endif
#if DEVELOPMENT || DEBUG
	if (new->vme_xnu_user_debug && vm_log_xnu_user_debug) {
		printf("FBDP %d[%s] %s:%d map %p entry %p [ 0x%llx 0x%llx ] resetting vme_xnu_user_debug\n",
		    proc_selfpid(),
		    (get_bsdtask_info(current_task())
		    ? proc_name_address(get_bsdtask_info(current_task()))
		    : "?"),
		    __FUNCTION__, __LINE__,
		    map, new, new->vme_start, new->vme_end);
	}
#endif /* DEVELOPMENT || DEBUG */
	new->vme_xnu_user_debug = FALSE;
}

/*
 * The "used_for_jit" flag was copied from OLD to NEW in vm_map_entry_copy().
 * But for security reasons on some platforms, we don't want the
 * new mapping to be "used for jit", so we reset the flag here.
 */
static inline void
vm_map_entry_copy_code_signing(
	vm_map_t map,
	vm_map_entry_t new,
	vm_map_entry_t old __unused)
{
	if (VM_MAP_POLICY_ALLOW_JIT_COPY(map)) {
		assert(new->used_for_jit == old->used_for_jit);
	} else {
		if (old->used_for_jit) {
			DTRACE_VM3(cs_wx,
			    uint64_t, new->vme_start,
			    uint64_t, new->vme_end,
			    vm_prot_t, new->protection);
			printf("CODE SIGNING: %d[%s] %s: curprot cannot be write+execute. %s\n",
			    proc_selfpid(),
			    (get_bsdtask_info(current_task())
			    ? proc_name_address(get_bsdtask_info(current_task()))
			    : "?"),
			    __FUNCTION__,
			    "removing execute access");
			new->protection &= ~VM_PROT_EXECUTE;
			new->max_protection &= ~VM_PROT_EXECUTE;
		}
		new->used_for_jit = FALSE;
	}
}

static inline void
vm_map_entry_copy_full(
	vm_map_entry_t new,
	vm_map_entry_t old)
{
#if MAP_ENTRY_CREATION_DEBUG
	btref_put(new->vme_creation_bt);
	btref_retain(old->vme_creation_bt);
#endif
#if MAP_ENTRY_INSERTION_DEBUG
	btref_put(new->vme_insertion_bt);
	btref_retain(old->vme_insertion_bt);
#endif
#if VM_BTLOG_TAGS
	/* Discard the btref that might be in the new entry */
	if (new->vme_kernel_object) {
		btref_put(new->vme_tag_btref);
	}
	/* Retain the btref in the old entry to account for its copy */
	if (old->vme_kernel_object) {
		btref_retain(old->vme_tag_btref);
	}
#endif /* VM_BTLOG_TAGS */
	*new = *old;
}

static inline void
vm_map_entry_copy(
	vm_map_t map,
	vm_map_entry_t new,
	vm_map_entry_t old)
{
	vm_map_entry_copy_full(new, old);

	new->is_shared = FALSE;
	new->needs_wakeup = FALSE;
	new->in_transition = FALSE;
	new->wired_count = 0;
	new->user_wired_count = 0;
	new->vme_permanent = FALSE;
	vm_map_entry_copy_code_signing(map, new, old);
	vm_map_entry_copy_csm_assoc(map, new, old);
	if (new->iokit_acct) {
		assertf(!new->use_pmap, "old %p new %p\n", old, new);
		new->iokit_acct = FALSE;
		new->use_pmap = TRUE;
	}
	new->vme_resilient_codesign = FALSE;
	new->vme_resilient_media = FALSE;
	new->vme_atomic = FALSE;
	new->vme_no_copy_on_read = FALSE;
}

/*
 * Normal lock_read_to_write() returns FALSE/0 on failure.
 * These functions evaluate to zero on success and non-zero value on failure.
 */
__attribute__((always_inline))
int
vm_map_lock_read_to_write(vm_map_t map)
{
	if (lck_rw_lock_shared_to_exclusive(&(map)->lock)) {
		DTRACE_VM(vm_map_lock_upgrade);
		return 0;
	}
	return 1;
}

__attribute__((always_inline))
boolean_t
vm_map_try_lock(vm_map_t map)
{
	if (lck_rw_try_lock_exclusive(&(map)->lock)) {
		DTRACE_VM(vm_map_lock_w);
		return TRUE;
	}
	return FALSE;
}

__attribute__((always_inline))
boolean_t
vm_map_try_lock_read(vm_map_t map)
{
	if (lck_rw_try_lock_shared(&(map)->lock)) {
		DTRACE_VM(vm_map_lock_r);
		return TRUE;
	}
	return FALSE;
}

/*!
 * @function kdp_vm_map_is_acquired_exclusive
 *
 * @abstract
 * Checks if vm map is acquired exclusive.
 *
 * @discussion
 * NOT SAFE: To be used only by kernel debugger.
 *
 * @param map map to check
 *
 * @returns TRUE if the map is acquired exclusively.
 */
boolean_t
kdp_vm_map_is_acquired_exclusive(vm_map_t map)
{
	return kdp_lck_rw_lock_is_acquired_exclusive(&map->lock);
}

/*
 * Routines to get the page size the caller should
 * use while inspecting the target address space.
 * Use the "_safely" variant if the caller is dealing with a user-provided
 * array whose size depends on the page size, to avoid any overflow or
 * underflow of a user-allocated buffer.
 */
int
vm_self_region_page_shift_safely(
	vm_map_t target_map)
{
	int effective_page_shift = 0;

	if (PAGE_SIZE == (4096)) {
		/* x86_64 and 4k watches: always use 4k */
		return PAGE_SHIFT;
	}
	/* did caller provide an explicit page size for this thread to use? */
	effective_page_shift = thread_self_region_page_shift();
	if (effective_page_shift) {
		/* use the explicitly-provided page size */
		return effective_page_shift;
	}
	/* no explicit page size: use the caller's page size... */
	effective_page_shift = VM_MAP_PAGE_SHIFT(current_map());
	if (effective_page_shift == VM_MAP_PAGE_SHIFT(target_map)) {
		/* page size match: safe to use */
		return effective_page_shift;
	}
	/* page size mismatch */
	return -1;
}
int
vm_self_region_page_shift(
	vm_map_t target_map)
{
	int effective_page_shift;

	effective_page_shift = vm_self_region_page_shift_safely(target_map);
	if (effective_page_shift == -1) {
		/* no safe value but OK to guess for caller */
		effective_page_shift = MIN(VM_MAP_PAGE_SHIFT(current_map()),
		    VM_MAP_PAGE_SHIFT(target_map));
	}
	return effective_page_shift;
}


/*
 *	Decide if we want to allow processes to execute from their data or stack areas.
 *	override_nx() returns true if we do.  Data/stack execution can be enabled independently
 *	for 32 and 64 bit processes.  Set the VM_ABI_32 or VM_ABI_64 flags in allow_data_exec
 *	or allow_stack_exec to enable data execution for that type of data area for that particular
 *	ABI (or both by or'ing the flags together).  These are initialized in the architecture
 *	specific pmap files since the default behavior varies according to architecture.  The
 *	main reason it varies is because of the need to provide binary compatibility with old
 *	applications that were written before these restrictions came into being.  In the old
 *	days, an app could execute anything it could read, but this has slowly been tightened
 *	up over time.  The default behavior is:
 *
 *	32-bit PPC apps		may execute from both stack and data areas
 *	32-bit Intel apps	may exeucte from data areas but not stack
 *	64-bit PPC/Intel apps	may not execute from either data or stack
 *
 *	An application on any architecture may override these defaults by explicitly
 *	adding PROT_EXEC permission to the page in question with the mprotect(2)
 *	system call.  This code here just determines what happens when an app tries to
 *      execute from a page that lacks execute permission.
 *
 *	Note that allow_data_exec or allow_stack_exec may also be modified by sysctl to change the
 *	default behavior for both 32 and 64 bit apps on a system-wide basis. Furthermore,
 *	a Mach-O header flag bit (MH_NO_HEAP_EXECUTION) can be used to forcibly disallow
 *	execution from data areas for a particular binary even if the arch normally permits it. As
 *	a final wrinkle, a posix_spawn attribute flag can be used to negate this opt-in header bit
 *	to support some complicated use cases, notably browsers with out-of-process plugins that
 *	are not all NX-safe.
 */

extern int allow_data_exec, allow_stack_exec;

int
override_nx(vm_map_t map, uint32_t user_tag) /* map unused on arm */
{
	int current_abi;

	if (map->pmap == kernel_pmap) {
		return FALSE;
	}

	/*
	 * Determine if the app is running in 32 or 64 bit mode.
	 */

	if (vm_map_is_64bit(map)) {
		current_abi = VM_ABI_64;
	} else {
		current_abi = VM_ABI_32;
	}

	/*
	 * Determine if we should allow the execution based on whether it's a
	 * stack or data area and the current architecture.
	 */

	if (user_tag == VM_MEMORY_STACK) {
		return allow_stack_exec & current_abi;
	}

	return (allow_data_exec & current_abi) && (map->map_disallow_data_exec == FALSE);
}


/*
 *	Virtual memory maps provide for the mapping, protection,
 *	and sharing of virtual memory objects.  In addition,
 *	this module provides for an efficient virtual copy of
 *	memory from one map to another.
 *
 *	Synchronization is required prior to most operations.
 *
 *	Maps consist of an ordered doubly-linked list of simple
 *	entries; a single hint is used to speed up lookups.
 *
 *	Sharing maps have been deleted from this version of Mach.
 *	All shared objects are now mapped directly into the respective
 *	maps.  This requires a change in the copy on write strategy;
 *	the asymmetric (delayed) strategy is used for shared temporary
 *	objects instead of the symmetric (shadow) strategy.  All maps
 *	are now "top level" maps (either task map, kernel map or submap
 *	of the kernel map).
 *
 *	Since portions of maps are specified by start/end addreses,
 *	which may not align with existing map entries, all
 *	routines merely "clip" entries to these start/end values.
 *	[That is, an entry is split into two, bordering at a
 *	start or end value.]  Note that these clippings may not
 *	always be necessary (as the two resulting entries are then
 *	not changed); however, the clipping is done for convenience.
 *	No attempt is currently made to "glue back together" two
 *	abutting entries.
 *
 *	The symmetric (shadow) copy strategy implements virtual copy
 *	by copying VM object references from one map to
 *	another, and then marking both regions as copy-on-write.
 *	It is important to note that only one writeable reference
 *	to a VM object region exists in any map when this strategy
 *	is used -- this means that shadow object creation can be
 *	delayed until a write operation occurs.  The symmetric (delayed)
 *	strategy allows multiple maps to have writeable references to
 *	the same region of a vm object, and hence cannot delay creating
 *	its copy objects.  See vm_object_copy_quickly() in vm_object.c.
 *	Copying of permanent objects is completely different; see
 *	vm_object_copy_strategically() in vm_object.c.
 */

ZONE_DECLARE_ID(ZONE_ID_VM_MAP_COPY, struct vm_map_copy);

#define VM_MAP_ZONE_NAME        "maps"
#define VM_MAP_ZFLAGS           (ZC_NOENCRYPT | ZC_VM)

#define VM_MAP_ENTRY_ZONE_NAME  "VM map entries"
#define VM_MAP_ENTRY_ZFLAGS     (ZC_NOENCRYPT | ZC_VM)

#define VM_MAP_HOLES_ZONE_NAME  "VM map holes"
#define VM_MAP_HOLES_ZFLAGS     (ZC_NOENCRYPT | ZC_VM)

/*
 * Asserts that a vm_map_copy object is coming from the
 * vm_map_copy_zone to ensure that it isn't a fake constructed
 * anywhere else.
 */
void
vm_map_copy_require(struct vm_map_copy *copy)
{
	zone_id_require(ZONE_ID_VM_MAP_COPY, sizeof(struct vm_map_copy), copy);
}

/*
 *	vm_map_require:
 *
 *	Ensures that the argument is memory allocated from the genuine
 *	vm map zone. (See zone_id_require_allow_foreign).
 */
void
vm_map_require(vm_map_t map)
{
	zone_id_require(ZONE_ID_VM_MAP, sizeof(struct _vm_map), map);
}

#define VM_MAP_EARLY_COUNT_MAX         16
static __startup_data vm_offset_t      map_data;
static __startup_data vm_size_t        map_data_size;
static __startup_data vm_offset_t      kentry_data;
static __startup_data vm_size_t        kentry_data_size;
static __startup_data vm_offset_t      map_holes_data;
static __startup_data vm_size_t        map_holes_data_size;
static __startup_data vm_map_t        *early_map_owners[VM_MAP_EARLY_COUNT_MAX];
static __startup_data uint32_t         early_map_count;

#if XNU_TARGET_OS_OSX
#define         NO_COALESCE_LIMIT  ((1024 * 128) - 1)
#else /* XNU_TARGET_OS_OSX */
#define         NO_COALESCE_LIMIT  0
#endif /* XNU_TARGET_OS_OSX */

/* Skip acquiring locks if we're in the midst of a kernel core dump */
unsigned int not_in_kdp = 1;

unsigned int vm_map_set_cache_attr_count = 0;

kern_return_t
vm_map_set_cache_attr(
	vm_map_t        map,
	vm_map_offset_t va)
{
	vm_map_entry_t  map_entry;
	vm_object_t     object;
	kern_return_t   kr = KERN_SUCCESS;

	vm_map_lock_read(map);

	if (!vm_map_lookup_entry(map, va, &map_entry) ||
	    map_entry->is_sub_map) {
		/*
		 * that memory is not properly mapped
		 */
		kr = KERN_INVALID_ARGUMENT;
		goto done;
	}
	object = VME_OBJECT(map_entry);

	if (object == VM_OBJECT_NULL) {
		/*
		 * there should be a VM object here at this point
		 */
		kr = KERN_INVALID_ARGUMENT;
		goto done;
	}
	vm_object_lock(object);
	object->set_cache_attr = TRUE;
	vm_object_unlock(object);

	vm_map_set_cache_attr_count++;
done:
	vm_map_unlock_read(map);

	return kr;
}


#if CONFIG_CODE_DECRYPTION
/*
 * vm_map_apple_protected:
 * This remaps the requested part of the object with an object backed by
 * the decrypting pager.
 * crypt_info contains entry points and session data for the crypt module.
 * The crypt_info block will be copied by vm_map_apple_protected. The data structures
 * referenced in crypt_info must remain valid until crypt_info->crypt_end() is called.
 */
kern_return_t
vm_map_apple_protected(
	vm_map_t                map,
	vm_map_offset_t         start,
	vm_map_offset_t         end,
	vm_object_offset_t      crypto_backing_offset,
	struct pager_crypt_info *crypt_info,
	uint32_t                cryptid)
{
	boolean_t       map_locked;
	kern_return_t   kr;
	vm_map_entry_t  map_entry;
	struct vm_map_entry tmp_entry;
	memory_object_t unprotected_mem_obj;
	vm_object_t     protected_object;
	vm_map_offset_t map_addr;
	vm_map_offset_t start_aligned, end_aligned;
	vm_object_offset_t      crypto_start, crypto_end;
	boolean_t       cache_pager;

	map_locked = FALSE;
	unprotected_mem_obj = MEMORY_OBJECT_NULL;

	if (__improbable(vm_map_range_overflows(map, start, end - start))) {
		return KERN_INVALID_ADDRESS;
	}
	start_aligned = vm_map_trunc_page(start, PAGE_MASK_64);
	end_aligned = vm_map_round_page(end, PAGE_MASK_64);
	start_aligned = vm_map_trunc_page(start_aligned, VM_MAP_PAGE_MASK(map));
	end_aligned = vm_map_round_page(end_aligned, VM_MAP_PAGE_MASK(map));

#if __arm64__
	/*
	 * "start" and "end" might be 4K-aligned but not 16K-aligned,
	 * so we might have to loop and establish up to 3 mappings:
	 *
	 * + the first 16K-page, which might overlap with the previous
	 *   4K-aligned mapping,
	 * + the center,
	 * + the last 16K-page, which might overlap with the next
	 *   4K-aligned mapping.
	 * Each of these mapping might be backed by a vnode pager (if
	 * properly page-aligned) or a "fourk_pager", itself backed by a
	 * vnode pager (if 4K-aligned but not page-aligned).
	 */
#endif /* __arm64__ */

	map_addr = start_aligned;
	for (map_addr = start_aligned;
	    map_addr < end;
	    map_addr = tmp_entry.vme_end) {
		vm_map_lock(map);
		map_locked = TRUE;

		/* lookup the protected VM object */
		if (!vm_map_lookup_entry(map,
		    map_addr,
		    &map_entry) ||
		    map_entry->is_sub_map ||
		    VME_OBJECT(map_entry) == VM_OBJECT_NULL) {
			/* that memory is not properly mapped */
			kr = KERN_INVALID_ARGUMENT;
			goto done;
		}

		/* ensure mapped memory is mapped as executable except
		 *  except for model decryption flow */
		if ((cryptid != CRYPTID_MODEL_ENCRYPTION) &&
		    !(map_entry->protection & VM_PROT_EXECUTE)) {
			kr = KERN_INVALID_ARGUMENT;
			goto done;
		}

		/* get the protected object to be decrypted */
		protected_object = VME_OBJECT(map_entry);
		if (protected_object == VM_OBJECT_NULL) {
			/* there should be a VM object here at this point */
			kr = KERN_INVALID_ARGUMENT;
			goto done;
		}
		/* ensure protected object stays alive while map is unlocked */
		vm_object_reference(protected_object);

		/* limit the map entry to the area we want to cover */
		vm_map_clip_start(map, map_entry, start_aligned);
		vm_map_clip_end(map, map_entry, end_aligned);

		tmp_entry = *map_entry;
		map_entry = VM_MAP_ENTRY_NULL; /* not valid after unlocking map */
		vm_map_unlock(map);
		map_locked = FALSE;

		/*
		 * This map entry might be only partially encrypted
		 * (if not fully "page-aligned").
		 */
		crypto_start = 0;
		crypto_end = tmp_entry.vme_end - tmp_entry.vme_start;
		if (tmp_entry.vme_start < start) {
			if (tmp_entry.vme_start != start_aligned) {
				kr = KERN_INVALID_ADDRESS;
				vm_object_deallocate(protected_object);
				goto done;
			}
			crypto_start += (start - tmp_entry.vme_start);
		}
		if (tmp_entry.vme_end > end) {
			if (tmp_entry.vme_end != end_aligned) {
				kr = KERN_INVALID_ADDRESS;
				vm_object_deallocate(protected_object);
				goto done;
			}
			crypto_end -= (tmp_entry.vme_end - end);
		}

		/*
		 * This "extra backing offset" is needed to get the decryption
		 * routine to use the right key.  It adjusts for the possibly
		 * relative offset of an interposed "4K" pager...
		 */
		if (crypto_backing_offset == (vm_object_offset_t) -1) {
			crypto_backing_offset = VME_OFFSET(&tmp_entry);
		}

		cache_pager = TRUE;
#if XNU_TARGET_OS_OSX
		if (vm_map_is_alien(map)) {
			cache_pager = FALSE;
		}
#endif /* XNU_TARGET_OS_OSX */

		/*
		 * Lookup (and create if necessary) the protected memory object
		 * matching that VM object.
		 * If successful, this also grabs a reference on the memory object,
		 * to guarantee that it doesn't go away before we get a chance to map
		 * it.
		 */
		unprotected_mem_obj = apple_protect_pager_setup(
			protected_object,
			VME_OFFSET(&tmp_entry),
			crypto_backing_offset,
			crypt_info,
			crypto_start,
			crypto_end,
			cache_pager);

		/* release extra ref on protected object */
		vm_object_deallocate(protected_object);

		if (unprotected_mem_obj == NULL) {
			kr = KERN_FAILURE;
			goto done;
		}

		/* can overwrite an immutable mapping */
		vm_map_kernel_flags_t vmk_flags = {
			.vmf_fixed = true,
			.vmf_overwrite = true,
			.vmkf_overwrite_immutable = true,
		};
#if __arm64__
		if (tmp_entry.used_for_jit &&
		    (VM_MAP_PAGE_SHIFT(map) != FOURK_PAGE_SHIFT ||
		    PAGE_SHIFT != FOURK_PAGE_SHIFT) &&
		    fourk_binary_compatibility_unsafe &&
		    fourk_binary_compatibility_allow_wx) {
			printf("** FOURK_COMPAT [%d]: "
			    "allowing write+execute at 0x%llx\n",
			    proc_selfpid(), tmp_entry.vme_start);
			vmk_flags.vmkf_map_jit = TRUE;
		}
#endif /* __arm64__ */

		/* map this memory object in place of the current one */
		map_addr = tmp_entry.vme_start;
		kr = vm_map_enter_mem_object(map,
		    &map_addr,
		    (tmp_entry.vme_end -
		    tmp_entry.vme_start),
		    (mach_vm_offset_t) 0,
		    vmk_flags,
		    (ipc_port_t)(uintptr_t) unprotected_mem_obj,
		    0,
		    TRUE,
		    tmp_entry.protection,
		    tmp_entry.max_protection,
		    tmp_entry.inheritance);
		assertf(kr == KERN_SUCCESS,
		    "kr = 0x%x\n", kr);
		assertf(map_addr == tmp_entry.vme_start,
		    "map_addr=0x%llx vme_start=0x%llx tmp_entry=%p\n",
		    (uint64_t)map_addr,
		    (uint64_t) tmp_entry.vme_start,
		    &tmp_entry);

#if VM_MAP_DEBUG_APPLE_PROTECT
		if (vm_map_debug_apple_protect) {
			printf("APPLE_PROTECT: map %p [0x%llx:0x%llx] pager %p:"
			    " backing:[object:%p,offset:0x%llx,"
			    "crypto_backing_offset:0x%llx,"
			    "crypto_start:0x%llx,crypto_end:0x%llx]\n",
			    map,
			    (uint64_t) map_addr,
			    (uint64_t) (map_addr + (tmp_entry.vme_end -
			    tmp_entry.vme_start)),
			    unprotected_mem_obj,
			    protected_object,
			    VME_OFFSET(&tmp_entry),
			    crypto_backing_offset,
			    crypto_start,
			    crypto_end);
		}
#endif /* VM_MAP_DEBUG_APPLE_PROTECT */

		/*
		 * Release the reference obtained by
		 * apple_protect_pager_setup().
		 * The mapping (if it succeeded) is now holding a reference on
		 * the memory object.
		 */
		memory_object_deallocate(unprotected_mem_obj);
		unprotected_mem_obj = MEMORY_OBJECT_NULL;

		/* continue with next map entry */
		crypto_backing_offset += (tmp_entry.vme_end -
		    tmp_entry.vme_start);
		crypto_backing_offset -= crypto_start;
	}
	kr = KERN_SUCCESS;

done:
	if (map_locked) {
		vm_map_unlock(map);
	}
	return kr;
}
#endif  /* CONFIG_CODE_DECRYPTION */


LCK_GRP_DECLARE(vm_map_lck_grp, "vm_map");
LCK_ATTR_DECLARE(vm_map_lck_attr, 0, 0);
LCK_ATTR_DECLARE(vm_map_lck_rw_attr, 0, LCK_ATTR_DEBUG);

#if XNU_TARGET_OS_OSX
#define MALLOC_NO_COW_DEFAULT 1
#define MALLOC_NO_COW_EXCEPT_FORK_DEFAULT 1
#else /* XNU_TARGET_OS_OSX */
#define MALLOC_NO_COW_DEFAULT 1
#define MALLOC_NO_COW_EXCEPT_FORK_DEFAULT 0
#endif /* XNU_TARGET_OS_OSX */
TUNABLE(int, malloc_no_cow, "malloc_no_cow", MALLOC_NO_COW_DEFAULT);
TUNABLE(int, malloc_no_cow_except_fork, "malloc_no_cow_except_fork", MALLOC_NO_COW_EXCEPT_FORK_DEFAULT);
uint64_t vm_memory_malloc_no_cow_mask = 0ULL;
#if DEBUG
int vm_check_map_sanity = 0;
#endif

/*
 *	vm_map_init:
 *
 *	Initialize the vm_map module.  Must be called before
 *	any other vm_map routines.
 *
 *	Map and entry structures are allocated from zones -- we must
 *	initialize those zones.
 *
 *	There are three zones of interest:
 *
 *	vm_map_zone:		used to allocate maps.
 *	vm_map_entry_zone:	used to allocate map entries.
 *
 *	LP32:
 *	vm_map_entry_reserved_zone:     fallback zone for kernel map entries
 *
 *	The kernel allocates map entries from a special zone that is initially
 *	"crammed" with memory.  It would be difficult (perhaps impossible) for
 *	the kernel to allocate more memory to a entry zone when it became
 *	empty since the very act of allocating memory implies the creation
 *	of a new entry.
 */
__startup_func
void
vm_map_init(void)
{

#if MACH_ASSERT
	PE_parse_boot_argn("debug4k_filter", &debug4k_filter,
	    sizeof(debug4k_filter));
#endif /* MACH_ASSERT */

	zone_create_ext(VM_MAP_ZONE_NAME, sizeof(struct _vm_map),
	    VM_MAP_ZFLAGS, ZONE_ID_VM_MAP, NULL);

	/*
	 * Don't quarantine because we always need elements available
	 * Disallow GC on this zone... to aid the GC.
	 */
	zone_create_ext(VM_MAP_ENTRY_ZONE_NAME,
	    sizeof(struct vm_map_entry), VM_MAP_ENTRY_ZFLAGS,
	    ZONE_ID_VM_MAP_ENTRY, ^(zone_t z) {
		z->z_elems_rsv = (uint16_t)(32 *
		(ml_early_cpu_max_number() + 1));
	});

	zone_create_ext(VM_MAP_HOLES_ZONE_NAME,
	    sizeof(struct vm_map_links), VM_MAP_HOLES_ZFLAGS,
	    ZONE_ID_VM_MAP_HOLES, ^(zone_t z) {
		z->z_elems_rsv = (uint16_t)(16 * 1024 / zone_elem_outer_size(z));
	});

	zone_create_ext("VM map copies", sizeof(struct vm_map_copy),
	    ZC_NOENCRYPT, ZONE_ID_VM_MAP_COPY, NULL);

	/*
	 * Add the stolen memory to zones, adjust zone size and stolen counts.
	 */
	zone_cram_early(vm_map_zone, map_data, map_data_size);
	zone_cram_early(vm_map_entry_zone, kentry_data, kentry_data_size);
	zone_cram_early(vm_map_holes_zone, map_holes_data, map_holes_data_size);
	printf("VM boostrap: %d maps, %d entries and %d holes available\n",
	    zone_count_free(vm_map_zone),
	    zone_count_free(vm_map_entry_zone),
	    zone_count_free(vm_map_holes_zone));

	/*
	 * Since these are covered by zones, remove them from stolen page accounting.
	 */
	VM_PAGE_MOVE_STOLEN(atop_64(map_data_size) + atop_64(kentry_data_size) + atop_64(map_holes_data_size));

#if VM_MAP_DEBUG_APPLE_PROTECT
	PE_parse_boot_argn("vm_map_debug_apple_protect",
	    &vm_map_debug_apple_protect,
	    sizeof(vm_map_debug_apple_protect));
#endif /* VM_MAP_DEBUG_APPLE_PROTECT */
#if VM_MAP_DEBUG_APPLE_FOURK
	PE_parse_boot_argn("vm_map_debug_fourk",
	    &vm_map_debug_fourk,
	    sizeof(vm_map_debug_fourk));
#endif /* VM_MAP_DEBUG_FOURK */

	if (malloc_no_cow) {
		vm_memory_malloc_no_cow_mask = 0ULL;
		vm_memory_malloc_no_cow_mask |= 1ULL << VM_MEMORY_MALLOC;
		vm_memory_malloc_no_cow_mask |= 1ULL << VM_MEMORY_MALLOC_SMALL;
		vm_memory_malloc_no_cow_mask |= 1ULL << VM_MEMORY_MALLOC_MEDIUM;
#if XNU_TARGET_OS_OSX
		/*
		 * On macOS, keep copy-on-write for MALLOC_LARGE because
		 * realloc() may use vm_copy() to transfer the old contents
		 * to the new location.
		 */
#else /* XNU_TARGET_OS_OSX */
		vm_memory_malloc_no_cow_mask |= 1ULL << VM_MEMORY_MALLOC_LARGE;
		vm_memory_malloc_no_cow_mask |= 1ULL << VM_MEMORY_MALLOC_LARGE_REUSABLE;
		vm_memory_malloc_no_cow_mask |= 1ULL << VM_MEMORY_MALLOC_LARGE_REUSED;
#endif /* XNU_TARGET_OS_OSX */
//		vm_memory_malloc_no_cow_mask |= 1ULL << VM_MEMORY_MALLOC_HUGE;
//		vm_memory_malloc_no_cow_mask |= 1ULL << VM_MEMORY_REALLOC;
		vm_memory_malloc_no_cow_mask |= 1ULL << VM_MEMORY_MALLOC_TINY;
		vm_memory_malloc_no_cow_mask |= 1ULL << VM_MEMORY_MALLOC_NANO;
//		vm_memory_malloc_no_cow_mask |= 1ULL << VM_MEMORY_TCMALLOC;
		PE_parse_boot_argn("vm_memory_malloc_no_cow_mask",
		    &vm_memory_malloc_no_cow_mask,
		    sizeof(vm_memory_malloc_no_cow_mask));
	}

#if CONFIG_MAP_RANGES
	vm_map_range_map_init();
#endif /* CONFIG_MAP_RANGES */

#if DEBUG
	PE_parse_boot_argn("vm_check_map_sanity", &vm_check_map_sanity, sizeof(vm_check_map_sanity));
	if (vm_check_map_sanity) {
		kprintf("VM sanity checking enabled\n");
	} else {
		kprintf("VM sanity checking disabled. Set bootarg vm_check_map_sanity=1 to enable\n");
	}
#endif /* DEBUG */

#if DEVELOPMENT || DEBUG
	PE_parse_boot_argn("panic_on_unsigned_execute",
	    &panic_on_unsigned_execute,
	    sizeof(panic_on_unsigned_execute));
	PE_parse_boot_argn("panic_on_mlock_failure",
	    &panic_on_mlock_failure,
	    sizeof(panic_on_mlock_failure));
#endif /* DEVELOPMENT || DEBUG */
}

__startup_func
static void
vm_map_steal_memory(void)
{
	/*
	 * We need to reserve enough memory to support boostraping VM maps
	 * and the zone subsystem.
	 *
	 * The VM Maps that need to function before zones can support them
	 * are the ones registered with vm_map_will_allocate_early_map(),
	 * which are:
	 * - the kernel map
	 * - the various submaps used by zones (pgz, meta, ...)
	 *
	 * We also need enough entries and holes to support them
	 * until zone_metadata_init() is called, which is when
	 * the zone allocator becomes capable of expanding dynamically.
	 *
	 * We need:
	 * - VM_MAP_EARLY_COUNT_MAX worth of VM Maps.
	 * - To allow for 3-4 entries per map, but the kernel map
	 *   needs a multiple of VM_MAP_EARLY_COUNT_MAX entries
	 *   to describe the submaps, so double it (and make it 8x too)
	 * - To allow for holes between entries,
	 *   hence needs the same budget as entries
	 */
	map_data_size = zone_get_early_alloc_size(VM_MAP_ZONE_NAME,
	    sizeof(struct _vm_map), VM_MAP_ZFLAGS,
	    VM_MAP_EARLY_COUNT_MAX);

	kentry_data_size = zone_get_early_alloc_size(VM_MAP_ENTRY_ZONE_NAME,
	    sizeof(struct vm_map_entry), VM_MAP_ENTRY_ZFLAGS,
	    8 * VM_MAP_EARLY_COUNT_MAX);

	map_holes_data_size = zone_get_early_alloc_size(VM_MAP_HOLES_ZONE_NAME,
	    sizeof(struct vm_map_links), VM_MAP_HOLES_ZFLAGS,
	    8 * VM_MAP_EARLY_COUNT_MAX);

	/*
	 * Steal a contiguous range of memory so that a simple range check
	 * can validate early addresses being freed/crammed to these
	 * zones
	 */
	map_data       = zone_early_mem_init(map_data_size + kentry_data_size +
	    map_holes_data_size);
	kentry_data    = map_data + map_data_size;
	map_holes_data = kentry_data + kentry_data_size;
}
STARTUP(PMAP_STEAL, STARTUP_RANK_FIRST, vm_map_steal_memory);

__startup_func
static void
vm_kernel_boostraped(void)
{
	zone_enable_caching(&zone_array[ZONE_ID_VM_MAP_ENTRY]);
	zone_enable_caching(&zone_array[ZONE_ID_VM_MAP_HOLES]);
	zone_enable_caching(&zone_array[ZONE_ID_VM_MAP_COPY]);

	printf("VM bootstrap done: %d maps, %d entries and %d holes left\n",
	    zone_count_free(vm_map_zone),
	    zone_count_free(vm_map_entry_zone),
	    zone_count_free(vm_map_holes_zone));
}
STARTUP(ZALLOC, STARTUP_RANK_SECOND, vm_kernel_boostraped);

void
vm_map_disable_hole_optimization(vm_map_t map)
{
	vm_map_entry_t  head_entry, hole_entry, next_hole_entry;

	if (map->holelistenabled) {
		head_entry = hole_entry = CAST_TO_VM_MAP_ENTRY(map->holes_list);

		while (hole_entry != NULL) {
			next_hole_entry = hole_entry->vme_next;

			hole_entry->vme_next = NULL;
			hole_entry->vme_prev = NULL;
			zfree_id(ZONE_ID_VM_MAP_HOLES, hole_entry);

			if (next_hole_entry == head_entry) {
				hole_entry = NULL;
			} else {
				hole_entry = next_hole_entry;
			}
		}

		map->holes_list = NULL;
		map->holelistenabled = FALSE;

		map->first_free = vm_map_first_entry(map);
		SAVE_HINT_HOLE_WRITE(map, NULL);
	}
}

boolean_t
vm_kernel_map_is_kernel(vm_map_t map)
{
	return map->pmap == kernel_pmap;
}

/*
 *	vm_map_create:
 *
 *	Creates and returns a new empty VM map with
 *	the given physical map structure, and having
 *	the given lower and upper address bounds.
 */

extern vm_map_t vm_map_create_external(
	pmap_t                  pmap,
	vm_map_offset_t         min_off,
	vm_map_offset_t         max_off,
	boolean_t               pageable);

vm_map_t
vm_map_create_external(
	pmap_t                  pmap,
	vm_map_offset_t         min,
	vm_map_offset_t         max,
	boolean_t               pageable)
{
	vm_map_create_options_t options = VM_MAP_CREATE_DEFAULT;

	if (pageable) {
		options |= VM_MAP_CREATE_PAGEABLE;
	}
	return vm_map_create_options(pmap, min, max, options);
}

__startup_func
void
vm_map_will_allocate_early_map(vm_map_t *owner)
{
	if (early_map_count >= VM_MAP_EARLY_COUNT_MAX) {
		panic("VM_MAP_EARLY_COUNT_MAX is too low");
	}

	early_map_owners[early_map_count++] = owner;
}

__startup_func
void
vm_map_relocate_early_maps(vm_offset_t delta)
{
	for (uint32_t i = 0; i < early_map_count; i++) {
		vm_address_t addr = (vm_address_t)*early_map_owners[i];

		*early_map_owners[i] = (vm_map_t)(addr + delta);
	}

	early_map_count = ~0u;
}

/*
 *	Routine:	vm_map_relocate_early_elem
 *
 *	Purpose:
 *		Early zone elements are allocated in a temporary part
 *		of the address space.
 *
 *		Once the zones live in their final place, the early
 *		VM maps, map entries and map holes need to be relocated.
 *
 *		It involves rewriting any vm_map_t, vm_map_entry_t or
 *		pointers to vm_map_links. Other pointers to other types
 *		are fine.
 *
 *		Fortunately, pointers to those types are self-contained
 *		in those zones, _except_ for pointers to VM maps,
 *		which are tracked during early boot and fixed with
 *		vm_map_relocate_early_maps().
 */
__startup_func
void
vm_map_relocate_early_elem(
	uint32_t                zone_id,
	vm_offset_t             new_addr,
	vm_offset_t             delta)
{
#define relocate(type_t, field)  ({ \
	typeof(((type_t)NULL)->field) *__field = &((type_t)new_addr)->field;   \
	if (*__field) {                                                        \
	        *__field = (typeof(*__field))((vm_offset_t)*__field + delta);  \
	}                                                                      \
})

	switch (zone_id) {
	case ZONE_ID_VM_MAP:
	case ZONE_ID_VM_MAP_ENTRY:
	case ZONE_ID_VM_MAP_HOLES:
		break;

	default:
		panic("Unexpected zone ID %d", zone_id);
	}

	if (zone_id == ZONE_ID_VM_MAP) {
		relocate(vm_map_t, hdr.links.prev);
		relocate(vm_map_t, hdr.links.next);
		((vm_map_t)new_addr)->pmap = kernel_pmap;
#ifdef VM_MAP_STORE_USE_RB
		relocate(vm_map_t, hdr.rb_head_store.rbh_root);
#endif /* VM_MAP_STORE_USE_RB */
		relocate(vm_map_t, hint);
		relocate(vm_map_t, hole_hint);
		relocate(vm_map_t, first_free);
		return;
	}

	relocate(struct vm_map_links *, prev);
	relocate(struct vm_map_links *, next);

	if (zone_id == ZONE_ID_VM_MAP_ENTRY) {
#ifdef VM_MAP_STORE_USE_RB
		relocate(vm_map_entry_t, store.entry.rbe_left);
		relocate(vm_map_entry_t, store.entry.rbe_right);
		relocate(vm_map_entry_t, store.entry.rbe_parent);
#endif /* VM_MAP_STORE_USE_RB */
		if (((vm_map_entry_t)new_addr)->is_sub_map) {
			/* no object to relocate because we haven't made any */
			((vm_map_entry_t)new_addr)->vme_submap +=
			    delta >> VME_SUBMAP_SHIFT;
		}
#if MAP_ENTRY_CREATION_DEBUG
		relocate(vm_map_entry_t, vme_creation_maphdr);
#endif /* MAP_ENTRY_CREATION_DEBUG */
	}

#undef relocate
}

vm_map_t
vm_map_create_options(
	pmap_t                  pmap,
	vm_map_offset_t         min,
	vm_map_offset_t         max,
	vm_map_create_options_t options)
{
	vm_map_t result;

#if DEBUG || DEVELOPMENT
	if (__improbable(startup_phase < STARTUP_SUB_ZALLOC)) {
		if (early_map_count != ~0u && early_map_count !=
		    zone_count_allocated(vm_map_zone) + 1) {
			panic("allocating %dth early map, owner not known",
			    zone_count_allocated(vm_map_zone) + 1);
		}
		if (early_map_count != ~0u && pmap && pmap != kernel_pmap) {
			panic("allocating %dth early map for non kernel pmap",
			    early_map_count);
		}
	}
#endif /* DEBUG || DEVELOPMENT */

	result = zalloc_id(ZONE_ID_VM_MAP, Z_WAITOK | Z_NOFAIL | Z_ZERO);

	vm_map_store_init(&result->hdr);
	result->hdr.entries_pageable = (bool)(options & VM_MAP_CREATE_PAGEABLE);
	vm_map_set_page_shift(result, PAGE_SHIFT);

	result->size_limit      = RLIM_INFINITY;        /* default unlimited */
	result->data_limit      = RLIM_INFINITY;        /* default unlimited */
	result->user_wire_limit = MACH_VM_MAX_ADDRESS;  /* default limit is unlimited */
	os_ref_init_count_raw(&result->map_refcnt, &map_refgrp, 1);
	result->pmap = pmap;
	result->min_offset = min;
	result->max_offset = max;
	result->first_free = vm_map_to_entry(result);
	result->hint = vm_map_to_entry(result);

	if (options & VM_MAP_CREATE_NEVER_FAULTS) {
		assert(pmap == kernel_pmap);
		result->never_faults = true;
	}

	/* "has_corpse_footprint" and "holelistenabled" are mutually exclusive */
	if (options & VM_MAP_CREATE_CORPSE_FOOTPRINT) {
		result->has_corpse_footprint = true;
	} else if (!(options & VM_MAP_CREATE_DISABLE_HOLELIST)) {
		struct vm_map_links *hole_entry;

		hole_entry = zalloc_id(ZONE_ID_VM_MAP_HOLES, Z_WAITOK | Z_NOFAIL);
		hole_entry->start = min;
#if defined(__arm64__)
		hole_entry->end = result->max_offset;
#else
		hole_entry->end = MAX(max, (vm_map_offset_t)MACH_VM_MAX_ADDRESS);
#endif
		result->holes_list = result->hole_hint = hole_entry;
		hole_entry->prev = hole_entry->next = CAST_TO_VM_MAP_ENTRY(hole_entry);
		result->holelistenabled = true;
	}

	vm_map_lock_init(result);

	return result;
}

/*
 * Adjusts a submap that was made by kmem_suballoc()
 * before it knew where it would be mapped,
 * so that it has the right min/max offsets.
 *
 * We do not need to hold any locks:
 * only the caller knows about this map,
 * and it is not published on any entry yet.
 */
static void
vm_map_adjust_offsets(
	vm_map_t                map,
	vm_map_offset_t         min_off,
	vm_map_offset_t         max_off)
{
	assert(map->min_offset == 0);
	assert(map->max_offset == max_off - min_off);
	assert(map->hdr.nentries == 0);
	assert(os_ref_get_count_raw(&map->map_refcnt) == 2);

	map->min_offset = min_off;
	map->max_offset = max_off;

	if (map->holelistenabled) {
		struct vm_map_links *hole = map->holes_list;

		hole->start = min_off;
#if defined(__arm64__)
		hole->end = max_off;
#else
		hole->end = MAX(max_off, (vm_map_offset_t)MACH_VM_MAX_ADDRESS);
#endif
	}
}


vm_map_size_t
vm_map_adjusted_size(vm_map_t map)
{
	const struct vm_reserved_region *regions = NULL;
	size_t num_regions = 0;
	mach_vm_size_t  reserved_size = 0, map_size = 0;

	if (map == NULL || (map->size == 0)) {
		return 0;
	}

	map_size = map->size;

	if (map->reserved_regions == FALSE || !vm_map_is_exotic(map) || map->terminated) {
		/*
		 * No special reserved regions or not an exotic map or the task
		 * is terminating and these special regions might have already
		 * been deallocated.
		 */
		return map_size;
	}

	num_regions = ml_get_vm_reserved_regions(vm_map_is_64bit(map), &regions);
	assert((num_regions == 0) || (num_regions > 0 && regions != NULL));

	while (num_regions) {
		reserved_size += regions[--num_regions].vmrr_size;
	}

	/*
	 * There are a few places where the map is being switched out due to
	 * 'termination' without that bit being set (e.g. exec and corpse purging).
	 * In those cases, we could have the map's regions being deallocated on
	 * a core while some accounting process is trying to get the map's size.
	 * So this assert can't be enabled till all those places are uniform in
	 * their use of the 'map->terminated' bit.
	 *
	 * assert(map_size >= reserved_size);
	 */

	return (map_size >= reserved_size) ? (map_size - reserved_size) : map_size;
}

/*
 *	vm_map_entry_create:	[ internal use only ]
 *
 *	Allocates a VM map entry for insertion in the
 *	given map (or map copy).  No fields are filled.
 *
 *	The VM entry will be zero initialized, except for:
 *	- behavior set to VM_BEHAVIOR_DEFAULT
 *	- inheritance set to VM_INHERIT_DEFAULT
 */
#define vm_map_entry_create(map)    _vm_map_entry_create(&(map)->hdr)

#define vm_map_copy_entry_create(copy) _vm_map_entry_create(&(copy)->cpy_hdr)

static vm_map_entry_t
_vm_map_entry_create(
	struct vm_map_header    *map_header __unused)
{
	vm_map_entry_t entry = NULL;

	entry = zalloc_id(ZONE_ID_VM_MAP_ENTRY, Z_WAITOK | Z_ZERO);

	/*
	 * Help the compiler with what we know to be true,
	 * so that the further bitfields inits have good codegen.
	 *
	 * See rdar://87041299
	 */
	__builtin_assume(entry->vme_object_value == 0);
	__builtin_assume(*(uint64_t *)(&entry->vme_object_value + 1) == 0);
	__builtin_assume(*(uint64_t *)(&entry->vme_object_value + 2) == 0);

	static_assert(VM_MAX_TAG_VALUE <= VME_ALIAS_MASK,
	    "VME_ALIAS_MASK covers tags");

	static_assert(VM_BEHAVIOR_DEFAULT == 0,
	    "can skip zeroing of the behavior field");
	entry->inheritance = VM_INHERIT_DEFAULT;

#if MAP_ENTRY_CREATION_DEBUG
	entry->vme_creation_maphdr = map_header;
	entry->vme_creation_bt = btref_get(__builtin_frame_address(0),
	    BTREF_GET_NOWAIT);
#endif
	return entry;
}

/*
 *	vm_map_entry_dispose:	[ internal use only ]
 *
 *	Inverse of vm_map_entry_create.
 *
 *      write map lock held so no need to
 *	do anything special to insure correctness
 *      of the stores
 */
static void
vm_map_entry_dispose(
	vm_map_entry_t          entry)
{
#if VM_BTLOG_TAGS
	if (entry->vme_kernel_object) {
		btref_put(entry->vme_tag_btref);
	}
#endif /* VM_BTLOG_TAGS */
#if MAP_ENTRY_CREATION_DEBUG
	btref_put(entry->vme_creation_bt);
#endif
#if MAP_ENTRY_INSERTION_DEBUG
	btref_put(entry->vme_insertion_bt);
#endif
	zfree(vm_map_entry_zone, entry);
}

#define vm_map_copy_entry_dispose(copy_entry) \
	vm_map_entry_dispose(copy_entry)

static vm_map_entry_t
vm_map_zap_first_entry(
	vm_map_zap_t            list)
{
	return list->vmz_head;
}

static vm_map_entry_t
vm_map_zap_last_entry(
	vm_map_zap_t            list)
{
	assert(vm_map_zap_first_entry(list));
	return __container_of(list->vmz_tail, struct vm_map_entry, vme_next);
}

static void
vm_map_zap_append(
	vm_map_zap_t            list,
	vm_map_entry_t          entry)
{
	entry->vme_next = VM_MAP_ENTRY_NULL;
	*list->vmz_tail = entry;
	list->vmz_tail = &entry->vme_next;
}

static vm_map_entry_t
vm_map_zap_pop(
	vm_map_zap_t            list)
{
	vm_map_entry_t head = list->vmz_head;

	if (head != VM_MAP_ENTRY_NULL &&
	    (list->vmz_head = head->vme_next) == VM_MAP_ENTRY_NULL) {
		list->vmz_tail = &list->vmz_head;
	}

	return head;
}

static void
vm_map_zap_dispose(
	vm_map_zap_t            list)
{
	vm_map_entry_t          entry;

	while ((entry = vm_map_zap_pop(list))) {
		if (entry->is_sub_map) {
			vm_map_deallocate(VME_SUBMAP(entry));
		} else {
			vm_object_deallocate(VME_OBJECT(entry));
		}

		vm_map_entry_dispose(entry);
	}
}

#if MACH_ASSERT
static boolean_t first_free_check = FALSE;
boolean_t
first_free_is_valid(
	vm_map_t        map)
{
	if (!first_free_check) {
		return TRUE;
	}

	return first_free_is_valid_store( map );
}
#endif /* MACH_ASSERT */


#define vm_map_copy_entry_link(copy, after_where, entry)                \
	_vm_map_store_entry_link(&(copy)->cpy_hdr, after_where, (entry))

#define vm_map_copy_entry_unlink(copy, entry)                           \
	_vm_map_store_entry_unlink(&(copy)->cpy_hdr, (entry), false)

/*
 *	vm_map_destroy:
 *
 *	Actually destroy a map.
 */
void
vm_map_destroy(
	vm_map_t        map)
{
	/* final cleanup: this is not allowed to fail */
	vmr_flags_t flags = VM_MAP_REMOVE_NO_FLAGS;

	VM_MAP_ZAP_DECLARE(zap);

	vm_map_lock(map);

	map->terminated = true;
	/* clean up regular map entries */
	(void)vm_map_delete(map, map->min_offset, map->max_offset, flags,
	    KMEM_GUARD_NONE, &zap);
	/* clean up leftover special mappings (commpage, GPU carveout, etc...) */
	(void)vm_map_delete(map, 0x0, 0xFFFFFFFFFFFFF000ULL, flags,
	    KMEM_GUARD_NONE, &zap);

	vm_map_disable_hole_optimization(map);
	vm_map_corpse_footprint_destroy(map);

	vm_map_unlock(map);

	vm_map_zap_dispose(&zap);

	assert(map->hdr.nentries == 0);

	if (map->pmap) {
		pmap_destroy(map->pmap);
	}

	lck_rw_destroy(&map->lock, &vm_map_lck_grp);

#if CONFIG_MAP_RANGES
	kfree_data(map->extra_ranges,
	    map->extra_ranges_count * sizeof(struct vm_map_user_range));
#endif

	zfree_id(ZONE_ID_VM_MAP, map);
}

/*
 * Returns pid of the task with the largest number of VM map entries.
 * Used in the zone-map-exhaustion jetsam path.
 */
pid_t
find_largest_process_vm_map_entries(void)
{
	pid_t victim_pid = -1;
	int max_vm_map_entries = 0;
	task_t task = TASK_NULL;
	queue_head_t *task_list = &tasks;

	lck_mtx_lock(&tasks_threads_lock);
	queue_iterate(task_list, task, task_t, tasks) {
		if (task == kernel_task || !task->active) {
			continue;
		}

		vm_map_t task_map = task->map;
		if (task_map != VM_MAP_NULL) {
			int task_vm_map_entries = task_map->hdr.nentries;
			if (task_vm_map_entries > max_vm_map_entries) {
				max_vm_map_entries = task_vm_map_entries;
				victim_pid = pid_from_task(task);
			}
		}
	}
	lck_mtx_unlock(&tasks_threads_lock);

	printf("zone_map_exhaustion: victim pid %d, vm region count: %d\n", victim_pid, max_vm_map_entries);
	return victim_pid;
}


/*
 *	vm_map_lookup_entry:	[ internal use only ]
 *
 *	Calls into the vm map store layer to find the map
 *	entry containing (or immediately preceding) the
 *	specified address in the given map; the entry is returned
 *	in the "entry" parameter.  The boolean
 *	result indicates whether the address is
 *	actually contained in the map.
 */
boolean_t
vm_map_lookup_entry(
	vm_map_t        map,
	vm_map_offset_t address,
	vm_map_entry_t  *entry)         /* OUT */
{
	if (VM_KERNEL_ADDRESS(address)) {
		address = VM_KERNEL_STRIP_UPTR(address);
	}


#if CONFIG_PROB_GZALLOC
	if (map->pmap == kernel_pmap) {
		assertf(!pgz_owned(address),
		    "it is the responsibility of callers to unguard PGZ addresses");
	}
#endif /* CONFIG_PROB_GZALLOC */
	return vm_map_store_lookup_entry( map, address, entry );
}

boolean_t
vm_map_lookup_entry_or_next(
	vm_map_t        map,
	vm_map_offset_t address,
	vm_map_entry_t  *entry)         /* OUT */
{
	if (vm_map_lookup_entry(map, address, entry)) {
		return true;
	}

	*entry = (*entry)->vme_next;
	return false;
}

#if CONFIG_PROB_GZALLOC
boolean_t
vm_map_lookup_entry_allow_pgz(
	vm_map_t        map,
	vm_map_offset_t address,
	vm_map_entry_t  *entry)         /* OUT */
{
	if (VM_KERNEL_ADDRESS(address)) {
		address = VM_KERNEL_STRIP_UPTR(address);
	}
	return vm_map_store_lookup_entry( map, address, entry );
}
#endif /* CONFIG_PROB_GZALLOC */

/*
 *	Routine:	vm_map_range_invalid_panic
 *	Purpose:
 *			Panic on detection of an invalid range id.
 */
__abortlike
static void
vm_map_range_invalid_panic(
	vm_map_t                map,
	vm_map_range_id_t       range_id)
{
	panic("invalid range ID (%u) for map %p", range_id, map);
}

/*
 *	Routine:	vm_map_get_range
 *	Purpose:
 *			Adjust bounds based on security policy.
 */
static struct mach_vm_range
vm_map_get_range(
	vm_map_t                map,
	vm_map_address_t       *address,
	vm_map_kernel_flags_t  *vmk_flags,
	vm_map_size_t           size,
	bool                   *is_ptr)
{
	struct mach_vm_range effective_range = {};
	vm_map_range_id_t range_id = vmk_flags->vmkf_range_id;

	if (map == kernel_map) {
		effective_range = kmem_ranges[range_id];

		if (startup_phase >= STARTUP_SUB_KMEM) {
			/*
			 * Hint provided by caller is zeroed as the range is restricted to a
			 * subset of the entire kernel_map VA, which could put the hint outside
			 * the range, causing vm_map_store_find_space to fail.
			 */
			*address = 0ull;
			/*
			 * Ensure that range_id passed in by the caller is within meaningful
			 * bounds. Range id of KMEM_RANGE_ID_NONE will cause vm_map_locate_space
			 * to fail as the corresponding range is invalid. Range id larger than
			 * KMEM_RANGE_ID_MAX will lead to an OOB access.
			 */
			if ((range_id == KMEM_RANGE_ID_NONE) ||
			    (range_id > KMEM_RANGE_ID_MAX)) {
				vm_map_range_invalid_panic(map, range_id);
			}

			/*
			 * Pointer ranges use kmem_locate_space to do allocations.
			 *
			 * Non pointer fronts look like [ Small | Large | Permanent ]
			 * Adjust range for allocations larger than KMEM_SMALLMAP_THRESHOLD.
			 * Allocations smaller than KMEM_SMALLMAP_THRESHOLD are allowed to
			 * use the entire range.
			 */
			if (range_id < KMEM_RANGE_ID_SPRAYQTN) {
				*is_ptr = true;
			} else if (size >= KMEM_SMALLMAP_THRESHOLD) {
				effective_range = kmem_large_ranges[range_id];
			}
		}
#if CONFIG_MAP_RANGES
	} else if (map->uses_user_ranges) {
		switch (range_id) {
		case UMEM_RANGE_ID_DEFAULT:
			effective_range = map->default_range;
			break;
		case UMEM_RANGE_ID_HEAP:
			effective_range = map->data_range;
			break;
		case UMEM_RANGE_ID_FIXED:
			/*
			 * anywhere allocations with an address in "FIXED"
			 * makes no sense, leave the range empty
			 */
			break;

		default:
			vm_map_range_invalid_panic(map, range_id);
		}
#endif /* CONFIG_MAP_RANGES */
	} else {
		/*
		 * If minimum is 0, bump it up by PAGE_SIZE.  We want to limit
		 * allocations of PAGEZERO to explicit requests since its
		 * normal use is to catch dereferences of NULL and many
		 * applications also treat pointers with a value of 0 as
		 * special and suddenly having address 0 contain useable
		 * memory would tend to confuse those applications.
		 */
		effective_range.min_address = MAX(map->min_offset, VM_MAP_PAGE_SIZE(map));
		effective_range.max_address = map->max_offset;
	}

	return effective_range;
}

/*
 *	Routine:	vm_map_locate_space
 *	Purpose:
 *		Finds a range in the specified virtual address map,
 *		returning the start of that range,
 *		as well as the entry right before it.
 */
kern_return_t
vm_map_locate_space(
	vm_map_t                map,
	vm_map_size_t           size,
	vm_map_offset_t         mask,
	vm_map_kernel_flags_t   vmk_flags,
	vm_map_offset_t        *start_inout,
	vm_map_entry_t         *entry_out)
{
	struct mach_vm_range effective_range = {};
	vm_map_size_t   guard_offset;
	vm_map_offset_t hint, limit;
	vm_map_entry_t  entry;
	bool            is_kmem_ptr_range = false;

	/*
	 * Only supported by vm_map_enter() with a fixed address.
	 */
	assert(!vmk_flags.vmkf_beyond_max);

	if (__improbable(map->wait_for_space)) {
		/*
		 * support for "wait_for_space" is minimal,
		 * its only consumer is the ipc_kernel_copy_map.
		 */
		assert(!map->holelistenabled &&
		    !vmk_flags.vmkf_last_free &&
		    !vmk_flags.vmkf_keep_map_locked &&
		    !vmk_flags.vmkf_map_jit &&
		    !vmk_flags.vmf_random_addr &&
		    *start_inout <= map->min_offset);
	} else if (vmk_flags.vmkf_last_free) {
		assert(!vmk_flags.vmkf_map_jit &&
		    !vmk_flags.vmf_random_addr);
	}

	if (vmk_flags.vmkf_guard_before) {
		guard_offset = VM_MAP_PAGE_SIZE(map);
		assert(size > guard_offset);
		size -= guard_offset;
	} else {
		assert(size != 0);
		guard_offset = 0;
	}

	/*
	 * Validate range_id from flags and get associated range
	 */
	effective_range = vm_map_get_range(map, start_inout, &vmk_flags, size,
	    &is_kmem_ptr_range);

	if (is_kmem_ptr_range) {
		return kmem_locate_space(size + guard_offset, vmk_flags.vmkf_range_id,
		           vmk_flags.vmkf_last_free, start_inout, entry_out);
	}

#if XNU_TARGET_OS_OSX
	if (__improbable(vmk_flags.vmkf_32bit_map_va)) {
		assert(map != kernel_map);
		effective_range.max_address = MIN(map->max_offset, 0x00000000FFFFF000ULL);
	}
#endif /* XNU_TARGET_OS_OSX */

again:
	if (vmk_flags.vmkf_last_free) {
		hint = *start_inout;

		if (hint == 0 || hint > effective_range.max_address) {
			hint = effective_range.max_address;
		}
		if (hint <= effective_range.min_address) {
			return KERN_NO_SPACE;
		}
		limit = effective_range.min_address;
	} else {
		hint = *start_inout;

		if (vmk_flags.vmkf_map_jit) {
			if (map->jit_entry_exists &&
			    !VM_MAP_POLICY_ALLOW_MULTIPLE_JIT(map)) {
				return KERN_INVALID_ARGUMENT;
			}
			if (VM_MAP_POLICY_ALLOW_JIT_RANDOM_ADDRESS(map)) {
				vmk_flags.vmf_random_addr = true;
			}
		}

		if (vmk_flags.vmf_random_addr) {
			kern_return_t kr;

			kr = vm_map_random_address_for_size(map, &hint, size, vmk_flags);
			if (kr != KERN_SUCCESS) {
				return kr;
			}
		}
#if __x86_64__
		else if ((hint == 0 || hint == vm_map_min(map)) &&
		    !map->disable_vmentry_reuse &&
		    map->vmmap_high_start != 0) {
			hint = map->vmmap_high_start;
		}
#endif /* __x86_64__ */

		if (hint < effective_range.min_address) {
			hint = effective_range.min_address;
		}
		if (effective_range.max_address <= hint) {
			return KERN_NO_SPACE;
		}

		limit = effective_range.max_address;
	}
	entry = vm_map_store_find_space(map,
	    hint, limit, vmk_flags.vmkf_last_free,
	    guard_offset, size, mask,
	    start_inout);

	if (__improbable(entry == NULL)) {
		if (map->wait_for_space &&
		    guard_offset + size <=
		    effective_range.max_address - effective_range.min_address) {
			assert_wait((event_t)map, THREAD_ABORTSAFE);
			vm_map_unlock(map);
			thread_block(THREAD_CONTINUE_NULL);
			vm_map_lock(map);
			goto again;
		}
		return KERN_NO_SPACE;
	}

	if (entry_out) {
		*entry_out = entry;
	}
	return KERN_SUCCESS;
}


/*
 *	Routine:	vm_map_find_space
 *	Purpose:
 *		Allocate a range in the specified virtual address map,
 *		returning the entry allocated for that range.
 *		Used by kmem_alloc, etc.
 *
 *		The map must be NOT be locked. It will be returned locked
 *		on KERN_SUCCESS, unlocked on failure.
 *
 *		If an entry is allocated, the object/offset fields
 *		are initialized to zero.
 */
kern_return_t
vm_map_find_space(
	vm_map_t                map,
	vm_map_offset_t         hint_address,
	vm_map_size_t           size,
	vm_map_offset_t         mask,
	vm_map_kernel_flags_t   vmk_flags,
	vm_map_entry_t          *o_entry)       /* OUT */
{
	vm_map_entry_t          new_entry, entry;
	kern_return_t           kr;

	if (size == 0) {
		return KERN_INVALID_ARGUMENT;
	}

	new_entry = vm_map_entry_create(map);
	new_entry->use_pmap = true;
	new_entry->protection = VM_PROT_DEFAULT;
	new_entry->max_protection = VM_PROT_ALL;

	if (VM_MAP_PAGE_SHIFT(map) != PAGE_SHIFT) {
		new_entry->map_aligned = true;
	}
	if (vmk_flags.vmf_permanent) {
		new_entry->vme_permanent = true;
	}

	vm_map_lock(map);

	kr = vm_map_locate_space(map, size, mask, vmk_flags,
	    &hint_address, &entry);
	if (kr != KERN_SUCCESS) {
		vm_map_unlock(map);
		vm_map_entry_dispose(new_entry);
		return kr;
	}
	new_entry->vme_start = hint_address;
	new_entry->vme_end = hint_address + size;

	/*
	 *	At this point,
	 *
	 *	- new_entry's "vme_start" and "vme_end" should define
	 *	  the endpoints of the available new range,
	 *
	 *	- and "entry" should refer to the region before
	 *	  the new range,
	 *
	 *	- and the map should still be locked.
	 */

	assert(page_aligned(new_entry->vme_start));
	assert(page_aligned(new_entry->vme_end));
	assert(VM_MAP_PAGE_ALIGNED(new_entry->vme_start, VM_MAP_PAGE_MASK(map)));
	assert(VM_MAP_PAGE_ALIGNED(new_entry->vme_end, VM_MAP_PAGE_MASK(map)));

	/*
	 *	Insert the new entry into the list
	 */

	vm_map_store_entry_link(map, entry, new_entry,
	    VM_MAP_KERNEL_FLAGS_NONE);
	map->size += size;

	/*
	 *	Update the lookup hint
	 */
	SAVE_HINT_MAP_WRITE(map, new_entry);

	*o_entry = new_entry;
	return KERN_SUCCESS;
}

int vm_map_pmap_enter_print = FALSE;
int vm_map_pmap_enter_enable = FALSE;

/*
 *	Routine:	vm_map_pmap_enter [internal only]
 *
 *	Description:
 *		Force pages from the specified object to be entered into
 *		the pmap at the specified address if they are present.
 *		As soon as a page not found in the object the scan ends.
 *
 *	Returns:
 *		Nothing.
 *
 *	In/out conditions:
 *		The source map should not be locked on entry.
 */
__unused static void
vm_map_pmap_enter(
	vm_map_t                map,
	vm_map_offset_t         addr,
	vm_map_offset_t         end_addr,
	vm_object_t             object,
	vm_object_offset_t      offset,
	vm_prot_t               protection)
{
	int                     type_of_fault;
	kern_return_t           kr;
	uint8_t                 object_lock_type = 0;
	struct vm_object_fault_info fault_info = {};

	if (map->pmap == 0) {
		return;
	}

	assert(VM_MAP_PAGE_SHIFT(map) == PAGE_SHIFT);

	while (addr < end_addr) {
		vm_page_t       m;


		/*
		 * TODO:
		 * From vm_map_enter(), we come into this function without the map
		 * lock held or the object lock held.
		 * We haven't taken a reference on the object either.
		 * We should do a proper lookup on the map to make sure
		 * that things are sane before we go locking objects that
		 * could have been deallocated from under us.
		 */

		object_lock_type = OBJECT_LOCK_EXCLUSIVE;
		vm_object_lock(object);

		m = vm_page_lookup(object, offset);

		if (m == VM_PAGE_NULL || m->vmp_busy || m->vmp_fictitious ||
		    (m->vmp_unusual && (VMP_ERROR_GET(m) || m->vmp_restart || m->vmp_absent))) {
			vm_object_unlock(object);
			return;
		}

		if (vm_map_pmap_enter_print) {
			printf("vm_map_pmap_enter:");
			printf("map: %p, addr: %llx, object: %p, offset: %llx\n",
			    map, (unsigned long long)addr, object, (unsigned long long)offset);
		}
		type_of_fault = DBG_CACHE_HIT_FAULT;
		kr = vm_fault_enter(m, map->pmap,
		    addr,
		    PAGE_SIZE, 0,
		    protection, protection,
		    VM_PAGE_WIRED(m),
		    FALSE,                 /* change_wiring */
		    VM_KERN_MEMORY_NONE,                 /* tag - not wiring */
		    &fault_info,
		    NULL,                  /* need_retry */
		    &type_of_fault,
		    &object_lock_type); /* Exclusive lock mode. Will remain unchanged.*/

		vm_object_unlock(object);

		offset += PAGE_SIZE_64;
		addr += PAGE_SIZE;
	}
}

#define MAX_TRIES_TO_GET_RANDOM_ADDRESS 1000
static kern_return_t
vm_map_random_address_for_size(
	vm_map_t                map,
	vm_map_offset_t        *address,
	vm_map_size_t           size,
	vm_map_kernel_flags_t   vmk_flags)
{
	kern_return_t   kr = KERN_SUCCESS;
	int             tries = 0;
	vm_map_offset_t random_addr = 0;
	vm_map_offset_t hole_end;

	vm_map_entry_t  next_entry = VM_MAP_ENTRY_NULL;
	vm_map_entry_t  prev_entry = VM_MAP_ENTRY_NULL;
	vm_map_size_t   vm_hole_size = 0;
	vm_map_size_t   addr_space_size;
	bool            is_kmem_ptr;
	struct mach_vm_range effective_range;

	effective_range = vm_map_get_range(map, address, &vmk_flags, size,
	    &is_kmem_ptr);

	addr_space_size = effective_range.max_address - effective_range.min_address;
	if (size >= addr_space_size) {
		return KERN_NO_SPACE;
	}
	addr_space_size -= size;

	assert(VM_MAP_PAGE_ALIGNED(size, VM_MAP_PAGE_MASK(map)));

	while (tries < MAX_TRIES_TO_GET_RANDOM_ADDRESS) {
		if (startup_phase < STARTUP_SUB_ZALLOC) {
			random_addr = (vm_map_offset_t)early_random();
		} else {
			random_addr = (vm_map_offset_t)random();
		}
		random_addr <<= VM_MAP_PAGE_SHIFT(map);
		random_addr = vm_map_trunc_page(
			effective_range.min_address + (random_addr % addr_space_size),
			VM_MAP_PAGE_MASK(map));

#if CONFIG_PROB_GZALLOC
		if (map->pmap == kernel_pmap && pgz_owned(random_addr)) {
			continue;
		}
#endif /* CONFIG_PROB_GZALLOC */

		if (vm_map_lookup_entry(map, random_addr, &prev_entry) == FALSE) {
			if (prev_entry == vm_map_to_entry(map)) {
				next_entry = vm_map_first_entry(map);
			} else {
				next_entry = prev_entry->vme_next;
			}
			if (next_entry == vm_map_to_entry(map)) {
				hole_end = vm_map_max(map);
			} else {
				hole_end = next_entry->vme_start;
			}
			vm_hole_size = hole_end - random_addr;
			if (vm_hole_size >= size) {
				*address = random_addr;
				break;
			}
		}
		tries++;
	}

	if (tries == MAX_TRIES_TO_GET_RANDOM_ADDRESS) {
		kr = KERN_NO_SPACE;
	}
	return kr;
}

static boolean_t
vm_memory_malloc_no_cow(
	int alias)
{
	uint64_t alias_mask;

	if (!malloc_no_cow) {
		return FALSE;
	}
	if (alias > 63) {
		return FALSE;
	}
	alias_mask = 1ULL << alias;
	if (alias_mask & vm_memory_malloc_no_cow_mask) {
		return TRUE;
	}
	return FALSE;
}

uint64_t vm_map_enter_RLIMIT_AS_count = 0;
uint64_t vm_map_enter_RLIMIT_DATA_count = 0;
/*
 *	Routine:	vm_map_enter
 *
 *	Description:
 *		Allocate a range in the specified virtual address map.
 *		The resulting range will refer to memory defined by
 *		the given memory object and offset into that object.
 *
 *		Arguments are as defined in the vm_map call.
 */
static unsigned int vm_map_enter_restore_successes = 0;
static unsigned int vm_map_enter_restore_failures = 0;
kern_return_t
vm_map_enter(
	vm_map_t                map,
	vm_map_offset_t         *address,       /* IN/OUT */
	vm_map_size_t           size,
	vm_map_offset_t         mask,
	vm_map_kernel_flags_t   vmk_flags,
	vm_object_t             object,
	vm_object_offset_t      offset,
	boolean_t               needs_copy,
	vm_prot_t               cur_protection,
	vm_prot_t               max_protection,
	vm_inherit_t            inheritance)
{
	vm_map_entry_t          entry, new_entry;
	vm_map_offset_t         start, tmp_start, tmp_offset;
	vm_map_offset_t         end, tmp_end;
	vm_map_offset_t         tmp2_start, tmp2_end;
	vm_map_offset_t         step;
	kern_return_t           result = KERN_SUCCESS;
	bool                    map_locked = FALSE;
	bool                    pmap_empty = TRUE;
	bool                    new_mapping_established = FALSE;
	const bool              keep_map_locked = vmk_flags.vmkf_keep_map_locked;
	const bool              anywhere = !vmk_flags.vmf_fixed;
	const bool              purgable = vmk_flags.vmf_purgeable;
	const bool              overwrite = vmk_flags.vmf_overwrite;
	const bool              no_cache = vmk_flags.vmf_no_cache;
	const bool              is_submap = vmk_flags.vmkf_submap;
	const bool              permanent = vmk_flags.vmf_permanent;
	const bool              no_copy_on_read = vmk_flags.vmkf_no_copy_on_read;
	const bool              entry_for_jit = vmk_flags.vmkf_map_jit;
	const bool              iokit_acct = vmk_flags.vmkf_iokit_acct;
	const bool              resilient_codesign = vmk_flags.vmf_resilient_codesign;
	const bool              resilient_media = vmk_flags.vmf_resilient_media;
	const bool              entry_for_tpro = vmk_flags.vmf_tpro;
	const unsigned int      superpage_size = vmk_flags.vmf_superpage_size;
	const vm_tag_t          alias = vmk_flags.vm_tag;
	vm_tag_t                user_alias;
	kern_return_t           kr;
	bool                    clear_map_aligned = FALSE;
	vm_map_size_t           chunk_size = 0;
	vm_object_t             caller_object;
	VM_MAP_ZAP_DECLARE(zap_old_list);
	VM_MAP_ZAP_DECLARE(zap_new_list);

	caller_object = object;

	assertf(vmk_flags.__vmkf_unused == 0, "vmk_flags unused=0x%x\n", vmk_flags.__vmkf_unused);

	if (vmk_flags.vmf_4gb_chunk) {
#if defined(__LP64__)
		chunk_size = (4ULL * 1024 * 1024 * 1024); /* max. 4GB chunks for the new allocation */
#else /* __LP64__ */
		chunk_size = ANON_CHUNK_SIZE;
#endif /* __LP64__ */
	} else {
		chunk_size = ANON_CHUNK_SIZE;
	}



	if (superpage_size) {
		switch (superpage_size) {
			/*
			 * Note that the current implementation only supports
			 * a single size for superpages, SUPERPAGE_SIZE, per
			 * architecture. As soon as more sizes are supposed
			 * to be supported, SUPERPAGE_SIZE has to be replaced
			 * with a lookup of the size depending on superpage_size.
			 */
#ifdef __x86_64__
		case SUPERPAGE_SIZE_ANY:
			/* handle it like 2 MB and round up to page size */
			size = (size + 2 * 1024 * 1024 - 1) & ~(2 * 1024 * 1024 - 1);
			OS_FALLTHROUGH;
		case SUPERPAGE_SIZE_2MB:
			break;
#endif
		default:
			return KERN_INVALID_ARGUMENT;
		}
		mask = SUPERPAGE_SIZE - 1;
		if (size & (SUPERPAGE_SIZE - 1)) {
			return KERN_INVALID_ARGUMENT;
		}
		inheritance = VM_INHERIT_NONE;  /* fork() children won't inherit superpages */
	}


	if ((cur_protection & VM_PROT_WRITE) &&
	    (cur_protection & VM_PROT_EXECUTE) &&
#if XNU_TARGET_OS_OSX
	    map->pmap != kernel_pmap &&
	    (cs_process_global_enforcement() ||
	    (vmk_flags.vmkf_cs_enforcement_override
	    ? vmk_flags.vmkf_cs_enforcement
	    : (vm_map_cs_enforcement(map)
#if __arm64__
	    || !VM_MAP_IS_EXOTIC(map)
#endif /* __arm64__ */
	    ))) &&
#endif /* XNU_TARGET_OS_OSX */
#if CODE_SIGNING_MONITOR
	    (csm_address_space_exempt(map->pmap) != KERN_SUCCESS) &&
#endif
	    (VM_MAP_POLICY_WX_FAIL(map) ||
	    VM_MAP_POLICY_WX_STRIP_X(map)) &&
	    !entry_for_jit) {
		boolean_t vm_protect_wx_fail = VM_MAP_POLICY_WX_FAIL(map);

		DTRACE_VM3(cs_wx,
		    uint64_t, 0,
		    uint64_t, 0,
		    vm_prot_t, cur_protection);
		printf("CODE SIGNING: %d[%s] %s: curprot cannot be write+execute. %s\n",
		    proc_selfpid(),
		    (get_bsdtask_info(current_task())
		    ? proc_name_address(get_bsdtask_info(current_task()))
		    : "?"),
		    __FUNCTION__,
		    (vm_protect_wx_fail ? "failing" : "turning off execute"));
		cur_protection &= ~VM_PROT_EXECUTE;
		if (vm_protect_wx_fail) {
			return KERN_PROTECTION_FAILURE;
		}
	}

	if (entry_for_jit
	    && cur_protection != VM_PROT_ALL) {
		/*
		 * Native macOS processes and all non-macOS processes are
		 * expected to create JIT regions via mmap(MAP_JIT, RWX) but
		 * the RWX requirement was not enforced, and thus, we must live
		 * with our sins. We are now dealing with a JIT mapping without
		 * RWX.
		 *
		 * We deal with these by letting the MAP_JIT stick in order
		 * to avoid CS violations when these pages are mapped executable
		 * down the line. In order to appease the page table monitor (you
		 * know what I'm talking about), these pages will end up being
		 * marked as XNU_USER_DEBUG, which will be allowed because we
		 * don't enforce the code signing monitor on macOS systems. If
		 * the user-space application ever changes permissions to RWX,
		 * which they are allowed to since the mapping was originally
		 * created with MAP_JIT, then they'll switch over to using the
		 * XNU_USER_JIT type, and won't be allowed to downgrade any
		 * more after that.
		 *
		 * When not on macOS, a MAP_JIT mapping without VM_PROT_ALL is
		 * strictly disallowed.
		 */

#if XNU_TARGET_OS_OSX
		/*
		 * Continue to allow non-RWX JIT
		 */
#else
		/* non-macOS: reject JIT regions without RWX */
		DTRACE_VM3(cs_wx,
		    uint64_t, 0,
		    uint64_t, 0,
		    vm_prot_t, cur_protection);
		printf("CODE SIGNING: %d[%s] %s(%d): JIT requires RWX: failing. \n",
		    proc_selfpid(),
		    (get_bsdtask_info(current_task())
		    ? proc_name_address(get_bsdtask_info(current_task()))
		    : "?"),
		    __FUNCTION__,
		    cur_protection);
		return KERN_PROTECTION_FAILURE;
#endif
	}

	/*
	 * If the task has requested executable lockdown,
	 * deny any new executable mapping.
	 */
	if (map->map_disallow_new_exec == TRUE) {
		if (cur_protection & VM_PROT_EXECUTE) {
			return KERN_PROTECTION_FAILURE;
		}
	}

	if (resilient_codesign) {
		assert(!is_submap);
		int reject_prot = (needs_copy ? VM_PROT_ALLEXEC : (VM_PROT_WRITE | VM_PROT_ALLEXEC));
		if ((cur_protection | max_protection) & reject_prot) {
			return KERN_PROTECTION_FAILURE;
		}
	}

	if (resilient_media) {
		assert(!is_submap);
//		assert(!needs_copy);
		if (object != VM_OBJECT_NULL &&
		    !object->internal) {
			/*
			 * This mapping is directly backed by an external
			 * memory manager (e.g. a vnode pager for a file):
			 * we would not have any safe place to inject
			 * a zero-filled page if an actual page is not
			 * available, without possibly impacting the actual
			 * contents of the mapped object (e.g. the file),
			 * so we can't provide any media resiliency here.
			 */
			return KERN_INVALID_ARGUMENT;
		}
	}

	if (entry_for_tpro) {
		/*
		 * TPRO overrides the effective permissions of the region
		 * and explicitly maps as RW. Ensure we have been passed
		 * the expected permissions. We accept `cur_protections`
		 * RO as that will be handled on fault.
		 */
		if (!(max_protection & VM_PROT_READ) ||
		    !(max_protection & VM_PROT_WRITE) ||
		    !(cur_protection & VM_PROT_READ)) {
			return KERN_PROTECTION_FAILURE;
		}

		/*
		 * We can now downgrade the cur_protection to RO. This is a mild lie
		 * to the VM layer. But TPRO will be responsible for toggling the
		 * protections between RO/RW
		 */
		cur_protection = VM_PROT_READ;
	}

	if (is_submap) {
		vm_map_t submap;
		if (purgable) {
			/* submaps can not be purgeable */
			return KERN_INVALID_ARGUMENT;
		}
		if (object == VM_OBJECT_NULL) {
			/* submaps can not be created lazily */
			return KERN_INVALID_ARGUMENT;
		}
		submap = (vm_map_t) object;
		if (VM_MAP_PAGE_SHIFT(submap) != VM_MAP_PAGE_SHIFT(map)) {
			/* page size mismatch */
			return KERN_INVALID_ARGUMENT;
		}
	}
	if (vmk_flags.vmkf_already) {
		/*
		 * VM_FLAGS_ALREADY says that it's OK if the same mapping
		 * is already present.  For it to be meaningul, the requested
		 * mapping has to be at a fixed address (!VM_FLAGS_ANYWHERE) and
		 * we shouldn't try and remove what was mapped there first
		 * (!VM_FLAGS_OVERWRITE).
		 */
		if (!vmk_flags.vmf_fixed || vmk_flags.vmf_overwrite) {
			return KERN_INVALID_ARGUMENT;
		}
	}

	if (size == 0 ||
	    (offset & MIN(VM_MAP_PAGE_MASK(map), PAGE_MASK_64)) != 0) {
		*address = 0;
		return KERN_INVALID_ARGUMENT;
	}

	if (map->pmap == kernel_pmap) {
		user_alias = VM_KERN_MEMORY_NONE;
	} else {
		user_alias = alias;
	}

	if (user_alias == VM_MEMORY_MALLOC_MEDIUM) {
		chunk_size = MALLOC_MEDIUM_CHUNK_SIZE;
	}

#define RETURN(value)   { result = value; goto BailOut; }

	assertf(VM_MAP_PAGE_ALIGNED(*address, FOURK_PAGE_MASK), "0x%llx", (uint64_t)*address);
	assertf(VM_MAP_PAGE_ALIGNED(size, FOURK_PAGE_MASK), "0x%llx", (uint64_t)size);
	if (VM_MAP_PAGE_MASK(map) >= PAGE_MASK) {
		assertf(page_aligned(*address), "0x%llx", (uint64_t)*address);
		assertf(page_aligned(size), "0x%llx", (uint64_t)size);
	}

	if (VM_MAP_PAGE_MASK(map) >= PAGE_MASK &&
	    !VM_MAP_PAGE_ALIGNED(size, VM_MAP_PAGE_MASK(map))) {
		/*
		 * In most cases, the caller rounds the size up to the
		 * map's page size.
		 * If we get a size that is explicitly not map-aligned here,
		 * we'll have to respect the caller's wish and mark the
		 * mapping as "not map-aligned" to avoid tripping the
		 * map alignment checks later.
		 */
		clear_map_aligned = TRUE;
	}
	if (!anywhere &&
	    VM_MAP_PAGE_MASK(map) >= PAGE_MASK &&
	    !VM_MAP_PAGE_ALIGNED(*address, VM_MAP_PAGE_MASK(map))) {
		/*
		 * We've been asked to map at a fixed address and that
		 * address is not aligned to the map's specific alignment.
		 * The caller should know what it's doing (i.e. most likely
		 * mapping some fragmented copy map, transferring memory from
		 * a VM map with a different alignment), so clear map_aligned
		 * for this new VM map entry and proceed.
		 */
		clear_map_aligned = TRUE;
	}

	/*
	 * Only zero-fill objects are allowed to be purgable.
	 * LP64todo - limit purgable objects to 32-bits for now
	 */
	if (purgable &&
	    (offset != 0 ||
	    (object != VM_OBJECT_NULL &&
	    (object->vo_size != size ||
	    object->purgable == VM_PURGABLE_DENY))
#if __LP64__
	    || size > ANON_MAX_SIZE
#endif
	    )) {
		return KERN_INVALID_ARGUMENT;
	}

	start = *address;

	if (anywhere) {
		vm_map_lock(map);
		map_locked = TRUE;

		result = vm_map_locate_space(map, size, mask, vmk_flags,
		    &start, &entry);
		if (result != KERN_SUCCESS) {
			goto BailOut;
		}

		*address = start;
		end = start + size;
		assert(VM_MAP_PAGE_ALIGNED(*address,
		    VM_MAP_PAGE_MASK(map)));
	} else {
		vm_map_offset_t effective_min_offset, effective_max_offset;

		effective_min_offset = map->min_offset;
		effective_max_offset = map->max_offset;

		if (vmk_flags.vmkf_beyond_max) {
			/*
			 * Allow an insertion beyond the map's max offset.
			 */
			effective_max_offset = 0x00000000FFFFF000ULL;
			if (vm_map_is_64bit(map)) {
				effective_max_offset = 0xFFFFFFFFFFFFF000ULL;
			}
#if XNU_TARGET_OS_OSX
		} else if (__improbable(vmk_flags.vmkf_32bit_map_va)) {
			effective_max_offset = MIN(map->max_offset, 0x00000000FFFFF000ULL);
#endif /* XNU_TARGET_OS_OSX */
		}

		if (VM_MAP_PAGE_SHIFT(map) < PAGE_SHIFT &&
		    !overwrite &&
		    user_alias == VM_MEMORY_REALLOC) {
			/*
			 * Force realloc() to switch to a new allocation,
			 * to prevent 4k-fragmented virtual ranges.
			 */
//			DEBUG4K_ERROR("no realloc in place");
			return KERN_NO_SPACE;
		}

		/*
		 *	Verify that:
		 *		the address doesn't itself violate
		 *		the mask requirement.
		 */

		vm_map_lock(map);
		map_locked = TRUE;
		if ((start & mask) != 0) {
			RETURN(KERN_NO_SPACE);
		}

#if CONFIG_MAP_RANGES
		if (map->uses_user_ranges) {
			struct mach_vm_range r;

			vm_map_user_range_resolve(map, start, 1, &r);
			if (r.max_address == 0) {
				RETURN(KERN_INVALID_ADDRESS);
			}
			effective_min_offset = r.min_address;
			effective_max_offset = r.max_address;
		}
#endif /* CONFIG_MAP_RANGES */

		if ((startup_phase >= STARTUP_SUB_KMEM) && !is_submap &&
		    (map == kernel_map)) {
			mach_vm_range_t r = kmem_validate_range_for_overwrite(start, size);
			effective_min_offset = r->min_address;
			effective_max_offset = r->max_address;
		}

		/*
		 *	...	the address is within bounds
		 */

		end = start + size;

		if ((start < effective_min_offset) ||
		    (end > effective_max_offset) ||
		    (start >= end)) {
			RETURN(KERN_INVALID_ADDRESS);
		}

		if (overwrite) {
			vmr_flags_t remove_flags = VM_MAP_REMOVE_NO_MAP_ALIGN | VM_MAP_REMOVE_TO_OVERWRITE;
			kern_return_t remove_kr;

			/*
			 * Fixed mapping and "overwrite" flag: attempt to
			 * remove all existing mappings in the specified
			 * address range, saving them in our "zap_old_list".
			 *
			 * This avoids releasing the VM map lock in
			 * vm_map_entry_delete() and allows atomicity
			 * when we want to replace some mappings with a new one.
			 * It also allows us to restore the old VM mappings if the
			 * new mapping fails.
			 */
			remove_flags |= VM_MAP_REMOVE_NO_YIELD;

			if (vmk_flags.vmkf_overwrite_immutable) {
				/* we can overwrite immutable mappings */
				remove_flags |= VM_MAP_REMOVE_IMMUTABLE;
			}
			if (vmk_flags.vmkf_remap_prot_copy) {
				remove_flags |= VM_MAP_REMOVE_IMMUTABLE_CODE;
			}
			remove_kr = vm_map_delete(map, start, end, remove_flags,
			    KMEM_GUARD_NONE, &zap_old_list).kmr_return;
			if (remove_kr) {
				/* XXX FBDP restore zap_old_list? */
				RETURN(remove_kr);
			}
		}

		/*
		 *	...	the starting address isn't allocated
		 */

		if (vm_map_lookup_entry(map, start, &entry)) {
			if (!(vmk_flags.vmkf_already)) {
				RETURN(KERN_NO_SPACE);
			}
			/*
			 * Check if what's already there is what we want.
			 */
			tmp_start = start;
			tmp_offset = offset;
			if (entry->vme_start < start) {
				tmp_start -= start - entry->vme_start;
				tmp_offset -= start - entry->vme_start;
			}
			for (; entry->vme_start < end;
			    entry = entry->vme_next) {
				/*
				 * Check if the mapping's attributes
				 * match the existing map entry.
				 */
				if (entry == vm_map_to_entry(map) ||
				    entry->vme_start != tmp_start ||
				    entry->is_sub_map != is_submap ||
				    VME_OFFSET(entry) != tmp_offset ||
				    entry->needs_copy != needs_copy ||
				    entry->protection != cur_protection ||
				    entry->max_protection != max_protection ||
				    entry->inheritance != inheritance ||
				    entry->iokit_acct != iokit_acct ||
				    VME_ALIAS(entry) != alias) {
					/* not the same mapping ! */
					RETURN(KERN_NO_SPACE);
				}
				/*
				 * Check if the same object is being mapped.
				 */
				if (is_submap) {
					if (VME_SUBMAP(entry) !=
					    (vm_map_t) object) {
						/* not the same submap */
						RETURN(KERN_NO_SPACE);
					}
				} else {
					if (VME_OBJECT(entry) != object) {
						/* not the same VM object... */
						vm_object_t obj2;

						obj2 = VME_OBJECT(entry);
						if ((obj2 == VM_OBJECT_NULL ||
						    obj2->internal) &&
						    (object == VM_OBJECT_NULL ||
						    object->internal)) {
							/*
							 * ... but both are
							 * anonymous memory,
							 * so equivalent.
							 */
						} else {
							RETURN(KERN_NO_SPACE);
						}
					}
				}

				tmp_offset += entry->vme_end - entry->vme_start;
				tmp_start += entry->vme_end - entry->vme_start;
				if (entry->vme_end >= end) {
					/* reached the end of our mapping */
					break;
				}
			}
			/* it all matches:  let's use what's already there ! */
			RETURN(KERN_MEMORY_PRESENT);
		}

		/*
		 *	...	the next region doesn't overlap the
		 *		end point.
		 */

		if ((entry->vme_next != vm_map_to_entry(map)) &&
		    (entry->vme_next->vme_start < end)) {
			RETURN(KERN_NO_SPACE);
		}
	}

	/*
	 *	At this point,
	 *		"start" and "end" should define the endpoints of the
	 *			available new range, and
	 *		"entry" should refer to the region before the new
	 *			range, and
	 *
	 *		the map should be locked.
	 */

	/*
	 *	See whether we can avoid creating a new entry (and object) by
	 *	extending one of our neighbors.  [So far, we only attempt to
	 *	extend from below.]  Note that we can never extend/join
	 *	purgable objects because they need to remain distinct
	 *	entities in order to implement their "volatile object"
	 *	semantics.
	 */

	if (purgable ||
	    entry_for_jit ||
	    entry_for_tpro ||
	    vm_memory_malloc_no_cow(user_alias)) {
		if (object == VM_OBJECT_NULL) {
			object = vm_object_allocate(size);
			vm_object_lock(object);
			object->copy_strategy = MEMORY_OBJECT_COPY_NONE;
			VM_OBJECT_SET_TRUE_SHARE(object, FALSE);
			if (malloc_no_cow_except_fork &&
			    !purgable &&
			    !entry_for_jit &&
			    !entry_for_tpro &&
			    vm_memory_malloc_no_cow(user_alias)) {
				object->copy_strategy = MEMORY_OBJECT_COPY_DELAY_FORK;
				VM_OBJECT_SET_TRUE_SHARE(object, TRUE);
			}
			if (purgable) {
				task_t owner;
				VM_OBJECT_SET_PURGABLE(object, VM_PURGABLE_NONVOLATILE);
				if (map->pmap == kernel_pmap) {
					/*
					 * Purgeable mappings made in a kernel
					 * map are "owned" by the kernel itself
					 * rather than the current user task
					 * because they're likely to be used by
					 * more than this user task (see
					 * execargs_purgeable_allocate(), for
					 * example).
					 */
					owner = kernel_task;
				} else {
					owner = current_task();
				}
				assert(object->vo_owner == NULL);
				assert(object->resident_page_count == 0);
				assert(object->wired_page_count == 0);
				vm_purgeable_nonvolatile_enqueue(object, owner);
			}
			vm_object_unlock(object);
			offset = (vm_object_offset_t)0;
		}
	} else if (VM_MAP_PAGE_SHIFT(map) < PAGE_SHIFT) {
		/* no coalescing if address space uses sub-pages */
	} else if ((is_submap == FALSE) &&
	    (object == VM_OBJECT_NULL) &&
	    (entry != vm_map_to_entry(map)) &&
	    (entry->vme_end == start) &&
	    (!entry->is_shared) &&
	    (!entry->is_sub_map) &&
	    (!entry->in_transition) &&
	    (!entry->needs_wakeup) &&
	    (entry->behavior == VM_BEHAVIOR_DEFAULT) &&
	    (entry->protection == cur_protection) &&
	    (entry->max_protection == max_protection) &&
	    (entry->inheritance == inheritance) &&
	    ((user_alias == VM_MEMORY_REALLOC) ||
	    (VME_ALIAS(entry) == alias)) &&
	    (entry->no_cache == no_cache) &&
	    (entry->vme_permanent == permanent) &&
	    /* no coalescing for immutable executable mappings */
	    !((entry->protection & VM_PROT_EXECUTE) &&
	    entry->vme_permanent) &&
	    (!entry->superpage_size && !superpage_size) &&
	    /*
	     * No coalescing if not map-aligned, to avoid propagating
	     * that condition any further than needed:
	     */
	    (!entry->map_aligned || !clear_map_aligned) &&
	    (!entry->zero_wired_pages) &&
	    (!entry->used_for_jit && !entry_for_jit) &&
#if __arm64e__
	    (!entry->used_for_tpro && !entry_for_tpro) &&
#endif
	    (!entry->csm_associated) &&
	    (entry->iokit_acct == iokit_acct) &&
	    (!entry->vme_resilient_codesign) &&
	    (!entry->vme_resilient_media) &&
	    (!entry->vme_atomic) &&
	    (entry->vme_no_copy_on_read == no_copy_on_read) &&

	    ((entry->vme_end - entry->vme_start) + size <=
	    (user_alias == VM_MEMORY_REALLOC ?
	    ANON_CHUNK_SIZE :
	    NO_COALESCE_LIMIT)) &&

	    (entry->wired_count == 0)) {        /* implies user_wired_count == 0 */
		if (vm_object_coalesce(VME_OBJECT(entry),
		    VM_OBJECT_NULL,
		    VME_OFFSET(entry),
		    (vm_object_offset_t) 0,
		    (vm_map_size_t)(entry->vme_end - entry->vme_start),
		    (vm_map_size_t)(end - entry->vme_end))) {
			/*
			 *	Coalesced the two objects - can extend
			 *	the previous map entry to include the
			 *	new range.
			 */
			map->size += (end - entry->vme_end);
			assert(entry->vme_start < end);
			assert(VM_MAP_PAGE_ALIGNED(end,
			    VM_MAP_PAGE_MASK(map)));
			if (__improbable(vm_debug_events)) {
				DTRACE_VM5(map_entry_extend, vm_map_t, map, vm_map_entry_t, entry, vm_address_t, entry->vme_start, vm_address_t, entry->vme_end, vm_address_t, end);
			}
			entry->vme_end = end;
			if (map->holelistenabled) {
				vm_map_store_update_first_free(map, entry, TRUE);
			} else {
				vm_map_store_update_first_free(map, map->first_free, TRUE);
			}
			new_mapping_established = TRUE;
			RETURN(KERN_SUCCESS);
		}
	}

	step = superpage_size ? SUPERPAGE_SIZE : (end - start);
	new_entry = NULL;

	if (vmk_flags.vmkf_submap_adjust) {
		vm_map_adjust_offsets((vm_map_t)caller_object, start, end);
		offset = start;
	}

	for (tmp2_start = start; tmp2_start < end; tmp2_start += step) {
		tmp2_end = tmp2_start + step;
		/*
		 *	Create a new entry
		 *
		 * XXX FBDP
		 * The reserved "page zero" in each process's address space can
		 * be arbitrarily large.  Splitting it into separate objects and
		 * therefore different VM map entries serves no purpose and just
		 * slows down operations on the VM map, so let's not split the
		 * allocation into chunks if the max protection is NONE.  That
		 * memory should never be accessible, so it will never get to the
		 * default pager.
		 */
		tmp_start = tmp2_start;
		if (!is_submap &&
		    object == VM_OBJECT_NULL &&
		    size > chunk_size &&
		    max_protection != VM_PROT_NONE &&
		    superpage_size == 0) {
			tmp_end = tmp_start + chunk_size;
		} else {
			tmp_end = tmp2_end;
		}
		do {
			if (!is_submap &&
			    object != VM_OBJECT_NULL &&
			    object->internal &&
			    offset + (tmp_end - tmp_start) > object->vo_size) {
//				printf("FBDP object %p size 0x%llx overmapping offset 0x%llx size 0x%llx\n", object, object->vo_size, offset, (uint64_t)(tmp_end - tmp_start));
				DTRACE_VM5(vm_map_enter_overmap,
				    vm_map_t, map,
				    vm_map_address_t, tmp_start,
				    vm_map_address_t, tmp_end,
				    vm_object_offset_t, offset,
				    vm_object_size_t, object->vo_size);
			}
			new_entry = vm_map_entry_insert(map,
			    entry, tmp_start, tmp_end,
			    object, offset, vmk_flags,
			    needs_copy,
			    cur_protection, max_protection,
			    (entry_for_jit && !VM_MAP_POLICY_ALLOW_JIT_INHERIT(map) ?
			    VM_INHERIT_NONE : inheritance),
			    clear_map_aligned);

			assert(!is_kernel_object(object) || (VM_KERN_MEMORY_NONE != alias));

			if (resilient_codesign) {
				int reject_prot = (needs_copy ? VM_PROT_ALLEXEC : (VM_PROT_WRITE | VM_PROT_ALLEXEC));
				if (!((cur_protection | max_protection) & reject_prot)) {
					new_entry->vme_resilient_codesign = TRUE;
				}
			}

			if (resilient_media &&
			    (object == VM_OBJECT_NULL ||
			    object->internal)) {
				new_entry->vme_resilient_media = TRUE;
			}

			assert(!new_entry->iokit_acct);
			if (!is_submap &&
			    object != VM_OBJECT_NULL &&
			    (object->purgable != VM_PURGABLE_DENY ||
			    object->vo_ledger_tag)) {
				assert(new_entry->use_pmap);
				assert(!new_entry->iokit_acct);
				/*
				 * Turn off pmap accounting since
				 * purgeable (or tagged) objects have their
				 * own ledgers.
				 */
				new_entry->use_pmap = FALSE;
			} else if (!is_submap &&
			    iokit_acct &&
			    object != VM_OBJECT_NULL &&
			    object->internal) {
				/* alternate accounting */
				assert(!new_entry->iokit_acct);
				assert(new_entry->use_pmap);
				new_entry->iokit_acct = TRUE;
				new_entry->use_pmap = FALSE;
				DTRACE_VM4(
					vm_map_iokit_mapped_region,
					vm_map_t, map,
					vm_map_offset_t, new_entry->vme_start,
					vm_map_offset_t, new_entry->vme_end,
					int, VME_ALIAS(new_entry));
				vm_map_iokit_mapped_region(
					map,
					(new_entry->vme_end -
					new_entry->vme_start));
			} else if (!is_submap) {
				assert(!new_entry->iokit_acct);
				assert(new_entry->use_pmap);
			}

			if (is_submap) {
				vm_map_t        submap;
				boolean_t       submap_is_64bit;
				boolean_t       use_pmap;

				assert(new_entry->is_sub_map);
				assert(!new_entry->use_pmap);
				assert(!new_entry->iokit_acct);
				submap = (vm_map_t) object;
				submap_is_64bit = vm_map_is_64bit(submap);
				use_pmap = vmk_flags.vmkf_nested_pmap;
#ifndef NO_NESTED_PMAP
				if (use_pmap && submap->pmap == NULL) {
					ledger_t ledger = map->pmap->ledger;
					/* we need a sub pmap to nest... */
					submap->pmap = pmap_create_options(ledger, 0,
					    submap_is_64bit ? PMAP_CREATE_64BIT : 0);
					if (submap->pmap == NULL) {
						/* let's proceed without nesting... */
					}
#if defined(__arm64__)
					else {
						pmap_set_nested(submap->pmap);
					}
#endif
				}
				if (use_pmap && submap->pmap != NULL) {
					if (VM_MAP_PAGE_SHIFT(map) != VM_MAP_PAGE_SHIFT(submap)) {
						DEBUG4K_ERROR("map %p (%d) submap %p (%d): incompatible page sizes\n", map, VM_MAP_PAGE_SHIFT(map), submap, VM_MAP_PAGE_SHIFT(submap));
						kr = KERN_FAILURE;
					} else {
						kr = pmap_nest(map->pmap,
						    submap->pmap,
						    tmp_start,
						    tmp_end - tmp_start);
					}
					if (kr != KERN_SUCCESS) {
						printf("vm_map_enter: "
						    "pmap_nest(0x%llx,0x%llx) "
						    "error 0x%x\n",
						    (long long)tmp_start,
						    (long long)tmp_end,
						    kr);
					} else {
						/* we're now nested ! */
						new_entry->use_pmap = TRUE;
						pmap_empty = FALSE;
					}
				}
#endif /* NO_NESTED_PMAP */
			}
			entry = new_entry;

			if (superpage_size) {
				vm_page_t pages, m;
				vm_object_t sp_object;
				vm_object_offset_t sp_offset;

				VME_OFFSET_SET(entry, 0);

				/* allocate one superpage */
				kr = cpm_allocate(SUPERPAGE_SIZE, &pages, 0, SUPERPAGE_NBASEPAGES - 1, TRUE, 0);
				if (kr != KERN_SUCCESS) {
					/* deallocate whole range... */
					new_mapping_established = TRUE;
					/* ... but only up to "tmp_end" */
					size -= end - tmp_end;
					RETURN(kr);
				}

				/* create one vm_object per superpage */
				sp_object = vm_object_allocate((vm_map_size_t)(entry->vme_end - entry->vme_start));
				vm_object_lock(sp_object);
				sp_object->copy_strategy = MEMORY_OBJECT_COPY_NONE;
				VM_OBJECT_SET_PHYS_CONTIGUOUS(sp_object, TRUE);
				sp_object->vo_shadow_offset = (vm_object_offset_t)VM_PAGE_GET_PHYS_PAGE(pages) * PAGE_SIZE;
				VME_OBJECT_SET(entry, sp_object, false, 0);
				assert(entry->use_pmap);

				/* enter the base pages into the object */
				for (sp_offset = 0;
				    sp_offset < SUPERPAGE_SIZE;
				    sp_offset += PAGE_SIZE) {
					m = pages;
					pmap_zero_page(VM_PAGE_GET_PHYS_PAGE(m));
					pages = NEXT_PAGE(m);
					*(NEXT_PAGE_PTR(m)) = VM_PAGE_NULL;
					vm_page_insert_wired(m, sp_object, sp_offset, VM_KERN_MEMORY_OSFMK);
				}
				vm_object_unlock(sp_object);
			}
		} while (tmp_end != tmp2_end &&
		    (tmp_start = tmp_end) &&
		    (tmp_end = (tmp2_end - tmp_end > chunk_size) ?
		    tmp_end + chunk_size : tmp2_end));
	}

	new_mapping_established = TRUE;

BailOut:
	assert(map_locked == TRUE);

	/*
	 * Address space limit enforcement (RLIMIT_AS and RLIMIT_DATA):
	 * If we have identified and possibly established the new mapping(s),
	 * make sure we did not go beyond the address space limit.
	 */
	if (result == KERN_SUCCESS) {
		if (map->size_limit != RLIM_INFINITY &&
		    map->size > map->size_limit) {
			/*
			 * Establishing the requested mappings would exceed
			 * the process's RLIMIT_AS limit: fail with
			 * KERN_NO_SPACE.
			 */
			result = KERN_NO_SPACE;
			printf("%d[%s] %s: map size 0x%llx over RLIMIT_AS 0x%llx\n",
			    proc_selfpid(),
			    (get_bsdtask_info(current_task())
			    ? proc_name_address(get_bsdtask_info(current_task()))
			    : "?"),
			    __FUNCTION__,
			    (uint64_t) map->size,
			    (uint64_t) map->size_limit);
			DTRACE_VM2(vm_map_enter_RLIMIT_AS,
			    vm_map_size_t, map->size,
			    uint64_t, map->size_limit);
			vm_map_enter_RLIMIT_AS_count++;
		} else if (map->data_limit != RLIM_INFINITY &&
		    map->size > map->data_limit) {
			/*
			 * Establishing the requested mappings would exceed
			 * the process's RLIMIT_DATA limit: fail with
			 * KERN_NO_SPACE.
			 */
			result = KERN_NO_SPACE;
			printf("%d[%s] %s: map size 0x%llx over RLIMIT_DATA 0x%llx\n",
			    proc_selfpid(),
			    (get_bsdtask_info(current_task())
			    ? proc_name_address(get_bsdtask_info(current_task()))
			    : "?"),
			    __FUNCTION__,
			    (uint64_t) map->size,
			    (uint64_t) map->data_limit);
			DTRACE_VM2(vm_map_enter_RLIMIT_DATA,
			    vm_map_size_t, map->size,
			    uint64_t, map->data_limit);
			vm_map_enter_RLIMIT_DATA_count++;
		}
	}

	if (result == KERN_SUCCESS) {
		vm_prot_t pager_prot;
		memory_object_t pager;

#if DEBUG
		if (pmap_empty &&
		    !(vmk_flags.vmkf_no_pmap_check)) {
			assert(pmap_is_empty(map->pmap,
			    *address,
			    *address + size));
		}
#endif /* DEBUG */

		/*
		 * For "named" VM objects, let the pager know that the
		 * memory object is being mapped.  Some pagers need to keep
		 * track of this, to know when they can reclaim the memory
		 * object, for example.
		 * VM calls memory_object_map() for each mapping (specifying
		 * the protection of each mapping) and calls
		 * memory_object_last_unmap() when all the mappings are gone.
		 */
		pager_prot = max_protection;
		if (needs_copy) {
			/*
			 * Copy-On-Write mapping: won't modify
			 * the memory object.
			 */
			pager_prot &= ~VM_PROT_WRITE;
		}
		if (!is_submap &&
		    object != VM_OBJECT_NULL &&
		    object->named &&
		    object->pager != MEMORY_OBJECT_NULL) {
			vm_object_lock(object);
			pager = object->pager;
			if (object->named &&
			    pager != MEMORY_OBJECT_NULL) {
				assert(object->pager_ready);
				vm_object_mapping_wait(object, THREAD_UNINT);
				vm_object_mapping_begin(object);
				vm_object_unlock(object);

				kr = memory_object_map(pager, pager_prot);
				assert(kr == KERN_SUCCESS);

				vm_object_lock(object);
				vm_object_mapping_end(object);
			}
			vm_object_unlock(object);
		}
	}

	assert(map_locked == TRUE);

	if (new_mapping_established) {
		/*
		 * If we release the map lock for any reason below,
		 * another thread could deallocate our new mapping,
		 * releasing the caller's reference on "caller_object",
		 * which was transferred to the mapping.
		 * If this was the only reference, the object could be
		 * destroyed.
		 *
		 * We need to take an extra reference on "caller_object"
		 * to keep it alive if we need to return the caller's
		 * reference to the caller in case of failure.
		 */
		if (is_submap) {
			vm_map_reference((vm_map_t)caller_object);
		} else {
			vm_object_reference(caller_object);
		}
	}

	if (!keep_map_locked) {
		vm_map_unlock(map);
		map_locked = FALSE;
		entry = VM_MAP_ENTRY_NULL;
		new_entry = VM_MAP_ENTRY_NULL;
	}

	/*
	 * We can't hold the map lock if we enter this block.
	 */

	if (result == KERN_SUCCESS) {
		/*	Wire down the new entry if the user
		 *	requested all new map entries be wired.
		 */
		if ((map->wiring_required) || (superpage_size)) {
			assert(!keep_map_locked);
			pmap_empty = FALSE; /* pmap won't be empty */
			kr = vm_map_wire_kernel(map, start, end,
			    cur_protection, VM_KERN_MEMORY_MLOCK,
			    TRUE);
			result = kr;
		}

	}

	if (result != KERN_SUCCESS) {
		if (new_mapping_established) {
			vmr_flags_t remove_flags = VM_MAP_REMOVE_NO_FLAGS;

			/*
			 * We have to get rid of the new mappings since we
			 * won't make them available to the user.
			 * Try and do that atomically, to minimize the risk
			 * that someone else create new mappings that range.
			 */
			if (!map_locked) {
				vm_map_lock(map);
				map_locked = TRUE;
			}
			remove_flags |= VM_MAP_REMOVE_NO_MAP_ALIGN;
			remove_flags |= VM_MAP_REMOVE_NO_YIELD;
			if (permanent) {
				remove_flags |= VM_MAP_REMOVE_IMMUTABLE;
			}
			(void) vm_map_delete(map,
			    *address, *address + size,
			    remove_flags,
			    KMEM_GUARD_NONE, &zap_new_list);
		}

		if (vm_map_zap_first_entry(&zap_old_list)) {
			vm_map_entry_t entry1, entry2;

			/*
			 * The new mapping failed.  Attempt to restore
			 * the old mappings, saved in the "zap_old_map".
			 */
			if (!map_locked) {
				vm_map_lock(map);
				map_locked = TRUE;
			}

			/* first check if the coast is still clear */
			start = vm_map_zap_first_entry(&zap_old_list)->vme_start;
			end   = vm_map_zap_last_entry(&zap_old_list)->vme_end;

			if (vm_map_lookup_entry(map, start, &entry1) ||
			    vm_map_lookup_entry(map, end, &entry2) ||
			    entry1 != entry2) {
				/*
				 * Part of that range has already been
				 * re-mapped:  we can't restore the old
				 * mappings...
				 */
				vm_map_enter_restore_failures++;
			} else {
				/*
				 * Transfer the saved map entries from
				 * "zap_old_map" to the original "map",
				 * inserting them all after "entry1".
				 */
				while ((entry2 = vm_map_zap_pop(&zap_old_list))) {
					vm_map_size_t entry_size;

					entry_size = (entry2->vme_end -
					    entry2->vme_start);
					vm_map_store_entry_link(map, entry1, entry2,
					    VM_MAP_KERNEL_FLAGS_NONE);
					map->size += entry_size;
					entry1 = entry2;
				}
				if (map->wiring_required) {
					/*
					 * XXX TODO: we should rewire the
					 * old pages here...
					 */
				}
				vm_map_enter_restore_successes++;
			}
		}
	}

	/*
	 * The caller is responsible for releasing the lock if it requested to
	 * keep the map locked.
	 */
	if (map_locked && !keep_map_locked) {
		vm_map_unlock(map);
	}

	vm_map_zap_dispose(&zap_old_list);
	vm_map_zap_dispose(&zap_new_list);

	if (new_mapping_established) {
		/*
		 * The caller had a reference on "caller_object" and we
		 * transferred that reference to the mapping.
		 * We also took an extra reference on "caller_object" to keep
		 * it alive while the map was unlocked.
		 */
		if (result == KERN_SUCCESS) {
			/*
			 * On success, the caller's reference on the object gets
			 * tranferred to the mapping.
			 * Release our extra reference.
			 */
			if (is_submap) {
				vm_map_deallocate((vm_map_t)caller_object);
			} else {
				vm_object_deallocate(caller_object);
			}
		} else {
			/*
			 * On error, the caller expects to still have a
			 * reference on the object it gave us.
			 * Let's use our extra reference for that.
			 */
		}
	}

	return result;

#undef  RETURN
}

#if __arm64__
extern const struct memory_object_pager_ops fourk_pager_ops;
kern_return_t
vm_map_enter_fourk(
	vm_map_t                map,
	vm_map_offset_t         *address,       /* IN/OUT */
	vm_map_size_t           size,
	vm_map_offset_t         mask,
	vm_map_kernel_flags_t   vmk_flags,
	vm_object_t             object,
	vm_object_offset_t      offset,
	boolean_t               needs_copy,
	vm_prot_t               cur_protection,
	vm_prot_t               max_protection,
	vm_inherit_t            inheritance)
{
	vm_map_entry_t          entry, new_entry;
	vm_map_offset_t         start, fourk_start;
	vm_map_offset_t         end, fourk_end;
	vm_map_size_t           fourk_size;
	kern_return_t           result = KERN_SUCCESS;
	boolean_t               map_locked = FALSE;
	boolean_t               pmap_empty = TRUE;
	boolean_t               new_mapping_established = FALSE;
	const bool              keep_map_locked = vmk_flags.vmkf_keep_map_locked;
	const bool              anywhere = !vmk_flags.vmf_fixed;
	const bool              purgable = vmk_flags.vmf_purgeable;
	const bool              overwrite = vmk_flags.vmf_overwrite;
	const bool              is_submap = vmk_flags.vmkf_submap;
	const bool              entry_for_jit = vmk_flags.vmkf_map_jit;
	const unsigned int      superpage_size = vmk_flags.vmf_superpage_size;
	vm_map_offset_t         effective_min_offset, effective_max_offset;
	kern_return_t           kr;
	boolean_t               clear_map_aligned = FALSE;
	memory_object_t         fourk_mem_obj;
	vm_object_t             fourk_object;
	vm_map_offset_t         fourk_pager_offset;
	int                     fourk_pager_index_start, fourk_pager_index_num;
	int                     cur_idx;
	boolean_t               fourk_copy;
	vm_object_t             copy_object;
	vm_object_offset_t      copy_offset;
	VM_MAP_ZAP_DECLARE(zap_list);

	if (VM_MAP_PAGE_MASK(map) < PAGE_MASK) {
		panic("%s:%d", __FUNCTION__, __LINE__);
	}
	fourk_mem_obj = MEMORY_OBJECT_NULL;
	fourk_object = VM_OBJECT_NULL;

	if (superpage_size) {
		return KERN_NOT_SUPPORTED;
	}

	if ((cur_protection & VM_PROT_WRITE) &&
	    (cur_protection & VM_PROT_EXECUTE) &&
#if XNU_TARGET_OS_OSX
	    map->pmap != kernel_pmap &&
	    (vm_map_cs_enforcement(map)
#if __arm64__
	    || !VM_MAP_IS_EXOTIC(map)
#endif /* __arm64__ */
	    ) &&
#endif /* XNU_TARGET_OS_OSX */
#if CODE_SIGNING_MONITOR
	    (csm_address_space_exempt(map->pmap) != KERN_SUCCESS) &&
#endif
	    !entry_for_jit) {
		DTRACE_VM3(cs_wx,
		    uint64_t, 0,
		    uint64_t, 0,
		    vm_prot_t, cur_protection);
		printf("CODE SIGNING: %d[%s] %s: curprot cannot be write+execute. "
		    "turning off execute\n",
		    proc_selfpid(),
		    (get_bsdtask_info(current_task())
		    ? proc_name_address(get_bsdtask_info(current_task()))
		    : "?"),
		    __FUNCTION__);
		cur_protection &= ~VM_PROT_EXECUTE;
	}

	/*
	 * If the task has requested executable lockdown,
	 * deny any new executable mapping.
	 */
	if (map->map_disallow_new_exec == TRUE) {
		if (cur_protection & VM_PROT_EXECUTE) {
			return KERN_PROTECTION_FAILURE;
		}
	}

	if (is_submap) {
		return KERN_NOT_SUPPORTED;
	}
	if (vmk_flags.vmkf_already) {
		return KERN_NOT_SUPPORTED;
	}
	if (purgable || entry_for_jit) {
		return KERN_NOT_SUPPORTED;
	}

	effective_min_offset = map->min_offset;

	if (vmk_flags.vmkf_beyond_max) {
		return KERN_NOT_SUPPORTED;
	} else {
		effective_max_offset = map->max_offset;
	}

	if (size == 0 ||
	    (offset & FOURK_PAGE_MASK) != 0) {
		*address = 0;
		return KERN_INVALID_ARGUMENT;
	}

#define RETURN(value)   { result = value; goto BailOut; }

	assert(VM_MAP_PAGE_ALIGNED(*address, FOURK_PAGE_MASK));
	assert(VM_MAP_PAGE_ALIGNED(size, FOURK_PAGE_MASK));

	if (!anywhere && overwrite) {
		return KERN_NOT_SUPPORTED;
	}

	fourk_start = *address;
	fourk_size = size;
	fourk_end = fourk_start + fourk_size;

	start = vm_map_trunc_page(*address, VM_MAP_PAGE_MASK(map));
	end = vm_map_round_page(fourk_end, VM_MAP_PAGE_MASK(map));
	size = end - start;

	if (anywhere) {
		return KERN_NOT_SUPPORTED;
	} else {
		/*
		 *	Verify that:
		 *		the address doesn't itself violate
		 *		the mask requirement.
		 */

		vm_map_lock(map);
		map_locked = TRUE;
		if ((start & mask) != 0) {
			RETURN(KERN_NO_SPACE);
		}

		/*
		 *	...	the address is within bounds
		 */

		end = start + size;

		if ((start < effective_min_offset) ||
		    (end > effective_max_offset) ||
		    (start >= end)) {
			RETURN(KERN_INVALID_ADDRESS);
		}

		/*
		 *	...	the starting address isn't allocated
		 */
		if (vm_map_lookup_entry(map, start, &entry)) {
			vm_object_t cur_object, shadow_object;

			/*
			 * We might already some 4K mappings
			 * in a 16K page here.
			 */

			if (entry->vme_end - entry->vme_start
			    != SIXTEENK_PAGE_SIZE) {
				RETURN(KERN_NO_SPACE);
			}
			if (entry->is_sub_map) {
				RETURN(KERN_NO_SPACE);
			}
			if (VME_OBJECT(entry) == VM_OBJECT_NULL) {
				RETURN(KERN_NO_SPACE);
			}

			/* go all the way down the shadow chain */
			cur_object = VME_OBJECT(entry);
			vm_object_lock(cur_object);
			while (cur_object->shadow != VM_OBJECT_NULL) {
				shadow_object = cur_object->shadow;
				vm_object_lock(shadow_object);
				vm_object_unlock(cur_object);
				cur_object = shadow_object;
				shadow_object = VM_OBJECT_NULL;
			}
			if (cur_object->internal ||
			    cur_object->pager == NULL) {
				vm_object_unlock(cur_object);
				RETURN(KERN_NO_SPACE);
			}
			if (cur_object->pager->mo_pager_ops
			    != &fourk_pager_ops) {
				vm_object_unlock(cur_object);
				RETURN(KERN_NO_SPACE);
			}
			fourk_object = cur_object;
			fourk_mem_obj = fourk_object->pager;

			/* keep the "4K" object alive */
			vm_object_reference_locked(fourk_object);
			memory_object_reference(fourk_mem_obj);
			vm_object_unlock(fourk_object);

			/* merge permissions */
			entry->protection |= cur_protection;
			entry->max_protection |= max_protection;

			if ((entry->protection & VM_PROT_WRITE) &&
			    (entry->protection & VM_PROT_ALLEXEC) &&
			    fourk_binary_compatibility_unsafe &&
			    fourk_binary_compatibility_allow_wx) {
				/* write+execute: need to be "jit" */
				entry->used_for_jit = TRUE;
			}
			goto map_in_fourk_pager;
		}

		/*
		 *	...	the next region doesn't overlap the
		 *		end point.
		 */

		if ((entry->vme_next != vm_map_to_entry(map)) &&
		    (entry->vme_next->vme_start < end)) {
			RETURN(KERN_NO_SPACE);
		}
	}

	/*
	 *	At this point,
	 *		"start" and "end" should define the endpoints of the
	 *			available new range, and
	 *		"entry" should refer to the region before the new
	 *			range, and
	 *
	 *		the map should be locked.
	 */

	/* create a new "4K" pager */
	fourk_mem_obj = fourk_pager_create();
	fourk_object = fourk_pager_to_vm_object(fourk_mem_obj);
	assert(fourk_object);

	/* keep the "4" object alive */
	vm_object_reference(fourk_object);

	/* create a "copy" object, to map the "4K" object copy-on-write */
	fourk_copy = TRUE;
	result = vm_object_copy_strategically(fourk_object,
	    0,
	    end - start,
	    false,                                   /* forking */
	    &copy_object,
	    &copy_offset,
	    &fourk_copy);
	assert(result == KERN_SUCCESS);
	assert(copy_object != VM_OBJECT_NULL);
	assert(copy_offset == 0);

	/* map the "4K" pager's copy object */
	new_entry = vm_map_entry_insert(map,
	    entry,
	    vm_map_trunc_page(start, VM_MAP_PAGE_MASK(map)),
	    vm_map_round_page(end, VM_MAP_PAGE_MASK(map)),
	    copy_object,
	    0,                      /* offset */
	    vmk_flags,
	    FALSE,                  /* needs_copy */
	    cur_protection, max_protection,
	    (entry_for_jit && !VM_MAP_POLICY_ALLOW_JIT_INHERIT(map) ?
	    VM_INHERIT_NONE : inheritance),
	    clear_map_aligned);
	entry = new_entry;

#if VM_MAP_DEBUG_FOURK
	if (vm_map_debug_fourk) {
		printf("FOURK_PAGER: map %p [0x%llx:0x%llx] new pager %p\n",
		    map,
		    (uint64_t) entry->vme_start,
		    (uint64_t) entry->vme_end,
		    fourk_mem_obj);
	}
#endif /* VM_MAP_DEBUG_FOURK */

	new_mapping_established = TRUE;

map_in_fourk_pager:
	/* "map" the original "object" where it belongs in the "4K" pager */
	fourk_pager_offset = (fourk_start & SIXTEENK_PAGE_MASK);
	fourk_pager_index_start = (int) (fourk_pager_offset / FOURK_PAGE_SIZE);
	if (fourk_size > SIXTEENK_PAGE_SIZE) {
		fourk_pager_index_num = 4;
	} else {
		fourk_pager_index_num = (int) (fourk_size / FOURK_PAGE_SIZE);
	}
	if (fourk_pager_index_start + fourk_pager_index_num > 4) {
		fourk_pager_index_num = 4 - fourk_pager_index_start;
	}
	for (cur_idx = 0;
	    cur_idx < fourk_pager_index_num;
	    cur_idx++) {
		vm_object_t             old_object;
		vm_object_offset_t      old_offset;

		kr = fourk_pager_populate(fourk_mem_obj,
		    TRUE,                       /* overwrite */
		    fourk_pager_index_start + cur_idx,
		    object,
		    (object
		    ? (offset +
		    (cur_idx * FOURK_PAGE_SIZE))
		    : 0),
		    &old_object,
		    &old_offset);
#if VM_MAP_DEBUG_FOURK
		if (vm_map_debug_fourk) {
			if (old_object == (vm_object_t) -1 &&
			    old_offset == (vm_object_offset_t) -1) {
				printf("FOURK_PAGER: map %p [0x%llx:0x%llx] "
				    "pager [%p:0x%llx] "
				    "populate[%d] "
				    "[object:%p,offset:0x%llx]\n",
				    map,
				    (uint64_t) entry->vme_start,
				    (uint64_t) entry->vme_end,
				    fourk_mem_obj,
				    VME_OFFSET(entry),
				    fourk_pager_index_start + cur_idx,
				    object,
				    (object
				    ? (offset + (cur_idx * FOURK_PAGE_SIZE))
				    : 0));
			} else {
				printf("FOURK_PAGER: map %p [0x%llx:0x%llx] "
				    "pager [%p:0x%llx] "
				    "populate[%d] [object:%p,offset:0x%llx] "
				    "old [%p:0x%llx]\n",
				    map,
				    (uint64_t) entry->vme_start,
				    (uint64_t) entry->vme_end,
				    fourk_mem_obj,
				    VME_OFFSET(entry),
				    fourk_pager_index_start + cur_idx,
				    object,
				    (object
				    ? (offset + (cur_idx * FOURK_PAGE_SIZE))
				    : 0),
				    old_object,
				    old_offset);
			}
		}
#endif /* VM_MAP_DEBUG_FOURK */

		assert(kr == KERN_SUCCESS);
		if (object != old_object &&
		    object != VM_OBJECT_NULL &&
		    object != (vm_object_t) -1) {
			vm_object_reference(object);
		}
		if (object != old_object &&
		    old_object != VM_OBJECT_NULL &&
		    old_object != (vm_object_t) -1) {
			vm_object_deallocate(old_object);
		}
	}

BailOut:
	assert(map_locked == TRUE);

	if (result == KERN_SUCCESS) {
		vm_prot_t pager_prot;
		memory_object_t pager;

#if DEBUG
		if (pmap_empty &&
		    !(vmk_flags.vmkf_no_pmap_check)) {
			assert(pmap_is_empty(map->pmap,
			    *address,
			    *address + size));
		}
#endif /* DEBUG */

		/*
		 * For "named" VM objects, let the pager know that the
		 * memory object is being mapped.  Some pagers need to keep
		 * track of this, to know when they can reclaim the memory
		 * object, for example.
		 * VM calls memory_object_map() for each mapping (specifying
		 * the protection of each mapping) and calls
		 * memory_object_last_unmap() when all the mappings are gone.
		 */
		pager_prot = max_protection;
		if (needs_copy) {
			/*
			 * Copy-On-Write mapping: won't modify
			 * the memory object.
			 */
			pager_prot &= ~VM_PROT_WRITE;
		}
		if (!is_submap &&
		    object != VM_OBJECT_NULL &&
		    object->named &&
		    object->pager != MEMORY_OBJECT_NULL) {
			vm_object_lock(object);
			pager = object->pager;
			if (object->named &&
			    pager != MEMORY_OBJECT_NULL) {
				assert(object->pager_ready);
				vm_object_mapping_wait(object, THREAD_UNINT);
				vm_object_mapping_begin(object);
				vm_object_unlock(object);

				kr = memory_object_map(pager, pager_prot);
				assert(kr == KERN_SUCCESS);

				vm_object_lock(object);
				vm_object_mapping_end(object);
			}
			vm_object_unlock(object);
		}
		if (!is_submap &&
		    fourk_object != VM_OBJECT_NULL &&
		    fourk_object->named &&
		    fourk_object->pager != MEMORY_OBJECT_NULL) {
			vm_object_lock(fourk_object);
			pager = fourk_object->pager;
			if (fourk_object->named &&
			    pager != MEMORY_OBJECT_NULL) {
				assert(fourk_object->pager_ready);
				vm_object_mapping_wait(fourk_object,
				    THREAD_UNINT);
				vm_object_mapping_begin(fourk_object);
				vm_object_unlock(fourk_object);

				kr = memory_object_map(pager, VM_PROT_READ);
				assert(kr == KERN_SUCCESS);

				vm_object_lock(fourk_object);
				vm_object_mapping_end(fourk_object);
			}
			vm_object_unlock(fourk_object);
		}
	}

	if (fourk_object != VM_OBJECT_NULL) {
		vm_object_deallocate(fourk_object);
		fourk_object = VM_OBJECT_NULL;
		memory_object_deallocate(fourk_mem_obj);
		fourk_mem_obj = MEMORY_OBJECT_NULL;
	}

	assert(map_locked == TRUE);

	if (!keep_map_locked) {
		vm_map_unlock(map);
		map_locked = FALSE;
	}

	/*
	 * We can't hold the map lock if we enter this block.
	 */

	if (result == KERN_SUCCESS) {
		/*	Wire down the new entry if the user
		 *	requested all new map entries be wired.
		 */
		if ((map->wiring_required) || (superpage_size)) {
			assert(!keep_map_locked);
			pmap_empty = FALSE; /* pmap won't be empty */
			kr = vm_map_wire_kernel(map, start, end,
			    new_entry->protection, VM_KERN_MEMORY_MLOCK,
			    TRUE);
			result = kr;
		}

	}

	if (result != KERN_SUCCESS) {
		if (new_mapping_established) {
			/*
			 * We have to get rid of the new mappings since we
			 * won't make them available to the user.
			 * Try and do that atomically, to minimize the risk
			 * that someone else create new mappings that range.
			 */

			if (!map_locked) {
				vm_map_lock(map);
				map_locked = TRUE;
			}
			(void)vm_map_delete(map, *address, *address + size,
			    VM_MAP_REMOVE_NO_MAP_ALIGN | VM_MAP_REMOVE_NO_YIELD,
			    KMEM_GUARD_NONE, &zap_list);
		}
	}

	/*
	 * The caller is responsible for releasing the lock if it requested to
	 * keep the map locked.
	 */
	if (map_locked && !keep_map_locked) {
		vm_map_unlock(map);
	}

	vm_map_zap_dispose(&zap_list);

	return result;

#undef  RETURN
}
#endif /* __arm64__ */

/*
 * Counters for the prefault optimization.
 */
int64_t vm_prefault_nb_pages = 0;
int64_t vm_prefault_nb_bailout = 0;

static kern_return_t
vm_map_enter_mem_object_helper(
	vm_map_t                target_map,
	vm_map_offset_t         *address,
	vm_map_size_t           initial_size,
	vm_map_offset_t         mask,
	vm_map_kernel_flags_t   vmk_flags,
	ipc_port_t              port,
	vm_object_offset_t      offset,
	boolean_t               copy,
	vm_prot_t               cur_protection,
	vm_prot_t               max_protection,
	vm_inherit_t            inheritance,
	upl_page_list_ptr_t     page_list,
	unsigned int            page_list_count)
{
	vm_map_address_t        map_addr;
	vm_map_size_t           map_size;
	vm_object_t             object;
	vm_object_size_t        size;
	kern_return_t           result;
	boolean_t               mask_cur_protection, mask_max_protection;
	boolean_t               kernel_prefault, try_prefault = (page_list_count != 0);
	vm_map_offset_t         offset_in_mapping = 0;
#if __arm64__
	boolean_t               fourk = vmk_flags.vmkf_fourk;
#endif /* __arm64__ */

	if (VM_MAP_PAGE_SHIFT(target_map) < PAGE_SHIFT) {
		/* XXX TODO4K prefaulting depends on page size... */
		try_prefault = FALSE;
	}

	assertf(vmk_flags.__vmkf_unused == 0, "vmk_flags unused=0x%x\n", vmk_flags.__vmkf_unused);
	vm_map_kernel_flags_update_range_id(&vmk_flags, target_map);

	mask_cur_protection = cur_protection & VM_PROT_IS_MASK;
	mask_max_protection = max_protection & VM_PROT_IS_MASK;
	cur_protection &= ~VM_PROT_IS_MASK;
	max_protection &= ~VM_PROT_IS_MASK;

	/*
	 * Check arguments for validity
	 */
	if ((target_map == VM_MAP_NULL) ||
	    (cur_protection & ~(VM_PROT_ALL | VM_PROT_ALLEXEC)) ||
	    (max_protection & ~(VM_PROT_ALL | VM_PROT_ALLEXEC)) ||
	    (inheritance > VM_INHERIT_LAST_VALID) ||
	    (try_prefault && (copy || !page_list)) ||
	    initial_size == 0) {
		return KERN_INVALID_ARGUMENT;
	}

	if (__improbable((cur_protection & max_protection) != cur_protection)) {
		/* cur is more permissive than max */
		cur_protection &= max_protection;
	}

#if __arm64__
	if (cur_protection & VM_PROT_EXECUTE) {
		cur_protection |= VM_PROT_READ;
	}

	if (fourk && VM_MAP_PAGE_SHIFT(target_map) < PAGE_SHIFT) {
		/* no "fourk" if map is using a sub-page page size */
		fourk = FALSE;
	}
	if (fourk) {
		map_addr = vm_map_trunc_page(*address, FOURK_PAGE_MASK);
		map_size = vm_map_round_page(initial_size, FOURK_PAGE_MASK);
	} else
#endif /* __arm64__ */
	{
		map_addr = vm_map_trunc_page(*address,
		    VM_MAP_PAGE_MASK(target_map));
		map_size = vm_map_round_page(initial_size,
		    VM_MAP_PAGE_MASK(target_map));
	}
	if (map_size == 0) {
		return KERN_INVALID_ARGUMENT;
	}
	size = vm_object_round_page(initial_size);

	/*
	 * Find the vm object (if any) corresponding to this port.
	 */
	if (!IP_VALID(port)) {
		object = VM_OBJECT_NULL;
		offset = 0;
		copy = FALSE;
	} else if (ip_kotype(port) == IKOT_NAMED_ENTRY) {
		vm_named_entry_t        named_entry;
		vm_object_offset_t      data_offset;

		named_entry = mach_memory_entry_from_port(port);

		if (vmk_flags.vmf_return_data_addr ||
		    vmk_flags.vmf_return_4k_data_addr) {
			data_offset = named_entry->data_offset;
			offset += named_entry->data_offset;
		} else {
			data_offset = 0;
		}

		/* a few checks to make sure user is obeying rules */
		if (mask_max_protection) {
			max_protection &= named_entry->protection;
		}
		if (mask_cur_protection) {
			cur_protection &= named_entry->protection;
		}
		if ((named_entry->protection & max_protection) !=
		    max_protection) {
			return KERN_INVALID_RIGHT;
		}
		if ((named_entry->protection & cur_protection) !=
		    cur_protection) {
			return KERN_INVALID_RIGHT;
		}
		if (offset + size <= offset) {
			/* overflow */
			return KERN_INVALID_ARGUMENT;
		}
		if (named_entry->size < (offset + initial_size)) {
			return KERN_INVALID_ARGUMENT;
		}

		if (named_entry->is_copy) {
			/* for a vm_map_copy, we can only map it whole */
			if ((size != named_entry->size) &&
			    (vm_map_round_page(size,
			    VM_MAP_PAGE_MASK(target_map)) ==
			    named_entry->size)) {
				/* XXX FBDP use the rounded size... */
				size = vm_map_round_page(
					size,
					VM_MAP_PAGE_MASK(target_map));
			}
		}

		/* the callers parameter offset is defined to be the */
		/* offset from beginning of named entry offset in object */
		offset = offset + named_entry->offset;

		if (!VM_MAP_PAGE_ALIGNED(size,
		    VM_MAP_PAGE_MASK(target_map))) {
			/*
			 * Let's not map more than requested;
			 * vm_map_enter() will handle this "not map-aligned"
			 * case.
			 */
			map_size = size;
		}

		named_entry_lock(named_entry);
		if (named_entry->is_sub_map) {
			vm_map_t                submap;

			if (vmk_flags.vmf_return_data_addr ||
			    vmk_flags.vmf_return_4k_data_addr) {
				panic("VM_FLAGS_RETURN_DATA_ADDR not expected for submap.");
			}

			submap = named_entry->backing.map;
			vm_map_reference(submap);
			named_entry_unlock(named_entry);

			vmk_flags.vmkf_submap = TRUE;

			result = vm_map_enter(target_map,
			    &map_addr,
			    map_size,
			    mask,
			    vmk_flags,
			    (vm_object_t)(uintptr_t) submap,
			    offset,
			    copy,
			    cur_protection,
			    max_protection,
			    inheritance);
			if (result != KERN_SUCCESS) {
				vm_map_deallocate(submap);
			} else {
				/*
				 * No need to lock "submap" just to check its
				 * "mapped" flag: that flag is never reset
				 * once it's been set and if we race, we'll
				 * just end up setting it twice, which is OK.
				 */
				if (submap->mapped_in_other_pmaps == FALSE &&
				    vm_map_pmap(submap) != PMAP_NULL &&
				    vm_map_pmap(submap) !=
				    vm_map_pmap(target_map)) {
					/*
					 * This submap is being mapped in a map
					 * that uses a different pmap.
					 * Set its "mapped_in_other_pmaps" flag
					 * to indicate that we now need to
					 * remove mappings from all pmaps rather
					 * than just the submap's pmap.
					 */
					vm_map_lock(submap);
					submap->mapped_in_other_pmaps = TRUE;
					vm_map_unlock(submap);
				}
				*address = map_addr;
			}
			return result;
		} else if (named_entry->is_copy) {
			kern_return_t   kr;
			vm_map_copy_t   copy_map;
			vm_map_entry_t  copy_entry;
			vm_map_offset_t copy_addr;
			vm_map_copy_t   target_copy_map;
			vm_map_offset_t overmap_start, overmap_end;
			vm_map_offset_t trimmed_start;
			vm_map_size_t   target_size;

			if (!vm_map_kernel_flags_check_vmflags(vmk_flags,
			    (VM_FLAGS_FIXED |
			    VM_FLAGS_ANYWHERE |
			    VM_FLAGS_OVERWRITE |
			    VM_FLAGS_RETURN_4K_DATA_ADDR |
			    VM_FLAGS_RETURN_DATA_ADDR))) {
				named_entry_unlock(named_entry);
				return KERN_INVALID_ARGUMENT;
			}

			copy_map = named_entry->backing.copy;
			assert(copy_map->type == VM_MAP_COPY_ENTRY_LIST);
			if (copy_map->type != VM_MAP_COPY_ENTRY_LIST) {
				/* unsupported type; should not happen */
				printf("vm_map_enter_mem_object: "
				    "memory_entry->backing.copy "
				    "unsupported type 0x%x\n",
				    copy_map->type);
				named_entry_unlock(named_entry);
				return KERN_INVALID_ARGUMENT;
			}

			if (VM_MAP_PAGE_SHIFT(target_map) != copy_map->cpy_hdr.page_shift) {
				DEBUG4K_SHARE("copy_map %p offset %llx size 0x%llx pgshift %d -> target_map %p pgshift %d\n", copy_map, offset, (uint64_t)map_size, copy_map->cpy_hdr.page_shift, target_map, VM_MAP_PAGE_SHIFT(target_map));
			}

			if (vmk_flags.vmf_return_data_addr ||
			    vmk_flags.vmf_return_4k_data_addr) {
				offset_in_mapping = offset & VM_MAP_PAGE_MASK(target_map);
				if (vmk_flags.vmf_return_4k_data_addr) {
					offset_in_mapping &= ~((signed)(0xFFF));
				}
			}

			target_copy_map = VM_MAP_COPY_NULL;
			target_size = copy_map->size;
			overmap_start = 0;
			overmap_end = 0;
			trimmed_start = 0;
			if (copy_map->cpy_hdr.page_shift != VM_MAP_PAGE_SHIFT(target_map)) {
				DEBUG4K_ADJUST("adjusting...\n");
				kr = vm_map_copy_adjust_to_target(
					copy_map,
					offset /* includes data_offset */,
					initial_size,
					target_map,
					copy,
					&target_copy_map,
					&overmap_start,
					&overmap_end,
					&trimmed_start);
				if (kr != KERN_SUCCESS) {
					named_entry_unlock(named_entry);
					return kr;
				}
				target_size = target_copy_map->size;
				if (trimmed_start >= data_offset) {
					data_offset = offset & VM_MAP_PAGE_MASK(target_map);
				} else {
					data_offset -= trimmed_start;
				}
			} else {
				/*
				 * Assert that the vm_map_copy is coming from the right
				 * zone and hasn't been forged
				 */
				vm_map_copy_require(copy_map);
				target_copy_map = copy_map;
			}

			vm_map_kernel_flags_t rsv_flags = vmk_flags;

			vm_map_kernel_flags_and_vmflags(&rsv_flags,
			    (VM_FLAGS_FIXED |
			    VM_FLAGS_ANYWHERE |
			    VM_FLAGS_OVERWRITE |
			    VM_FLAGS_RETURN_4K_DATA_ADDR |
			    VM_FLAGS_RETURN_DATA_ADDR));

			/* reserve a contiguous range */
			kr = vm_map_enter(target_map,
			    &map_addr,
			    vm_map_round_page(target_size, VM_MAP_PAGE_MASK(target_map)),
			    mask,
			    rsv_flags,
			    VM_OBJECT_NULL,
			    0,
			    FALSE,               /* copy */
			    cur_protection,
			    max_protection,
			    inheritance);
			if (kr != KERN_SUCCESS) {
				DEBUG4K_ERROR("kr 0x%x\n", kr);
				if (target_copy_map != copy_map) {
					vm_map_copy_discard(target_copy_map);
					target_copy_map = VM_MAP_COPY_NULL;
				}
				named_entry_unlock(named_entry);
				return kr;
			}

			copy_addr = map_addr;

			for (copy_entry = vm_map_copy_first_entry(target_copy_map);
			    copy_entry != vm_map_copy_to_entry(target_copy_map);
			    copy_entry = copy_entry->vme_next) {
				vm_map_t                copy_submap = VM_MAP_NULL;
				vm_object_t             copy_object = VM_OBJECT_NULL;
				vm_map_size_t           copy_size;
				vm_object_offset_t      copy_offset;
				boolean_t               do_copy = false;

				if (copy_entry->is_sub_map) {
					copy_submap = VME_SUBMAP(copy_entry);
					copy_object = (vm_object_t)copy_submap;
				} else {
					copy_object = VME_OBJECT(copy_entry);
				}
				copy_offset = VME_OFFSET(copy_entry);
				copy_size = (copy_entry->vme_end -
				    copy_entry->vme_start);

				/* sanity check */
				if ((copy_addr + copy_size) >
				    (map_addr +
				    overmap_start + overmap_end +
				    named_entry->size /* XXX full size */)) {
					/* over-mapping too much !? */
					kr = KERN_INVALID_ARGUMENT;
					DEBUG4K_ERROR("kr 0x%x\n", kr);
					/* abort */
					break;
				}

				/* take a reference on the object */
				if (copy_entry->is_sub_map) {
					vm_map_reference(copy_submap);
				} else {
					if (!copy &&
					    copy_object != VM_OBJECT_NULL &&
					    copy_object->copy_strategy == MEMORY_OBJECT_COPY_SYMMETRIC) {
						bool is_writable;

						/*
						 * We need to resolve our side of this
						 * "symmetric" copy-on-write now; we
						 * need a new object to map and share,
						 * instead of the current one which
						 * might still be shared with the
						 * original mapping.
						 *
						 * Note: A "vm_map_copy_t" does not
						 * have a lock but we're protected by
						 * the named entry's lock here.
						 */
						// assert(copy_object->copy_strategy == MEMORY_OBJECT_COPY_SYMMETRIC);
						VME_OBJECT_SHADOW(copy_entry, copy_size, TRUE);
						assert(copy_object != VME_OBJECT(copy_entry));
						is_writable = false;
						if (copy_entry->protection & VM_PROT_WRITE) {
							is_writable = true;
#if __arm64e__
						} else if (copy_entry->used_for_tpro) {
							is_writable = true;
#endif /* __arm64e__ */
						}
						if (!copy_entry->needs_copy && is_writable) {
							vm_prot_t prot;

							prot = copy_entry->protection & ~VM_PROT_WRITE;
							vm_object_pmap_protect(copy_object,
							    copy_offset,
							    copy_size,
							    PMAP_NULL,
							    PAGE_SIZE,
							    0,
							    prot);
						}
						copy_entry->needs_copy = FALSE;
						copy_entry->is_shared = TRUE;
						copy_object = VME_OBJECT(copy_entry);
						copy_offset = VME_OFFSET(copy_entry);
						vm_object_lock(copy_object);
						/* we're about to make a shared mapping of this object */
						copy_object->copy_strategy = MEMORY_OBJECT_COPY_DELAY;
						VM_OBJECT_SET_TRUE_SHARE(copy_object, TRUE);
						vm_object_unlock(copy_object);
					}

					if (copy_object != VM_OBJECT_NULL &&
					    copy_object->named &&
					    copy_object->pager != MEMORY_OBJECT_NULL &&
					    copy_object->copy_strategy != MEMORY_OBJECT_COPY_NONE) {
						memory_object_t pager;
						vm_prot_t       pager_prot;

						/*
						 * For "named" VM objects, let the pager know that the
						 * memory object is being mapped.  Some pagers need to keep
						 * track of this, to know when they can reclaim the memory
						 * object, for example.
						 * VM calls memory_object_map() for each mapping (specifying
						 * the protection of each mapping) and calls
						 * memory_object_last_unmap() when all the mappings are gone.
						 */
						pager_prot = max_protection;
						if (copy) {
							/*
							 * Copy-On-Write mapping: won't modify the
							 * memory object.
							 */
							pager_prot &= ~VM_PROT_WRITE;
						}
						vm_object_lock(copy_object);
						pager = copy_object->pager;
						if (copy_object->named &&
						    pager != MEMORY_OBJECT_NULL &&
						    copy_object->copy_strategy != MEMORY_OBJECT_COPY_NONE) {
							assert(copy_object->pager_ready);
							vm_object_mapping_wait(copy_object, THREAD_UNINT);
							vm_object_mapping_begin(copy_object);
							vm_object_unlock(copy_object);

							kr = memory_object_map(pager, pager_prot);
							assert(kr == KERN_SUCCESS);

							vm_object_lock(copy_object);
							vm_object_mapping_end(copy_object);
						}
						vm_object_unlock(copy_object);
					}

					/*
					 *	Perform the copy if requested
					 */

					if (copy && copy_object != VM_OBJECT_NULL) {
						vm_object_t             new_object;
						vm_object_offset_t      new_offset;

						result = vm_object_copy_strategically(copy_object, copy_offset,
						    copy_size,
						    false,                                   /* forking */
						    &new_object, &new_offset,
						    &do_copy);


						if (result == KERN_MEMORY_RESTART_COPY) {
							boolean_t success;
							boolean_t src_needs_copy;

							/*
							 * XXX
							 * We currently ignore src_needs_copy.
							 * This really is the issue of how to make
							 * MEMORY_OBJECT_COPY_SYMMETRIC safe for
							 * non-kernel users to use. Solution forthcoming.
							 * In the meantime, since we don't allow non-kernel
							 * memory managers to specify symmetric copy,
							 * we won't run into problems here.
							 */
							new_object = copy_object;
							new_offset = copy_offset;
							success = vm_object_copy_quickly(new_object,
							    new_offset,
							    copy_size,
							    &src_needs_copy,
							    &do_copy);
							assert(success);
							result = KERN_SUCCESS;
						}
						if (result != KERN_SUCCESS) {
							kr = result;
							break;
						}

						copy_object = new_object;
						copy_offset = new_offset;
						/*
						 * No extra object reference for the mapping:
						 * the mapping should be the only thing keeping
						 * this new object alive.
						 */
					} else {
						/*
						 * We already have the right object
						 * to map.
						 */
						copy_object = VME_OBJECT(copy_entry);
						/* take an extra ref for the mapping below */
						vm_object_reference(copy_object);
					}
				}

				/*
				 * If the caller does not want a specific
				 * tag for this new mapping:  use
				 * the tag of the original mapping.
				 */
				vm_map_kernel_flags_t vmk_remap_flags = {
					.vmkf_submap = copy_entry->is_sub_map,
				};

				vm_map_kernel_flags_set_vmflags(&vmk_remap_flags,
				    vm_map_kernel_flags_vmflags(vmk_flags),
				    vmk_flags.vm_tag ?: VME_ALIAS(copy_entry));

				/* over-map the object into destination */
				vmk_remap_flags.vmf_fixed = true;
				vmk_remap_flags.vmf_overwrite = true;

				if (!copy && !copy_entry->is_sub_map) {
					/*
					 * copy-on-write should have been
					 * resolved at this point, or we would
					 * end up sharing instead of copying.
					 */
					assert(!copy_entry->needs_copy);
				}
#if XNU_TARGET_OS_OSX
				if (copy_entry->used_for_jit) {
					vmk_remap_flags.vmkf_map_jit = TRUE;
				}
#endif /* XNU_TARGET_OS_OSX */

				kr = vm_map_enter(target_map,
				    &copy_addr,
				    copy_size,
				    (vm_map_offset_t) 0,
				    vmk_remap_flags,
				    copy_object,
				    copy_offset,
				    ((copy_object == NULL)
				    ? FALSE
				    : (copy || copy_entry->needs_copy)),
				    cur_protection,
				    max_protection,
				    inheritance);
				if (kr != KERN_SUCCESS) {
					DEBUG4K_SHARE("failed kr 0x%x\n", kr);
					if (copy_entry->is_sub_map) {
						vm_map_deallocate(copy_submap);
					} else {
						vm_object_deallocate(copy_object);
					}
					/* abort */
					break;
				}

				/* next mapping */
				copy_addr += copy_size;
			}

			if (kr == KERN_SUCCESS) {
				if (vmk_flags.vmf_return_data_addr ||
				    vmk_flags.vmf_return_4k_data_addr) {
					*address = map_addr + offset_in_mapping;
				} else {
					*address = map_addr;
				}
				if (overmap_start) {
					*address += overmap_start;
					DEBUG4K_SHARE("map %p map_addr 0x%llx offset_in_mapping 0x%llx overmap_start 0x%llx -> *address 0x%llx\n", target_map, (uint64_t)map_addr, (uint64_t) offset_in_mapping, (uint64_t)overmap_start, (uint64_t)*address);
				}
			}
			named_entry_unlock(named_entry);
			if (target_copy_map != copy_map) {
				vm_map_copy_discard(target_copy_map);
				target_copy_map = VM_MAP_COPY_NULL;
			}

			if (kr != KERN_SUCCESS && !vmk_flags.vmf_overwrite) {
				/* deallocate the contiguous range */
				(void) vm_deallocate(target_map,
				    map_addr,
				    map_size);
			}

			return kr;
		}

		if (named_entry->is_object) {
			unsigned int    access;
			unsigned int    wimg_mode;

			/* we are mapping a VM object */

			access = named_entry->access;

			if (vmk_flags.vmf_return_data_addr ||
			    vmk_flags.vmf_return_4k_data_addr) {
				offset_in_mapping = offset - VM_MAP_TRUNC_PAGE(offset, VM_MAP_PAGE_MASK(target_map));
				if (vmk_flags.vmf_return_4k_data_addr) {
					offset_in_mapping &= ~((signed)(0xFFF));
				}
				offset = VM_MAP_TRUNC_PAGE(offset, VM_MAP_PAGE_MASK(target_map));
				map_size = VM_MAP_ROUND_PAGE((offset + offset_in_mapping + initial_size) - offset, VM_MAP_PAGE_MASK(target_map));
			}

			object = vm_named_entry_to_vm_object(named_entry);
			assert(object != VM_OBJECT_NULL);
			vm_object_lock(object);
			named_entry_unlock(named_entry);

			vm_object_reference_locked(object);

			wimg_mode = object->wimg_bits;
			vm_prot_to_wimg(access, &wimg_mode);
			if (object->wimg_bits != wimg_mode) {
				vm_object_change_wimg_mode(object, wimg_mode);
			}

			vm_object_unlock(object);
		} else {
			panic("invalid VM named entry %p", named_entry);
		}
	} else if (ip_kotype(port) == IKOT_MEMORY_OBJECT) {
		/*
		 * JMM - This is temporary until we unify named entries
		 * and raw memory objects.
		 *
		 * Detected fake ip_kotype for a memory object.  In
		 * this case, the port isn't really a port at all, but
		 * instead is just a raw memory object.
		 */
		if (vmk_flags.vmf_return_data_addr ||
		    vmk_flags.vmf_return_4k_data_addr) {
			panic("VM_FLAGS_RETURN_DATA_ADDR not expected for raw memory object.");
		}

		object = memory_object_to_vm_object((memory_object_t)port);
		if (object == VM_OBJECT_NULL) {
			return KERN_INVALID_OBJECT;
		}
		vm_object_reference(object);

		/* wait for object (if any) to be ready */
		if (object != VM_OBJECT_NULL) {
			if (is_kernel_object(object)) {
				printf("Warning: Attempt to map kernel object"
				    " by a non-private kernel entity\n");
				return KERN_INVALID_OBJECT;
			}
			if (!object->pager_ready) {
				vm_object_lock(object);

				while (!object->pager_ready) {
					vm_object_wait(object,
					    VM_OBJECT_EVENT_PAGER_READY,
					    THREAD_UNINT);
					vm_object_lock(object);
				}
				vm_object_unlock(object);
			}
		}
	} else {
		return KERN_INVALID_OBJECT;
	}

	if (object != VM_OBJECT_NULL &&
	    object->named &&
	    object->pager != MEMORY_OBJECT_NULL &&
	    object->copy_strategy != MEMORY_OBJECT_COPY_NONE) {
		memory_object_t pager;
		vm_prot_t       pager_prot;
		kern_return_t   kr;

		/*
		 * For "named" VM objects, let the pager know that the
		 * memory object is being mapped.  Some pagers need to keep
		 * track of this, to know when they can reclaim the memory
		 * object, for example.
		 * VM calls memory_object_map() for each mapping (specifying
		 * the protection of each mapping) and calls
		 * memory_object_last_unmap() when all the mappings are gone.
		 */
		pager_prot = max_protection;
		if (copy) {
			/*
			 * Copy-On-Write mapping: won't modify the
			 * memory object.
			 */
			pager_prot &= ~VM_PROT_WRITE;
		}
		vm_object_lock(object);
		pager = object->pager;
		if (object->named &&
		    pager != MEMORY_OBJECT_NULL &&
		    object->copy_strategy != MEMORY_OBJECT_COPY_NONE) {
			assert(object->pager_ready);
			vm_object_mapping_wait(object, THREAD_UNINT);
			vm_object_mapping_begin(object);
			vm_object_unlock(object);

			kr = memory_object_map(pager, pager_prot);
			assert(kr == KERN_SUCCESS);

			vm_object_lock(object);
			vm_object_mapping_end(object);
		}
		vm_object_unlock(object);
	}

	/*
	 *	Perform the copy if requested
	 */

	if (copy) {
		vm_object_t             new_object;
		vm_object_offset_t      new_offset;

		result = vm_object_copy_strategically(object, offset,
		    map_size,
		    false,                                   /* forking */
		    &new_object, &new_offset,
		    &copy);


		if (result == KERN_MEMORY_RESTART_COPY) {
			boolean_t success;
			boolean_t src_needs_copy;

			/*
			 * XXX
			 * We currently ignore src_needs_copy.
			 * This really is the issue of how to make
			 * MEMORY_OBJECT_COPY_SYMMETRIC safe for
			 * non-kernel users to use. Solution forthcoming.
			 * In the meantime, since we don't allow non-kernel
			 * memory managers to specify symmetric copy,
			 * we won't run into problems here.
			 */
			new_object = object;
			new_offset = offset;
			success = vm_object_copy_quickly(new_object,
			    new_offset,
			    map_size,
			    &src_needs_copy,
			    &copy);
			assert(success);
			result = KERN_SUCCESS;
		}
		/*
		 *	Throw away the reference to the
		 *	original object, as it won't be mapped.
		 */

		vm_object_deallocate(object);

		if (result != KERN_SUCCESS) {
			return result;
		}

		object = new_object;
		offset = new_offset;
	}

	/*
	 * If non-kernel users want to try to prefault pages, the mapping and prefault
	 * needs to be atomic.
	 */
	kernel_prefault = (try_prefault && vm_kernel_map_is_kernel(target_map));
	vmk_flags.vmkf_keep_map_locked = (try_prefault && !kernel_prefault);

#if __arm64__
	if (fourk) {
		/* map this object in a "4K" pager */
		result = vm_map_enter_fourk(target_map,
		    &map_addr,
		    map_size,
		    (vm_map_offset_t) mask,
		    vmk_flags,
		    object,
		    offset,
		    copy,
		    cur_protection,
		    max_protection,
		    inheritance);
	} else
#endif /* __arm64__ */
	{
		result = vm_map_enter(target_map,
		    &map_addr, map_size,
		    (vm_map_offset_t)mask,
		    vmk_flags,
		    object, offset,
		    copy,
		    cur_protection, max_protection,
		    inheritance);
	}
	if (result != KERN_SUCCESS) {
		vm_object_deallocate(object);
	}

	/*
	 * Try to prefault, and do not forget to release the vm map lock.
	 */
	if (result == KERN_SUCCESS && try_prefault) {
		mach_vm_address_t va = map_addr;
		kern_return_t kr = KERN_SUCCESS;
		unsigned int i = 0;
		int pmap_options;

		pmap_options = kernel_prefault ? 0 : PMAP_OPTIONS_NOWAIT;
		if (object->internal) {
			pmap_options |= PMAP_OPTIONS_INTERNAL;
		}

		for (i = 0; i < page_list_count; ++i) {
			if (!UPL_VALID_PAGE(page_list, i)) {
				if (kernel_prefault) {
					assertf(FALSE, "kernel_prefault && !UPL_VALID_PAGE");
					result = KERN_MEMORY_ERROR;
					break;
				}
			} else {
				/*
				 * If this function call failed, we should stop
				 * trying to optimize, other calls are likely
				 * going to fail too.
				 *
				 * We are not gonna report an error for such
				 * failure though. That's an optimization, not
				 * something critical.
				 */
				kr = pmap_enter_options(target_map->pmap,
				    va, UPL_PHYS_PAGE(page_list, i),
				    cur_protection, VM_PROT_NONE,
				    0, TRUE, pmap_options, NULL, PMAP_MAPPING_TYPE_INFER);
				if (kr != KERN_SUCCESS) {
					OSIncrementAtomic64(&vm_prefault_nb_bailout);
					if (kernel_prefault) {
						result = kr;
					}
					break;
				}
				OSIncrementAtomic64(&vm_prefault_nb_pages);
			}

			/* Next virtual address */
			va += PAGE_SIZE;
		}
		if (vmk_flags.vmkf_keep_map_locked) {
			vm_map_unlock(target_map);
		}
	}

	if (vmk_flags.vmf_return_data_addr ||
	    vmk_flags.vmf_return_4k_data_addr) {
		*address = map_addr + offset_in_mapping;
	} else {
		*address = map_addr;
	}
	return result;
}

kern_return_t
vm_map_enter_mem_object(
	vm_map_t                target_map,
	vm_map_offset_t         *address,
	vm_map_size_t           initial_size,
	vm_map_offset_t         mask,
	vm_map_kernel_flags_t   vmk_flags,
	ipc_port_t              port,
	vm_object_offset_t      offset,
	boolean_t               copy,
	vm_prot_t               cur_protection,
	vm_prot_t               max_protection,
	vm_inherit_t            inheritance)
{
	kern_return_t ret;

	/* range_id is set by vm_map_enter_mem_object_helper */
	ret = vm_map_enter_mem_object_helper(target_map,
	    address,
	    initial_size,
	    mask,
	    vmk_flags,
	    port,
	    offset,
	    copy,
	    cur_protection,
	    max_protection,
	    inheritance,
	    NULL,
	    0);

#if KASAN
	if (ret == KERN_SUCCESS && address && target_map->pmap == kernel_pmap) {
		kasan_notify_address(*address, initial_size);
	}
#endif

	return ret;
}

kern_return_t
vm_map_enter_mem_object_prefault(
	vm_map_t                target_map,
	vm_map_offset_t         *address,
	vm_map_size_t           initial_size,
	vm_map_offset_t         mask,
	vm_map_kernel_flags_t   vmk_flags,
	ipc_port_t              port,
	vm_object_offset_t      offset,
	vm_prot_t               cur_protection,
	vm_prot_t               max_protection,
	upl_page_list_ptr_t     page_list,
	unsigned int            page_list_count)
{
	kern_return_t ret;

	/* range_id is set by vm_map_enter_mem_object_helper */
	ret = vm_map_enter_mem_object_helper(target_map,
	    address,
	    initial_size,
	    mask,
	    vmk_flags,
	    port,
	    offset,
	    FALSE,
	    cur_protection,
	    max_protection,
	    VM_INHERIT_DEFAULT,
	    page_list,
	    page_list_count);

#if KASAN
	if (ret == KERN_SUCCESS && address && target_map->pmap == kernel_pmap) {
		kasan_notify_address(*address, initial_size);
	}
#endif

	return ret;
}


kern_return_t
vm_map_enter_mem_object_control(
	vm_map_t                target_map,
	vm_map_offset_t         *address,
	vm_map_size_t           initial_size,
	vm_map_offset_t         mask,
	vm_map_kernel_flags_t   vmk_flags,
	memory_object_control_t control,
	vm_object_offset_t      offset,
	boolean_t               copy,
	vm_prot_t               cur_protection,
	vm_prot_t               max_protection,
	vm_inherit_t            inheritance)
{
	vm_map_address_t        map_addr;
	vm_map_size_t           map_size;
	vm_object_t             object;
	vm_object_size_t        size;
	kern_return_t           result;
	memory_object_t         pager;
	vm_prot_t               pager_prot;
	kern_return_t           kr;
#if __arm64__
	boolean_t               fourk = vmk_flags.vmkf_fourk;
#endif /* __arm64__ */

	/*
	 * Check arguments for validity
	 */
	if ((target_map == VM_MAP_NULL) ||
	    (cur_protection & ~(VM_PROT_ALL | VM_PROT_ALLEXEC)) ||
	    (max_protection & ~(VM_PROT_ALL | VM_PROT_ALLEXEC)) ||
	    (inheritance > VM_INHERIT_LAST_VALID) ||
	    initial_size == 0) {
		return KERN_INVALID_ARGUMENT;
	}

	if (__improbable((cur_protection & max_protection) != cur_protection)) {
		/* cur is more permissive than max */
		cur_protection &= max_protection;
	}

#if __arm64__
	if (fourk && VM_MAP_PAGE_MASK(target_map) < PAGE_MASK) {
		fourk = FALSE;
	}

	if (fourk) {
		map_addr = vm_map_trunc_page(*address,
		    FOURK_PAGE_MASK);
		map_size = vm_map_round_page(initial_size,
		    FOURK_PAGE_MASK);
	} else
#endif /* __arm64__ */
	{
		map_addr = vm_map_trunc_page(*address,
		    VM_MAP_PAGE_MASK(target_map));
		map_size = vm_map_round_page(initial_size,
		    VM_MAP_PAGE_MASK(target_map));
	}
	size = vm_object_round_page(initial_size);

	object = memory_object_control_to_vm_object(control);

	if (object == VM_OBJECT_NULL) {
		return KERN_INVALID_OBJECT;
	}

	if (is_kernel_object(object)) {
		printf("Warning: Attempt to map kernel object"
		    " by a non-private kernel entity\n");
		return KERN_INVALID_OBJECT;
	}

	vm_object_lock(object);
	object->ref_count++;

	/*
	 * For "named" VM objects, let the pager know that the
	 * memory object is being mapped.  Some pagers need to keep
	 * track of this, to know when they can reclaim the memory
	 * object, for example.
	 * VM calls memory_object_map() for each mapping (specifying
	 * the protection of each mapping) and calls
	 * memory_object_last_unmap() when all the mappings are gone.
	 */
	pager_prot = max_protection;
	if (copy) {
		pager_prot &= ~VM_PROT_WRITE;
	}
	pager = object->pager;
	if (object->named &&
	    pager != MEMORY_OBJECT_NULL &&
	    object->copy_strategy != MEMORY_OBJECT_COPY_NONE) {
		assert(object->pager_ready);
		vm_object_mapping_wait(object, THREAD_UNINT);
		vm_object_mapping_begin(object);
		vm_object_unlock(object);

		kr = memory_object_map(pager, pager_prot);
		assert(kr == KERN_SUCCESS);

		vm_object_lock(object);
		vm_object_mapping_end(object);
	}
	vm_object_unlock(object);

	/*
	 *	Perform the copy if requested
	 */

	if (copy) {
		vm_object_t             new_object;
		vm_object_offset_t      new_offset;

		result = vm_object_copy_strategically(object, offset, size,
		    false,                                   /* forking */
		    &new_object, &new_offset,
		    &copy);


		if (result == KERN_MEMORY_RESTART_COPY) {
			boolean_t success;
			boolean_t src_needs_copy;

			/*
			 * XXX
			 * We currently ignore src_needs_copy.
			 * This really is the issue of how to make
			 * MEMORY_OBJECT_COPY_SYMMETRIC safe for
			 * non-kernel users to use. Solution forthcoming.
			 * In the meantime, since we don't allow non-kernel
			 * memory managers to specify symmetric copy,
			 * we won't run into problems here.
			 */
			new_object = object;
			new_offset = offset;
			success = vm_object_copy_quickly(new_object,
			    new_offset, size,
			    &src_needs_copy,
			    &copy);
			assert(success);
			result = KERN_SUCCESS;
		}
		/*
		 *	Throw away the reference to the
		 *	original object, as it won't be mapped.
		 */

		vm_object_deallocate(object);

		if (result != KERN_SUCCESS) {
			return result;
		}

		object = new_object;
		offset = new_offset;
	}

#if __arm64__
	if (fourk) {
		result = vm_map_enter_fourk(target_map,
		    &map_addr,
		    map_size,
		    (vm_map_offset_t)mask,
		    vmk_flags,
		    object, offset,
		    copy,
		    cur_protection, max_protection,
		    inheritance);
	} else
#endif /* __arm64__ */
	{
		result = vm_map_enter(target_map,
		    &map_addr, map_size,
		    (vm_map_offset_t)mask,
		    vmk_flags,
		    object, offset,
		    copy,
		    cur_protection, max_protection,
		    inheritance);
	}
	if (result != KERN_SUCCESS) {
		vm_object_deallocate(object);
	}
	*address = map_addr;

	return result;
}


#if     VM_CPM

#ifdef MACH_ASSERT
extern pmap_paddr_t     avail_start, avail_end;
#endif

/*
 *	Allocate memory in the specified map, with the caveat that
 *	the memory is physically contiguous.  This call may fail
 *	if the system can't find sufficient contiguous memory.
 *	This call may cause or lead to heart-stopping amounts of
 *	paging activity.
 *
 *	Memory obtained from this call should be freed in the
 *	normal way, viz., via vm_deallocate.
 */
kern_return_t
vm_map_enter_cpm(
	vm_map_t                map,
	vm_map_offset_t        *addr,
	vm_map_size_t           size,
	vm_map_kernel_flags_t   vmk_flags)
{
	vm_object_t             cpm_obj;
	pmap_t                  pmap;
	vm_page_t               m, pages;
	kern_return_t           kr;
	vm_map_offset_t         va, start, end, offset;
#if     MACH_ASSERT
	vm_map_offset_t         prev_addr = 0;
#endif  /* MACH_ASSERT */
	uint8_t                 object_lock_type = 0;

	if (VM_MAP_PAGE_SHIFT(map) != PAGE_SHIFT) {
		/* XXX TODO4K do we need to support this? */
		*addr = 0;
		return KERN_NOT_SUPPORTED;
	}

	if (size == 0) {
		*addr = 0;
		return KERN_SUCCESS;
	}
	if (vmk_flags.vmf_fixed) {
		*addr = vm_map_trunc_page(*addr,
		    VM_MAP_PAGE_MASK(map));
	} else {
		*addr = vm_map_min(map);
	}
	size = vm_map_round_page(size,
	    VM_MAP_PAGE_MASK(map));

	/*
	 * LP64todo - cpm_allocate should probably allow
	 * allocations of >4GB, but not with the current
	 * algorithm, so just cast down the size for now.
	 */
	if (size > VM_MAX_ADDRESS) {
		return KERN_RESOURCE_SHORTAGE;
	}
	if ((kr = cpm_allocate(CAST_DOWN(vm_size_t, size),
	    &pages, 0, 0, TRUE, flags)) != KERN_SUCCESS) {
		return kr;
	}

	cpm_obj = vm_object_allocate((vm_object_size_t)size);
	assert(cpm_obj != VM_OBJECT_NULL);
	assert(cpm_obj->internal);
	assert(cpm_obj->vo_size == (vm_object_size_t)size);
	assert(cpm_obj->can_persist == FALSE);
	assert(cpm_obj->pager_created == FALSE);
	assert(cpm_obj->pageout == FALSE);
	assert(cpm_obj->shadow == VM_OBJECT_NULL);

	/*
	 *	Insert pages into object.
	 */
	object_lock_type = OBJECT_LOCK_EXCLUSIVE;
	vm_object_lock(cpm_obj);
	for (offset = 0; offset < size; offset += PAGE_SIZE) {
		m = pages;
		pages = NEXT_PAGE(m);
		*(NEXT_PAGE_PTR(m)) = VM_PAGE_NULL;

		assert(!m->vmp_gobbled);
		assert(!m->vmp_wanted);
		assert(!m->vmp_pageout);
		assert(!m->vmp_tabled);
		assert(VM_PAGE_WIRED(m));
		assert(m->vmp_busy);
		assert(VM_PAGE_GET_PHYS_PAGE(m) >= (avail_start >> PAGE_SHIFT) && VM_PAGE_GET_PHYS_PAGE(m) <= (avail_end >> PAGE_SHIFT));

		m->vmp_busy = FALSE;
		vm_page_insert(m, cpm_obj, offset);
	}
	assert(cpm_obj->resident_page_count == size / PAGE_SIZE);
	vm_object_unlock(cpm_obj);

	/*
	 *	Hang onto a reference on the object in case a
	 *	multi-threaded application for some reason decides
	 *	to deallocate the portion of the address space into
	 *	which we will insert this object.
	 *
	 *	Unfortunately, we must insert the object now before
	 *	we can talk to the pmap module about which addresses
	 *	must be wired down.  Hence, the race with a multi-
	 *	threaded app.
	 */
	vm_object_reference(cpm_obj);

	/*
	 *	Insert object into map.
	 */

	kr = vm_map_enter(
		map,
		addr,
		size,
		(vm_map_offset_t)0,
		vmk_flags,
		cpm_obj,
		(vm_object_offset_t)0,
		FALSE,
		VM_PROT_ALL,
		VM_PROT_ALL,
		VM_INHERIT_DEFAULT);

	if (kr != KERN_SUCCESS) {
		/*
		 *	A CPM object doesn't have can_persist set,
		 *	so all we have to do is deallocate it to
		 *	free up these pages.
		 */
		assert(cpm_obj->pager_created == FALSE);
		assert(cpm_obj->can_persist == FALSE);
		assert(cpm_obj->pageout == FALSE);
		assert(cpm_obj->shadow == VM_OBJECT_NULL);
		vm_object_deallocate(cpm_obj); /* kill acquired ref */
		vm_object_deallocate(cpm_obj); /* kill creation ref */
	}

	/*
	 *	Inform the physical mapping system that the
	 *	range of addresses may not fault, so that
	 *	page tables and such can be locked down as well.
	 */
	start = *addr;
	end = start + size;
	pmap = vm_map_pmap(map);
	pmap_pageable(pmap, start, end, FALSE);

	/*
	 *	Enter each page into the pmap, to avoid faults.
	 *	Note that this loop could be coded more efficiently,
	 *	if the need arose, rather than looking up each page
	 *	again.
	 */
	for (offset = 0, va = start; offset < size;
	    va += PAGE_SIZE, offset += PAGE_SIZE) {
		int type_of_fault;

		vm_object_lock(cpm_obj);
		m = vm_page_lookup(cpm_obj, (vm_object_offset_t)offset);
		assert(m != VM_PAGE_NULL);

		vm_page_zero_fill(m);

		type_of_fault = DBG_ZERO_FILL_FAULT;

		vm_fault_enter(m, pmap, va,
		    PAGE_SIZE, 0,
		    VM_PROT_ALL, VM_PROT_WRITE,
		    VM_PAGE_WIRED(m),
		    FALSE,                             /* change_wiring */
		    VM_KERN_MEMORY_NONE,                             /* tag - not wiring */
		    FALSE,                             /* cs_bypass */
		    0,                                 /* user_tag */
		    0,                             /* pmap_options */
		    NULL,                              /* need_retry */
		    &type_of_fault,
		    &object_lock_type);                 /* Exclusive lock mode. Will remain unchanged.*/

		vm_object_unlock(cpm_obj);
	}

#if     MACH_ASSERT
	/*
	 *	Verify ordering in address space.
	 */
	for (offset = 0; offset < size; offset += PAGE_SIZE) {
		vm_object_lock(cpm_obj);
		m = vm_page_lookup(cpm_obj, (vm_object_offset_t)offset);
		vm_object_unlock(cpm_obj);
		if (m == VM_PAGE_NULL) {
			panic("vm_allocate_cpm:  obj %p off 0x%llx no page",
			    cpm_obj, (uint64_t)offset);
		}
		assert(m->vmp_tabled);
		assert(!m->vmp_busy);
		assert(!m->vmp_wanted);
		assert(!m->vmp_fictitious);
		assert(!m->vmp_private);
		assert(!m->vmp_absent);
		assert(!m->vmp_cleaning);
		assert(!m->vmp_laundry);
		assert(!m->vmp_precious);
		assert(!m->vmp_clustered);
		if (offset != 0) {
			if (VM_PAGE_GET_PHYS_PAGE(m) != prev_addr + 1) {
				printf("start 0x%llx end 0x%llx va 0x%llx\n",
				    (uint64_t)start, (uint64_t)end, (uint64_t)va);
				printf("obj %p off 0x%llx\n", cpm_obj, (uint64_t)offset);
				printf("m %p prev_address 0x%llx\n", m, (uint64_t)prev_addr);
				panic("vm_allocate_cpm:  pages not contig!");
			}
		}
		prev_addr = VM_PAGE_GET_PHYS_PAGE(m);
	}
#endif  /* MACH_ASSERT */

	vm_object_deallocate(cpm_obj); /* kill extra ref */

	return kr;
}


#else   /* VM_CPM */

/*
 *	Interface is defined in all cases, but unless the kernel
 *	is built explicitly for this option, the interface does
 *	nothing.
 */

kern_return_t
vm_map_enter_cpm(
	__unused vm_map_t                map,
	__unused vm_map_offset_t        *addr,
	__unused vm_map_size_t           size,
	__unused vm_map_kernel_flags_t   vmk_flags)
{
	return KERN_FAILURE;
}
#endif /* VM_CPM */

/* Not used without nested pmaps */
#ifndef NO_NESTED_PMAP
/*
 * Clip and unnest a portion of a nested submap mapping.
 */


static void
vm_map_clip_unnest(
	vm_map_t        map,
	vm_map_entry_t  entry,
	vm_map_offset_t start_unnest,
	vm_map_offset_t end_unnest)
{
	vm_map_offset_t old_start_unnest = start_unnest;
	vm_map_offset_t old_end_unnest = end_unnest;

	assert(entry->is_sub_map);
	assert(VME_SUBMAP(entry) != NULL);
	assert(entry->use_pmap);

	/*
	 * Query the platform for the optimal unnest range.
	 * DRK: There's some duplication of effort here, since
	 * callers may have adjusted the range to some extent. This
	 * routine was introduced to support 1GiB subtree nesting
	 * for x86 platforms, which can also nest on 2MiB boundaries
	 * depending on size/alignment.
	 */
	if (pmap_adjust_unnest_parameters(map->pmap, &start_unnest, &end_unnest)) {
		assert(VME_SUBMAP(entry)->is_nested_map);
		assert(!VME_SUBMAP(entry)->disable_vmentry_reuse);
		log_unnest_badness(map,
		    old_start_unnest,
		    old_end_unnest,
		    VME_SUBMAP(entry)->is_nested_map,
		    (entry->vme_start +
		    VME_SUBMAP(entry)->lowest_unnestable_start -
		    VME_OFFSET(entry)));
	}

	if (entry->vme_start > start_unnest ||
	    entry->vme_end < end_unnest) {
		panic("vm_map_clip_unnest(0x%llx,0x%llx): "
		    "bad nested entry: start=0x%llx end=0x%llx\n",
		    (long long)start_unnest, (long long)end_unnest,
		    (long long)entry->vme_start, (long long)entry->vme_end);
	}

	if (start_unnest > entry->vme_start) {
		_vm_map_clip_start(&map->hdr,
		    entry,
		    start_unnest);
		if (map->holelistenabled) {
			vm_map_store_update_first_free(map, NULL, FALSE);
		} else {
			vm_map_store_update_first_free(map, map->first_free, FALSE);
		}
	}
	if (entry->vme_end > end_unnest) {
		_vm_map_clip_end(&map->hdr,
		    entry,
		    end_unnest);
		if (map->holelistenabled) {
			vm_map_store_update_first_free(map, NULL, FALSE);
		} else {
			vm_map_store_update_first_free(map, map->first_free, FALSE);
		}
	}

	pmap_unnest(map->pmap,
	    entry->vme_start,
	    entry->vme_end - entry->vme_start);
	if ((map->mapped_in_other_pmaps) && os_ref_get_count_raw(&map->map_refcnt) != 0) {
		/* clean up parent map/maps */
		vm_map_submap_pmap_clean(
			map, entry->vme_start,
			entry->vme_end,
			VME_SUBMAP(entry),
			VME_OFFSET(entry));
	}
	entry->use_pmap = FALSE;
	if ((map->pmap != kernel_pmap) &&
	    (VME_ALIAS(entry) == VM_MEMORY_SHARED_PMAP)) {
		VME_ALIAS_SET(entry, VM_MEMORY_UNSHARED_PMAP);
	}
}
#endif  /* NO_NESTED_PMAP */

__abortlike
static void
__vm_map_clip_atomic_entry_panic(
	vm_map_t        map,
	vm_map_entry_t  entry,
	vm_map_offset_t where)
{
	panic("vm_map_clip(%p): Attempting to clip an atomic VM map entry "
	    "%p [0x%llx:0x%llx] at 0x%llx", map, entry,
	    (uint64_t)entry->vme_start,
	    (uint64_t)entry->vme_end,
	    (uint64_t)where);
}

/*
 *	vm_map_clip_start:	[ internal use only ]
 *
 *	Asserts that the given entry begins at or after
 *	the specified address; if necessary,
 *	it splits the entry into two.
 */
void
vm_map_clip_start(
	vm_map_t        map,
	vm_map_entry_t  entry,
	vm_map_offset_t startaddr)
{
#ifndef NO_NESTED_PMAP
	if (entry->is_sub_map &&
	    entry->use_pmap &&
	    startaddr >= entry->vme_start) {
		vm_map_offset_t start_unnest, end_unnest;

		/*
		 * Make sure "startaddr" is no longer in a nested range
		 * before we clip.  Unnest only the minimum range the platform
		 * can handle.
		 * vm_map_clip_unnest may perform additional adjustments to
		 * the unnest range.
		 */
		start_unnest = startaddr & ~(pmap_shared_region_size_min(map->pmap) - 1);
		end_unnest = start_unnest + pmap_shared_region_size_min(map->pmap);
		vm_map_clip_unnest(map, entry, start_unnest, end_unnest);
	}
#endif /* NO_NESTED_PMAP */
	if (startaddr > entry->vme_start) {
		if (!entry->is_sub_map &&
		    VME_OBJECT(entry) &&
		    VME_OBJECT(entry)->phys_contiguous) {
			pmap_remove(map->pmap,
			    (addr64_t)(entry->vme_start),
			    (addr64_t)(entry->vme_end));
		}
		if (entry->vme_atomic) {
			__vm_map_clip_atomic_entry_panic(map, entry, startaddr);
		}

		DTRACE_VM5(
			vm_map_clip_start,
			vm_map_t, map,
			vm_map_offset_t, entry->vme_start,
			vm_map_offset_t, entry->vme_end,
			vm_map_offset_t, startaddr,
			int, VME_ALIAS(entry));

		_vm_map_clip_start(&map->hdr, entry, startaddr);
		if (map->holelistenabled) {
			vm_map_store_update_first_free(map, NULL, FALSE);
		} else {
			vm_map_store_update_first_free(map, map->first_free, FALSE);
		}
	}
}


#define vm_map_copy_clip_start(copy, entry, startaddr) \
	MACRO_BEGIN \
	if ((startaddr) > (entry)->vme_start) \
	        _vm_map_clip_start(&(copy)->cpy_hdr,(entry),(startaddr)); \
	MACRO_END

/*
 *	This routine is called only when it is known that
 *	the entry must be split.
 */
static void
_vm_map_clip_start(
	struct vm_map_header    *map_header,
	vm_map_entry_t          entry,
	vm_map_offset_t         start)
{
	vm_map_entry_t  new_entry;

	/*
	 *	Split off the front portion --
	 *	note that we must insert the new
	 *	entry BEFORE this one, so that
	 *	this entry has the specified starting
	 *	address.
	 */

	if (entry->map_aligned) {
		assert(VM_MAP_PAGE_ALIGNED(start,
		    VM_MAP_HDR_PAGE_MASK(map_header)));
	}

	new_entry = _vm_map_entry_create(map_header);
	vm_map_entry_copy_full(new_entry, entry);

	new_entry->vme_end = start;
	assert(new_entry->vme_start < new_entry->vme_end);
	VME_OFFSET_SET(entry, VME_OFFSET(entry) + (start - entry->vme_start));
	if (__improbable(start >= entry->vme_end)) {
		panic("mapHdr %p entry %p start 0x%llx end 0x%llx new start 0x%llx", map_header, entry, entry->vme_start, entry->vme_end, start);
	}
	assert(start < entry->vme_end);
	entry->vme_start = start;

#if VM_BTLOG_TAGS
	if (new_entry->vme_kernel_object) {
		btref_retain(new_entry->vme_tag_btref);
	}
#endif /* VM_BTLOG_TAGS */

	_vm_map_store_entry_link(map_header, entry->vme_prev, new_entry);

	if (entry->is_sub_map) {
		vm_map_reference(VME_SUBMAP(new_entry));
	} else {
		vm_object_reference(VME_OBJECT(new_entry));
	}
}


/*
 *	vm_map_clip_end:	[ internal use only ]
 *
 *	Asserts that the given entry ends at or before
 *	the specified address; if necessary,
 *	it splits the entry into two.
 */
void
vm_map_clip_end(
	vm_map_t        map,
	vm_map_entry_t  entry,
	vm_map_offset_t endaddr)
{
	if (endaddr > entry->vme_end) {
		/*
		 * Within the scope of this clipping, limit "endaddr" to
		 * the end of this map entry...
		 */
		endaddr = entry->vme_end;
	}
#ifndef NO_NESTED_PMAP
	if (entry->is_sub_map && entry->use_pmap) {
		vm_map_offset_t start_unnest, end_unnest;

		/*
		 * Make sure the range between the start of this entry and
		 * the new "endaddr" is no longer nested before we clip.
		 * Unnest only the minimum range the platform can handle.
		 * vm_map_clip_unnest may perform additional adjustments to
		 * the unnest range.
		 */
		start_unnest = entry->vme_start;
		end_unnest =
		    (endaddr + pmap_shared_region_size_min(map->pmap) - 1) &
		    ~(pmap_shared_region_size_min(map->pmap) - 1);
		vm_map_clip_unnest(map, entry, start_unnest, end_unnest);
	}
#endif /* NO_NESTED_PMAP */
	if (endaddr < entry->vme_end) {
		if (!entry->is_sub_map &&
		    VME_OBJECT(entry) &&
		    VME_OBJECT(entry)->phys_contiguous) {
			pmap_remove(map->pmap,
			    (addr64_t)(entry->vme_start),
			    (addr64_t)(entry->vme_end));
		}
		if (entry->vme_atomic) {
			__vm_map_clip_atomic_entry_panic(map, entry, endaddr);
		}
		DTRACE_VM5(
			vm_map_clip_end,
			vm_map_t, map,
			vm_map_offset_t, entry->vme_start,
			vm_map_offset_t, entry->vme_end,
			vm_map_offset_t, endaddr,
			int, VME_ALIAS(entry));

		_vm_map_clip_end(&map->hdr, entry, endaddr);
		if (map->holelistenabled) {
			vm_map_store_update_first_free(map, NULL, FALSE);
		} else {
			vm_map_store_update_first_free(map, map->first_free, FALSE);
		}
	}
}


#define vm_map_copy_clip_end(copy, entry, endaddr) \
	MACRO_BEGIN \
	if ((endaddr) < (entry)->vme_end) \
	        _vm_map_clip_end(&(copy)->cpy_hdr,(entry),(endaddr)); \
	MACRO_END

/*
 *	This routine is called only when it is known that
 *	the entry must be split.
 */
static void
_vm_map_clip_end(
	struct vm_map_header    *map_header,
	vm_map_entry_t          entry,
	vm_map_offset_t         end)
{
	vm_map_entry_t  new_entry;

	/*
	 *	Create a new entry and insert it
	 *	AFTER the specified entry
	 */

	if (entry->map_aligned) {
		assert(VM_MAP_PAGE_ALIGNED(end,
		    VM_MAP_HDR_PAGE_MASK(map_header)));
	}

	new_entry = _vm_map_entry_create(map_header);
	vm_map_entry_copy_full(new_entry, entry);

	if (__improbable(end <= entry->vme_start)) {
		panic("mapHdr %p entry %p start 0x%llx end 0x%llx new end 0x%llx", map_header, entry, entry->vme_start, entry->vme_end, end);
	}
	assert(entry->vme_start < end);
	new_entry->vme_start = entry->vme_end = end;
	VME_OFFSET_SET(new_entry,
	    VME_OFFSET(new_entry) + (end - entry->vme_start));
	assert(new_entry->vme_start < new_entry->vme_end);

#if VM_BTLOG_TAGS
	if (new_entry->vme_kernel_object) {
		btref_retain(new_entry->vme_tag_btref);
	}
#endif /* VM_BTLOG_TAGS */

	_vm_map_store_entry_link(map_header, entry, new_entry);

	if (entry->is_sub_map) {
		vm_map_reference(VME_SUBMAP(new_entry));
	} else {
		vm_object_reference(VME_OBJECT(new_entry));
	}
}


/*
 *	VM_MAP_RANGE_CHECK:	[ internal use only ]
 *
 *	Asserts that the starting and ending region
 *	addresses fall within the valid range of the map.
 */
#define VM_MAP_RANGE_CHECK(map, start, end)     \
	MACRO_BEGIN                             \
	if (start < vm_map_min(map))            \
	        start = vm_map_min(map);        \
	if (end > vm_map_max(map))              \
	        end = vm_map_max(map);          \
	if (start > end)                        \
	        start = end;                    \
	MACRO_END

/*
 *	vm_map_range_check:	[ internal use only ]
 *
 *	Check that the region defined by the specified start and
 *	end addresses are wholly contained within a single map
 *	entry or set of adjacent map entries of the spacified map,
 *	i.e. the specified region contains no unmapped space.
 *	If any or all of the region is unmapped, FALSE is returned.
 *	Otherwise, TRUE is returned and if the output argument 'entry'
 *	is not NULL it points to the map entry containing the start
 *	of the region.
 *
 *	The map is locked for reading on entry and is left locked.
 */
static boolean_t
vm_map_range_check(
	vm_map_t                map,
	vm_map_offset_t         start,
	vm_map_offset_t         end,
	vm_map_entry_t          *entry)
{
	vm_map_entry_t          cur;
	vm_map_offset_t         prev;

	/*
	 *      Basic sanity checks first
	 */
	if (start < vm_map_min(map) || end > vm_map_max(map) || start > end) {
		return FALSE;
	}

	/*
	 *      Check first if the region starts within a valid
	 *	mapping for the map.
	 */
	if (!vm_map_lookup_entry(map, start, &cur)) {
		return FALSE;
	}

	/*
	 *	Optimize for the case that the region is contained
	 *	in a single map entry.
	 */
	if (entry != (vm_map_entry_t *) NULL) {
		*entry = cur;
	}
	if (end <= cur->vme_end) {
		return TRUE;
	}

	/*
	 *      If the region is not wholly contained within a
	 *      single entry, walk the entries looking for holes.
	 */
	prev = cur->vme_end;
	cur = cur->vme_next;
	while ((cur != vm_map_to_entry(map)) && (prev == cur->vme_start)) {
		if (end <= cur->vme_end) {
			return TRUE;
		}
		prev = cur->vme_end;
		cur = cur->vme_next;
	}
	return FALSE;
}

/*
 *	vm_map_protect:
 *
 *	Sets the protection of the specified address
 *	region in the target map.  If "set_max" is
 *	specified, the maximum protection is to be set;
 *	otherwise, only the current protection is affected.
 */
kern_return_t
vm_map_protect(
	vm_map_t        map,
	vm_map_offset_t start,
	vm_map_offset_t end,
	vm_prot_t       new_prot,
	boolean_t       set_max)
{
	vm_map_entry_t                  current;
	vm_map_offset_t                 prev;
	vm_map_entry_t                  entry;
	vm_prot_t                       new_max;
	int                             pmap_options = 0;
	kern_return_t                   kr;

	if (__improbable(vm_map_range_overflows(map, start, end - start))) {
		return KERN_INVALID_ARGUMENT;
	}

	if (new_prot & VM_PROT_COPY) {
		vm_map_offset_t         new_start;
		vm_prot_t               cur_prot, max_prot;
		vm_map_kernel_flags_t   kflags;

		/* LP64todo - see below */
		if (start >= map->max_offset) {
			return KERN_INVALID_ADDRESS;
		}

		if ((new_prot & VM_PROT_ALLEXEC) &&
		    map->pmap != kernel_pmap &&
		    (vm_map_cs_enforcement(map)
#if XNU_TARGET_OS_OSX && __arm64__
		    || !VM_MAP_IS_EXOTIC(map)
#endif /* XNU_TARGET_OS_OSX && __arm64__ */
		    ) &&
		    VM_MAP_POLICY_WX_FAIL(map)) {
			DTRACE_VM3(cs_wx,
			    uint64_t, (uint64_t) start,
			    uint64_t, (uint64_t) end,
			    vm_prot_t, new_prot);
			printf("CODE SIGNING: %d[%s] %s:%d(0x%llx,0x%llx,0x%x) can't have both write and exec at the same time\n",
			    proc_selfpid(),
			    (get_bsdtask_info(current_task())
			    ? proc_name_address(get_bsdtask_info(current_task()))
			    : "?"),
			    __FUNCTION__, __LINE__,
#if DEVELOPMENT || DEBUG
			    (uint64_t)start,
			    (uint64_t)end,
#else /* DEVELOPMENT || DEBUG */
			    (uint64_t)0,
			    (uint64_t)0,
#endif /* DEVELOPMENT || DEBUG */
			    new_prot);
			return KERN_PROTECTION_FAILURE;
		}

		/*
		 * Let vm_map_remap_extract() know that it will need to:
		 * + make a copy of the mapping
		 * + add VM_PROT_WRITE to the max protections
		 * + remove any protections that are no longer allowed from the
		 *   max protections (to avoid any WRITE/EXECUTE conflict, for
		 *   example).
		 * Note that "max_prot" is an IN/OUT parameter only for this
		 * specific (VM_PROT_COPY) case.  It's usually an OUT parameter
		 * only.
		 */
		max_prot = new_prot & (VM_PROT_ALL | VM_PROT_ALLEXEC);
		cur_prot = VM_PROT_NONE;
		kflags = VM_MAP_KERNEL_FLAGS_FIXED(.vmf_overwrite = true);
		kflags.vmkf_remap_prot_copy = true;
		kflags.vmkf_tpro_enforcement_override = !vm_map_tpro_enforcement(map);
		new_start = start;
		kr = vm_map_remap(map,
		    &new_start,
		    end - start,
		    0, /* mask */
		    kflags,
		    map,
		    start,
		    TRUE, /* copy-on-write remapping! */
		    &cur_prot, /* IN/OUT */
		    &max_prot, /* IN/OUT */
		    VM_INHERIT_DEFAULT);
		if (kr != KERN_SUCCESS) {
			return kr;
		}
		new_prot &= ~VM_PROT_COPY;
	}

	vm_map_lock(map);

	/* LP64todo - remove this check when vm_map_commpage64()
	 * no longer has to stuff in a map_entry for the commpage
	 * above the map's max_offset.
	 */
	if (start >= map->max_offset) {
		vm_map_unlock(map);
		return KERN_INVALID_ADDRESS;
	}

	while (1) {
		/*
		 *      Lookup the entry.  If it doesn't start in a valid
		 *	entry, return an error.
		 */
		if (!vm_map_lookup_entry(map, start, &entry)) {
			vm_map_unlock(map);
			return KERN_INVALID_ADDRESS;
		}

		if (entry->superpage_size && (start & (SUPERPAGE_SIZE - 1))) { /* extend request to whole entry */
			start = SUPERPAGE_ROUND_DOWN(start);
			continue;
		}
		break;
	}
	if (entry->superpage_size) {
		end = SUPERPAGE_ROUND_UP(end);
	}

	/*
	 *	Make a first pass to check for protection and address
	 *	violations.
	 */

	current = entry;
	prev = current->vme_start;
	while ((current != vm_map_to_entry(map)) &&
	    (current->vme_start < end)) {
		/*
		 * If there is a hole, return an error.
		 */
		if (current->vme_start != prev) {
			vm_map_unlock(map);
			return KERN_INVALID_ADDRESS;
		}

		new_max = current->max_protection;

#if defined(__x86_64__)
		/* Allow max mask to include execute prot bits if this map doesn't enforce CS */
		if (set_max && (new_prot & VM_PROT_ALLEXEC) && !vm_map_cs_enforcement(map)) {
			new_max = (new_max & ~VM_PROT_ALLEXEC) | (new_prot & VM_PROT_ALLEXEC);
		}
#elif CODE_SIGNING_MONITOR
		if (set_max && (new_prot & VM_PROT_EXECUTE) && (csm_address_space_exempt(map->pmap) == KERN_SUCCESS)) {
			new_max |= VM_PROT_EXECUTE;
		}
#endif
		if ((new_prot & new_max) != new_prot) {
			vm_map_unlock(map);
			return KERN_PROTECTION_FAILURE;
		}

		if (current->used_for_jit &&
		    pmap_has_prot_policy(map->pmap, current->translated_allow_execute, current->protection)) {
			vm_map_unlock(map);
			return KERN_PROTECTION_FAILURE;
		}

#if __arm64e__
		/* Disallow remapping hw assisted TPRO mappings */
		if (current->used_for_tpro) {
			vm_map_unlock(map);
			return KERN_PROTECTION_FAILURE;
		}
#endif /* __arm64e__ */


		if ((new_prot & VM_PROT_WRITE) &&
		    (new_prot & VM_PROT_ALLEXEC) &&
#if XNU_TARGET_OS_OSX
		    map->pmap != kernel_pmap &&
		    (vm_map_cs_enforcement(map)
#if __arm64__
		    || !VM_MAP_IS_EXOTIC(map)
#endif /* __arm64__ */
		    ) &&
#endif /* XNU_TARGET_OS_OSX */
#if CODE_SIGNING_MONITOR
		    (csm_address_space_exempt(map->pmap) != KERN_SUCCESS) &&
#endif
		    !(current->used_for_jit)) {
			DTRACE_VM3(cs_wx,
			    uint64_t, (uint64_t) current->vme_start,
			    uint64_t, (uint64_t) current->vme_end,
			    vm_prot_t, new_prot);
			printf("CODE SIGNING: %d[%s] %s:%d(0x%llx,0x%llx,0x%x) can't have both write and exec at the same time\n",
			    proc_selfpid(),
			    (get_bsdtask_info(current_task())
			    ? proc_name_address(get_bsdtask_info(current_task()))
			    : "?"),
			    __FUNCTION__, __LINE__,
#if DEVELOPMENT || DEBUG
			    (uint64_t)current->vme_start,
			    (uint64_t)current->vme_end,
#else /* DEVELOPMENT || DEBUG */
			    (uint64_t)0,
			    (uint64_t)0,
#endif /* DEVELOPMENT || DEBUG */
			    new_prot);
			new_prot &= ~VM_PROT_ALLEXEC;
			if (VM_MAP_POLICY_WX_FAIL(map)) {
				vm_map_unlock(map);
				return KERN_PROTECTION_FAILURE;
			}
		}

		/*
		 * If the task has requested executable lockdown,
		 * deny both:
		 * - adding executable protections OR
		 * - adding write protections to an existing executable mapping.
		 */
		if (map->map_disallow_new_exec == TRUE) {
			if ((new_prot & VM_PROT_ALLEXEC) ||
			    ((current->protection & VM_PROT_EXECUTE) && (new_prot & VM_PROT_WRITE))) {
				vm_map_unlock(map);
				return KERN_PROTECTION_FAILURE;
			}
		}

		prev = current->vme_end;
		current = current->vme_next;
	}

#if __arm64__
	if (end > prev &&
	    end == vm_map_round_page(prev, VM_MAP_PAGE_MASK(map))) {
		vm_map_entry_t prev_entry;

		prev_entry = current->vme_prev;
		if (prev_entry != vm_map_to_entry(map) &&
		    !prev_entry->map_aligned &&
		    (vm_map_round_page(prev_entry->vme_end,
		    VM_MAP_PAGE_MASK(map))
		    == end)) {
			/*
			 * The last entry in our range is not "map-aligned"
			 * but it would have reached all the way to "end"
			 * if it had been map-aligned, so this is not really
			 * a hole in the range and we can proceed.
			 */
			prev = end;
		}
	}
#endif /* __arm64__ */

	if (end > prev) {
		vm_map_unlock(map);
		return KERN_INVALID_ADDRESS;
	}

	/*
	 *	Go back and fix up protections.
	 *	Clip to start here if the range starts within
	 *	the entry.
	 */

	current = entry;
	if (current != vm_map_to_entry(map)) {
		/* clip and unnest if necessary */
		vm_map_clip_start(map, current, start);
	}

	while ((current != vm_map_to_entry(map)) &&
	    (current->vme_start < end)) {
		vm_prot_t       old_prot;

		vm_map_clip_end(map, current, end);

#if DEVELOPMENT || DEBUG
		if (current->csm_associated && vm_log_xnu_user_debug) {
			printf("FBDP %d[%s] %s(0x%llx,0x%llx,0x%x) on map %p entry %p [0x%llx:0x%llx 0x%x/0x%x] csm_associated\n",
			    proc_selfpid(),
			    (get_bsdtask_info(current_task())
			    ? proc_name_address(get_bsdtask_info(current_task()))
			    : "?"),
			    __FUNCTION__,
			    (uint64_t)start,
			    (uint64_t)end,
			    new_prot,
			    map, current,
			    current->vme_start,
			    current->vme_end,
			    current->protection,
			    current->max_protection);
		}
#endif /* DEVELOPMENT || DEBUG */

		if (current->is_sub_map) {
			/* clipping did unnest if needed */
			assert(!current->use_pmap);
		}

		old_prot = current->protection;

		if (set_max) {
			current->max_protection = new_prot;
			/* Consider either EXECUTE or UEXEC as EXECUTE for this masking */
			current->protection = (new_prot & old_prot);
		} else {
			current->protection = new_prot;
		}

#if CODE_SIGNING_MONITOR
		if (!current->vme_xnu_user_debug &&
		    /* a !csm_associated mapping becoming executable */
		    ((!current->csm_associated &&
		    !(old_prot & VM_PROT_EXECUTE) &&
		    (current->protection & VM_PROT_EXECUTE))
		    ||
		    /* a csm_associated mapping becoming writable */
		    (current->csm_associated &&
		    !(old_prot & VM_PROT_WRITE) &&
		    (current->protection & VM_PROT_WRITE)))) {
			/*
			 * This mapping has not already been marked as
			 * "user_debug" and it is either:
			 * 1. not code-signing-monitored and becoming executable
			 * 2. code-signing-monitored and becoming writable,
			 * so inform the CodeSigningMonitor and mark the
			 * mapping as "user_debug" if appropriate.
			 */
			vm_map_kernel_flags_t vmk_flags;
			vmk_flags = VM_MAP_KERNEL_FLAGS_NONE;
			/* pretend it's a vm_protect(VM_PROT_COPY)... */
			vmk_flags.vmkf_remap_prot_copy = true;
			kr = vm_map_entry_cs_associate(map, current, vmk_flags);
#if DEVELOPMENT || DEBUG
			if (vm_log_xnu_user_debug) {
				printf("FBDP %d[%s] %s:%d map %p entry %p [ 0x%llx 0x%llx ] prot 0x%x -> 0x%x cs_associate -> %d user_debug=%d\n",
				    proc_selfpid(),
				    (get_bsdtask_info(current_task()) ? proc_name_address(get_bsdtask_info(current_task())) : "?"),
				    __FUNCTION__, __LINE__,
				    map, current,
				    current->vme_start, current->vme_end,
				    old_prot, current->protection,
				    kr, current->vme_xnu_user_debug);
			}
#endif /* DEVELOPMENT || DEBUG */
		}
#endif /* CODE_SIGNING_MONITOR */

		/*
		 *	Update physical map if necessary.
		 *	If the request is to turn off write protection,
		 *	we won't do it for real (in pmap). This is because
		 *	it would cause copy-on-write to fail.  We've already
		 *	set, the new protection in the map, so if a
		 *	write-protect fault occurred, it will be fixed up
		 *	properly, COW or not.
		 */
		if (current->protection != old_prot) {
			/* Look one level in we support nested pmaps */
			/* from mapped submaps which are direct entries */
			/* in our map */

			vm_prot_t prot;

			prot = current->protection;
			if (current->is_sub_map || (VME_OBJECT(current) == NULL) || (VME_OBJECT(current) != compressor_object)) {
				prot &= ~VM_PROT_WRITE;
			} else {
				assert(!VME_OBJECT(current)->code_signed);
				assert(VME_OBJECT(current)->copy_strategy == MEMORY_OBJECT_COPY_NONE);
				if (prot & VM_PROT_WRITE) {
					/*
					 * For write requests on the
					 * compressor, we wil ask the
					 * pmap layer to prevent us from
					 * taking a write fault when we
					 * attempt to access the mapping
					 * next.
					 */
					pmap_options |= PMAP_OPTIONS_PROTECT_IMMEDIATE;
				}
			}

			if (override_nx(map, VME_ALIAS(current)) && prot) {
				prot |= VM_PROT_EXECUTE;
			}

#if DEVELOPMENT || DEBUG
			if (!(old_prot & VM_PROT_EXECUTE) &&
			    (prot & VM_PROT_EXECUTE) &&
			    panic_on_unsigned_execute &&
			    (proc_selfcsflags() & CS_KILL)) {
				panic("vm_map_protect(%p,0x%llx,0x%llx) old=0x%x new=0x%x - <rdar://23770418> code-signing bypass?", map, (uint64_t)current->vme_start, (uint64_t)current->vme_end, old_prot, prot);
			}
#endif /* DEVELOPMENT || DEBUG */

			if (pmap_has_prot_policy(map->pmap, current->translated_allow_execute, prot)) {
				if (current->wired_count) {
					panic("vm_map_protect(%p,0x%llx,0x%llx) new=0x%x wired=%x",
					    map, (uint64_t)current->vme_start, (uint64_t)current->vme_end, prot, current->wired_count);
				}

				/* If the pmap layer cares about this
				 * protection type, force a fault for
				 * each page so that vm_fault will
				 * repopulate the page with the full
				 * set of protections.
				 */
				/*
				 * TODO: We don't seem to need this,
				 * but this is due to an internal
				 * implementation detail of
				 * pmap_protect.  Do we want to rely
				 * on this?
				 */
				prot = VM_PROT_NONE;
			}

			if (current->is_sub_map && current->use_pmap) {
				pmap_protect(VME_SUBMAP(current)->pmap,
				    current->vme_start,
				    current->vme_end,
				    prot);
			} else {
				pmap_protect_options(map->pmap,
				    current->vme_start,
				    current->vme_end,
				    prot,
				    pmap_options,
				    NULL);
			}
		}
		current = current->vme_next;
	}

	current = entry;
	while ((current != vm_map_to_entry(map)) &&
	    (current->vme_start <= end)) {
		vm_map_simplify_entry(map, current);
		current = current->vme_next;
	}

	vm_map_unlock(map);
	return KERN_SUCCESS;
}

/*
 *	vm_map_inherit:
 *
 *	Sets the inheritance of the specified address
 *	range in the target map.  Inheritance
 *	affects how the map will be shared with
 *	child maps at the time of vm_map_fork.
 */
kern_return_t
vm_map_inherit(
	vm_map_t        map,
	vm_map_offset_t start,
	vm_map_offset_t end,
	vm_inherit_t    new_inheritance)
{
	vm_map_entry_t  entry;
	vm_map_entry_t  temp_entry;

	vm_map_lock(map);

	VM_MAP_RANGE_CHECK(map, start, end);

	if (__improbable(vm_map_range_overflows(map, start, end - start))) {
		vm_map_unlock(map);
		return KERN_INVALID_ADDRESS;
	}

	if (vm_map_lookup_entry(map, start, &temp_entry)) {
		entry = temp_entry;
	} else {
		temp_entry = temp_entry->vme_next;
		entry = temp_entry;
	}

	/* first check entire range for submaps which can't support the */
	/* given inheritance. */
	while ((entry != vm_map_to_entry(map)) && (entry->vme_start < end)) {
		if (entry->is_sub_map) {
			if (new_inheritance == VM_INHERIT_COPY) {
				vm_map_unlock(map);
				return KERN_INVALID_ARGUMENT;
			}
		}

		entry = entry->vme_next;
	}

	entry = temp_entry;
	if (entry != vm_map_to_entry(map)) {
		/* clip and unnest if necessary */
		vm_map_clip_start(map, entry, start);
	}

	while ((entry != vm_map_to_entry(map)) && (entry->vme_start < end)) {
		vm_map_clip_end(map, entry, end);
		if (entry->is_sub_map) {
			/* clip did unnest if needed */
			assert(!entry->use_pmap);
		}

		entry->inheritance = new_inheritance;

		entry = entry->vme_next;
	}

	vm_map_unlock(map);
	return KERN_SUCCESS;
}

/*
 * Update the accounting for the amount of wired memory in this map.  If the user has
 * exceeded the defined limits, then we fail.  Wiring on behalf of the kernel never fails.
 */

static kern_return_t
add_wire_counts(
	vm_map_t        map,
	vm_map_entry_t  entry,
	boolean_t       user_wire)
{
	vm_map_size_t   size;

	bool first_wire = entry->wired_count == 0 && entry->user_wired_count == 0;

	if (user_wire) {
		unsigned int total_wire_count =  vm_page_wire_count + vm_lopage_free_count;

		/*
		 * We're wiring memory at the request of the user.  Check if this is the first time the user is wiring
		 * this map entry.
		 */

		if (entry->user_wired_count == 0) {
			size = entry->vme_end - entry->vme_start;

			/*
			 * Since this is the first time the user is wiring this map entry, check to see if we're
			 * exceeding the user wire limits.  There is a per map limit which is the smaller of either
			 * the process's rlimit or the global vm_per_task_user_wire_limit which caps this value.  There is also
			 * a system-wide limit on the amount of memory all users can wire.  If the user is over either
			 * limit, then we fail.
			 */

			if (size + map->user_wire_size > MIN(map->user_wire_limit, vm_per_task_user_wire_limit) ||
			    size + ptoa_64(total_wire_count) > vm_global_user_wire_limit) {
				if (size + ptoa_64(total_wire_count) > vm_global_user_wire_limit) {
#if DEVELOPMENT || DEBUG
					if (panic_on_mlock_failure) {
						panic("mlock: Over global wire limit. %llu bytes wired and requested to wire %llu bytes more", ptoa_64(total_wire_count), (uint64_t) size);
					}
#endif /* DEVELOPMENT || DEBUG */
					os_atomic_inc(&vm_add_wire_count_over_global_limit, relaxed);
				} else {
					os_atomic_inc(&vm_add_wire_count_over_user_limit, relaxed);
#if DEVELOPMENT || DEBUG
					if (panic_on_mlock_failure) {
						panic("mlock: Over process wire limit. %llu bytes wired and requested to wire %llu bytes more", (uint64_t) map->user_wire_size, (uint64_t) size);
					}
#endif /* DEVELOPMENT || DEBUG */
				}
				return KERN_RESOURCE_SHORTAGE;
			}

			/*
			 * The first time the user wires an entry, we also increment the wired_count and add this to
			 * the total that has been wired in the map.
			 */

			if (entry->wired_count >= MAX_WIRE_COUNT) {
				return KERN_FAILURE;
			}

			entry->wired_count++;
			map->user_wire_size += size;
		}

		if (entry->user_wired_count >= MAX_WIRE_COUNT) {
			return KERN_FAILURE;
		}

		entry->user_wired_count++;
	} else {
		/*
		 * The kernel's wiring the memory.  Just bump the count and continue.
		 */

		if (entry->wired_count >= MAX_WIRE_COUNT) {
			panic("vm_map_wire: too many wirings");
		}

		entry->wired_count++;
	}

	if (first_wire) {
		vme_btref_consider_and_set(entry, __builtin_frame_address(0));
	}

	return KERN_SUCCESS;
}

/*
 * Update the memory wiring accounting now that the given map entry is being unwired.
 */

static void
subtract_wire_counts(
	vm_map_t        map,
	vm_map_entry_t  entry,
	boolean_t       user_wire)
{
	if (user_wire) {
		/*
		 * We're unwiring memory at the request of the user.  See if we're removing the last user wire reference.
		 */

		if (entry->user_wired_count == 1) {
			/*
			 * We're removing the last user wire reference.  Decrement the wired_count and the total
			 * user wired memory for this map.
			 */

			assert(entry->wired_count >= 1);
			entry->wired_count--;
			map->user_wire_size -= entry->vme_end - entry->vme_start;
		}

		assert(entry->user_wired_count >= 1);
		entry->user_wired_count--;
	} else {
		/*
		 * The kernel is unwiring the memory.   Just update the count.
		 */

		assert(entry->wired_count >= 1);
		entry->wired_count--;
	}

	vme_btref_consider_and_put(entry);
}

int cs_executable_wire = 0;

/*
 *	vm_map_wire:
 *
 *	Sets the pageability of the specified address range in the
 *	target map as wired.  Regions specified as not pageable require
 *	locked-down physical memory and physical page maps.  The
 *	access_type variable indicates types of accesses that must not
 *	generate page faults.  This is checked against protection of
 *	memory being locked-down.
 *
 *	The map must not be locked, but a reference must remain to the
 *	map throughout the call.
 */
static kern_return_t
vm_map_wire_nested(
	vm_map_t                map,
	vm_map_offset_t         start,
	vm_map_offset_t         end,
	vm_prot_t               caller_prot,
	vm_tag_t                tag,
	boolean_t               user_wire,
	pmap_t                  map_pmap,
	vm_map_offset_t         pmap_addr,
	ppnum_t                 *physpage_p)
{
	vm_map_entry_t          entry;
	vm_prot_t               access_type;
	struct vm_map_entry     *first_entry, tmp_entry;
	vm_map_t                real_map;
	vm_map_offset_t         s, e;
	kern_return_t           rc;
	boolean_t               need_wakeup;
	boolean_t               main_map = FALSE;
	wait_interrupt_t        interruptible_state;
	thread_t                cur_thread;
	unsigned int            last_timestamp;
	vm_map_size_t           size;
	boolean_t               wire_and_extract;
	vm_prot_t               extra_prots;

	extra_prots = VM_PROT_COPY;
	extra_prots |= VM_PROT_COPY_FAIL_IF_EXECUTABLE;
#if XNU_TARGET_OS_OSX
	if (map->pmap == kernel_pmap ||
	    !vm_map_cs_enforcement(map)) {
		extra_prots &= ~VM_PROT_COPY_FAIL_IF_EXECUTABLE;
	}
#endif /* XNU_TARGET_OS_OSX */
#if CODE_SIGNING_MONITOR
	if (csm_address_space_exempt(map->pmap) == KERN_SUCCESS) {
		extra_prots &= ~VM_PROT_COPY_FAIL_IF_EXECUTABLE;
	}
#endif /* CODE_SIGNING_MONITOR */

	access_type = (caller_prot & (VM_PROT_ALL | VM_PROT_ALLEXEC));

	wire_and_extract = FALSE;
	if (physpage_p != NULL) {
		/*
		 * The caller wants the physical page number of the
		 * wired page.  We return only one physical page number
		 * so this works for only one page at a time.
		 */
		if ((end - start) != PAGE_SIZE) {
			return KERN_INVALID_ARGUMENT;
		}
		wire_and_extract = TRUE;
		*physpage_p = 0;
	}

	vm_map_lock(map);
	if (map_pmap == NULL) {
		main_map = TRUE;
	}
	last_timestamp = map->timestamp;

	VM_MAP_RANGE_CHECK(map, start, end);
	assert(VM_MAP_PAGE_ALIGNED(start, VM_MAP_PAGE_MASK(map)));
	assert(VM_MAP_PAGE_ALIGNED(end, VM_MAP_PAGE_MASK(map)));

	if (start == end) {
		/* We wired what the caller asked for, zero pages */
		vm_map_unlock(map);
		return KERN_SUCCESS;
	}

	if (__improbable(vm_map_range_overflows(map, start, end - start))) {
		vm_map_unlock(map);
		return KERN_INVALID_ADDRESS;
	}

	need_wakeup = FALSE;
	cur_thread = current_thread();

	s = start;
	rc = KERN_SUCCESS;

	if (vm_map_lookup_entry(map, s, &first_entry)) {
		entry = first_entry;
		/*
		 * vm_map_clip_start will be done later.
		 * We don't want to unnest any nested submaps here !
		 */
	} else {
		/* Start address is not in map */
		rc = KERN_INVALID_ADDRESS;
		goto done;
	}

	while ((entry != vm_map_to_entry(map)) && (s < end)) {
		/*
		 * At this point, we have wired from "start" to "s".
		 * We still need to wire from "s" to "end".
		 *
		 * "entry" hasn't been clipped, so it could start before "s"
		 * and/or end after "end".
		 */

		/* "e" is how far we want to wire in this entry */
		e = entry->vme_end;
		if (e > end) {
			e = end;
		}

		/*
		 * If another thread is wiring/unwiring this entry then
		 * block after informing other thread to wake us up.
		 */
		if (entry->in_transition) {
			wait_result_t wait_result;

			/*
			 * We have not clipped the entry.  Make sure that
			 * the start address is in range so that the lookup
			 * below will succeed.
			 * "s" is the current starting point: we've already
			 * wired from "start" to "s" and we still have
			 * to wire from "s" to "end".
			 */

			entry->needs_wakeup = TRUE;

			/*
			 * wake up anybody waiting on entries that we have
			 * already wired.
			 */
			if (need_wakeup) {
				vm_map_entry_wakeup(map);
				need_wakeup = FALSE;
			}
			/*
			 * User wiring is interruptible
			 */
			wait_result = vm_map_entry_wait(map,
			    (user_wire) ? THREAD_ABORTSAFE :
			    THREAD_UNINT);
			if (user_wire && wait_result == THREAD_INTERRUPTED) {
				/*
				 * undo the wirings we have done so far
				 * We do not clear the needs_wakeup flag,
				 * because we cannot tell if we were the
				 * only one waiting.
				 */
				rc = KERN_FAILURE;
				goto done;
			}

			/*
			 * Cannot avoid a lookup here. reset timestamp.
			 */
			last_timestamp = map->timestamp;

			/*
			 * The entry could have been clipped, look it up again.
			 * Worse that can happen is, it may not exist anymore.
			 */
			if (!vm_map_lookup_entry(map, s, &first_entry)) {
				/*
				 * User: undo everything upto the previous
				 * entry.  let vm_map_unwire worry about
				 * checking the validity of the range.
				 */
				rc = KERN_FAILURE;
				goto done;
			}
			entry = first_entry;
			continue;
		}

		if (entry->is_sub_map) {
			vm_map_offset_t sub_start;
			vm_map_offset_t sub_end;
			vm_map_offset_t local_start;
			vm_map_offset_t local_end;
			pmap_t          pmap;

			if (wire_and_extract) {
				/*
				 * Wiring would result in copy-on-write
				 * which would not be compatible with
				 * the sharing we have with the original
				 * provider of this memory.
				 */
				rc = KERN_INVALID_ARGUMENT;
				goto done;
			}

			vm_map_clip_start(map, entry, s);
			vm_map_clip_end(map, entry, end);

			sub_start = VME_OFFSET(entry);
			sub_end = entry->vme_end;
			sub_end += VME_OFFSET(entry) - entry->vme_start;

			local_end = entry->vme_end;
			if (map_pmap == NULL) {
				vm_object_t             object;
				vm_object_offset_t      offset;
				vm_prot_t               prot;
				boolean_t               wired;
				vm_map_entry_t          local_entry;
				vm_map_version_t         version;
				vm_map_t                lookup_map;

				if (entry->use_pmap) {
					pmap = VME_SUBMAP(entry)->pmap;
					/* ppc implementation requires that */
					/* submaps pmap address ranges line */
					/* up with parent map */
#ifdef notdef
					pmap_addr = sub_start;
#endif
					pmap_addr = s;
				} else {
					pmap = map->pmap;
					pmap_addr = s;
				}

				if (entry->wired_count) {
					if ((rc = add_wire_counts(map, entry, user_wire)) != KERN_SUCCESS) {
						goto done;
					}

					/*
					 * The map was not unlocked:
					 * no need to goto re-lookup.
					 * Just go directly to next entry.
					 */
					entry = entry->vme_next;
					s = entry->vme_start;
					continue;
				}

				/* call vm_map_lookup_and_lock_object to */
				/* cause any needs copy to be   */
				/* evaluated */
				local_start = entry->vme_start;
				lookup_map = map;
				vm_map_lock_write_to_read(map);
				rc = vm_map_lookup_and_lock_object(
					&lookup_map, local_start,
					(access_type | extra_prots),
					OBJECT_LOCK_EXCLUSIVE,
					&version, &object,
					&offset, &prot, &wired,
					NULL,
					&real_map, NULL);
				if (rc != KERN_SUCCESS) {
					vm_map_unlock_read(lookup_map);
					assert(map_pmap == NULL);
					vm_map_unwire(map, start,
					    s, user_wire);
					return rc;
				}
				vm_object_unlock(object);
				if (real_map != lookup_map) {
					vm_map_unlock(real_map);
				}
				vm_map_unlock_read(lookup_map);
				vm_map_lock(map);

				/* we unlocked, so must re-lookup */
				if (!vm_map_lookup_entry(map,
				    local_start,
				    &local_entry)) {
					rc = KERN_FAILURE;
					goto done;
				}

				/*
				 * entry could have been "simplified",
				 * so re-clip
				 */
				entry = local_entry;
				assert(s == local_start);
				vm_map_clip_start(map, entry, s);
				vm_map_clip_end(map, entry, end);
				/* re-compute "e" */
				e = entry->vme_end;
				if (e > end) {
					e = end;
				}

				/* did we have a change of type? */
				if (!entry->is_sub_map) {
					last_timestamp = map->timestamp;
					continue;
				}
			} else {
				local_start = entry->vme_start;
				pmap = map_pmap;
			}

			if ((rc = add_wire_counts(map, entry, user_wire)) != KERN_SUCCESS) {
				goto done;
			}

			entry->in_transition = TRUE;

			vm_map_unlock(map);
			rc = vm_map_wire_nested(VME_SUBMAP(entry),
			    sub_start, sub_end,
			    caller_prot, tag,
			    user_wire, pmap, pmap_addr,
			    NULL);
			vm_map_lock(map);

			/*
			 * Find the entry again.  It could have been clipped
			 * after we unlocked the map.
			 */
			if (!vm_map_lookup_entry(map, local_start,
			    &first_entry)) {
				panic("vm_map_wire: re-lookup failed");
			}
			entry = first_entry;

			assert(local_start == s);
			/* re-compute "e" */
			e = entry->vme_end;
			if (e > end) {
				e = end;
			}

			last_timestamp = map->timestamp;
			while ((entry != vm_map_to_entry(map)) &&
			    (entry->vme_start < e)) {
				assert(entry->in_transition);
				entry->in_transition = FALSE;
				if (entry->needs_wakeup) {
					entry->needs_wakeup = FALSE;
					need_wakeup = TRUE;
				}
				if (rc != KERN_SUCCESS) {/* from vm_*_wire */
					subtract_wire_counts(map, entry, user_wire);
				}
				entry = entry->vme_next;
			}
			if (rc != KERN_SUCCESS) {       /* from vm_*_wire */
				goto done;
			}

			/* no need to relookup again */
			s = entry->vme_start;
			continue;
		}

		/*
		 * If this entry is already wired then increment
		 * the appropriate wire reference count.
		 */
		if (entry->wired_count) {
			if ((entry->protection & access_type) != access_type) {
				/* found a protection problem */

				/*
				 * XXX FBDP
				 * We should always return an error
				 * in this case but since we didn't
				 * enforce it before, let's do
				 * it only for the new "wire_and_extract"
				 * code path for now...
				 */
				if (wire_and_extract) {
					rc = KERN_PROTECTION_FAILURE;
					goto done;
				}
			}

			/*
			 * entry is already wired down, get our reference
			 * after clipping to our range.
			 */
			vm_map_clip_start(map, entry, s);
			vm_map_clip_end(map, entry, end);

			if ((rc = add_wire_counts(map, entry, user_wire)) != KERN_SUCCESS) {
				goto done;
			}

			if (wire_and_extract) {
				vm_object_t             object;
				vm_object_offset_t      offset;
				vm_page_t               m;

				/*
				 * We don't have to "wire" the page again
				 * bit we still have to "extract" its
				 * physical page number, after some sanity
				 * checks.
				 */
				assert((entry->vme_end - entry->vme_start)
				    == PAGE_SIZE);
				assert(!entry->needs_copy);
				assert(!entry->is_sub_map);
				assert(VME_OBJECT(entry));
				if (((entry->vme_end - entry->vme_start)
				    != PAGE_SIZE) ||
				    entry->needs_copy ||
				    entry->is_sub_map ||
				    VME_OBJECT(entry) == VM_OBJECT_NULL) {
					rc = KERN_INVALID_ARGUMENT;
					goto done;
				}

				object = VME_OBJECT(entry);
				offset = VME_OFFSET(entry);
				/* need exclusive lock to update m->dirty */
				if (entry->protection & VM_PROT_WRITE) {
					vm_object_lock(object);
				} else {
					vm_object_lock_shared(object);
				}
				m = vm_page_lookup(object, offset);
				assert(m != VM_PAGE_NULL);
				assert(VM_PAGE_WIRED(m));
				if (m != VM_PAGE_NULL && VM_PAGE_WIRED(m)) {
					*physpage_p = VM_PAGE_GET_PHYS_PAGE(m);
					if (entry->protection & VM_PROT_WRITE) {
						vm_object_lock_assert_exclusive(
							object);
						m->vmp_dirty = TRUE;
					}
				} else {
					/* not already wired !? */
					*physpage_p = 0;
				}
				vm_object_unlock(object);
			}

			/* map was not unlocked: no need to relookup */
			entry = entry->vme_next;
			s = entry->vme_start;
			continue;
		}

		/*
		 * Unwired entry or wire request transmitted via submap
		 */

		/*
		 * Wiring would copy the pages to the shadow object.
		 * The shadow object would not be code-signed so
		 * attempting to execute code from these copied pages
		 * would trigger a code-signing violation.
		 */

		if ((entry->protection & VM_PROT_EXECUTE)
#if XNU_TARGET_OS_OSX
		    &&
		    map->pmap != kernel_pmap &&
		    (vm_map_cs_enforcement(map)
#if __arm64__
		    || !VM_MAP_IS_EXOTIC(map)
#endif /* __arm64__ */
		    )
#endif /* XNU_TARGET_OS_OSX */
#if CODE_SIGNING_MONITOR
		    &&
		    (csm_address_space_exempt(map->pmap) != KERN_SUCCESS)
#endif
		    ) {
#if MACH_ASSERT
			printf("pid %d[%s] wiring executable range from "
			    "0x%llx to 0x%llx: rejected to preserve "
			    "code-signing\n",
			    proc_selfpid(),
			    (get_bsdtask_info(current_task())
			    ? proc_name_address(get_bsdtask_info(current_task()))
			    : "?"),
			    (uint64_t) entry->vme_start,
			    (uint64_t) entry->vme_end);
#endif /* MACH_ASSERT */
			DTRACE_VM2(cs_executable_wire,
			    uint64_t, (uint64_t)entry->vme_start,
			    uint64_t, (uint64_t)entry->vme_end);
			cs_executable_wire++;
			rc = KERN_PROTECTION_FAILURE;
			goto done;
		}

		/*
		 * Perform actions of vm_map_lookup that need the write
		 * lock on the map: create a shadow object for a
		 * copy-on-write region, or an object for a zero-fill
		 * region.
		 */
		size = entry->vme_end - entry->vme_start;
		/*
		 * If wiring a copy-on-write page, we need to copy it now
		 * even if we're only (currently) requesting read access.
		 * This is aggressive, but once it's wired we can't move it.
		 */
		if (entry->needs_copy) {
			if (wire_and_extract) {
				/*
				 * We're supposed to share with the original
				 * provider so should not be "needs_copy"
				 */
				rc = KERN_INVALID_ARGUMENT;
				goto done;
			}

			VME_OBJECT_SHADOW(entry, size,
			    vm_map_always_shadow(map));
			entry->needs_copy = FALSE;
		} else if (VME_OBJECT(entry) == VM_OBJECT_NULL) {
			if (wire_and_extract) {
				/*
				 * We're supposed to share with the original
				 * provider so should already have an object.
				 */
				rc = KERN_INVALID_ARGUMENT;
				goto done;
			}
			VME_OBJECT_SET(entry, vm_object_allocate(size), false, 0);
			VME_OFFSET_SET(entry, (vm_object_offset_t)0);
			assert(entry->use_pmap);
		} else if (VME_OBJECT(entry)->copy_strategy == MEMORY_OBJECT_COPY_SYMMETRIC) {
			if (wire_and_extract) {
				/*
				 * We're supposed to share with the original
				 * provider so should not be COPY_SYMMETRIC.
				 */
				rc = KERN_INVALID_ARGUMENT;
				goto done;
			}
			/*
			 * Force an unrequested "copy-on-write" but only for
			 * the range we're wiring.
			 */
//			printf("FBDP %s:%d map %p entry %p [ 0x%llx 0x%llx ] s 0x%llx end 0x%llx wire&extract=%d\n", __FUNCTION__, __LINE__, map, entry, (uint64_t)entry->vme_start, (uint64_t)entry->vme_end, (uint64_t)s, (uint64_t)end, wire_and_extract);
			vm_map_clip_start(map, entry, s);
			vm_map_clip_end(map, entry, end);
			/* recompute "size" */
			size = entry->vme_end - entry->vme_start;
			/* make a shadow object */
			vm_object_t orig_object;
			vm_object_offset_t orig_offset;
			orig_object = VME_OBJECT(entry);
			orig_offset = VME_OFFSET(entry);
			VME_OBJECT_SHADOW(entry, size, vm_map_always_shadow(map));
			if (VME_OBJECT(entry) != orig_object) {
				/*
				 * This mapping has not been shared (or it would be
				 * COPY_DELAY instead of COPY_SYMMETRIC) and it has
				 * not been copied-on-write (or it would be marked
				 * as "needs_copy" and would have been handled above
				 * and also already write-protected).
				 * We still need to write-protect here to prevent
				 * other threads from modifying these pages while
				 * we're in the process of copying and wiring
				 * the copied pages.
				 * Since the mapping is neither shared nor COWed,
				 * we only need to write-protect the PTEs for this
				 * mapping.
				 */
				vm_object_pmap_protect(orig_object,
				    orig_offset,
				    size,
				    map->pmap,
				    VM_MAP_PAGE_SIZE(map),
				    entry->vme_start,
				    entry->protection & ~VM_PROT_WRITE);
			}
		}
		if (VME_OBJECT(entry)->copy_strategy == MEMORY_OBJECT_COPY_SYMMETRIC) {
			/*
			 * Make the object COPY_DELAY to get a stable object
			 * to wire.
			 * That should avoid creating long shadow chains while
			 * wiring/unwiring the same range repeatedly.
			 * That also prevents part of the object from being
			 * wired while another part is "needs_copy", which
			 * could result in conflicting rules wrt copy-on-write.
			 */
			vm_object_t object;

			object = VME_OBJECT(entry);
			vm_object_lock(object);
			if (object->copy_strategy == MEMORY_OBJECT_COPY_SYMMETRIC) {
				assertf(vm_object_round_page(VME_OFFSET(entry) + size) - vm_object_trunc_page(VME_OFFSET(entry)) == object->vo_size,
				    "object %p size 0x%llx entry %p [0x%llx:0x%llx:0x%llx] size 0x%llx\n",
				    object, (uint64_t)object->vo_size,
				    entry,
				    (uint64_t)entry->vme_start,
				    (uint64_t)entry->vme_end,
				    (uint64_t)VME_OFFSET(entry),
				    (uint64_t)size);
				assertf(object->ref_count == 1,
				    "object %p ref_count %d\n",
				    object, object->ref_count);
				assertf(!entry->needs_copy,
				    "entry %p\n", entry);
				object->copy_strategy = MEMORY_OBJECT_COPY_DELAY;
				VM_OBJECT_SET_TRUE_SHARE(object, TRUE);
			}
			vm_object_unlock(object);
		}

		vm_map_clip_start(map, entry, s);
		vm_map_clip_end(map, entry, end);

		/* re-compute "e" */
		e = entry->vme_end;
		if (e > end) {
			e = end;
		}

		/*
		 * Check for holes and protection mismatch.
		 * Holes: Next entry should be contiguous unless this
		 *	  is the end of the region.
		 * Protection: Access requested must be allowed, unless
		 *	wiring is by protection class
		 */
		if ((entry->vme_end < end) &&
		    ((entry->vme_next == vm_map_to_entry(map)) ||
		    (entry->vme_next->vme_start > entry->vme_end))) {
			/* found a hole */
			rc = KERN_INVALID_ADDRESS;
			goto done;
		}
		if ((entry->protection & access_type) != access_type) {
			/* found a protection problem */
			rc = KERN_PROTECTION_FAILURE;
			goto done;
		}

		assert(entry->wired_count == 0 && entry->user_wired_count == 0);

		if ((rc = add_wire_counts(map, entry, user_wire)) != KERN_SUCCESS) {
			goto done;
		}

		entry->in_transition = TRUE;

		/*
		 * This entry might get split once we unlock the map.
		 * In vm_fault_wire(), we need the current range as
		 * defined by this entry.  In order for this to work
		 * along with a simultaneous clip operation, we make a
		 * temporary copy of this entry and use that for the
		 * wiring.  Note that the underlying objects do not
		 * change during a clip.
		 */
		tmp_entry = *entry;

		/*
		 * The in_transition state guarentees that the entry
		 * (or entries for this range, if split occured) will be
		 * there when the map lock is acquired for the second time.
		 */
		vm_map_unlock(map);

		if (!user_wire && cur_thread != THREAD_NULL) {
			interruptible_state = thread_interrupt_level(THREAD_UNINT);
		} else {
			interruptible_state = THREAD_UNINT;
		}

		if (map_pmap) {
			rc = vm_fault_wire(map,
			    &tmp_entry, caller_prot, tag, map_pmap, pmap_addr,
			    physpage_p);
		} else {
			rc = vm_fault_wire(map,
			    &tmp_entry, caller_prot, tag, map->pmap,
			    tmp_entry.vme_start,
			    physpage_p);
		}

		if (!user_wire && cur_thread != THREAD_NULL) {
			thread_interrupt_level(interruptible_state);
		}

		vm_map_lock(map);

		if (last_timestamp + 1 != map->timestamp) {
			/*
			 * Find the entry again.  It could have been clipped
			 * after we unlocked the map.
			 */
			if (!vm_map_lookup_entry(map, tmp_entry.vme_start,
			    &first_entry)) {
				panic("vm_map_wire: re-lookup failed");
			}

			entry = first_entry;
		}

		last_timestamp = map->timestamp;

		while ((entry != vm_map_to_entry(map)) &&
		    (entry->vme_start < tmp_entry.vme_end)) {
			assert(entry->in_transition);
			entry->in_transition = FALSE;
			if (entry->needs_wakeup) {
				entry->needs_wakeup = FALSE;
				need_wakeup = TRUE;
			}
			if (rc != KERN_SUCCESS) {       /* from vm_*_wire */
				subtract_wire_counts(map, entry, user_wire);
			}
			entry = entry->vme_next;
		}

		if (rc != KERN_SUCCESS) {               /* from vm_*_wire */
			goto done;
		}

		if ((entry != vm_map_to_entry(map)) && /* we still have entries in the map */
		    (tmp_entry.vme_end != end) &&    /* AND, we are not at the end of the requested range */
		    (entry->vme_start != tmp_entry.vme_end)) { /* AND, the next entry is not contiguous. */
			/* found a "new" hole */
			s = tmp_entry.vme_end;
			rc = KERN_INVALID_ADDRESS;
			goto done;
		}

		s = entry->vme_start;
	} /* end while loop through map entries */

done:
	if (rc == KERN_SUCCESS) {
		/* repair any damage we may have made to the VM map */
		vm_map_simplify_range(map, start, end);
	}

	vm_map_unlock(map);

	/*
	 * wake up anybody waiting on entries we wired.
	 */
	if (need_wakeup) {
		vm_map_entry_wakeup(map);
	}

	if (rc != KERN_SUCCESS) {
		/* undo what has been wired so far */
		vm_map_unwire_nested(map, start, s, user_wire,
		    map_pmap, pmap_addr);
		if (physpage_p) {
			*physpage_p = 0;
		}
	}

	return rc;
}

kern_return_t
vm_map_wire_external(
	vm_map_t                map,
	vm_map_offset_t         start,
	vm_map_offset_t         end,
	vm_prot_t               caller_prot,
	boolean_t               user_wire)
{
	kern_return_t   kret;

	kret = vm_map_wire_nested(map, start, end, caller_prot, vm_tag_bt(),
	    user_wire, (pmap_t)NULL, 0, NULL);
	return kret;
}

kern_return_t
vm_map_wire_kernel(
	vm_map_t                map,
	vm_map_offset_t         start,
	vm_map_offset_t         end,
	vm_prot_t               caller_prot,
	vm_tag_t                tag,
	boolean_t               user_wire)
{
	kern_return_t   kret;

	kret = vm_map_wire_nested(map, start, end, caller_prot, tag,
	    user_wire, (pmap_t)NULL, 0, NULL);
	return kret;
}

kern_return_t
vm_map_wire_and_extract_external(
	vm_map_t        map,
	vm_map_offset_t start,
	vm_prot_t       caller_prot,
	boolean_t       user_wire,
	ppnum_t         *physpage_p)
{
	kern_return_t   kret;

	kret = vm_map_wire_nested(map,
	    start,
	    start + VM_MAP_PAGE_SIZE(map),
	    caller_prot,
	    vm_tag_bt(),
	    user_wire,
	    (pmap_t)NULL,
	    0,
	    physpage_p);
	if (kret != KERN_SUCCESS &&
	    physpage_p != NULL) {
		*physpage_p = 0;
	}
	return kret;
}

/*
 *	vm_map_unwire:
 *
 *	Sets the pageability of the specified address range in the target
 *	as pageable.  Regions specified must have been wired previously.
 *
 *	The map must not be locked, but a reference must remain to the map
 *	throughout the call.
 *
 *	Kernel will panic on failures.  User unwire ignores holes and
 *	unwired and intransition entries to avoid losing memory by leaving
 *	it unwired.
 */
static kern_return_t
vm_map_unwire_nested(
	vm_map_t                map,
	vm_map_offset_t         start,
	vm_map_offset_t         end,
	boolean_t               user_wire,
	pmap_t                  map_pmap,
	vm_map_offset_t         pmap_addr)
{
	vm_map_entry_t          entry;
	struct vm_map_entry     *first_entry, tmp_entry;
	boolean_t               need_wakeup;
	boolean_t               main_map = FALSE;
	unsigned int            last_timestamp;

	vm_map_lock(map);
	if (map_pmap == NULL) {
		main_map = TRUE;
	}
	last_timestamp = map->timestamp;

	VM_MAP_RANGE_CHECK(map, start, end);
	assert(VM_MAP_PAGE_ALIGNED(start, VM_MAP_PAGE_MASK(map)));
	assert(VM_MAP_PAGE_ALIGNED(end, VM_MAP_PAGE_MASK(map)));

	if (start == end) {
		/* We unwired what the caller asked for: zero pages */
		vm_map_unlock(map);
		return KERN_SUCCESS;
	}

	if (__improbable(vm_map_range_overflows(map, start, end - start))) {
		vm_map_unlock(map);
		return KERN_INVALID_ADDRESS;
	}

	if (vm_map_lookup_entry(map, start, &first_entry)) {
		entry = first_entry;
		/*
		 * vm_map_clip_start will be done later.
		 * We don't want to unnest any nested sub maps here !
		 */
	} else {
		if (!user_wire) {
			panic("vm_map_unwire: start not found");
		}
		/*	Start address is not in map. */
		vm_map_unlock(map);
		return KERN_INVALID_ADDRESS;
	}

	if (entry->superpage_size) {
		/* superpages are always wired */
		vm_map_unlock(map);
		return KERN_INVALID_ADDRESS;
	}

	need_wakeup = FALSE;
	while ((entry != vm_map_to_entry(map)) && (entry->vme_start < end)) {
		if (entry->in_transition) {
			/*
			 * 1)
			 * Another thread is wiring down this entry. Note
			 * that if it is not for the other thread we would
			 * be unwiring an unwired entry.  This is not
			 * permitted.  If we wait, we will be unwiring memory
			 * we did not wire.
			 *
			 * 2)
			 * Another thread is unwiring this entry.  We did not
			 * have a reference to it, because if we did, this
			 * entry will not be getting unwired now.
			 */
			if (!user_wire) {
				/*
				 * XXX FBDP
				 * This could happen:  there could be some
				 * overlapping vslock/vsunlock operations
				 * going on.
				 * We should probably just wait and retry,
				 * but then we have to be careful that this
				 * entry could get "simplified" after
				 * "in_transition" gets unset and before
				 * we re-lookup the entry, so we would
				 * have to re-clip the entry to avoid
				 * re-unwiring what we have already unwired...
				 * See vm_map_wire_nested().
				 *
				 * Or we could just ignore "in_transition"
				 * here and proceed to decement the wired
				 * count(s) on this entry.  That should be fine
				 * as long as "wired_count" doesn't drop all
				 * the way to 0 (and we should panic if THAT
				 * happens).
				 */
				panic("vm_map_unwire: in_transition entry");
			}

			entry = entry->vme_next;
			continue;
		}

		if (entry->is_sub_map) {
			vm_map_offset_t sub_start;
			vm_map_offset_t sub_end;
			vm_map_offset_t local_end;
			pmap_t          pmap;

			vm_map_clip_start(map, entry, start);
			vm_map_clip_end(map, entry, end);

			sub_start = VME_OFFSET(entry);
			sub_end = entry->vme_end - entry->vme_start;
			sub_end += VME_OFFSET(entry);
			local_end = entry->vme_end;
			if (map_pmap == NULL) {
				if (entry->use_pmap) {
					pmap = VME_SUBMAP(entry)->pmap;
					pmap_addr = sub_start;
				} else {
					pmap = map->pmap;
					pmap_addr = start;
				}
				if (entry->wired_count == 0 ||
				    (user_wire && entry->user_wired_count == 0)) {
					if (!user_wire) {
						panic("vm_map_unwire: entry is unwired");
					}
					entry = entry->vme_next;
					continue;
				}

				/*
				 * Check for holes
				 * Holes: Next entry should be contiguous unless
				 * this is the end of the region.
				 */
				if (((entry->vme_end < end) &&
				    ((entry->vme_next == vm_map_to_entry(map)) ||
				    (entry->vme_next->vme_start
				    > entry->vme_end)))) {
					if (!user_wire) {
						panic("vm_map_unwire: non-contiguous region");
					}
/*
 *                                       entry = entry->vme_next;
 *                                       continue;
 */
				}

				subtract_wire_counts(map, entry, user_wire);

				if (entry->wired_count != 0) {
					entry = entry->vme_next;
					continue;
				}

				entry->in_transition = TRUE;
				tmp_entry = *entry;/* see comment in vm_map_wire() */

				/*
				 * We can unlock the map now. The in_transition state
				 * guarantees existance of the entry.
				 */
				vm_map_unlock(map);
				vm_map_unwire_nested(VME_SUBMAP(entry),
				    sub_start, sub_end, user_wire, pmap, pmap_addr);
				vm_map_lock(map);

				if (last_timestamp + 1 != map->timestamp) {
					/*
					 * Find the entry again.  It could have been
					 * clipped or deleted after we unlocked the map.
					 */
					if (!vm_map_lookup_entry(map,
					    tmp_entry.vme_start,
					    &first_entry)) {
						if (!user_wire) {
							panic("vm_map_unwire: re-lookup failed");
						}
						entry = first_entry->vme_next;
					} else {
						entry = first_entry;
					}
				}
				last_timestamp = map->timestamp;

				/*
				 * clear transition bit for all constituent entries
				 * that were in the original entry (saved in
				 * tmp_entry).  Also check for waiters.
				 */
				while ((entry != vm_map_to_entry(map)) &&
				    (entry->vme_start < tmp_entry.vme_end)) {
					assert(entry->in_transition);
					entry->in_transition = FALSE;
					if (entry->needs_wakeup) {
						entry->needs_wakeup = FALSE;
						need_wakeup = TRUE;
					}
					entry = entry->vme_next;
				}
				continue;
			} else {
				tmp_entry = *entry;
				vm_map_unlock(map);
				vm_map_unwire_nested(VME_SUBMAP(entry),
				    sub_start, sub_end, user_wire, map_pmap,
				    pmap_addr);
				vm_map_lock(map);

				if (last_timestamp + 1 != map->timestamp) {
					/*
					 * Find the entry again.  It could have been
					 * clipped or deleted after we unlocked the map.
					 */
					if (!vm_map_lookup_entry(map,
					    tmp_entry.vme_start,
					    &first_entry)) {
						if (!user_wire) {
							panic("vm_map_unwire: re-lookup failed");
						}
						entry = first_entry->vme_next;
					} else {
						entry = first_entry;
					}
				}
				last_timestamp = map->timestamp;
			}
		}


		if ((entry->wired_count == 0) ||
		    (user_wire && entry->user_wired_count == 0)) {
			if (!user_wire) {
				panic("vm_map_unwire: entry is unwired");
			}

			entry = entry->vme_next;
			continue;
		}

		assert(entry->wired_count > 0 &&
		    (!user_wire || entry->user_wired_count > 0));

		vm_map_clip_start(map, entry, start);
		vm_map_clip_end(map, entry, end);

		/*
		 * Check for holes
		 * Holes: Next entry should be contiguous unless
		 *	  this is the end of the region.
		 */
		if (((entry->vme_end < end) &&
		    ((entry->vme_next == vm_map_to_entry(map)) ||
		    (entry->vme_next->vme_start > entry->vme_end)))) {
			if (!user_wire) {
				panic("vm_map_unwire: non-contiguous region");
			}
			entry = entry->vme_next;
			continue;
		}

		subtract_wire_counts(map, entry, user_wire);

		if (entry->wired_count != 0) {
			entry = entry->vme_next;
			continue;
		}

		if (entry->zero_wired_pages) {
			entry->zero_wired_pages = FALSE;
		}

		entry->in_transition = TRUE;
		tmp_entry = *entry;     /* see comment in vm_map_wire() */

		/*
		 * We can unlock the map now. The in_transition state
		 * guarantees existance of the entry.
		 */
		vm_map_unlock(map);
		if (map_pmap) {
			vm_fault_unwire(map, &tmp_entry, FALSE, map_pmap,
			    pmap_addr, tmp_entry.vme_end);
		} else {
			vm_fault_unwire(map, &tmp_entry, FALSE, map->pmap,
			    tmp_entry.vme_start, tmp_entry.vme_end);
		}
		vm_map_lock(map);

		if (last_timestamp + 1 != map->timestamp) {
			/*
			 * Find the entry again.  It could have been clipped
			 * or deleted after we unlocked the map.
			 */
			if (!vm_map_lookup_entry(map, tmp_entry.vme_start,
			    &first_entry)) {
				if (!user_wire) {
					panic("vm_map_unwire: re-lookup failed");
				}
				entry = first_entry->vme_next;
			} else {
				entry = first_entry;
			}
		}
		last_timestamp = map->timestamp;

		/*
		 * clear transition bit for all constituent entries that
		 * were in the original entry (saved in tmp_entry).  Also
		 * check for waiters.
		 */
		while ((entry != vm_map_to_entry(map)) &&
		    (entry->vme_start < tmp_entry.vme_end)) {
			assert(entry->in_transition);
			entry->in_transition = FALSE;
			if (entry->needs_wakeup) {
				entry->needs_wakeup = FALSE;
				need_wakeup = TRUE;
			}
			entry = entry->vme_next;
		}
	}

	/*
	 * We might have fragmented the address space when we wired this
	 * range of addresses.  Attempt to re-coalesce these VM map entries
	 * with their neighbors now that they're no longer wired.
	 * Under some circumstances, address space fragmentation can
	 * prevent VM object shadow chain collapsing, which can cause
	 * swap space leaks.
	 */
	vm_map_simplify_range(map, start, end);

	vm_map_unlock(map);
	/*
	 * wake up anybody waiting on entries that we have unwired.
	 */
	if (need_wakeup) {
		vm_map_entry_wakeup(map);
	}
	return KERN_SUCCESS;
}

kern_return_t
vm_map_unwire(
	vm_map_t                map,
	vm_map_offset_t         start,
	vm_map_offset_t         end,
	boolean_t               user_wire)
{
	return vm_map_unwire_nested(map, start, end,
	           user_wire, (pmap_t)NULL, 0);
}


/*
 *	vm_map_entry_zap:	[ internal use only ]
 *
 *	Remove the entry from the target map
 *	and put it on a zap list.
 */
static void
vm_map_entry_zap(
	vm_map_t                map,
	vm_map_entry_t          entry,
	vm_map_zap_t            zap)
{
	vm_map_offset_t s, e;

	s = entry->vme_start;
	e = entry->vme_end;
	assert(VM_MAP_PAGE_ALIGNED(s, FOURK_PAGE_MASK));
	assert(VM_MAP_PAGE_ALIGNED(e, FOURK_PAGE_MASK));
	if (VM_MAP_PAGE_MASK(map) >= PAGE_MASK) {
		assert(page_aligned(s));
		assert(page_aligned(e));
	}
	if (entry->map_aligned == TRUE) {
		assert(VM_MAP_PAGE_ALIGNED(s, VM_MAP_PAGE_MASK(map)));
		assert(VM_MAP_PAGE_ALIGNED(e, VM_MAP_PAGE_MASK(map)));
	}
	assert(entry->wired_count == 0);
	assert(entry->user_wired_count == 0);
	assert(!entry->vme_permanent);

	vm_map_store_entry_unlink(map, entry, false);
	map->size -= e - s;

	vm_map_zap_append(zap, entry);
}

static void
vm_map_submap_pmap_clean(
	vm_map_t        map,
	vm_map_offset_t start,
	vm_map_offset_t end,
	vm_map_t        sub_map,
	vm_map_offset_t offset)
{
	vm_map_offset_t submap_start;
	vm_map_offset_t submap_end;
	vm_map_size_t   remove_size;
	vm_map_entry_t  entry;

	submap_end = offset + (end - start);
	submap_start = offset;

	vm_map_lock_read(sub_map);
	if (vm_map_lookup_entry(sub_map, offset, &entry)) {
		remove_size = (entry->vme_end - entry->vme_start);
		if (offset > entry->vme_start) {
			remove_size -= offset - entry->vme_start;
		}


		if (submap_end < entry->vme_end) {
			remove_size -=
			    entry->vme_end - submap_end;
		}
		if (entry->is_sub_map) {
			vm_map_submap_pmap_clean(
				sub_map,
				start,
				start + remove_size,
				VME_SUBMAP(entry),
				VME_OFFSET(entry));
		} else {
			if (map->mapped_in_other_pmaps &&
			    os_ref_get_count_raw(&map->map_refcnt) != 0 &&
			    VME_OBJECT(entry) != NULL) {
				vm_object_pmap_protect_options(
					VME_OBJECT(entry),
					(VME_OFFSET(entry) +
					offset -
					entry->vme_start),
					remove_size,
					PMAP_NULL,
					PAGE_SIZE,
					entry->vme_start,
					VM_PROT_NONE,
					PMAP_OPTIONS_REMOVE);
			} else {
				pmap_remove(map->pmap,
				    (addr64_t)start,
				    (addr64_t)(start + remove_size));
			}
		}
	}

	entry = entry->vme_next;

	while ((entry != vm_map_to_entry(sub_map))
	    && (entry->vme_start < submap_end)) {
		remove_size = (entry->vme_end - entry->vme_start);
		if (submap_end < entry->vme_end) {
			remove_size -= entry->vme_end - submap_end;
		}
		if (entry->is_sub_map) {
			vm_map_submap_pmap_clean(
				sub_map,
				(start + entry->vme_start) - offset,
				((start + entry->vme_start) - offset) + remove_size,
				VME_SUBMAP(entry),
				VME_OFFSET(entry));
		} else {
			if (map->mapped_in_other_pmaps &&
			    os_ref_get_count_raw(&map->map_refcnt) != 0 &&
			    VME_OBJECT(entry) != NULL) {
				vm_object_pmap_protect_options(
					VME_OBJECT(entry),
					VME_OFFSET(entry),
					remove_size,
					PMAP_NULL,
					PAGE_SIZE,
					entry->vme_start,
					VM_PROT_NONE,
					PMAP_OPTIONS_REMOVE);
			} else {
				pmap_remove(map->pmap,
				    (addr64_t)((start + entry->vme_start)
				    - offset),
				    (addr64_t)(((start + entry->vme_start)
				    - offset) + remove_size));
			}
		}
		entry = entry->vme_next;
	}
	vm_map_unlock_read(sub_map);
	return;
}

/*
 *     virt_memory_guard_ast:
 *
 *     Handle the AST callout for a virtual memory guard.
 *	   raise an EXC_GUARD exception and terminate the task
 *     if configured to do so.
 */
void
virt_memory_guard_ast(
	thread_t thread,
	mach_exception_data_type_t code,
	mach_exception_data_type_t subcode)
{
	task_t task = get_threadtask(thread);
	assert(task != kernel_task);
	assert(task == current_task());
	kern_return_t sync_exception_result;
	uint32_t behavior;

	behavior = task->task_exc_guard;

	/* Is delivery enabled */
	if ((behavior & TASK_EXC_GUARD_VM_DELIVER) == 0) {
		return;
	}

	/* If only once, make sure we're that once */
	while (behavior & TASK_EXC_GUARD_VM_ONCE) {
		uint32_t new_behavior = behavior & ~TASK_EXC_GUARD_VM_DELIVER;

		if (OSCompareAndSwap(behavior, new_behavior, &task->task_exc_guard)) {
			break;
		}
		behavior = task->task_exc_guard;
		if ((behavior & TASK_EXC_GUARD_VM_DELIVER) == 0) {
			return;
		}
	}

	const bool fatal = task->task_exc_guard & TASK_EXC_GUARD_VM_FATAL;
	/* Raise exception synchronously and see if handler claimed it */
	sync_exception_result = task_exception_notify(EXC_GUARD, code, subcode, fatal);

	if (fatal) {
		/*
		 * If Synchronous EXC_GUARD delivery was successful then
		 * kill the process and return, else kill the process
		 * and deliver the exception via EXC_CORPSE_NOTIFY.
		 */
		if (sync_exception_result == KERN_SUCCESS) {
			task_bsdtask_kill(current_task());
		} else {
			exit_with_guard_exception(current_proc(), code, subcode);
		}
	} else if (task->task_exc_guard & TASK_EXC_GUARD_VM_CORPSE) {
		/*
		 * If the synchronous EXC_GUARD delivery was not successful,
		 * raise a simulated crash.
		 */
		if (sync_exception_result != KERN_SUCCESS) {
			task_violated_guard(code, subcode, NULL, FALSE);
		}
	}
}

/*
 *     vm_map_guard_exception:
 *
 *     Generate a GUARD_TYPE_VIRTUAL_MEMORY EXC_GUARD exception.
 *
 *     Right now, we do this when we find nothing mapped, or a
 *     gap in the mapping when a user address space deallocate
 *     was requested. We report the address of the first gap found.
 */
static void
vm_map_guard_exception(
	vm_map_offset_t gap_start,
	unsigned reason)
{
	mach_exception_code_t code = 0;
	unsigned int guard_type = GUARD_TYPE_VIRT_MEMORY;
	unsigned int target = 0; /* should we pass in pid associated with map? */
	mach_exception_data_type_t subcode = (uint64_t)gap_start;
	boolean_t fatal = FALSE;

	task_t task = current_task_early();

	/* Can't deliver exceptions to a NULL task (early boot) or kernel task */
	if (task == NULL || task == kernel_task) {
		return;
	}

	EXC_GUARD_ENCODE_TYPE(code, guard_type);
	EXC_GUARD_ENCODE_FLAVOR(code, reason);
	EXC_GUARD_ENCODE_TARGET(code, target);

	if (task->task_exc_guard & TASK_EXC_GUARD_VM_FATAL) {
		fatal = TRUE;
	}
	thread_guard_violation(current_thread(), code, subcode, fatal);
}

static kern_return_t
vm_map_delete_submap_recurse(
	vm_map_t submap,
	vm_map_offset_t submap_start,
	vm_map_offset_t submap_end)
{
	vm_map_entry_t submap_entry;

	/*
	 * Verify that the submap does not contain any "permanent" entries
	 * within the specified range.
	 * We do not care about gaps.
	 */

	vm_map_lock(submap);

	if (!vm_map_lookup_entry(submap, submap_start, &submap_entry)) {
		submap_entry = submap_entry->vme_next;
	}

	for (;
	    submap_entry != vm_map_to_entry(submap) &&
	    submap_entry->vme_start < submap_end;
	    submap_entry = submap_entry->vme_next) {
		if (submap_entry->vme_permanent) {
			/* "permanent" entry -> fail */
			vm_map_unlock(submap);
			return KERN_PROTECTION_FAILURE;
		}
	}
	/* no "permanent" entries in the range -> success */
	vm_map_unlock(submap);
	return KERN_SUCCESS;
}

__abortlike
static void
__vm_map_delete_misaligned_panic(
	vm_map_t                map,
	vm_map_offset_t         start,
	vm_map_offset_t         end)
{
	panic("vm_map_delete(%p,0x%llx,0x%llx): start is not aligned to 0x%x",
	    map, (uint64_t)start, (uint64_t)end, VM_MAP_PAGE_SIZE(map));
}

__abortlike
static void
__vm_map_delete_failed_panic(
	vm_map_t                map,
	vm_map_offset_t         start,
	vm_map_offset_t         end,
	kern_return_t           kr)
{
	panic("vm_map_delete(%p,0x%llx,0x%llx): failed unexpected with %d",
	    map, (uint64_t)start, (uint64_t)end, kr);
}

__abortlike
static void
__vm_map_delete_gap_panic(
	vm_map_t                map,
	vm_map_offset_t         where,
	vm_map_offset_t         start,
	vm_map_offset_t         end)
{
	panic("vm_map_delete(%p,0x%llx,0x%llx): no map entry at 0x%llx",
	    map, (uint64_t)start, (uint64_t)end, (uint64_t)where);
}

__abortlike
static void
__vm_map_delete_permanent_panic(
	vm_map_t                map,
	vm_map_offset_t         start,
	vm_map_offset_t         end,
	vm_map_entry_t          entry)
{
	panic("vm_map_delete(%p,0x%llx,0x%llx): "
	    "Attempting to remove permanent VM map entry %p [0x%llx:0x%llx]",
	    map, (uint64_t)start, (uint64_t)end, entry,
	    (uint64_t)entry->vme_start,
	    (uint64_t)entry->vme_end);
}

__options_decl(vm_map_delete_state_t, uint32_t, {
	VMDS_NONE               = 0x0000,

	VMDS_FOUND_GAP          = 0x0001,
	VMDS_GAPS_OK            = 0x0002,

	VMDS_KERNEL_PMAP        = 0x0004,
	VMDS_NEEDS_LOOKUP       = 0x0008,
	VMDS_NEEDS_WAKEUP       = 0x0010,
	VMDS_KERNEL_KMEMPTR     = 0x0020
});

/*
 *	vm_map_delete:	[ internal use only ]
 *
 *	Deallocates the given address range from the target map.
 *	Removes all user wirings. Unwires one kernel wiring if
 *	VM_MAP_REMOVE_KUNWIRE is set.  Waits for kernel wirings to go
 *	away if VM_MAP_REMOVE_WAIT_FOR_KWIRE is set.  Sleeps
 *	interruptibly if VM_MAP_REMOVE_INTERRUPTIBLE is set.
 *
 *
 *	When the map is a kernel map, then any error in removing mappings
 *	will lead to a panic so that clients do not have to repeat the panic
 *	code at each call site.  If VM_MAP_REMOVE_INTERRUPTIBLE
 *	is also passed, then KERN_ABORTED will not lead to a panic.
 *
 *	This routine is called with map locked and leaves map locked.
 */
static kmem_return_t
vm_map_delete(
	vm_map_t                map,
	vm_map_offset_t         start,
	vm_map_offset_t         end,
	vmr_flags_t             flags,
	kmem_guard_t            guard,
	vm_map_zap_t            zap_list)
{
	vm_map_entry_t          entry, next;
	int                     interruptible;
	vm_map_offset_t         gap_start = 0;
	vm_map_offset_t         clear_in_transition_end = 0;
	__unused vm_map_offset_t save_start = start;
	__unused vm_map_offset_t save_end = end;
	vm_map_delete_state_t   state = VMDS_NONE;
	kmem_return_t           ret = { };
	vm_map_range_id_t       range_id = 0;
	struct kmem_page_meta  *meta = NULL;
	uint32_t                size_idx, slot_idx;
	struct mach_vm_range    slot;

	if (vm_map_pmap(map) == kernel_pmap) {
		state |= VMDS_KERNEL_PMAP;
		range_id = kmem_addr_get_range(start, end - start);
		if (kmem_is_ptr_range(range_id)) {
			state |= VMDS_KERNEL_KMEMPTR;
			slot_idx = kmem_addr_get_slot_idx(start, end, range_id, &meta,
			    &size_idx, &slot);
		}
	}

	if (map->terminated || os_ref_get_count_raw(&map->map_refcnt) == 0) {
		state |= VMDS_GAPS_OK;
	}

	if (map->corpse_source &&
	    !(flags & VM_MAP_REMOVE_TO_OVERWRITE) &&
	    !map->terminated) {
		/*
		 * The map is being used for corpses related diagnostics.
		 * So skip any entry removal to avoid perturbing the map state.
		 * The cleanup will happen in task_terminate_internal after the
		 * call to task_port_no_senders.
		 */
		goto out;
	}

	interruptible = (flags & VM_MAP_REMOVE_INTERRUPTIBLE) ?
	    THREAD_ABORTSAFE : THREAD_UNINT;

	if ((flags & VM_MAP_REMOVE_NO_MAP_ALIGN) == 0 &&
	    (start & VM_MAP_PAGE_MASK(map))) {
		__vm_map_delete_misaligned_panic(map, start, end);
	}

	if ((state & VMDS_GAPS_OK) == 0) {
		/*
		 * If the map isn't terminated then all deletions must have
		 * no gaps, and be within the [min, max) of the map.
		 *
		 * We got here without VM_MAP_RANGE_CHECK() being called,
		 * and hence must validate bounds manually.
		 *
		 * It is worth noting that because vm_deallocate() will
		 * round_page() the deallocation size, it's possible for "end"
		 * to be 0 here due to overflow. We hence must treat it as being
		 * beyond vm_map_max(map).
		 *
		 * Similarly, end < start means some wrap around happend,
		 * which should cause an error or panic.
		 */
		if (end == 0 || end > vm_map_max(map)) {
			state |= VMDS_FOUND_GAP;
			gap_start = vm_map_max(map);
			if (state & VMDS_KERNEL_PMAP) {
				__vm_map_delete_gap_panic(map,
				    gap_start, start, end);
			}
			goto out;
		}

		if (end < start) {
			if (state & VMDS_KERNEL_PMAP) {
				__vm_map_delete_gap_panic(map,
				    vm_map_max(map), start, end);
			}
			ret.kmr_return = KERN_INVALID_ARGUMENT;
			goto out;
		}

		if (start < vm_map_min(map)) {
			state |= VMDS_FOUND_GAP;
			gap_start = start;
			if (state & VMDS_KERNEL_PMAP) {
				__vm_map_delete_gap_panic(map,
				    gap_start, start, end);
			}
			goto out;
		}
	} else {
		/*
		 * If the map is terminated, we must accept start/end
		 * being beyond the boundaries of the map as this is
		 * how some of the mappings like commpage mappings
		 * can be destroyed (they're outside of those bounds).
		 *
		 * end < start is still something we can't cope with,
		 * so just bail.
		 */
		if (end < start) {
			goto out;
		}
	}


	/*
	 *	Find the start of the region.
	 *
	 *	If in a superpage, extend the range
	 *	to include the start of the mapping.
	 */
	while (vm_map_lookup_entry_or_next(map, start, &entry)) {
		if (entry->superpage_size && (start & ~SUPERPAGE_MASK)) {
			start = SUPERPAGE_ROUND_DOWN(start);
		} else {
			SAVE_HINT_MAP_WRITE(map, entry->vme_prev);
			break;
		}
	}

	if (entry->superpage_size) {
		end = SUPERPAGE_ROUND_UP(end);
	}

	/*
	 *	Step through all entries in this region
	 */
	for (vm_map_offset_t s = start; s < end;) {
		/*
		 * At this point, we have deleted all the memory entries
		 * in [start, s) and are proceeding with the [s, end) range.
		 *
		 * This loop might drop the map lock, and it is possible that
		 * some memory was already reallocated within [start, s)
		 * and we don't want to mess with those entries.
		 *
		 * Some of those entries could even have been re-assembled
		 * with an entry after "s" (in vm_map_simplify_entry()), so
		 * we may have to vm_map_clip_start() again.
		 *
		 * When clear_in_transition_end is set, the we had marked
		 * [start, clear_in_transition_end) as "in_transition"
		 * during a previous iteration and we need to clear it.
		 */

		/*
		 * Step 1: If needed (because we dropped locks),
		 *         lookup the entry again.
		 *
		 *         If we're coming back from unwiring (Step 5),
		 *         we also need to mark the entries as no longer
		 *         in transition after that.
		 */

		if (state & VMDS_NEEDS_LOOKUP) {
			state &= ~VMDS_NEEDS_LOOKUP;

			if (vm_map_lookup_entry_or_next(map, s, &entry)) {
				SAVE_HINT_MAP_WRITE(map, entry->vme_prev);
			}

			if (state & VMDS_KERNEL_KMEMPTR) {
				kmem_validate_slot(s, meta, size_idx, slot_idx);
			}
		}

		if (clear_in_transition_end) {
			for (vm_map_entry_t it = entry;
			    it != vm_map_to_entry(map) &&
			    it->vme_start < clear_in_transition_end;
			    it = it->vme_next) {
				assert(it->in_transition);
				it->in_transition = FALSE;
				if (it->needs_wakeup) {
					it->needs_wakeup = FALSE;
					state |= VMDS_NEEDS_WAKEUP;
				}
			}

			clear_in_transition_end = 0;
		}


		/*
		 * Step 2: Perform various policy checks
		 *         before we do _anything_ to this entry.
		 */

		if (entry == vm_map_to_entry(map) || s < entry->vme_start) {
			if (state & (VMDS_GAPS_OK | VMDS_FOUND_GAP)) {
				/*
				 * Either we found a gap already,
				 * or we are tearing down a map,
				 * keep going.
				 */
			} else if (state & VMDS_KERNEL_PMAP) {
				__vm_map_delete_gap_panic(map, s, start, end);
			} else if (s < end) {
				state |= VMDS_FOUND_GAP;
				gap_start = s;
			}

			if (entry == vm_map_to_entry(map) ||
			    end <= entry->vme_start) {
				break;
			}

			s = entry->vme_start;
		}

		if (state & VMDS_KERNEL_PMAP) {
			/*
			 * In the kernel map and its submaps,
			 * permanent entries never die, even
			 * if VM_MAP_REMOVE_IMMUTABLE is passed.
			 */
			if (entry->vme_permanent) {
				__vm_map_delete_permanent_panic(map, start, end, entry);
			}

			if (flags & VM_MAP_REMOVE_GUESS_SIZE) {
				end = entry->vme_end;
				flags &= ~VM_MAP_REMOVE_GUESS_SIZE;
			}

			/*
			 * In the kernel map and its submaps,
			 * the removal of an atomic/guarded entry is strict.
			 *
			 * An atomic entry is processed only if it was
			 * specifically targeted.
			 *
			 * We might have deleted non-atomic entries before
			 * we reach this this point however...
			 */
			kmem_entry_validate_guard(map, entry,
			    start, end - start, guard);
		}

		/*
		 * Step 2.1: handle "permanent" and "submap" entries
		 * *before* clipping to avoid triggering some unnecessary
		 * un-nesting of the shared region.
		 */
		if (entry->vme_permanent && entry->is_sub_map) {
//			printf("FBDP %s:%d permanent submap...\n", __FUNCTION__, __LINE__);
			/*
			 * Un-mapping a "permanent" mapping of a user-space
			 * submap is not allowed unless...
			 */
			if (flags & VM_MAP_REMOVE_IMMUTABLE) {
				/*
				 * a. explicitly requested by the kernel caller.
				 */
//				printf("FBDP %s:%d flags & REMOVE_IMMUTABLE\n", __FUNCTION__, __LINE__);
			} else if ((flags & VM_MAP_REMOVE_IMMUTABLE_CODE) &&
			    developer_mode_state()) {
				/*
				 * b. we're in "developer" mode (for
				 *    breakpoints, dtrace probes, ...).
				 */
//				printf("FBDP %s:%d flags & REMOVE_IMMUTABLE_CODE\n", __FUNCTION__, __LINE__);
			} else if (map->terminated) {
				/*
				 * c. this is the final address space cleanup.
				 */
//				printf("FBDP %s:%d map->terminated\n", __FUNCTION__, __LINE__);
			} else {
				vm_map_offset_t submap_start, submap_end;
				kern_return_t submap_kr;

				/*
				 * Check if there are any "permanent" mappings
				 * in this range in the submap.
				 */
				if (entry->in_transition) {
					/* can that even happen ? */
					goto in_transition;
				}
				/* compute the clipped range in the submap */
				submap_start = s - entry->vme_start;
				submap_start += VME_OFFSET(entry);
				submap_end = end - entry->vme_start;
				submap_end += VME_OFFSET(entry);
				submap_kr = vm_map_delete_submap_recurse(
					VME_SUBMAP(entry),
					submap_start,
					submap_end);
				if (submap_kr != KERN_SUCCESS) {
					/*
					 * There are some "permanent" mappings
					 * in the submap: we are not allowed
					 * to remove this range.
					 */
					printf("%d[%s] removing permanent submap entry "
					    "%p [0x%llx:0x%llx] prot 0x%x/0x%x -> KERN_PROT_FAILURE\n",
					    proc_selfpid(),
					    (get_bsdtask_info(current_task())
					    ? proc_name_address(get_bsdtask_info(current_task()))
					    : "?"), entry,
					    (uint64_t)entry->vme_start,
					    (uint64_t)entry->vme_end,
					    entry->protection,
					    entry->max_protection);
					DTRACE_VM6(vm_map_delete_permanent_deny_submap,
					    vm_map_entry_t, entry,
					    vm_map_offset_t, entry->vme_start,
					    vm_map_offset_t, entry->vme_end,
					    vm_prot_t, entry->protection,
					    vm_prot_t, entry->max_protection,
					    int, VME_ALIAS(entry));
					ret.kmr_return = KERN_PROTECTION_FAILURE;
					goto out;
				}
				/* no permanent mappings: proceed */
			}
		}

		/*
		 * Step 3: Perform any clipping needed.
		 *
		 *         After this, "entry" starts at "s", ends before "end"
		 */

		if (entry->vme_start < s) {
			if ((flags & VM_MAP_REMOVE_NO_MAP_ALIGN) &&
			    entry->map_aligned &&
			    !VM_MAP_PAGE_ALIGNED(s, VM_MAP_PAGE_MASK(map))) {
				/*
				 * The entry will no longer be map-aligned
				 * after clipping and the caller said it's OK.
				 */
				entry->map_aligned = FALSE;
			}
			vm_map_clip_start(map, entry, s);
			SAVE_HINT_MAP_WRITE(map, entry->vme_prev);
		}

		if (end < entry->vme_end) {
			if ((flags & VM_MAP_REMOVE_NO_MAP_ALIGN) &&
			    entry->map_aligned &&
			    !VM_MAP_PAGE_ALIGNED(end, VM_MAP_PAGE_MASK(map))) {
				/*
				 * The entry will no longer be map-aligned
				 * after clipping and the caller said it's OK.
				 */
				entry->map_aligned = FALSE;
			}
			vm_map_clip_end(map, entry, end);
		}

		if (entry->vme_permanent && entry->is_sub_map) {
			/*
			 * We already went through step 2.1 which did not deny
			 * the removal of this "permanent" and "is_sub_map"
			 * entry.
			 * Now that we've clipped what we actually want to
			 * delete, undo the "permanent" part to allow the
			 * removal to proceed.
			 */
			DTRACE_VM6(vm_map_delete_permanent_allow_submap,
			    vm_map_entry_t, entry,
			    vm_map_offset_t, entry->vme_start,
			    vm_map_offset_t, entry->vme_end,
			    vm_prot_t, entry->protection,
			    vm_prot_t, entry->max_protection,
			    int, VME_ALIAS(entry));
			entry->vme_permanent = false;
		}

		assert(s == entry->vme_start);
		assert(entry->vme_end <= end);


		/*
		 * Step 4: If the entry is in flux, wait for this to resolve.
		 */

		if (entry->in_transition) {
			wait_result_t wait_result;

in_transition:
			/*
			 * Another thread is wiring/unwiring this entry.
			 * Let the other thread know we are waiting.
			 */

			entry->needs_wakeup = TRUE;

			/*
			 * wake up anybody waiting on entries that we have
			 * already unwired/deleted.
			 */
			if (state & VMDS_NEEDS_WAKEUP) {
				vm_map_entry_wakeup(map);
				state &= ~VMDS_NEEDS_WAKEUP;
			}

			wait_result = vm_map_entry_wait(map, interruptible);

			if (interruptible &&
			    wait_result == THREAD_INTERRUPTED) {
				/*
				 * We do not clear the needs_wakeup flag,
				 * since we cannot tell if we were the only one.
				 */
				ret.kmr_return = KERN_ABORTED;
				return ret;
			}

			/*
			 * The entry could have been clipped or it
			 * may not exist anymore.  Look it up again.
			 */
			state |= VMDS_NEEDS_LOOKUP;
			continue;
		}


		/*
		 * Step 5: Handle wiring
		 */

		if (entry->wired_count) {
			struct vm_map_entry tmp_entry;
			boolean_t           user_wire;
			unsigned int        last_timestamp;

			user_wire = entry->user_wired_count > 0;

			/*
			 *      Remove a kernel wiring if requested
			 */
			if (flags & VM_MAP_REMOVE_KUNWIRE) {
				entry->wired_count--;
				vme_btref_consider_and_put(entry);
			}

			/*
			 *	Remove all user wirings for proper accounting
			 */
			while (entry->user_wired_count) {
				subtract_wire_counts(map, entry, user_wire);
			}

			/*
			 * All our DMA I/O operations in IOKit are currently
			 * done by wiring through the map entries of the task
			 * requesting the I/O.
			 *
			 * Because of this, we must always wait for kernel wirings
			 * to go away on the entries before deleting them.
			 *
			 * Any caller who wants to actually remove a kernel wiring
			 * should explicitly set the VM_MAP_REMOVE_KUNWIRE flag to
			 * properly remove one wiring instead of blasting through
			 * them all.
			 */
			if (entry->wired_count != 0) {
				assert(map != kernel_map);
				/*
				 * Cannot continue.  Typical case is when
				 * a user thread has physical io pending on
				 * on this page.  Either wait for the
				 * kernel wiring to go away or return an
				 * error.
				 */
				wait_result_t wait_result;

				entry->needs_wakeup = TRUE;
				wait_result = vm_map_entry_wait(map,
				    interruptible);

				if (interruptible &&
				    wait_result == THREAD_INTERRUPTED) {
					/*
					 * We do not clear the
					 * needs_wakeup flag, since we
					 * cannot tell if we were the
					 * only one.
					 */
					ret.kmr_return = KERN_ABORTED;
					return ret;
				}


				/*
				 * The entry could have been clipped or
				 * it may not exist anymore.  Look it
				 * up again.
				 */
				state |= VMDS_NEEDS_LOOKUP;
				continue;
			}

			/*
			 * We can unlock the map now.
			 *
			 * The entry might be split once we unlock the map,
			 * but we need the range as defined by this entry
			 * to be stable. So we must make a local copy.
			 *
			 * The underlying objects do not change during clips,
			 * and the in_transition state guarentees existence
			 * of the entry.
			 */
			last_timestamp = map->timestamp;
			entry->in_transition = TRUE;
			tmp_entry = *entry;
			vm_map_unlock(map);

			if (tmp_entry.is_sub_map) {
				vm_map_t sub_map;
				vm_map_offset_t sub_start, sub_end;
				pmap_t pmap;
				vm_map_offset_t pmap_addr;


				sub_map = VME_SUBMAP(&tmp_entry);
				sub_start = VME_OFFSET(&tmp_entry);
				sub_end = sub_start + (tmp_entry.vme_end -
				    tmp_entry.vme_start);
				if (tmp_entry.use_pmap) {
					pmap = sub_map->pmap;
					pmap_addr = tmp_entry.vme_start;
				} else {
					pmap = map->pmap;
					pmap_addr = tmp_entry.vme_start;
				}
				(void) vm_map_unwire_nested(sub_map,
				    sub_start, sub_end,
				    user_wire,
				    pmap, pmap_addr);
			} else {
				vm_map_offset_t entry_end = tmp_entry.vme_end;
				vm_map_offset_t max_end;

				if (flags & VM_MAP_REMOVE_NOKUNWIRE_LAST) {
					max_end = end - VM_MAP_PAGE_SIZE(map);
					if (entry_end > max_end) {
						entry_end = max_end;
					}
				}

				if (tmp_entry.vme_kernel_object) {
					pmap_protect_options(
						map->pmap,
						tmp_entry.vme_start,
						entry_end,
						VM_PROT_NONE,
						PMAP_OPTIONS_REMOVE,
						NULL);
				}
				vm_fault_unwire(map, &tmp_entry,
				    tmp_entry.vme_kernel_object, map->pmap,
				    tmp_entry.vme_start, entry_end);
			}

			vm_map_lock(map);

			/*
			 * Unwiring happened, we can now go back to deleting
			 * them (after we clear the in_transition bit for the range).
			 */
			if (last_timestamp + 1 != map->timestamp) {
				state |= VMDS_NEEDS_LOOKUP;
			}
			clear_in_transition_end = tmp_entry.vme_end;
			continue;
		}

		assert(entry->wired_count == 0);
		assert(entry->user_wired_count == 0);


		/*
		 * Step 6: Entry is unwired and ready for us to delete !
		 */

		if (!entry->vme_permanent) {
			/*
			 * Typical case: the entry really shouldn't be permanent
			 */
		} else if ((flags & VM_MAP_REMOVE_IMMUTABLE_CODE) &&
		    (entry->protection & VM_PROT_EXECUTE) &&
		    developer_mode_state()) {
			/*
			 * Allow debuggers to undo executable mappings
			 * when developer mode is on.
			 */
#if 0
			printf("FBDP %d[%s] removing permanent executable entry "
			    "%p [0x%llx:0x%llx] prot 0x%x/0x%x\n",
			    proc_selfpid(),
			    (current_task()->bsd_info
			    ? proc_name_address(current_task()->bsd_info)
			    : "?"), entry,
			    (uint64_t)entry->vme_start,
			    (uint64_t)entry->vme_end,
			    entry->protection,
			    entry->max_protection);
#endif
			entry->vme_permanent = FALSE;
		} else if ((flags & VM_MAP_REMOVE_IMMUTABLE) || map->terminated) {
#if 0
			printf("FBDP %d[%s] removing permanent entry "
			    "%p [0x%llx:0x%llx] prot 0x%x/0x%x\n",
			    proc_selfpid(),
			    (current_task()->bsd_info
			    ? proc_name_address(current_task()->bsd_info)
			    : "?"), entry,
			    (uint64_t)entry->vme_start,
			    (uint64_t)entry->vme_end,
			    entry->protection,
			    entry->max_protection);
#endif
			entry->vme_permanent = FALSE;
#if CODE_SIGNING_MONITOR
		} else if ((entry->protection & VM_PROT_EXECUTE) && !csm_enabled()) {
			entry->vme_permanent = FALSE;

			printf("%d[%s] %s(0x%llx,0x%llx): "
			    "code signing monitor disabled, allowing for permanent executable entry [0x%llx:0x%llx] "
			    "prot 0x%x/0x%x\n",
			    proc_selfpid(),
			    (get_bsdtask_info(current_task())
			    ? proc_name_address(get_bsdtask_info(current_task()))
			    : "?"),
			    __FUNCTION__,
			    (uint64_t)start,
			    (uint64_t)end,
			    (uint64_t)entry->vme_start,
			    (uint64_t)entry->vme_end,
			    entry->protection,
			    entry->max_protection);
#endif
		} else {
			DTRACE_VM6(vm_map_delete_permanent,
			    vm_map_entry_t, entry,
			    vm_map_offset_t, entry->vme_start,
			    vm_map_offset_t, entry->vme_end,
			    vm_prot_t, entry->protection,
			    vm_prot_t, entry->max_protection,
			    int, VME_ALIAS(entry));
		}

		if (entry->is_sub_map) {
			assertf(VM_MAP_PAGE_SHIFT(VME_SUBMAP(entry)) >= VM_MAP_PAGE_SHIFT(map),
			    "map %p (%d) entry %p submap %p (%d)\n",
			    map, VM_MAP_PAGE_SHIFT(map), entry,
			    VME_SUBMAP(entry),
			    VM_MAP_PAGE_SHIFT(VME_SUBMAP(entry)));
			if (entry->use_pmap) {
#ifndef NO_NESTED_PMAP
				int pmap_flags;

				if (map->terminated) {
					/*
					 * This is the final cleanup of the
					 * address space being terminated.
					 * No new mappings are expected and
					 * we don't really need to unnest the
					 * shared region (and lose the "global"
					 * pmap mappings, if applicable).
					 *
					 * Tell the pmap layer that we're
					 * "clean" wrt nesting.
					 */
					pmap_flags = PMAP_UNNEST_CLEAN;
				} else {
					/*
					 * We're unmapping part of the nested
					 * shared region, so we can't keep the
					 * nested pmap.
					 */
					pmap_flags = 0;
				}
				pmap_unnest_options(
					map->pmap,
					(addr64_t)entry->vme_start,
					entry->vme_end - entry->vme_start,
					pmap_flags);
#endif  /* NO_NESTED_PMAP */
				if (map->mapped_in_other_pmaps &&
				    os_ref_get_count_raw(&map->map_refcnt) != 0) {
					/* clean up parent map/maps */
					vm_map_submap_pmap_clean(
						map, entry->vme_start,
						entry->vme_end,
						VME_SUBMAP(entry),
						VME_OFFSET(entry));
				}
			} else {
				vm_map_submap_pmap_clean(
					map, entry->vme_start, entry->vme_end,
					VME_SUBMAP(entry),
					VME_OFFSET(entry));
			}
		} else if (entry->vme_kernel_object ||
		    VME_OBJECT(entry) == compressor_object) {
			/*
			 * nothing to do
			 */
		} else if (map->mapped_in_other_pmaps &&
		    os_ref_get_count_raw(&map->map_refcnt) != 0) {
			vm_object_pmap_protect_options(
				VME_OBJECT(entry), VME_OFFSET(entry),
				entry->vme_end - entry->vme_start,
				PMAP_NULL,
				PAGE_SIZE,
				entry->vme_start,
				VM_PROT_NONE,
				PMAP_OPTIONS_REMOVE);
		} else if ((VME_OBJECT(entry) != VM_OBJECT_NULL) ||
		    (state & VMDS_KERNEL_PMAP)) {
			/* Remove translations associated
			 * with this range unless the entry
			 * does not have an object, or
			 * it's the kernel map or a descendant
			 * since the platform could potentially
			 * create "backdoor" mappings invisible
			 * to the VM. It is expected that
			 * objectless, non-kernel ranges
			 * do not have such VM invisible
			 * translations.
			 */
			pmap_remove_options(map->pmap,
			    (addr64_t)entry->vme_start,
			    (addr64_t)entry->vme_end,
			    PMAP_OPTIONS_REMOVE);
		}

#if DEBUG
		/*
		 * All pmap mappings for this map entry must have been
		 * cleared by now.
		 */
		assert(pmap_is_empty(map->pmap,
		    entry->vme_start,
		    entry->vme_end));
#endif /* DEBUG */

		if (entry->iokit_acct) {
			/* alternate accounting */
			DTRACE_VM4(vm_map_iokit_unmapped_region,
			    vm_map_t, map,
			    vm_map_offset_t, entry->vme_start,
			    vm_map_offset_t, entry->vme_end,
			    int, VME_ALIAS(entry));
			vm_map_iokit_unmapped_region(map,
			    (entry->vme_end -
			    entry->vme_start));
			entry->iokit_acct = FALSE;
			entry->use_pmap = FALSE;
		}

		/* move "s" forward */
		s    = entry->vme_end;
		next = entry->vme_next;
		if (!entry->map_aligned) {
			vm_map_offset_t rounded_s;

			/*
			 * Skip artificial gap due to mis-aligned entry
			 * on devices with a page size smaller than the
			 * map's page size (i.e. 16k task on a 4k device).
			 */
			rounded_s = VM_MAP_ROUND_PAGE(s, VM_MAP_PAGE_MASK(map));
			if (next == vm_map_to_entry(map)) {
				s = rounded_s;
			} else if (s < rounded_s) {
				s = MIN(rounded_s, next->vme_start);
			}
		}
		ret.kmr_size += s - entry->vme_start;

		if (entry->vme_permanent) {
			/*
			 * A permanent entry can not be removed, so leave it
			 * in place but remove all access permissions.
			 */
			if (!entry->csm_associated) {
				printf("%s:%d %d[%s] map %p entry %p [ 0x%llx - 0x%llx ] submap %d prot 0x%x/0x%x -> 0/0\n",
				    __FUNCTION__, __LINE__,
				    proc_selfpid(),
				    (get_bsdtask_info(current_task())
				    ? proc_name_address(get_bsdtask_info(current_task()))
				    : "?"),
				    map,
				    entry,
				    (uint64_t)entry->vme_start,
				    (uint64_t)entry->vme_end,
				    entry->is_sub_map,
				    entry->protection,
				    entry->max_protection);
			}
			DTRACE_VM6(vm_map_delete_permanent_prot_none,
			    vm_map_entry_t, entry,
			    vm_map_offset_t, entry->vme_start,
			    vm_map_offset_t, entry->vme_end,
			    vm_prot_t, entry->protection,
			    vm_prot_t, entry->max_protection,
			    int, VME_ALIAS(entry));
			entry->protection = VM_PROT_NONE;
			entry->max_protection = VM_PROT_NONE;
		} else {
			vm_map_entry_zap(map, entry, zap_list);
		}

		entry = next;
		next  = VM_MAP_ENTRY_NULL;

		if ((flags & VM_MAP_REMOVE_NO_YIELD) == 0 && s < end) {
			unsigned int last_timestamp = map->timestamp++;

			if (lck_rw_lock_yield_exclusive(&map->lock,
			    LCK_RW_YIELD_ANY_WAITER)) {
				if (last_timestamp != map->timestamp + 1) {
					state |= VMDS_NEEDS_LOOKUP;
				}
			} else {
				/* we didn't yield, undo our change */
				map->timestamp--;
			}
		}
	}

	if (map->wait_for_space) {
		thread_wakeup((event_t) map);
	}

	if (state & VMDS_NEEDS_WAKEUP) {
		vm_map_entry_wakeup(map);
	}

out:
	if ((state & VMDS_KERNEL_PMAP) && ret.kmr_return) {
		__vm_map_delete_failed_panic(map, start, end, ret.kmr_return);
	}

	if (state & VMDS_KERNEL_KMEMPTR) {
		kmem_free_space(start, end, range_id, &slot);
	}

	if (state & VMDS_FOUND_GAP) {
		DTRACE_VM3(kern_vm_deallocate_gap,
		    vm_map_offset_t, gap_start,
		    vm_map_offset_t, save_start,
		    vm_map_offset_t, save_end);
		if (flags & VM_MAP_REMOVE_GAPS_FAIL) {
			ret.kmr_return = KERN_INVALID_VALUE;
		} else {
			vm_map_guard_exception(gap_start, kGUARD_EXC_DEALLOC_GAP);
		}
	}

	return ret;
}

kmem_return_t
vm_map_remove_and_unlock(
	vm_map_t        map,
	vm_map_offset_t start,
	vm_map_offset_t end,
	vmr_flags_t     flags,
	kmem_guard_t    guard)
{
	kmem_return_t ret;
	VM_MAP_ZAP_DECLARE(zap);

	ret = vm_map_delete(map, start, end, flags, guard, &zap);
	vm_map_unlock(map);

	vm_map_zap_dispose(&zap);

	return ret;
}

/*
 *	vm_map_remove_guard:
 *
 *	Remove the given address range from the target map.
 *	This is the exported form of vm_map_delete.
 */
kmem_return_t
vm_map_remove_guard(
	vm_map_t        map,
	vm_map_offset_t start,
	vm_map_offset_t end,
	vmr_flags_t     flags,
	kmem_guard_t    guard)
{
	vm_map_lock(map);
	return vm_map_remove_and_unlock(map, start, end, flags, guard);
}

/*
 *	vm_map_terminate:
 *
 *	Clean out a task's map.
 */
kern_return_t
vm_map_terminate(
	vm_map_t        map)
{
	vm_map_lock(map);
	map->terminated = TRUE;
	vm_map_disable_hole_optimization(map);
	(void)vm_map_remove_and_unlock(map, map->min_offset, map->max_offset,
	    VM_MAP_REMOVE_NO_FLAGS, KMEM_GUARD_NONE);
	return KERN_SUCCESS;
}

/*
 *	Routine:	vm_map_copy_allocate
 *
 *	Description:
 *		Allocates and initializes a map copy object.
 */
static vm_map_copy_t
vm_map_copy_allocate(uint16_t type)
{
	vm_map_copy_t new_copy;

	new_copy = zalloc_id(ZONE_ID_VM_MAP_COPY, Z_WAITOK | Z_ZERO);
	new_copy->type = type;
	if (type == VM_MAP_COPY_ENTRY_LIST) {
		new_copy->c_u.hdr.rb_head_store.rbh_root = (void*)(int)SKIP_RB_TREE;
		vm_map_store_init(&new_copy->cpy_hdr);
	}
	return new_copy;
}

/*
 *	Routine:	vm_map_copy_discard
 *
 *	Description:
 *		Dispose of a map copy object (returned by
 *		vm_map_copyin).
 */
void
vm_map_copy_discard(
	vm_map_copy_t   copy)
{
	if (copy == VM_MAP_COPY_NULL) {
		return;
	}

	/*
	 * Assert that the vm_map_copy is coming from the right
	 * zone and hasn't been forged
	 */
	vm_map_copy_require(copy);

	switch (copy->type) {
	case VM_MAP_COPY_ENTRY_LIST:
		while (vm_map_copy_first_entry(copy) !=
		    vm_map_copy_to_entry(copy)) {
			vm_map_entry_t  entry = vm_map_copy_first_entry(copy);

			vm_map_copy_entry_unlink(copy, entry);
			if (entry->is_sub_map) {
				vm_map_deallocate(VME_SUBMAP(entry));
			} else {
				vm_object_deallocate(VME_OBJECT(entry));
			}
			vm_map_copy_entry_dispose(entry);
		}
		break;
	case VM_MAP_COPY_KERNEL_BUFFER:

		/*
		 * The vm_map_copy_t and possibly the data buffer were
		 * allocated by a single call to kalloc_data(), i.e. the
		 * vm_map_copy_t was not allocated out of the zone.
		 */
		if (copy->size > msg_ool_size_small || copy->offset) {
			panic("Invalid vm_map_copy_t sz:%lld, ofst:%lld",
			    (long long)copy->size, (long long)copy->offset);
		}
		kfree_data(copy->cpy_kdata, copy->size);
	}
	zfree_id(ZONE_ID_VM_MAP_COPY, copy);
}

#if XNU_PLATFORM_MacOSX

/*
 *	Routine:	vm_map_copy_copy
 *
 *	Description:
 *			Move the information in a map copy object to
 *			a new map copy object, leaving the old one
 *			empty.
 *
 *			This is used by kernel routines that need
 *			to look at out-of-line data (in copyin form)
 *			before deciding whether to return SUCCESS.
 *			If the routine returns FAILURE, the original
 *			copy object will be deallocated; therefore,
 *			these routines must make a copy of the copy
 *			object and leave the original empty so that
 *			deallocation will not fail.
 */
vm_map_copy_t
vm_map_copy_copy(
	vm_map_copy_t   copy)
{
	vm_map_copy_t   new_copy;

	if (copy == VM_MAP_COPY_NULL) {
		return VM_MAP_COPY_NULL;
	}

	/*
	 * Assert that the vm_map_copy is coming from the right
	 * zone and hasn't been forged
	 */
	vm_map_copy_require(copy);

	/*
	 * Allocate a new copy object, and copy the information
	 * from the old one into it.
	 */

	new_copy = zalloc_id(ZONE_ID_VM_MAP_COPY, Z_WAITOK | Z_ZERO | Z_NOFAIL);
	memcpy((void *) new_copy, (void *) copy, sizeof(struct vm_map_copy));
#if __has_feature(ptrauth_calls)
	if (copy->type == VM_MAP_COPY_KERNEL_BUFFER) {
		new_copy->cpy_kdata = copy->cpy_kdata;
	}
#endif

	if (copy->type == VM_MAP_COPY_ENTRY_LIST) {
		/*
		 * The links in the entry chain must be
		 * changed to point to the new copy object.
		 */
		vm_map_copy_first_entry(copy)->vme_prev
		        = vm_map_copy_to_entry(new_copy);
		vm_map_copy_last_entry(copy)->vme_next
		        = vm_map_copy_to_entry(new_copy);
	}

	/*
	 * Change the old copy object into one that contains
	 * nothing to be deallocated.
	 */
	bzero(copy, sizeof(struct vm_map_copy));
	copy->type = VM_MAP_COPY_KERNEL_BUFFER;

	/*
	 * Return the new object.
	 */
	return new_copy;
}

#endif /* XNU_PLATFORM_MacOSX */

static boolean_t
vm_map_entry_is_overwritable(
	vm_map_t        dst_map __unused,
	vm_map_entry_t  entry)
{
	if (!(entry->protection & VM_PROT_WRITE)) {
		/* can't overwrite if not writable */
		return FALSE;
	}
#if !__x86_64__
	if (entry->used_for_jit &&
	    vm_map_cs_enforcement(dst_map) &&
	    !dst_map->cs_debugged) {
		/*
		 * Can't overwrite a JIT region while cs_enforced
		 * and not cs_debugged.
		 */
		return FALSE;
	}

#if __arm64e__
	/* Do not allow overwrite HW assisted TPRO entries */
	if (entry->used_for_tpro) {
		return FALSE;
	}
#endif /* __arm64e__ */

	if (entry->vme_permanent) {
		if (entry->is_sub_map) {
			/*
			 * We can't tell if the submap contains "permanent"
			 * entries within the range targeted by the caller.
			 * The caller will have to check for that with
			 * vm_map_overwrite_submap_recurse() for example.
			 */
		} else {
			/*
			 * Do not allow overwriting of a "permanent"
			 * entry.
			 */
			DTRACE_VM6(vm_map_delete_permanent_deny_overwrite,
			    vm_map_entry_t, entry,
			    vm_map_offset_t, entry->vme_start,
			    vm_map_offset_t, entry->vme_end,
			    vm_prot_t, entry->protection,
			    vm_prot_t, entry->max_protection,
			    int, VME_ALIAS(entry));
			return FALSE;
		}
	}
#endif /* !__x86_64__ */
	return TRUE;
}

static kern_return_t
vm_map_overwrite_submap_recurse(
	vm_map_t        dst_map,
	vm_map_offset_t dst_addr,
	vm_map_size_t   dst_size)
{
	vm_map_offset_t dst_end;
	vm_map_entry_t  tmp_entry;
	vm_map_entry_t  entry;
	kern_return_t   result;
	boolean_t       encountered_sub_map = FALSE;



	/*
	 *	Verify that the destination is all writeable
	 *	initially.  We have to trunc the destination
	 *	address and round the copy size or we'll end up
	 *	splitting entries in strange ways.
	 */

	dst_end = vm_map_round_page(dst_addr + dst_size,
	    VM_MAP_PAGE_MASK(dst_map));
	vm_map_lock(dst_map);

start_pass_1:
	if (!vm_map_lookup_entry(dst_map, dst_addr, &tmp_entry)) {
		vm_map_unlock(dst_map);
		return KERN_INVALID_ADDRESS;
	}

	vm_map_clip_start(dst_map,
	    tmp_entry,
	    vm_map_trunc_page(dst_addr,
	    VM_MAP_PAGE_MASK(dst_map)));
	if (tmp_entry->is_sub_map) {
		/* clipping did unnest if needed */
		assert(!tmp_entry->use_pmap);
	}

	for (entry = tmp_entry;;) {
		vm_map_entry_t  next;

		next = entry->vme_next;
		while (entry->is_sub_map) {
			vm_map_offset_t sub_start;
			vm_map_offset_t sub_end;
			vm_map_offset_t local_end;

			if (entry->in_transition) {
				/*
				 * Say that we are waiting, and wait for entry.
				 */
				entry->needs_wakeup = TRUE;
				vm_map_entry_wait(dst_map, THREAD_UNINT);

				goto start_pass_1;
			}

			encountered_sub_map = TRUE;
			sub_start = VME_OFFSET(entry);

			if (entry->vme_end < dst_end) {
				sub_end = entry->vme_end;
			} else {
				sub_end = dst_end;
			}
			sub_end -= entry->vme_start;
			sub_end += VME_OFFSET(entry);
			local_end = entry->vme_end;
			vm_map_unlock(dst_map);

			result = vm_map_overwrite_submap_recurse(
				VME_SUBMAP(entry),
				sub_start,
				sub_end - sub_start);

			if (result != KERN_SUCCESS) {
				return result;
			}
			if (dst_end <= entry->vme_end) {
				return KERN_SUCCESS;
			}
			vm_map_lock(dst_map);
			if (!vm_map_lookup_entry(dst_map, local_end,
			    &tmp_entry)) {
				vm_map_unlock(dst_map);
				return KERN_INVALID_ADDRESS;
			}
			entry = tmp_entry;
			next = entry->vme_next;
		}

		if (!(entry->protection & VM_PROT_WRITE)) {
			vm_map_unlock(dst_map);
			return KERN_PROTECTION_FAILURE;
		}

		if (!vm_map_entry_is_overwritable(dst_map, entry)) {
			vm_map_unlock(dst_map);
			return KERN_PROTECTION_FAILURE;
		}

		/*
		 *	If the entry is in transition, we must wait
		 *	for it to exit that state.  Anything could happen
		 *	when we unlock the map, so start over.
		 */
		if (entry->in_transition) {
			/*
			 * Say that we are waiting, and wait for entry.
			 */
			entry->needs_wakeup = TRUE;
			vm_map_entry_wait(dst_map, THREAD_UNINT);

			goto start_pass_1;
		}

/*
 *		our range is contained completely within this map entry
 */
		if (dst_end <= entry->vme_end) {
			vm_map_unlock(dst_map);
			return KERN_SUCCESS;
		}
/*
 *		check that range specified is contiguous region
 */
		if ((next == vm_map_to_entry(dst_map)) ||
		    (next->vme_start != entry->vme_end)) {
			vm_map_unlock(dst_map);
			return KERN_INVALID_ADDRESS;
		}

		/*
		 *	Check for permanent objects in the destination.
		 */
		if ((VME_OBJECT(entry) != VM_OBJECT_NULL) &&
		    ((!VME_OBJECT(entry)->internal) ||
		    (VME_OBJECT(entry)->true_share))) {
			if (encountered_sub_map) {
				vm_map_unlock(dst_map);
				return KERN_FAILURE;
			}
		}


		entry = next;
	}/* for */
	vm_map_unlock(dst_map);
	return KERN_SUCCESS;
}

/*
 *	Routine:	vm_map_copy_overwrite
 *
 *	Description:
 *		Copy the memory described by the map copy
 *		object (copy; returned by vm_map_copyin) onto
 *		the specified destination region (dst_map, dst_addr).
 *		The destination must be writeable.
 *
 *		Unlike vm_map_copyout, this routine actually
 *		writes over previously-mapped memory.  If the
 *		previous mapping was to a permanent (user-supplied)
 *		memory object, it is preserved.
 *
 *		The attributes (protection and inheritance) of the
 *		destination region are preserved.
 *
 *		If successful, consumes the copy object.
 *		Otherwise, the caller is responsible for it.
 *
 *	Implementation notes:
 *		To overwrite aligned temporary virtual memory, it is
 *		sufficient to remove the previous mapping and insert
 *		the new copy.  This replacement is done either on
 *		the whole region (if no permanent virtual memory
 *		objects are embedded in the destination region) or
 *		in individual map entries.
 *
 *		To overwrite permanent virtual memory , it is necessary
 *		to copy each page, as the external memory management
 *		interface currently does not provide any optimizations.
 *
 *		Unaligned memory also has to be copied.  It is possible
 *		to use 'vm_trickery' to copy the aligned data.  This is
 *		not done but not hard to implement.
 *
 *		Once a page of permanent memory has been overwritten,
 *		it is impossible to interrupt this function; otherwise,
 *		the call would be neither atomic nor location-independent.
 *		The kernel-state portion of a user thread must be
 *		interruptible.
 *
 *		It may be expensive to forward all requests that might
 *		overwrite permanent memory (vm_write, vm_copy) to
 *		uninterruptible kernel threads.  This routine may be
 *		called by interruptible threads; however, success is
 *		not guaranteed -- if the request cannot be performed
 *		atomically and interruptibly, an error indication is
 *		returned.
 *
 *		Callers of this function must call vm_map_copy_require on
 *		previously created vm_map_copy_t or pass a newly created
 *		one to ensure that it hasn't been forged.
 */
static kern_return_t
vm_map_copy_overwrite_nested(
	vm_map_t                dst_map,
	vm_map_address_t        dst_addr,
	vm_map_copy_t           copy,
	boolean_t               interruptible,
	pmap_t                  pmap,
	boolean_t               discard_on_success)
{
	vm_map_offset_t         dst_end;
	vm_map_entry_t          tmp_entry;
	vm_map_entry_t          entry;
	kern_return_t           kr;
	boolean_t               aligned = TRUE;
	boolean_t               contains_permanent_objects = FALSE;
	boolean_t               encountered_sub_map = FALSE;
	vm_map_offset_t         base_addr;
	vm_map_size_t           copy_size;
	vm_map_size_t           total_size;
	uint16_t                copy_page_shift;

	/*
	 *	Check for special kernel buffer allocated
	 *	by new_ipc_kmsg_copyin.
	 */

	if (copy->type == VM_MAP_COPY_KERNEL_BUFFER) {
		kr = vm_map_copyout_kernel_buffer(
			dst_map, &dst_addr,
			copy, copy->size, TRUE, discard_on_success);
		return kr;
	}

	/*
	 *      Only works for entry lists at the moment.  Will
	 *	support page lists later.
	 */

	assert(copy->type == VM_MAP_COPY_ENTRY_LIST);

	if (copy->size == 0) {
		if (discard_on_success) {
			vm_map_copy_discard(copy);
		}
		return KERN_SUCCESS;
	}

	copy_page_shift = copy->cpy_hdr.page_shift;

	/*
	 *	Verify that the destination is all writeable
	 *	initially.  We have to trunc the destination
	 *	address and round the copy size or we'll end up
	 *	splitting entries in strange ways.
	 */

	if (!VM_MAP_PAGE_ALIGNED(copy->size,
	    VM_MAP_PAGE_MASK(dst_map)) ||
	    !VM_MAP_PAGE_ALIGNED(copy->offset,
	    VM_MAP_PAGE_MASK(dst_map)) ||
	    !VM_MAP_PAGE_ALIGNED(dst_addr,
	    VM_MAP_PAGE_MASK(dst_map)) ||
	    copy_page_shift != VM_MAP_PAGE_SHIFT(dst_map)) {
		aligned = FALSE;
		dst_end = vm_map_round_page(dst_addr + copy->size,
		    VM_MAP_PAGE_MASK(dst_map));
	} else {
		dst_end = dst_addr + copy->size;
	}

	vm_map_lock(dst_map);

	/* LP64todo - remove this check when vm_map_commpage64()
	 * no longer has to stuff in a map_entry for the commpage
	 * above the map's max_offset.
	 */
	if (dst_addr >= dst_map->max_offset) {
		vm_map_unlock(dst_map);
		return KERN_INVALID_ADDRESS;
	}

start_pass_1:
	if (!vm_map_lookup_entry(dst_map, dst_addr, &tmp_entry)) {
		vm_map_unlock(dst_map);
		return KERN_INVALID_ADDRESS;
	}
	vm_map_clip_start(dst_map,
	    tmp_entry,
	    vm_map_trunc_page(dst_addr,
	    VM_MAP_PAGE_MASK(dst_map)));
	for (entry = tmp_entry;;) {
		vm_map_entry_t  next = entry->vme_next;

		while (entry->is_sub_map) {
			vm_map_offset_t sub_start;
			vm_map_offset_t sub_end;
			vm_map_offset_t local_end;

			if (entry->in_transition) {
				/*
				 * Say that we are waiting, and wait for entry.
				 */
				entry->needs_wakeup = TRUE;
				vm_map_entry_wait(dst_map, THREAD_UNINT);

				goto start_pass_1;
			}

			local_end = entry->vme_end;
			if (!(entry->needs_copy)) {
				/* if needs_copy we are a COW submap */
				/* in such a case we just replace so */
				/* there is no need for the follow-  */
				/* ing check.                        */
				encountered_sub_map = TRUE;
				sub_start = VME_OFFSET(entry);

				if (entry->vme_end < dst_end) {
					sub_end = entry->vme_end;
				} else {
					sub_end = dst_end;
				}
				sub_end -= entry->vme_start;
				sub_end += VME_OFFSET(entry);
				vm_map_unlock(dst_map);

				kr = vm_map_overwrite_submap_recurse(
					VME_SUBMAP(entry),
					sub_start,
					sub_end - sub_start);
				if (kr != KERN_SUCCESS) {
					return kr;
				}
				vm_map_lock(dst_map);
			}

			if (dst_end <= entry->vme_end) {
				goto start_overwrite;
			}
			if (!vm_map_lookup_entry(dst_map, local_end,
			    &entry)) {
				vm_map_unlock(dst_map);
				return KERN_INVALID_ADDRESS;
			}
			next = entry->vme_next;
		}

		if (!(entry->protection & VM_PROT_WRITE)) {
			vm_map_unlock(dst_map);
			return KERN_PROTECTION_FAILURE;
		}

		if (!vm_map_entry_is_overwritable(dst_map, entry)) {
			vm_map_unlock(dst_map);
			return KERN_PROTECTION_FAILURE;
		}

		/*
		 *	If the entry is in transition, we must wait
		 *	for it to exit that state.  Anything could happen
		 *	when we unlock the map, so start over.
		 */
		if (entry->in_transition) {
			/*
			 * Say that we are waiting, and wait for entry.
			 */
			entry->needs_wakeup = TRUE;
			vm_map_entry_wait(dst_map, THREAD_UNINT);

			goto start_pass_1;
		}

/*
 *		our range is contained completely within this map entry
 */
		if (dst_end <= entry->vme_end) {
			break;
		}
/*
 *		check that range specified is contiguous region
 */
		if ((next == vm_map_to_entry(dst_map)) ||
		    (next->vme_start != entry->vme_end)) {
			vm_map_unlock(dst_map);
			return KERN_INVALID_ADDRESS;
		}


		/*
		 *	Check for permanent objects in the destination.
		 */
		if ((VME_OBJECT(entry) != VM_OBJECT_NULL) &&
		    ((!VME_OBJECT(entry)->internal) ||
		    (VME_OBJECT(entry)->true_share))) {
			contains_permanent_objects = TRUE;
		}

		entry = next;
	}/* for */

start_overwrite:
	/*
	 *	If there are permanent objects in the destination, then
	 *	the copy cannot be interrupted.
	 */

	if (interruptible && contains_permanent_objects) {
		vm_map_unlock(dst_map);
		return KERN_FAILURE;   /* XXX */
	}

	/*
	 *
	 *	Make a second pass, overwriting the data
	 *	At the beginning of each loop iteration,
	 *	the next entry to be overwritten is "tmp_entry"
	 *	(initially, the value returned from the lookup above),
	 *	and the starting address expected in that entry
	 *	is "start".
	 */

	total_size = copy->size;
	if (encountered_sub_map) {
		copy_size = 0;
		/* re-calculate tmp_entry since we've had the map */
		/* unlocked */
		if (!vm_map_lookup_entry( dst_map, dst_addr, &tmp_entry)) {
			vm_map_unlock(dst_map);
			return KERN_INVALID_ADDRESS;
		}
	} else {
		copy_size = copy->size;
	}

	base_addr = dst_addr;
	while (TRUE) {
		/* deconstruct the copy object and do in parts */
		/* only in sub_map, interruptable case */
		vm_map_entry_t  copy_entry;
		vm_map_entry_t  previous_prev = VM_MAP_ENTRY_NULL;
		vm_map_entry_t  next_copy = VM_MAP_ENTRY_NULL;
		int             nentries;
		int             remaining_entries = 0;
		vm_map_offset_t new_offset = 0;

		for (entry = tmp_entry; copy_size == 0;) {
			vm_map_entry_t  next;

			next = entry->vme_next;

			/* tmp_entry and base address are moved along */
			/* each time we encounter a sub-map.  Otherwise */
			/* entry can outpase tmp_entry, and the copy_size */
			/* may reflect the distance between them */
			/* if the current entry is found to be in transition */
			/* we will start over at the beginning or the last */
			/* encounter of a submap as dictated by base_addr */
			/* we will zero copy_size accordingly. */
			if (entry->in_transition) {
				/*
				 * Say that we are waiting, and wait for entry.
				 */
				entry->needs_wakeup = TRUE;
				vm_map_entry_wait(dst_map, THREAD_UNINT);

				if (!vm_map_lookup_entry(dst_map, base_addr,
				    &tmp_entry)) {
					vm_map_unlock(dst_map);
					return KERN_INVALID_ADDRESS;
				}
				copy_size = 0;
				entry = tmp_entry;
				continue;
			}
			if (entry->is_sub_map) {
				vm_map_offset_t sub_start;
				vm_map_offset_t sub_end;
				vm_map_offset_t local_end;

				if (entry->needs_copy) {
					/* if this is a COW submap */
					/* just back the range with a */
					/* anonymous entry */
					assert(!entry->vme_permanent);
					if (entry->vme_end < dst_end) {
						sub_end = entry->vme_end;
					} else {
						sub_end = dst_end;
					}
					if (entry->vme_start < base_addr) {
						sub_start = base_addr;
					} else {
						sub_start = entry->vme_start;
					}
					vm_map_clip_end(
						dst_map, entry, sub_end);
					vm_map_clip_start(
						dst_map, entry, sub_start);
					assert(!entry->use_pmap);
					assert(!entry->iokit_acct);
					entry->use_pmap = TRUE;
					vm_map_deallocate(VME_SUBMAP(entry));
					assert(!entry->vme_permanent);
					VME_OBJECT_SET(entry, VM_OBJECT_NULL, false, 0);
					VME_OFFSET_SET(entry, 0);
					entry->is_shared = FALSE;
					entry->needs_copy = FALSE;
					entry->protection = VM_PROT_DEFAULT;
					entry->max_protection = VM_PROT_ALL;
					entry->wired_count = 0;
					entry->user_wired_count = 0;
					if (entry->inheritance
					    == VM_INHERIT_SHARE) {
						entry->inheritance = VM_INHERIT_COPY;
					}
					continue;
				}
				/* first take care of any non-sub_map */
				/* entries to send */
				if (base_addr < entry->vme_start) {
					/* stuff to send */
					copy_size =
					    entry->vme_start - base_addr;
					break;
				}
				sub_start = VME_OFFSET(entry);

				if (entry->vme_end < dst_end) {
					sub_end = entry->vme_end;
				} else {
					sub_end = dst_end;
				}
				sub_end -= entry->vme_start;
				sub_end += VME_OFFSET(entry);
				local_end = entry->vme_end;
				vm_map_unlock(dst_map);
				copy_size = sub_end - sub_start;

				/* adjust the copy object */
				if (total_size > copy_size) {
					vm_map_size_t   local_size = 0;
					vm_map_size_t   entry_size;

					nentries = 1;
					new_offset = copy->offset;
					copy_entry = vm_map_copy_first_entry(copy);
					while (copy_entry !=
					    vm_map_copy_to_entry(copy)) {
						entry_size = copy_entry->vme_end -
						    copy_entry->vme_start;
						if ((local_size < copy_size) &&
						    ((local_size + entry_size)
						    >= copy_size)) {
							vm_map_copy_clip_end(copy,
							    copy_entry,
							    copy_entry->vme_start +
							    (copy_size - local_size));
							entry_size = copy_entry->vme_end -
							    copy_entry->vme_start;
							local_size += entry_size;
							new_offset += entry_size;
						}
						if (local_size >= copy_size) {
							next_copy = copy_entry->vme_next;
							copy_entry->vme_next =
							    vm_map_copy_to_entry(copy);
							previous_prev =
							    copy->cpy_hdr.links.prev;
							copy->cpy_hdr.links.prev = copy_entry;
							copy->size = copy_size;
							remaining_entries =
							    copy->cpy_hdr.nentries;
							remaining_entries -= nentries;
							copy->cpy_hdr.nentries = nentries;
							break;
						} else {
							local_size += entry_size;
							new_offset += entry_size;
							nentries++;
						}
						copy_entry = copy_entry->vme_next;
					}
				}

				if ((entry->use_pmap) && (pmap == NULL)) {
					kr = vm_map_copy_overwrite_nested(
						VME_SUBMAP(entry),
						sub_start,
						copy,
						interruptible,
						VME_SUBMAP(entry)->pmap,
						TRUE);
				} else if (pmap != NULL) {
					kr = vm_map_copy_overwrite_nested(
						VME_SUBMAP(entry),
						sub_start,
						copy,
						interruptible, pmap,
						TRUE);
				} else {
					kr = vm_map_copy_overwrite_nested(
						VME_SUBMAP(entry),
						sub_start,
						copy,
						interruptible,
						dst_map->pmap,
						TRUE);
				}
				if (kr != KERN_SUCCESS) {
					if (next_copy != NULL) {
						copy->cpy_hdr.nentries +=
						    remaining_entries;
						copy->cpy_hdr.links.prev->vme_next =
						    next_copy;
						copy->cpy_hdr.links.prev
						        = previous_prev;
						copy->size = total_size;
					}
					return kr;
				}
				if (dst_end <= local_end) {
					return KERN_SUCCESS;
				}
				/* otherwise copy no longer exists, it was */
				/* destroyed after successful copy_overwrite */
				copy = vm_map_copy_allocate(VM_MAP_COPY_ENTRY_LIST);
				copy->offset = new_offset;
				copy->cpy_hdr.page_shift = copy_page_shift;

				total_size -= copy_size;
				copy_size = 0;
				/* put back remainder of copy in container */
				if (next_copy != NULL) {
					copy->cpy_hdr.nentries = remaining_entries;
					copy->cpy_hdr.links.next = next_copy;
					copy->cpy_hdr.links.prev = previous_prev;
					copy->size = total_size;
					next_copy->vme_prev =
					    vm_map_copy_to_entry(copy);
					next_copy = NULL;
				}
				base_addr = local_end;
				vm_map_lock(dst_map);
				if (!vm_map_lookup_entry(dst_map,
				    local_end, &tmp_entry)) {
					vm_map_unlock(dst_map);
					return KERN_INVALID_ADDRESS;
				}
				entry = tmp_entry;
				continue;
			}
			if (dst_end <= entry->vme_end) {
				copy_size = dst_end - base_addr;
				break;
			}

			if ((next == vm_map_to_entry(dst_map)) ||
			    (next->vme_start != entry->vme_end)) {
				vm_map_unlock(dst_map);
				return KERN_INVALID_ADDRESS;
			}

			entry = next;
		}/* for */

		next_copy = NULL;
		nentries = 1;

		/* adjust the copy object */
		if (total_size > copy_size) {
			vm_map_size_t   local_size = 0;
			vm_map_size_t   entry_size;

			new_offset = copy->offset;
			copy_entry = vm_map_copy_first_entry(copy);
			while (copy_entry != vm_map_copy_to_entry(copy)) {
				entry_size = copy_entry->vme_end -
				    copy_entry->vme_start;
				if ((local_size < copy_size) &&
				    ((local_size + entry_size)
				    >= copy_size)) {
					vm_map_copy_clip_end(copy, copy_entry,
					    copy_entry->vme_start +
					    (copy_size - local_size));
					entry_size = copy_entry->vme_end -
					    copy_entry->vme_start;
					local_size += entry_size;
					new_offset += entry_size;
				}
				if (local_size >= copy_size) {
					next_copy = copy_entry->vme_next;
					copy_entry->vme_next =
					    vm_map_copy_to_entry(copy);
					previous_prev =
					    copy->cpy_hdr.links.prev;
					copy->cpy_hdr.links.prev = copy_entry;
					copy->size = copy_size;
					remaining_entries =
					    copy->cpy_hdr.nentries;
					remaining_entries -= nentries;
					copy->cpy_hdr.nentries = nentries;
					break;
				} else {
					local_size += entry_size;
					new_offset += entry_size;
					nentries++;
				}
				copy_entry = copy_entry->vme_next;
			}
		}

		if (aligned) {
			pmap_t  local_pmap;

			if (pmap) {
				local_pmap = pmap;
			} else {
				local_pmap = dst_map->pmap;
			}

			if ((kr =  vm_map_copy_overwrite_aligned(
				    dst_map, tmp_entry, copy,
				    base_addr, local_pmap)) != KERN_SUCCESS) {
				if (next_copy != NULL) {
					copy->cpy_hdr.nentries +=
					    remaining_entries;
					copy->cpy_hdr.links.prev->vme_next =
					    next_copy;
					copy->cpy_hdr.links.prev =
					    previous_prev;
					copy->size += copy_size;
				}
				return kr;
			}
			vm_map_unlock(dst_map);
		} else {
			/*
			 * Performance gain:
			 *
			 * if the copy and dst address are misaligned but the same
			 * offset within the page we can copy_not_aligned the
			 * misaligned parts and copy aligned the rest.  If they are
			 * aligned but len is unaligned we simply need to copy
			 * the end bit unaligned.  We'll need to split the misaligned
			 * bits of the region in this case !
			 */
			/* ALWAYS UNLOCKS THE dst_map MAP */
			kr = vm_map_copy_overwrite_unaligned(
				dst_map,
				tmp_entry,
				copy,
				base_addr,
				discard_on_success);
			if (kr != KERN_SUCCESS) {
				if (next_copy != NULL) {
					copy->cpy_hdr.nentries +=
					    remaining_entries;
					copy->cpy_hdr.links.prev->vme_next =
					    next_copy;
					copy->cpy_hdr.links.prev =
					    previous_prev;
					copy->size += copy_size;
				}
				return kr;
			}
		}
		total_size -= copy_size;
		if (total_size == 0) {
			break;
		}
		base_addr += copy_size;
		copy_size = 0;
		copy->offset = new_offset;
		if (next_copy != NULL) {
			copy->cpy_hdr.nentries = remaining_entries;
			copy->cpy_hdr.links.next = next_copy;
			copy->cpy_hdr.links.prev = previous_prev;
			next_copy->vme_prev = vm_map_copy_to_entry(copy);
			copy->size = total_size;
		}
		vm_map_lock(dst_map);
		while (TRUE) {
			if (!vm_map_lookup_entry(dst_map,
			    base_addr, &tmp_entry)) {
				vm_map_unlock(dst_map);
				return KERN_INVALID_ADDRESS;
			}
			if (tmp_entry->in_transition) {
				entry->needs_wakeup = TRUE;
				vm_map_entry_wait(dst_map, THREAD_UNINT);
			} else {
				break;
			}
		}
		vm_map_clip_start(dst_map,
		    tmp_entry,
		    vm_map_trunc_page(base_addr,
		    VM_MAP_PAGE_MASK(dst_map)));

		entry = tmp_entry;
	} /* while */

	/*
	 *	Throw away the vm_map_copy object
	 */
	if (discard_on_success) {
		vm_map_copy_discard(copy);
	}

	return KERN_SUCCESS;
}/* vm_map_copy_overwrite */

kern_return_t
vm_map_copy_overwrite(
	vm_map_t        dst_map,
	vm_map_offset_t dst_addr,
	vm_map_copy_t   copy,
	vm_map_size_t   copy_size,
	boolean_t       interruptible)
{
	vm_map_size_t   head_size, tail_size;
	vm_map_copy_t   head_copy, tail_copy;
	vm_map_offset_t head_addr, tail_addr;
	vm_map_entry_t  entry;
	kern_return_t   kr;
	vm_map_offset_t effective_page_mask, effective_page_size;
	uint16_t        copy_page_shift;

	head_size = 0;
	tail_size = 0;
	head_copy = NULL;
	tail_copy = NULL;
	head_addr = 0;
	tail_addr = 0;

	/*
	 *	Check for null copy object.
	 */
	if (copy == VM_MAP_COPY_NULL) {
		return KERN_SUCCESS;
	}

	if (__improbable(vm_map_range_overflows(dst_map, dst_addr, copy_size))) {
		return KERN_INVALID_ADDRESS;
	}

	/*
	 * Assert that the vm_map_copy is coming from the right
	 * zone and hasn't been forged
	 */
	vm_map_copy_require(copy);

	if (interruptible ||
	    copy->type != VM_MAP_COPY_ENTRY_LIST) {
		/*
		 * We can't split the "copy" map if we're interruptible
		 * or if we don't have a "copy" map...
		 */
blunt_copy:
		kr = vm_map_copy_overwrite_nested(dst_map,
		    dst_addr,
		    copy,
		    interruptible,
		    (pmap_t) NULL,
		    TRUE);
		if (kr) {
			ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_COPYOVERWRITE_FULL_NESTED_ERROR), kr /* arg */);
		}
		return kr;
	}

	copy_page_shift = VM_MAP_COPY_PAGE_SHIFT(copy);
	if (copy_page_shift < PAGE_SHIFT ||
	    VM_MAP_PAGE_SHIFT(dst_map) < PAGE_SHIFT) {
		goto blunt_copy;
	}

	if (VM_MAP_PAGE_SHIFT(dst_map) < PAGE_SHIFT) {
		effective_page_mask = VM_MAP_PAGE_MASK(dst_map);
	} else {
		effective_page_mask = MAX(VM_MAP_PAGE_MASK(dst_map), PAGE_MASK);
		effective_page_mask = MAX(VM_MAP_COPY_PAGE_MASK(copy),
		    effective_page_mask);
	}
	effective_page_size = effective_page_mask + 1;

	if (copy_size < VM_MAP_COPY_OVERWRITE_OPTIMIZATION_THRESHOLD_PAGES * effective_page_size) {
		/*
		 * Too small to bother with optimizing...
		 */
		goto blunt_copy;
	}

	if ((dst_addr & effective_page_mask) !=
	    (copy->offset & effective_page_mask)) {
		/*
		 * Incompatible mis-alignment of source and destination...
		 */
		goto blunt_copy;
	}

	/*
	 * Proper alignment or identical mis-alignment at the beginning.
	 * Let's try and do a small unaligned copy first (if needed)
	 * and then an aligned copy for the rest.
	 */
	if (!vm_map_page_aligned(dst_addr, effective_page_mask)) {
		head_addr = dst_addr;
		head_size = (effective_page_size -
		    (copy->offset & effective_page_mask));
		head_size = MIN(head_size, copy_size);
	}
	if (!vm_map_page_aligned(copy->offset + copy_size,
	    effective_page_mask)) {
		/*
		 * Mis-alignment at the end.
		 * Do an aligned copy up to the last page and
		 * then an unaligned copy for the remaining bytes.
		 */
		tail_size = ((copy->offset + copy_size) &
		    effective_page_mask);
		tail_size = MIN(tail_size, copy_size);
		tail_addr = dst_addr + copy_size - tail_size;
		assert(tail_addr >= head_addr + head_size);
	}
	assert(head_size + tail_size <= copy_size);

	if (head_size + tail_size == copy_size) {
		/*
		 * It's all unaligned, no optimization possible...
		 */
		goto blunt_copy;
	}

	/*
	 * Can't optimize if there are any submaps in the
	 * destination due to the way we free the "copy" map
	 * progressively in vm_map_copy_overwrite_nested()
	 * in that case.
	 */
	vm_map_lock_read(dst_map);
	if (!vm_map_lookup_entry(dst_map, dst_addr, &entry)) {
		vm_map_unlock_read(dst_map);
		goto blunt_copy;
	}
	for (;
	    (entry != vm_map_to_entry(dst_map) &&
	    entry->vme_start < dst_addr + copy_size);
	    entry = entry->vme_next) {
		if (entry->is_sub_map) {
			vm_map_unlock_read(dst_map);
			goto blunt_copy;
		}
	}
	vm_map_unlock_read(dst_map);

	if (head_size) {
		/*
		 * Unaligned copy of the first "head_size" bytes, to reach
		 * a page boundary.
		 */

		/*
		 * Extract "head_copy" out of "copy".
		 */
		head_copy = vm_map_copy_allocate(VM_MAP_COPY_ENTRY_LIST);
		head_copy->cpy_hdr.entries_pageable =
		    copy->cpy_hdr.entries_pageable;
		head_copy->cpy_hdr.page_shift = copy_page_shift;

		entry = vm_map_copy_first_entry(copy);
		if (entry->vme_end < copy->offset + head_size) {
			head_size = entry->vme_end - copy->offset;
		}

		head_copy->offset = copy->offset;
		head_copy->size = head_size;
		copy->offset += head_size;
		copy->size -= head_size;
		copy_size -= head_size;
		assert(copy_size > 0);

		vm_map_copy_clip_end(copy, entry, copy->offset);
		vm_map_copy_entry_unlink(copy, entry);
		vm_map_copy_entry_link(head_copy,
		    vm_map_copy_to_entry(head_copy),
		    entry);

		/*
		 * Do the unaligned copy.
		 */
		kr = vm_map_copy_overwrite_nested(dst_map,
		    head_addr,
		    head_copy,
		    interruptible,
		    (pmap_t) NULL,
		    FALSE);
		if (kr != KERN_SUCCESS) {
			ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_COPYOVERWRITE_PARTIAL_HEAD_NESTED_ERROR), kr /* arg */);
			goto done;
		}
	}

	if (tail_size) {
		/*
		 * Extract "tail_copy" out of "copy".
		 */
		tail_copy = vm_map_copy_allocate(VM_MAP_COPY_ENTRY_LIST);
		tail_copy->cpy_hdr.entries_pageable =
		    copy->cpy_hdr.entries_pageable;
		tail_copy->cpy_hdr.page_shift = copy_page_shift;

		tail_copy->offset = copy->offset + copy_size - tail_size;
		tail_copy->size = tail_size;

		copy->size -= tail_size;
		copy_size -= tail_size;
		assert(copy_size > 0);

		entry = vm_map_copy_last_entry(copy);
		vm_map_copy_clip_start(copy, entry, tail_copy->offset);
		entry = vm_map_copy_last_entry(copy);
		vm_map_copy_entry_unlink(copy, entry);
		vm_map_copy_entry_link(tail_copy,
		    vm_map_copy_last_entry(tail_copy),
		    entry);
	}

	/*
	 * If we are here from ipc_kmsg_copyout_ool_descriptor(),
	 * we want to avoid TOCTOU issues w.r.t copy->size but
	 * we don't need to change vm_map_copy_overwrite_nested()
	 * and all other vm_map_copy_overwrite variants.
	 *
	 * So we assign the original copy_size that was passed into
	 * this routine back to copy.
	 *
	 * This use of local 'copy_size' passed into this routine is
	 * to try and protect against TOCTOU attacks where the kernel
	 * has been exploited. We don't expect this to be an issue
	 * during normal system operation.
	 */
	assertf(copy->size == copy_size,
	    "Mismatch of copy sizes. Expected 0x%llx, Got 0x%llx\n", (uint64_t) copy_size, (uint64_t) copy->size);
	copy->size = copy_size;

	/*
	 * Copy most (or possibly all) of the data.
	 */
	kr = vm_map_copy_overwrite_nested(dst_map,
	    dst_addr + head_size,
	    copy,
	    interruptible,
	    (pmap_t) NULL,
	    FALSE);
	if (kr != KERN_SUCCESS) {
		ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_COPYOVERWRITE_PARTIAL_NESTED_ERROR), kr /* arg */);
		goto done;
	}

	if (tail_size) {
		kr = vm_map_copy_overwrite_nested(dst_map,
		    tail_addr,
		    tail_copy,
		    interruptible,
		    (pmap_t) NULL,
		    FALSE);
		if (kr) {
			ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_COPYOVERWRITE_PARTIAL_TAIL_NESTED_ERROR), kr /* arg */);
		}
	}

done:
	assert(copy->type == VM_MAP_COPY_ENTRY_LIST);
	if (kr == KERN_SUCCESS) {
		/*
		 * Discard all the copy maps.
		 */
		if (head_copy) {
			vm_map_copy_discard(head_copy);
			head_copy = NULL;
		}
		vm_map_copy_discard(copy);
		if (tail_copy) {
			vm_map_copy_discard(tail_copy);
			tail_copy = NULL;
		}
	} else {
		/*
		 * Re-assemble the original copy map.
		 */
		if (head_copy) {
			entry = vm_map_copy_first_entry(head_copy);
			vm_map_copy_entry_unlink(head_copy, entry);
			vm_map_copy_entry_link(copy,
			    vm_map_copy_to_entry(copy),
			    entry);
			copy->offset -= head_size;
			copy->size += head_size;
			vm_map_copy_discard(head_copy);
			head_copy = NULL;
		}
		if (tail_copy) {
			entry = vm_map_copy_last_entry(tail_copy);
			vm_map_copy_entry_unlink(tail_copy, entry);
			vm_map_copy_entry_link(copy,
			    vm_map_copy_last_entry(copy),
			    entry);
			copy->size += tail_size;
			vm_map_copy_discard(tail_copy);
			tail_copy = NULL;
		}
	}
	return kr;
}


/*
 *	Routine: vm_map_copy_overwrite_unaligned	[internal use only]
 *
 *	Decription:
 *	Physically copy unaligned data
 *
 *	Implementation:
 *	Unaligned parts of pages have to be physically copied.  We use
 *	a modified form of vm_fault_copy (which understands none-aligned
 *	page offsets and sizes) to do the copy.  We attempt to copy as
 *	much memory in one go as possibly, however vm_fault_copy copies
 *	within 1 memory object so we have to find the smaller of "amount left"
 *	"source object data size" and "target object data size".  With
 *	unaligned data we don't need to split regions, therefore the source
 *	(copy) object should be one map entry, the target range may be split
 *	over multiple map entries however.  In any event we are pessimistic
 *	about these assumptions.
 *
 *	Callers of this function must call vm_map_copy_require on
 *	previously created vm_map_copy_t or pass a newly created
 *	one to ensure that it hasn't been forged.
 *
 *	Assumptions:
 *	dst_map is locked on entry and is return locked on success,
 *	unlocked on error.
 */

static kern_return_t
vm_map_copy_overwrite_unaligned(
	vm_map_t        dst_map,
	vm_map_entry_t  entry,
	vm_map_copy_t   copy,
	vm_map_offset_t start,
	boolean_t       discard_on_success)
{
	vm_map_entry_t          copy_entry;
	vm_map_entry_t          copy_entry_next;
	vm_map_version_t        version;
	vm_object_t             dst_object;
	vm_object_offset_t      dst_offset;
	vm_object_offset_t      src_offset;
	vm_object_offset_t      entry_offset;
	vm_map_offset_t         entry_end;
	vm_map_size_t           src_size,
	    dst_size,
	    copy_size,
	    amount_left;
	kern_return_t           kr = KERN_SUCCESS;


	copy_entry = vm_map_copy_first_entry(copy);

	vm_map_lock_write_to_read(dst_map);

	src_offset = copy->offset - trunc_page_mask_64(copy->offset, VM_MAP_COPY_PAGE_MASK(copy));
	amount_left = copy->size;
/*
 *	unaligned so we never clipped this entry, we need the offset into
 *	the vm_object not just the data.
 */
	while (amount_left > 0) {
		if (entry == vm_map_to_entry(dst_map)) {
			vm_map_unlock_read(dst_map);
			return KERN_INVALID_ADDRESS;
		}

		/* "start" must be within the current map entry */
		assert((start >= entry->vme_start) && (start < entry->vme_end));

		/*
		 *	Check protection again
		 */
		if (!(entry->protection & VM_PROT_WRITE)) {
			vm_map_unlock_read(dst_map);
			return KERN_PROTECTION_FAILURE;
		}
		if (!vm_map_entry_is_overwritable(dst_map, entry)) {
			vm_map_unlock_read(dst_map);
			return KERN_PROTECTION_FAILURE;
		}

		/*
		 *	If the entry is in transition, we must wait
		 *	for it to exit that state.  Anything could happen
		 *	when we unlock the map, so start over.
		 */
		if (entry->in_transition) {
			/*
			 * Say that we are waiting, and wait for entry.
			 */
			entry->needs_wakeup = TRUE;
			vm_map_entry_wait(dst_map, THREAD_UNINT);

			goto RetryLookup;
		}

		dst_offset = start - entry->vme_start;

		dst_size = entry->vme_end - start;

		src_size = copy_entry->vme_end -
		    (copy_entry->vme_start + src_offset);

		if (dst_size < src_size) {
/*
 *			we can only copy dst_size bytes before
 *			we have to get the next destination entry
 */
			copy_size = dst_size;
		} else {
/*
 *			we can only copy src_size bytes before
 *			we have to get the next source copy entry
 */
			copy_size = src_size;
		}

		if (copy_size > amount_left) {
			copy_size = amount_left;
		}
/*
 *		Entry needs copy, create a shadow shadow object for
 *		Copy on write region.
 */
		if (entry->needs_copy) {
			if (vm_map_lock_read_to_write(dst_map)) {
				vm_map_lock_read(dst_map);
				goto RetryLookup;
			}
			VME_OBJECT_SHADOW(entry,
			    (vm_map_size_t)(entry->vme_end
			    - entry->vme_start),
			    vm_map_always_shadow(dst_map));
			entry->needs_copy = FALSE;
			vm_map_lock_write_to_read(dst_map);
		}
		dst_object = VME_OBJECT(entry);
/*
 *		unlike with the virtual (aligned) copy we're going
 *		to fault on it therefore we need a target object.
 */
		if (dst_object == VM_OBJECT_NULL) {
			if (vm_map_lock_read_to_write(dst_map)) {
				vm_map_lock_read(dst_map);
				goto RetryLookup;
			}
			dst_object = vm_object_allocate((vm_map_size_t)
			    entry->vme_end - entry->vme_start);
			VME_OBJECT_SET(entry, dst_object, false, 0);
			VME_OFFSET_SET(entry, 0);
			assert(entry->use_pmap);
			vm_map_lock_write_to_read(dst_map);
		}
/*
 *		Take an object reference and unlock map. The "entry" may
 *		disappear or change when the map is unlocked.
 */
		vm_object_reference(dst_object);
		version.main_timestamp = dst_map->timestamp;
		entry_offset = VME_OFFSET(entry);
		entry_end = entry->vme_end;
		vm_map_unlock_read(dst_map);
/*
 *		Copy as much as possible in one pass
 */
		kr = vm_fault_copy(
			VME_OBJECT(copy_entry),
			VME_OFFSET(copy_entry) + src_offset,
			&copy_size,
			dst_object,
			entry_offset + dst_offset,
			dst_map,
			&version,
			THREAD_UNINT );

		start += copy_size;
		src_offset += copy_size;
		amount_left -= copy_size;
/*
 *		Release the object reference
 */
		vm_object_deallocate(dst_object);
/*
 *		If a hard error occurred, return it now
 */
		if (kr != KERN_SUCCESS) {
			return kr;
		}

		if ((copy_entry->vme_start + src_offset) == copy_entry->vme_end
		    || amount_left == 0) {
/*
 *			all done with this copy entry, dispose.
 */
			copy_entry_next = copy_entry->vme_next;

			if (discard_on_success) {
				vm_map_copy_entry_unlink(copy, copy_entry);
				assert(!copy_entry->is_sub_map);
				vm_object_deallocate(VME_OBJECT(copy_entry));
				vm_map_copy_entry_dispose(copy_entry);
			}

			if (copy_entry_next == vm_map_copy_to_entry(copy) &&
			    amount_left) {
/*
 *				not finished copying but run out of source
 */
				return KERN_INVALID_ADDRESS;
			}

			copy_entry = copy_entry_next;

			src_offset = 0;
		}

		if (amount_left == 0) {
			return KERN_SUCCESS;
		}

		vm_map_lock_read(dst_map);
		if (version.main_timestamp == dst_map->timestamp) {
			if (start == entry_end) {
/*
 *				destination region is split.  Use the version
 *				information to avoid a lookup in the normal
 *				case.
 */
				entry = entry->vme_next;
/*
 *				should be contiguous. Fail if we encounter
 *				a hole in the destination.
 */
				if (start != entry->vme_start) {
					vm_map_unlock_read(dst_map);
					return KERN_INVALID_ADDRESS;
				}
			}
		} else {
/*
 *			Map version check failed.
 *			we must lookup the entry because somebody
 *			might have changed the map behind our backs.
 */
RetryLookup:
			if (!vm_map_lookup_entry(dst_map, start, &entry)) {
				vm_map_unlock_read(dst_map);
				return KERN_INVALID_ADDRESS;
			}
		}
	}/* while */

	return KERN_SUCCESS;
}/* vm_map_copy_overwrite_unaligned */

/*
 *	Routine: vm_map_copy_overwrite_aligned	[internal use only]
 *
 *	Description:
 *	Does all the vm_trickery possible for whole pages.
 *
 *	Implementation:
 *
 *	If there are no permanent objects in the destination,
 *	and the source and destination map entry zones match,
 *	and the destination map entry is not shared,
 *	then the map entries can be deleted and replaced
 *	with those from the copy.  The following code is the
 *	basic idea of what to do, but there are lots of annoying
 *	little details about getting protection and inheritance
 *	right.  Should add protection, inheritance, and sharing checks
 *	to the above pass and make sure that no wiring is involved.
 *
 *	Callers of this function must call vm_map_copy_require on
 *	previously created vm_map_copy_t or pass a newly created
 *	one to ensure that it hasn't been forged.
 */

int vm_map_copy_overwrite_aligned_src_not_internal = 0;
int vm_map_copy_overwrite_aligned_src_not_symmetric = 0;
int vm_map_copy_overwrite_aligned_src_large = 0;

static kern_return_t
vm_map_copy_overwrite_aligned(
	vm_map_t        dst_map,
	vm_map_entry_t  tmp_entry,
	vm_map_copy_t   copy,
	vm_map_offset_t start,
	__unused pmap_t pmap)
{
	vm_object_t     object;
	vm_map_entry_t  copy_entry;
	vm_map_size_t   copy_size;
	vm_map_size_t   size;
	vm_map_entry_t  entry;

	while ((copy_entry = vm_map_copy_first_entry(copy))
	    != vm_map_copy_to_entry(copy)) {
		copy_size = (copy_entry->vme_end - copy_entry->vme_start);

		entry = tmp_entry;
		if (entry->is_sub_map) {
			/* unnested when clipped earlier */
			assert(!entry->use_pmap);
		}
		if (entry == vm_map_to_entry(dst_map)) {
			vm_map_unlock(dst_map);
			return KERN_INVALID_ADDRESS;
		}
		size = (entry->vme_end - entry->vme_start);
		/*
		 *	Make sure that no holes popped up in the
		 *	address map, and that the protection is
		 *	still valid, in case the map was unlocked
		 *	earlier.
		 */

		if ((entry->vme_start != start) || ((entry->is_sub_map)
		    && !entry->needs_copy)) {
			vm_map_unlock(dst_map);
			return KERN_INVALID_ADDRESS;
		}
		assert(entry != vm_map_to_entry(dst_map));

		/*
		 *	Check protection again
		 */

		if (!(entry->protection & VM_PROT_WRITE)) {
			vm_map_unlock(dst_map);
			return KERN_PROTECTION_FAILURE;
		}

		if (!vm_map_entry_is_overwritable(dst_map, entry)) {
			vm_map_unlock(dst_map);
			return KERN_PROTECTION_FAILURE;
		}

		/*
		 *	If the entry is in transition, we must wait
		 *	for it to exit that state.  Anything could happen
		 *	when we unlock the map, so start over.
		 */
		if (entry->in_transition) {
			/*
			 * Say that we are waiting, and wait for entry.
			 */
			entry->needs_wakeup = TRUE;
			vm_map_entry_wait(dst_map, THREAD_UNINT);

			goto RetryLookup;
		}

		/*
		 *	Adjust to source size first
		 */

		if (copy_size < size) {
			if (entry->map_aligned &&
			    !VM_MAP_PAGE_ALIGNED(entry->vme_start + copy_size,
			    VM_MAP_PAGE_MASK(dst_map))) {
				/* no longer map-aligned */
				entry->map_aligned = FALSE;
			}
			vm_map_clip_end(dst_map, entry, entry->vme_start + copy_size);
			size = copy_size;
		}

		/*
		 *	Adjust to destination size
		 */

		if (size < copy_size) {
			vm_map_copy_clip_end(copy, copy_entry,
			    copy_entry->vme_start + size);
			copy_size = size;
		}

		assert((entry->vme_end - entry->vme_start) == size);
		assert((tmp_entry->vme_end - tmp_entry->vme_start) == size);
		assert((copy_entry->vme_end - copy_entry->vme_start) == size);

		/*
		 *	If the destination contains temporary unshared memory,
		 *	we can perform the copy by throwing it away and
		 *	installing the source data.
		 *
		 *	Exceptions for mappings with special semantics:
		 *	+ "permanent" entries,
		 *	+ JIT regions,
		 *	+ TPRO regions,
		 *      + pmap-specific protection policies,
		 *	+ VM objects with COPY_NONE copy strategy.
		 */

		object = VME_OBJECT(entry);
		if ((!entry->is_shared &&
		    !entry->vme_permanent &&
		    !entry->used_for_jit &&
#if __arm64e__
		    !entry->used_for_tpro &&
#endif /* __arm64e__ */
		    !(entry->protection & VM_PROT_EXECUTE) &&
		    !pmap_has_prot_policy(dst_map->pmap, entry->translated_allow_execute, entry->protection) &&
		    ((object == VM_OBJECT_NULL) ||
		    (object->internal &&
		    !object->true_share &&
		    object->copy_strategy != MEMORY_OBJECT_COPY_NONE))) ||
		    entry->needs_copy) {
			vm_object_t     old_object = VME_OBJECT(entry);
			vm_object_offset_t      old_offset = VME_OFFSET(entry);
			vm_object_offset_t      offset;

			/*
			 * Ensure that the source and destination aren't
			 * identical
			 */
			if (old_object == VME_OBJECT(copy_entry) &&
			    old_offset == VME_OFFSET(copy_entry)) {
				vm_map_copy_entry_unlink(copy, copy_entry);
				vm_map_copy_entry_dispose(copy_entry);

				if (old_object != VM_OBJECT_NULL) {
					vm_object_deallocate(old_object);
				}

				start = tmp_entry->vme_end;
				tmp_entry = tmp_entry->vme_next;
				continue;
			}

#if XNU_TARGET_OS_OSX
#define __TRADEOFF1_OBJ_SIZE (64 * 1024 * 1024) /* 64 MB */
#define __TRADEOFF1_COPY_SIZE (128 * 1024)      /* 128 KB */
			if (VME_OBJECT(copy_entry) != VM_OBJECT_NULL &&
			    VME_OBJECT(copy_entry)->vo_size >= __TRADEOFF1_OBJ_SIZE &&
			    copy_size <= __TRADEOFF1_COPY_SIZE) {
				/*
				 * Virtual vs. Physical copy tradeoff #1.
				 *
				 * Copying only a few pages out of a large
				 * object:  do a physical copy instead of
				 * a virtual copy, to avoid possibly keeping
				 * the entire large object alive because of
				 * those few copy-on-write pages.
				 */
				vm_map_copy_overwrite_aligned_src_large++;
				goto slow_copy;
			}
#endif /* XNU_TARGET_OS_OSX */

			if ((dst_map->pmap != kernel_pmap) &&
			    (VME_ALIAS(entry) >= VM_MEMORY_MALLOC) &&
			    (VME_ALIAS(entry) <= VM_MEMORY_MALLOC_MEDIUM)) {
				vm_object_t new_object, new_shadow;

				/*
				 * We're about to map something over a mapping
				 * established by malloc()...
				 */
				new_object = VME_OBJECT(copy_entry);
				if (new_object != VM_OBJECT_NULL) {
					vm_object_lock_shared(new_object);
				}
				while (new_object != VM_OBJECT_NULL &&
#if XNU_TARGET_OS_OSX
				    !new_object->true_share &&
				    new_object->copy_strategy == MEMORY_OBJECT_COPY_SYMMETRIC &&
#endif /* XNU_TARGET_OS_OSX */
				    new_object->internal) {
					new_shadow = new_object->shadow;
					if (new_shadow == VM_OBJECT_NULL) {
						break;
					}
					vm_object_lock_shared(new_shadow);
					vm_object_unlock(new_object);
					new_object = new_shadow;
				}
				if (new_object != VM_OBJECT_NULL) {
					if (!new_object->internal) {
						/*
						 * The new mapping is backed
						 * by an external object.  We
						 * don't want malloc'ed memory
						 * to be replaced with such a
						 * non-anonymous mapping, so
						 * let's go off the optimized
						 * path...
						 */
						vm_map_copy_overwrite_aligned_src_not_internal++;
						vm_object_unlock(new_object);
						goto slow_copy;
					}
#if XNU_TARGET_OS_OSX
					if (new_object->true_share ||
					    new_object->copy_strategy != MEMORY_OBJECT_COPY_SYMMETRIC) {
						/*
						 * Same if there's a "true_share"
						 * object in the shadow chain, or
						 * an object with a non-default
						 * (SYMMETRIC) copy strategy.
						 */
						vm_map_copy_overwrite_aligned_src_not_symmetric++;
						vm_object_unlock(new_object);
						goto slow_copy;
					}
#endif /* XNU_TARGET_OS_OSX */
					vm_object_unlock(new_object);
				}
				/*
				 * The new mapping is still backed by
				 * anonymous (internal) memory, so it's
				 * OK to substitute it for the original
				 * malloc() mapping.
				 */
			}

			if (old_object != VM_OBJECT_NULL) {
				assert(!entry->vme_permanent);
				if (entry->is_sub_map) {
					if (entry->use_pmap) {
#ifndef NO_NESTED_PMAP
						pmap_unnest(dst_map->pmap,
						    (addr64_t)entry->vme_start,
						    entry->vme_end - entry->vme_start);
#endif  /* NO_NESTED_PMAP */
						if (dst_map->mapped_in_other_pmaps) {
							/* clean up parent */
							/* map/maps */
							vm_map_submap_pmap_clean(
								dst_map, entry->vme_start,
								entry->vme_end,
								VME_SUBMAP(entry),
								VME_OFFSET(entry));
						}
					} else {
						vm_map_submap_pmap_clean(
							dst_map, entry->vme_start,
							entry->vme_end,
							VME_SUBMAP(entry),
							VME_OFFSET(entry));
					}
					vm_map_deallocate(VME_SUBMAP(entry));
				} else {
					if (dst_map->mapped_in_other_pmaps) {
						vm_object_pmap_protect_options(
							VME_OBJECT(entry),
							VME_OFFSET(entry),
							entry->vme_end
							- entry->vme_start,
							PMAP_NULL,
							PAGE_SIZE,
							entry->vme_start,
							VM_PROT_NONE,
							PMAP_OPTIONS_REMOVE);
					} else {
						pmap_remove_options(
							dst_map->pmap,
							(addr64_t)(entry->vme_start),
							(addr64_t)(entry->vme_end),
							PMAP_OPTIONS_REMOVE);
					}
					vm_object_deallocate(old_object);
				}
			}

			if (entry->iokit_acct) {
				/* keep using iokit accounting */
				entry->use_pmap = FALSE;
			} else {
				/* use pmap accounting */
				entry->use_pmap = TRUE;
			}
			assert(!entry->vme_permanent);
			VME_OBJECT_SET(entry, VME_OBJECT(copy_entry), false, 0);
			object = VME_OBJECT(entry);
			entry->needs_copy = copy_entry->needs_copy;
			entry->wired_count = 0;
			entry->user_wired_count = 0;
			offset = VME_OFFSET(copy_entry);
			VME_OFFSET_SET(entry, offset);

			vm_map_copy_entry_unlink(copy, copy_entry);
			vm_map_copy_entry_dispose(copy_entry);

			/*
			 * we could try to push pages into the pmap at this point, BUT
			 * this optimization only saved on average 2 us per page if ALL
			 * the pages in the source were currently mapped
			 * and ALL the pages in the dest were touched, if there were fewer
			 * than 2/3 of the pages touched, this optimization actually cost more cycles
			 * it also puts a lot of pressure on the pmap layer w/r to mapping structures
			 */

			/*
			 *	Set up for the next iteration.  The map
			 *	has not been unlocked, so the next
			 *	address should be at the end of this
			 *	entry, and the next map entry should be
			 *	the one following it.
			 */

			start = tmp_entry->vme_end;
			tmp_entry = tmp_entry->vme_next;
		} else {
			vm_map_version_t        version;
			vm_object_t             dst_object;
			vm_object_offset_t      dst_offset;
			kern_return_t           r;

slow_copy:
			if (entry->needs_copy) {
				VME_OBJECT_SHADOW(entry,
				    (entry->vme_end -
				    entry->vme_start),
				    vm_map_always_shadow(dst_map));
				entry->needs_copy = FALSE;
			}

			dst_object = VME_OBJECT(entry);
			dst_offset = VME_OFFSET(entry);

			/*
			 *	Take an object reference, and record
			 *	the map version information so that the
			 *	map can be safely unlocked.
			 */

			if (dst_object == VM_OBJECT_NULL) {
				/*
				 * We would usually have just taken the
				 * optimized path above if the destination
				 * object has not been allocated yet.  But we
				 * now disable that optimization if the copy
				 * entry's object is not backed by anonymous
				 * memory to avoid replacing malloc'ed
				 * (i.e. re-usable) anonymous memory with a
				 * not-so-anonymous mapping.
				 * So we have to handle this case here and
				 * allocate a new VM object for this map entry.
				 */
				dst_object = vm_object_allocate(
					entry->vme_end - entry->vme_start);
				dst_offset = 0;
				VME_OBJECT_SET(entry, dst_object, false, 0);
				VME_OFFSET_SET(entry, dst_offset);
				assert(entry->use_pmap);
			}

			vm_object_reference(dst_object);

			/* account for unlock bumping up timestamp */
			version.main_timestamp = dst_map->timestamp + 1;

			vm_map_unlock(dst_map);

			/*
			 *	Copy as much as possible in one pass
			 */

			copy_size = size;
			r = vm_fault_copy(
				VME_OBJECT(copy_entry),
				VME_OFFSET(copy_entry),
				&copy_size,
				dst_object,
				dst_offset,
				dst_map,
				&version,
				THREAD_UNINT );

			/*
			 *	Release the object reference
			 */

			vm_object_deallocate(dst_object);

			/*
			 *	If a hard error occurred, return it now
			 */

			if (r != KERN_SUCCESS) {
				return r;
			}

			if (copy_size != 0) {
				/*
				 *	Dispose of the copied region
				 */

				vm_map_copy_clip_end(copy, copy_entry,
				    copy_entry->vme_start + copy_size);
				vm_map_copy_entry_unlink(copy, copy_entry);
				vm_object_deallocate(VME_OBJECT(copy_entry));
				vm_map_copy_entry_dispose(copy_entry);
			}

			/*
			 *	Pick up in the destination map where we left off.
			 *
			 *	Use the version information to avoid a lookup
			 *	in the normal case.
			 */

			start += copy_size;
			vm_map_lock(dst_map);
			if (version.main_timestamp == dst_map->timestamp &&
			    copy_size != 0) {
				/* We can safely use saved tmp_entry value */

				if (tmp_entry->map_aligned &&
				    !VM_MAP_PAGE_ALIGNED(
					    start,
					    VM_MAP_PAGE_MASK(dst_map))) {
					/* no longer map-aligned */
					tmp_entry->map_aligned = FALSE;
				}
				vm_map_clip_end(dst_map, tmp_entry, start);
				tmp_entry = tmp_entry->vme_next;
			} else {
				/* Must do lookup of tmp_entry */

RetryLookup:
				if (!vm_map_lookup_entry(dst_map, start, &tmp_entry)) {
					vm_map_unlock(dst_map);
					return KERN_INVALID_ADDRESS;
				}
				if (tmp_entry->map_aligned &&
				    !VM_MAP_PAGE_ALIGNED(
					    start,
					    VM_MAP_PAGE_MASK(dst_map))) {
					/* no longer map-aligned */
					tmp_entry->map_aligned = FALSE;
				}
				vm_map_clip_start(dst_map, tmp_entry, start);
			}
		}
	}/* while */

	return KERN_SUCCESS;
}/* vm_map_copy_overwrite_aligned */

/*
 *	Routine: vm_map_copyin_kernel_buffer [internal use only]
 *
 *	Description:
 *		Copy in data to a kernel buffer from space in the
 *		source map. The original space may be optionally
 *		deallocated.
 *
 *		If successful, returns a new copy object.
 */
static kern_return_t
vm_map_copyin_kernel_buffer(
	vm_map_t        src_map,
	vm_map_offset_t src_addr,
	vm_map_size_t   len,
	boolean_t       src_destroy,
	vm_map_copy_t   *copy_result)
{
	kern_return_t kr;
	vm_map_copy_t copy;
	void *kdata;

	if (len > msg_ool_size_small) {
		return KERN_INVALID_ARGUMENT;
	}

	kdata = kalloc_data(len, Z_WAITOK);
	if (kdata == NULL) {
		return KERN_RESOURCE_SHORTAGE;
	}
	kr = copyinmap(src_map, src_addr, kdata, (vm_size_t)len);
	if (kr != KERN_SUCCESS) {
		kfree_data(kdata, len);
		return kr;
	}

	copy = vm_map_copy_allocate(VM_MAP_COPY_KERNEL_BUFFER);
	copy->cpy_kdata = kdata;
	copy->size = len;
	copy->offset = 0;

	if (src_destroy) {
		vmr_flags_t flags = VM_MAP_REMOVE_INTERRUPTIBLE;

		if (src_map == kernel_map) {
			flags |= VM_MAP_REMOVE_KUNWIRE;
		}

		(void)vm_map_remove_guard(src_map,
		    vm_map_trunc_page(src_addr, VM_MAP_PAGE_MASK(src_map)),
		    vm_map_round_page(src_addr + len, VM_MAP_PAGE_MASK(src_map)),
		    flags, KMEM_GUARD_NONE);
	}

	*copy_result = copy;
	return KERN_SUCCESS;
}

/*
 *	Routine: vm_map_copyout_kernel_buffer	[internal use only]
 *
 *	Description:
 *		Copy out data from a kernel buffer into space in the
 *		destination map. The space may be otpionally dynamically
 *		allocated.
 *
 *		If successful, consumes the copy object.
 *		Otherwise, the caller is responsible for it.
 *
 *		Callers of this function must call vm_map_copy_require on
 *		previously created vm_map_copy_t or pass a newly created
 *		one to ensure that it hasn't been forged.
 */
static int vm_map_copyout_kernel_buffer_failures = 0;
static kern_return_t
vm_map_copyout_kernel_buffer(
	vm_map_t                map,
	vm_map_address_t        *addr,  /* IN/OUT */
	vm_map_copy_t           copy,
	vm_map_size_t           copy_size,
	boolean_t               overwrite,
	boolean_t               consume_on_success)
{
	kern_return_t kr = KERN_SUCCESS;
	thread_t thread = current_thread();

	assert(copy->size == copy_size);

	/*
	 * check for corrupted vm_map_copy structure
	 */
	if (copy_size > msg_ool_size_small || copy->offset) {
		panic("Invalid vm_map_copy_t sz:%lld, ofst:%lld",
		    (long long)copy->size, (long long)copy->offset);
	}

	if (!overwrite) {
		/*
		 * Allocate space in the target map for the data
		 */
		vm_map_kernel_flags_t vmk_flags = VM_MAP_KERNEL_FLAGS_ANYWHERE();

		if (map == kernel_map) {
			vmk_flags.vmkf_range_id = KMEM_RANGE_ID_DATA;
		}

		*addr = 0;
		kr = vm_map_enter(map,
		    addr,
		    vm_map_round_page(copy_size,
		    VM_MAP_PAGE_MASK(map)),
		    (vm_map_offset_t) 0,
		    vmk_flags,
		    VM_OBJECT_NULL,
		    (vm_object_offset_t) 0,
		    FALSE,
		    VM_PROT_DEFAULT,
		    VM_PROT_ALL,
		    VM_INHERIT_DEFAULT);
		if (kr != KERN_SUCCESS) {
			return kr;
		}
#if KASAN
		if (map->pmap == kernel_pmap) {
			kasan_notify_address(*addr, copy->size);
		}
#endif
	}

	/*
	 * Copyout the data from the kernel buffer to the target map.
	 */
	if (thread->map == map) {
		/*
		 * If the target map is the current map, just do
		 * the copy.
		 */
		assert((vm_size_t)copy_size == copy_size);
		if (copyout(copy->cpy_kdata, *addr, (vm_size_t)copy_size)) {
			kr = KERN_INVALID_ADDRESS;
		}
	} else {
		vm_map_t oldmap;

		/*
		 * If the target map is another map, assume the
		 * target's address space identity for the duration
		 * of the copy.
		 */
		vm_map_reference(map);
		oldmap = vm_map_switch(map);

		assert((vm_size_t)copy_size == copy_size);
		if (copyout(copy->cpy_kdata, *addr, (vm_size_t)copy_size)) {
			vm_map_copyout_kernel_buffer_failures++;
			kr = KERN_INVALID_ADDRESS;
		}

		(void) vm_map_switch(oldmap);
		vm_map_deallocate(map);
	}

	if (kr != KERN_SUCCESS) {
		/* the copy failed, clean up */
		if (!overwrite) {
			/*
			 * Deallocate the space we allocated in the target map.
			 */
			(void) vm_map_remove(map,
			    vm_map_trunc_page(*addr,
			    VM_MAP_PAGE_MASK(map)),
			    vm_map_round_page((*addr +
			    vm_map_round_page(copy_size,
			    VM_MAP_PAGE_MASK(map))),
			    VM_MAP_PAGE_MASK(map)));
			*addr = 0;
		}
	} else {
		/* copy was successful, dicard the copy structure */
		if (consume_on_success) {
			kfree_data(copy->cpy_kdata, copy_size);
			zfree_id(ZONE_ID_VM_MAP_COPY, copy);
		}
	}

	return kr;
}

/*
 *	Routine:	vm_map_copy_insert      [internal use only]
 *
 *	Description:
 *		Link a copy chain ("copy") into a map at the
 *		specified location (after "where").
 *
 *		Callers of this function must call vm_map_copy_require on
 *		previously created vm_map_copy_t or pass a newly created
 *		one to ensure that it hasn't been forged.
 *	Side effects:
 *		The copy chain is destroyed.
 */
static void
vm_map_copy_insert(
	vm_map_t        map,
	vm_map_entry_t  after_where,
	vm_map_copy_t   copy)
{
	vm_map_entry_t  entry;

	while (vm_map_copy_first_entry(copy) != vm_map_copy_to_entry(copy)) {
		entry = vm_map_copy_first_entry(copy);
		vm_map_copy_entry_unlink(copy, entry);
		vm_map_store_entry_link(map, after_where, entry,
		    VM_MAP_KERNEL_FLAGS_NONE);
		after_where = entry;
	}
	zfree_id(ZONE_ID_VM_MAP_COPY, copy);
}

/*
 * Callers of this function must call vm_map_copy_require on
 * previously created vm_map_copy_t or pass a newly created
 * one to ensure that it hasn't been forged.
 */
void
vm_map_copy_remap(
	vm_map_t        map,
	vm_map_entry_t  where,
	vm_map_copy_t   copy,
	vm_map_offset_t adjustment,
	vm_prot_t       cur_prot,
	vm_prot_t       max_prot,
	vm_inherit_t    inheritance)
{
	vm_map_entry_t  copy_entry, new_entry;

	for (copy_entry = vm_map_copy_first_entry(copy);
	    copy_entry != vm_map_copy_to_entry(copy);
	    copy_entry = copy_entry->vme_next) {
		/* get a new VM map entry for the map */
		new_entry = vm_map_entry_create(map);
		/* copy the "copy entry" to the new entry */
		vm_map_entry_copy(map, new_entry, copy_entry);
		/* adjust "start" and "end" */
		new_entry->vme_start += adjustment;
		new_entry->vme_end += adjustment;
		/* clear some attributes */
		new_entry->inheritance = inheritance;
		new_entry->protection = cur_prot;
		new_entry->max_protection = max_prot;
		new_entry->behavior = VM_BEHAVIOR_DEFAULT;
		/* take an extra reference on the entry's "object" */
		if (new_entry->is_sub_map) {
			assert(!new_entry->use_pmap); /* not nested */
			vm_map_reference(VME_SUBMAP(new_entry));
		} else {
			vm_object_reference(VME_OBJECT(new_entry));
		}
		/* insert the new entry in the map */
		vm_map_store_entry_link(map, where, new_entry,
		    VM_MAP_KERNEL_FLAGS_NONE);
		/* continue inserting the "copy entries" after the new entry */
		where = new_entry;
	}
}


/*
 * Returns true if *size matches (or is in the range of) copy->size.
 * Upon returning true, the *size field is updated with the actual size of the
 * copy object (may be different for VM_MAP_COPY_ENTRY_LIST types)
 */
boolean_t
vm_map_copy_validate_size(
	vm_map_t                dst_map,
	vm_map_copy_t           copy,
	vm_map_size_t           *size)
{
	if (copy == VM_MAP_COPY_NULL) {
		return FALSE;
	}

	/*
	 * Assert that the vm_map_copy is coming from the right
	 * zone and hasn't been forged
	 */
	vm_map_copy_require(copy);

	vm_map_size_t copy_sz = copy->size;
	vm_map_size_t sz = *size;
	switch (copy->type) {
	case VM_MAP_COPY_KERNEL_BUFFER:
		if (sz == copy_sz) {
			return TRUE;
		}
		break;
	case VM_MAP_COPY_ENTRY_LIST:
		/*
		 * potential page-size rounding prevents us from exactly
		 * validating this flavor of vm_map_copy, but we can at least
		 * assert that it's within a range.
		 */
		if (copy_sz >= sz &&
		    copy_sz <= vm_map_round_page(sz, VM_MAP_PAGE_MASK(dst_map))) {
			*size = copy_sz;
			return TRUE;
		}
		break;
	default:
		break;
	}
	return FALSE;
}

/*
 *	Routine:	vm_map_copyout_size
 *
 *	Description:
 *		Copy out a copy chain ("copy") into newly-allocated
 *		space in the destination map. Uses a prevalidated
 *		size for the copy object (vm_map_copy_validate_size).
 *
 *		If successful, consumes the copy object.
 *		Otherwise, the caller is responsible for it.
 */
kern_return_t
vm_map_copyout_size(
	vm_map_t                dst_map,
	vm_map_address_t        *dst_addr,      /* OUT */
	vm_map_copy_t           copy,
	vm_map_size_t           copy_size)
{
	return vm_map_copyout_internal(dst_map, dst_addr, copy, copy_size,
	           TRUE,                     /* consume_on_success */
	           VM_PROT_DEFAULT,
	           VM_PROT_ALL,
	           VM_INHERIT_DEFAULT);
}

/*
 *	Routine:	vm_map_copyout
 *
 *	Description:
 *		Copy out a copy chain ("copy") into newly-allocated
 *		space in the destination map.
 *
 *		If successful, consumes the copy object.
 *		Otherwise, the caller is responsible for it.
 */
kern_return_t
vm_map_copyout(
	vm_map_t                dst_map,
	vm_map_address_t        *dst_addr,      /* OUT */
	vm_map_copy_t           copy)
{
	return vm_map_copyout_internal(dst_map, dst_addr, copy, copy ? copy->size : 0,
	           TRUE,                     /* consume_on_success */
	           VM_PROT_DEFAULT,
	           VM_PROT_ALL,
	           VM_INHERIT_DEFAULT);
}

kern_return_t
vm_map_copyout_internal(
	vm_map_t                dst_map,
	vm_map_address_t        *dst_addr,      /* OUT */
	vm_map_copy_t           copy,
	vm_map_size_t           copy_size,
	boolean_t               consume_on_success,
	vm_prot_t               cur_protection,
	vm_prot_t               max_protection,
	vm_inherit_t            inheritance)
{
	vm_map_size_t           size;
	vm_map_size_t           adjustment;
	vm_map_offset_t         start;
	vm_object_offset_t      vm_copy_start;
	vm_map_entry_t          last;
	vm_map_entry_t          entry;
	vm_map_copy_t           original_copy;
	kern_return_t           kr;
	vm_map_kernel_flags_t   vmk_flags = VM_MAP_KERNEL_FLAGS_ANYWHERE();

	/*
	 *	Check for null copy object.
	 */

	if (copy == VM_MAP_COPY_NULL) {
		*dst_addr = 0;
		return KERN_SUCCESS;
	}

	/*
	 * Assert that the vm_map_copy is coming from the right
	 * zone and hasn't been forged
	 */
	vm_map_copy_require(copy);

	if (copy->size != copy_size) {
		*dst_addr = 0;
		ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_COPYOUT_INTERNAL_SIZE_ERROR), KERN_FAILURE /* arg */);
		return KERN_FAILURE;
	}

	/*
	 *	Check for special kernel buffer allocated
	 *	by new_ipc_kmsg_copyin.
	 */

	if (copy->type == VM_MAP_COPY_KERNEL_BUFFER) {
		kr = vm_map_copyout_kernel_buffer(dst_map, dst_addr,
		    copy, copy_size, FALSE,
		    consume_on_success);
		if (kr) {
			ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_COPYOUT_KERNEL_BUFFER_ERROR), kr /* arg */);
		}
		return kr;
	}

	original_copy = copy;
	if (copy->cpy_hdr.page_shift != VM_MAP_PAGE_SHIFT(dst_map)) {
		vm_map_copy_t target_copy;
		vm_map_offset_t overmap_start, overmap_end, trimmed_start;

		target_copy = VM_MAP_COPY_NULL;
		DEBUG4K_ADJUST("adjusting...\n");
		kr = vm_map_copy_adjust_to_target(
			copy,
			0, /* offset */
			copy->size, /* size */
			dst_map,
			TRUE, /* copy */
			&target_copy,
			&overmap_start,
			&overmap_end,
			&trimmed_start);
		if (kr != KERN_SUCCESS) {
			DEBUG4K_COPY("adjust failed 0x%x\n", kr);
			ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_COPYOUT_INTERNAL_ADJUSTING_ERROR), kr /* arg */);
			return kr;
		}
		DEBUG4K_COPY("copy %p (%d 0x%llx 0x%llx) dst_map %p (%d) target_copy %p (%d 0x%llx 0x%llx) overmap_start 0x%llx overmap_end 0x%llx trimmed_start 0x%llx\n", copy, copy->cpy_hdr.page_shift, copy->offset, (uint64_t)copy->size, dst_map, VM_MAP_PAGE_SHIFT(dst_map), target_copy, target_copy->cpy_hdr.page_shift, target_copy->offset, (uint64_t)target_copy->size, (uint64_t)overmap_start, (uint64_t)overmap_end, (uint64_t)trimmed_start);
		if (target_copy != copy) {
			copy = target_copy;
		}
		copy_size = copy->size;
	}

	/*
	 *	Find space for the data
	 */

	vm_copy_start = vm_map_trunc_page((vm_map_size_t)copy->offset,
	    VM_MAP_COPY_PAGE_MASK(copy));
	size = vm_map_round_page((vm_map_size_t)copy->offset + copy_size,
	    VM_MAP_COPY_PAGE_MASK(copy))
	    - vm_copy_start;

	vm_map_kernel_flags_update_range_id(&vmk_flags, dst_map);

	vm_map_lock(dst_map);
	kr = vm_map_locate_space(dst_map, size, 0, vmk_flags,
	    &start, &last);
	if (kr != KERN_SUCCESS) {
		vm_map_unlock(dst_map);
		ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_COPYOUT_INTERNAL_SPACE_ERROR), kr /* arg */);
		return kr;
	}

	adjustment = start - vm_copy_start;
	if (!consume_on_success) {
		/*
		 * We're not allowed to consume "copy", so we'll have to
		 * copy its map entries into the destination map below.
		 * No need to re-allocate map entries from the correct
		 * (pageable or not) zone, since we'll get new map entries
		 * during the transfer.
		 * We'll also adjust the map entries's "start" and "end"
		 * during the transfer, to keep "copy"'s entries consistent
		 * with its "offset".
		 */
		goto after_adjustments;
	}

	/*
	 *	Since we're going to just drop the map
	 *	entries from the copy into the destination
	 *	map, they must come from the same pool.
	 */

	if (copy->cpy_hdr.entries_pageable != dst_map->hdr.entries_pageable) {
		/*
		 * Mismatches occur when dealing with the default
		 * pager.
		 */
		vm_map_entry_t  next, new;

		/*
		 * Find the zone that the copies were allocated from
		 */

		entry = vm_map_copy_first_entry(copy);

		/*
		 * Reinitialize the copy so that vm_map_copy_entry_link
		 * will work.
		 */
		vm_map_store_copy_reset(copy, entry);
		copy->cpy_hdr.entries_pageable = dst_map->hdr.entries_pageable;

		/*
		 * Copy each entry.
		 */
		while (entry != vm_map_copy_to_entry(copy)) {
			new = vm_map_copy_entry_create(copy);
			vm_map_entry_copy_full(new, entry);
			new->vme_no_copy_on_read = FALSE;
			assert(!new->iokit_acct);
			if (new->is_sub_map) {
				/* clr address space specifics */
				new->use_pmap = FALSE;
			}
			vm_map_copy_entry_link(copy,
			    vm_map_copy_last_entry(copy),
			    new);
			next = entry->vme_next;
			vm_map_entry_dispose(entry);
			entry = next;
		}
	}

	/*
	 *	Adjust the addresses in the copy chain, and
	 *	reset the region attributes.
	 */

	for (entry = vm_map_copy_first_entry(copy);
	    entry != vm_map_copy_to_entry(copy);
	    entry = entry->vme_next) {
		if (VM_MAP_PAGE_SHIFT(dst_map) == PAGE_SHIFT) {
			/*
			 * We're injecting this copy entry into a map that
			 * has the standard page alignment, so clear
			 * "map_aligned" (which might have been inherited
			 * from the original map entry).
			 */
			entry->map_aligned = FALSE;
		}

		entry->vme_start += adjustment;
		entry->vme_end += adjustment;

		if (entry->map_aligned) {
			assert(VM_MAP_PAGE_ALIGNED(entry->vme_start,
			    VM_MAP_PAGE_MASK(dst_map)));
			assert(VM_MAP_PAGE_ALIGNED(entry->vme_end,
			    VM_MAP_PAGE_MASK(dst_map)));
		}

		entry->inheritance = VM_INHERIT_DEFAULT;
		entry->protection = VM_PROT_DEFAULT;
		entry->max_protection = VM_PROT_ALL;
		entry->behavior = VM_BEHAVIOR_DEFAULT;

		/*
		 * If the entry is now wired,
		 * map the pages into the destination map.
		 */
		if (entry->wired_count != 0) {
			vm_map_offset_t va;
			vm_object_offset_t       offset;
			vm_object_t object;
			vm_prot_t prot;
			int     type_of_fault;
			uint8_t object_lock_type = OBJECT_LOCK_EXCLUSIVE;

			/* TODO4K would need to use actual page size */
			assert(VM_MAP_PAGE_SHIFT(dst_map) == PAGE_SHIFT);

			object = VME_OBJECT(entry);
			offset = VME_OFFSET(entry);
			va = entry->vme_start;

			pmap_pageable(dst_map->pmap,
			    entry->vme_start,
			    entry->vme_end,
			    TRUE);

			while (va < entry->vme_end) {
				vm_page_t       m;
				struct vm_object_fault_info fault_info = {};

				/*
				 * Look up the page in the object.
				 * Assert that the page will be found in the
				 * top object:
				 * either
				 *	the object was newly created by
				 *	vm_object_copy_slowly, and has
				 *	copies of all of the pages from
				 *	the source object
				 * or
				 *	the object was moved from the old
				 *	map entry; because the old map
				 *	entry was wired, all of the pages
				 *	were in the top-level object.
				 *	(XXX not true if we wire pages for
				 *	 reading)
				 */
				vm_object_lock(object);

				m = vm_page_lookup(object, offset);
				if (m == VM_PAGE_NULL || !VM_PAGE_WIRED(m) ||
				    m->vmp_absent) {
					panic("vm_map_copyout: wiring %p", m);
				}

				prot = entry->protection;

				if (override_nx(dst_map, VME_ALIAS(entry)) &&
				    prot) {
					prot |= VM_PROT_EXECUTE;
				}

				type_of_fault = DBG_CACHE_HIT_FAULT;

				fault_info.user_tag = VME_ALIAS(entry);
				fault_info.pmap_options = 0;
				if (entry->iokit_acct ||
				    (!entry->is_sub_map && !entry->use_pmap)) {
					fault_info.pmap_options |= PMAP_OPTIONS_ALT_ACCT;
				}
				if (entry->vme_xnu_user_debug &&
				    !VM_PAGE_OBJECT(m)->code_signed) {
					/*
					 * Modified code-signed executable
					 * region: this page does not belong
					 * to a code-signed VM object, so it
					 * must have been copied and should
					 * therefore be typed XNU_USER_DEBUG
					 * rather than XNU_USER_EXEC.
					 */
					fault_info.pmap_options |= PMAP_OPTIONS_XNU_USER_DEBUG;
				}

				vm_fault_enter(m,
				    dst_map->pmap,
				    va,
				    PAGE_SIZE, 0,
				    prot,
				    prot,
				    VM_PAGE_WIRED(m),
				    FALSE,            /* change_wiring */
				    VM_KERN_MEMORY_NONE,            /* tag - not wiring */
				    &fault_info,
				    NULL,             /* need_retry */
				    &type_of_fault,
				    &object_lock_type); /*Exclusive mode lock. Will remain unchanged.*/

				vm_object_unlock(object);

				offset += PAGE_SIZE_64;
				va += PAGE_SIZE;
			}
		}
	}

after_adjustments:

	/*
	 *	Correct the page alignment for the result
	 */

	*dst_addr = start + (copy->offset - vm_copy_start);

#if KASAN
	kasan_notify_address(*dst_addr, size);
#endif

	/*
	 *	Update the hints and the map size
	 */

	if (consume_on_success) {
		SAVE_HINT_MAP_WRITE(dst_map, vm_map_copy_last_entry(copy));
	} else {
		SAVE_HINT_MAP_WRITE(dst_map, last);
	}

	dst_map->size += size;

	/*
	 *	Link in the copy
	 */

	if (consume_on_success) {
		vm_map_copy_insert(dst_map, last, copy);
		if (copy != original_copy) {
			vm_map_copy_discard(original_copy);
			original_copy = VM_MAP_COPY_NULL;
		}
	} else {
		vm_map_copy_remap(dst_map, last, copy, adjustment,
		    cur_protection, max_protection,
		    inheritance);
		if (copy != original_copy && original_copy != VM_MAP_COPY_NULL) {
			vm_map_copy_discard(copy);
			copy = original_copy;
		}
	}


	vm_map_unlock(dst_map);

	/*
	 * XXX	If wiring_required, call vm_map_pageable
	 */

	return KERN_SUCCESS;
}

/*
 *	Routine:	vm_map_copyin
 *
 *	Description:
 *		see vm_map_copyin_common.  Exported via Unsupported.exports.
 *
 */

#undef vm_map_copyin

kern_return_t
vm_map_copyin(
	vm_map_t                        src_map,
	vm_map_address_t        src_addr,
	vm_map_size_t           len,
	boolean_t                       src_destroy,
	vm_map_copy_t           *copy_result)   /* OUT */
{
	return vm_map_copyin_common(src_map, src_addr, len, src_destroy,
	           FALSE, copy_result, FALSE);
}

/*
 *	Routine:	vm_map_copyin_common
 *
 *	Description:
 *		Copy the specified region (src_addr, len) from the
 *		source address space (src_map), possibly removing
 *		the region from the source address space (src_destroy).
 *
 *	Returns:
 *		A vm_map_copy_t object (copy_result), suitable for
 *		insertion into another address space (using vm_map_copyout),
 *		copying over another address space region (using
 *		vm_map_copy_overwrite).  If the copy is unused, it
 *		should be destroyed (using vm_map_copy_discard).
 *
 *	In/out conditions:
 *		The source map should not be locked on entry.
 */

typedef struct submap_map {
	vm_map_t        parent_map;
	vm_map_offset_t base_start;
	vm_map_offset_t base_end;
	vm_map_size_t   base_len;
	struct submap_map *next;
} submap_map_t;

kern_return_t
vm_map_copyin_common(
	vm_map_t        src_map,
	vm_map_address_t src_addr,
	vm_map_size_t   len,
	boolean_t       src_destroy,
	__unused boolean_t      src_volatile,
	vm_map_copy_t   *copy_result,   /* OUT */
	boolean_t       use_maxprot)
{
	int flags;

	flags = 0;
	if (src_destroy) {
		flags |= VM_MAP_COPYIN_SRC_DESTROY;
	}
	if (use_maxprot) {
		flags |= VM_MAP_COPYIN_USE_MAXPROT;
	}
	return vm_map_copyin_internal(src_map,
	           src_addr,
	           len,
	           flags,
	           copy_result);
}
kern_return_t
vm_map_copyin_internal(
	vm_map_t        src_map,
	vm_map_address_t src_addr,
	vm_map_size_t   len,
	int             flags,
	vm_map_copy_t   *copy_result)   /* OUT */
{
	vm_map_entry_t  tmp_entry;      /* Result of last map lookup --
	                                 * in multi-level lookup, this
	                                 * entry contains the actual
	                                 * vm_object/offset.
	                                 */
	vm_map_entry_t  new_entry = VM_MAP_ENTRY_NULL;  /* Map entry for copy */

	vm_map_offset_t src_start;      /* Start of current entry --
	                                 * where copy is taking place now
	                                 */
	vm_map_offset_t src_end;        /* End of entire region to be
	                                 * copied */
	vm_map_offset_t src_base;
	vm_map_t        base_map = src_map;
	boolean_t       map_share = FALSE;
	submap_map_t    *parent_maps = NULL;

	vm_map_copy_t   copy;           /* Resulting copy */
	vm_map_address_t copy_addr;
	vm_map_size_t   copy_size;
	boolean_t       src_destroy;
	boolean_t       use_maxprot;
	boolean_t       preserve_purgeable;
	boolean_t       entry_was_shared;
	vm_map_entry_t  saved_src_entry;


	if (flags & ~VM_MAP_COPYIN_ALL_FLAGS) {
		return KERN_INVALID_ARGUMENT;
	}

#if CONFIG_KERNEL_TAGGING
	if (src_map->pmap == kernel_pmap) {
		src_addr = vm_memtag_canonicalize_address(src_addr);
	}
#endif /* CONFIG_KERNEL_TAGGING */

	src_destroy = (flags & VM_MAP_COPYIN_SRC_DESTROY) ? TRUE : FALSE;
	use_maxprot = (flags & VM_MAP_COPYIN_USE_MAXPROT) ? TRUE : FALSE;
	preserve_purgeable =
	    (flags & VM_MAP_COPYIN_PRESERVE_PURGEABLE) ? TRUE : FALSE;

	/*
	 *	Check for copies of zero bytes.
	 */

	if (len == 0) {
		*copy_result = VM_MAP_COPY_NULL;
		return KERN_SUCCESS;
	}

	/*
	 *	Check that the end address doesn't overflow
	 */
	if (__improbable(vm_map_range_overflows(src_map, src_addr, len))) {
		return KERN_INVALID_ADDRESS;
	}
	src_end = src_addr + len;
	if (src_end < src_addr) {
		return KERN_INVALID_ADDRESS;
	}

	/*
	 *	Compute (page aligned) start and end of region
	 */
	src_start = vm_map_trunc_page(src_addr,
	    VM_MAP_PAGE_MASK(src_map));
	src_end = vm_map_round_page(src_end,
	    VM_MAP_PAGE_MASK(src_map));
	if (src_end < src_addr) {
		return KERN_INVALID_ADDRESS;
	}

	/*
	 * If the copy is sufficiently small, use a kernel buffer instead
	 * of making a virtual copy.  The theory being that the cost of
	 * setting up VM (and taking C-O-W faults) dominates the copy costs
	 * for small regions.
	 */
	if ((len <= msg_ool_size_small) &&
	    !use_maxprot &&
	    !preserve_purgeable &&
	    !(flags & VM_MAP_COPYIN_ENTRY_LIST) &&
	    /*
	     * Since the "msg_ool_size_small" threshold was increased and
	     * vm_map_copyin_kernel_buffer() doesn't handle accesses beyond the
	     * address space limits, we revert to doing a virtual copy if the
	     * copied range goes beyond those limits.  Otherwise, mach_vm_read()
	     * of the commpage would now fail when it used to work.
	     */
	    (src_start >= vm_map_min(src_map) &&
	    src_start < vm_map_max(src_map) &&
	    src_end >= vm_map_min(src_map) &&
	    src_end < vm_map_max(src_map))) {
		return vm_map_copyin_kernel_buffer(src_map, src_addr, len,
		           src_destroy, copy_result);
	}

	/*
	 *	Allocate a header element for the list.
	 *
	 *	Use the start and end in the header to
	 *	remember the endpoints prior to rounding.
	 */

	copy = vm_map_copy_allocate(VM_MAP_COPY_ENTRY_LIST);
	copy->cpy_hdr.entries_pageable = TRUE;
	copy->cpy_hdr.page_shift = (uint16_t)VM_MAP_PAGE_SHIFT(src_map);
	copy->offset = src_addr;
	copy->size = len;

	new_entry = vm_map_copy_entry_create(copy);

#define RETURN(x)                                               \
	MACRO_BEGIN                                             \
	vm_map_unlock(src_map);                                 \
	if(src_map != base_map)                                 \
	        vm_map_deallocate(src_map);                     \
	if (new_entry != VM_MAP_ENTRY_NULL)                     \
	        vm_map_copy_entry_dispose(new_entry);           \
	vm_map_copy_discard(copy);                              \
	{                                                       \
	        submap_map_t	*_ptr;                          \
                                                                \
	        for(_ptr = parent_maps; _ptr != NULL; _ptr = parent_maps) { \
	                parent_maps=parent_maps->next;          \
	                if (_ptr->parent_map != base_map)       \
	                        vm_map_deallocate(_ptr->parent_map);    \
	                kfree_type(submap_map_t, _ptr);         \
	        }                                               \
	}                                                       \
	MACRO_RETURN(x);                                        \
	MACRO_END

	/*
	 *	Find the beginning of the region.
	 */

	vm_map_lock(src_map);

	/*
	 * Lookup the original "src_addr" rather than the truncated
	 * "src_start", in case "src_start" falls in a non-map-aligned
	 * map entry *before* the map entry that contains "src_addr"...
	 */
	if (!vm_map_lookup_entry(src_map, src_addr, &tmp_entry)) {
		RETURN(KERN_INVALID_ADDRESS);
	}
	if (!tmp_entry->is_sub_map) {
		/*
		 * ... but clip to the map-rounded "src_start" rather than
		 * "src_addr" to preserve map-alignment.  We'll adjust the
		 * first copy entry at the end, if needed.
		 */
		vm_map_clip_start(src_map, tmp_entry, src_start);
	}
	if (src_start < tmp_entry->vme_start) {
		/*
		 * Move "src_start" up to the start of the
		 * first map entry to copy.
		 */
		src_start = tmp_entry->vme_start;
	}
	/* set for later submap fix-up */
	copy_addr = src_start;

	/*
	 *	Go through entries until we get to the end.
	 */

	while (TRUE) {
		vm_map_entry_t  src_entry = tmp_entry;  /* Top-level entry */
		vm_map_size_t   src_size;               /* Size of source
		                                         * map entry (in both
		                                         * maps)
		                                         */

		vm_object_t             src_object;     /* Object to copy */
		vm_object_offset_t      src_offset;

		vm_object_t             new_copy_object;/* vm_object_copy_* result */

		boolean_t       src_needs_copy;         /* Should source map
		                                         * be made read-only
		                                         * for copy-on-write?
		                                         */

		boolean_t       new_entry_needs_copy;   /* Will new entry be COW? */

		boolean_t       was_wired;              /* Was source wired? */
		boolean_t       saved_used_for_jit;     /* Saved used_for_jit. */
		vm_map_version_t version;               /* Version before locks
		                                         * dropped to make copy
		                                         */
		kern_return_t   result;                 /* Return value from
		                                         * copy_strategically.
		                                         */
		while (tmp_entry->is_sub_map) {
			vm_map_size_t submap_len;
			submap_map_t *ptr;

			ptr = kalloc_type(submap_map_t, Z_WAITOK);
			ptr->next = parent_maps;
			parent_maps = ptr;
			ptr->parent_map = src_map;
			ptr->base_start = src_start;
			ptr->base_end = src_end;
			submap_len = tmp_entry->vme_end - src_start;
			if (submap_len > (src_end - src_start)) {
				submap_len = src_end - src_start;
			}
			ptr->base_len = submap_len;

			src_start -= tmp_entry->vme_start;
			src_start += VME_OFFSET(tmp_entry);
			src_end = src_start + submap_len;
			src_map = VME_SUBMAP(tmp_entry);
			vm_map_lock(src_map);
			/* keep an outstanding reference for all maps in */
			/* the parents tree except the base map */
			vm_map_reference(src_map);
			vm_map_unlock(ptr->parent_map);
			if (!vm_map_lookup_entry(
				    src_map, src_start, &tmp_entry)) {
				RETURN(KERN_INVALID_ADDRESS);
			}
			map_share = TRUE;
			if (!tmp_entry->is_sub_map) {
				vm_map_clip_start(src_map, tmp_entry, src_start);
			}
			src_entry = tmp_entry;
		}
		/* we are now in the lowest level submap... */

		if ((VME_OBJECT(tmp_entry) != VM_OBJECT_NULL) &&
		    (VME_OBJECT(tmp_entry)->phys_contiguous)) {
			/* This is not, supported for now.In future */
			/* we will need to detect the phys_contig   */
			/* condition and then upgrade copy_slowly   */
			/* to do physical copy from the device mem  */
			/* based object. We can piggy-back off of   */
			/* the was wired boolean to set-up the      */
			/* proper handling */
			RETURN(KERN_PROTECTION_FAILURE);
		}
		/*
		 *	Create a new address map entry to hold the result.
		 *	Fill in the fields from the appropriate source entries.
		 *	We must unlock the source map to do this if we need
		 *	to allocate a map entry.
		 */
		if (new_entry == VM_MAP_ENTRY_NULL) {
			version.main_timestamp = src_map->timestamp;
			vm_map_unlock(src_map);

			new_entry = vm_map_copy_entry_create(copy);

			vm_map_lock(src_map);
			if ((version.main_timestamp + 1) != src_map->timestamp) {
				if (!vm_map_lookup_entry(src_map, src_start,
				    &tmp_entry)) {
					RETURN(KERN_INVALID_ADDRESS);
				}
				if (!tmp_entry->is_sub_map) {
					vm_map_clip_start(src_map, tmp_entry, src_start);
				}
				continue; /* restart w/ new tmp_entry */
			}
		}

		/*
		 *	Verify that the region can be read.
		 */
		if (((src_entry->protection & VM_PROT_READ) == VM_PROT_NONE &&
		    !use_maxprot) ||
		    (src_entry->max_protection & VM_PROT_READ) == 0) {
			RETURN(KERN_PROTECTION_FAILURE);
		}

		/*
		 *	Clip against the endpoints of the entire region.
		 */

		vm_map_clip_end(src_map, src_entry, src_end);

		src_size = src_entry->vme_end - src_start;
		src_object = VME_OBJECT(src_entry);
		src_offset = VME_OFFSET(src_entry);
		was_wired = (src_entry->wired_count != 0);

		vm_map_entry_copy(src_map, new_entry, src_entry);
		if (new_entry->is_sub_map) {
			/* clr address space specifics */
			new_entry->use_pmap = FALSE;
		} else {
			/*
			 * We're dealing with a copy-on-write operation,
			 * so the resulting mapping should not inherit the
			 * original mapping's accounting settings.
			 * "iokit_acct" should have been cleared in
			 * vm_map_entry_copy().
			 * "use_pmap" should be reset to its default (TRUE)
			 * so that the new mapping gets accounted for in
			 * the task's memory footprint.
			 */
			assert(!new_entry->iokit_acct);
			new_entry->use_pmap = TRUE;
		}

		/*
		 *	Attempt non-blocking copy-on-write optimizations.
		 */

		/*
		 * If we are destroying the source, and the object
		 * is internal, we could move the object reference
		 * from the source to the copy.  The copy is
		 * copy-on-write only if the source is.
		 * We make another reference to the object, because
		 * destroying the source entry will deallocate it.
		 *
		 * This memory transfer has to be atomic, (to prevent
		 * the VM object from being shared or copied while
		 * it's being moved here), so we could only do this
		 * if we won't have to unlock the VM map until the
		 * original mapping has been fully removed.
		 */

RestartCopy:
		if ((src_object == VM_OBJECT_NULL ||
		    (!was_wired && !map_share && !tmp_entry->is_shared
		    && !(debug4k_no_cow_copyin && VM_MAP_PAGE_SHIFT(src_map) < PAGE_SHIFT))) &&
		    vm_object_copy_quickly(
			    VME_OBJECT(new_entry),
			    src_offset,
			    src_size,
			    &src_needs_copy,
			    &new_entry_needs_copy)) {
			new_entry->needs_copy = new_entry_needs_copy;

			/*
			 *	Handle copy-on-write obligations
			 */

			if (src_needs_copy && !tmp_entry->needs_copy) {
				vm_prot_t prot;

				prot = src_entry->protection & ~VM_PROT_WRITE;

				if (override_nx(src_map, VME_ALIAS(src_entry))
				    && prot) {
					prot |= VM_PROT_EXECUTE;
				}

				vm_object_pmap_protect(
					src_object,
					src_offset,
					src_size,
					(src_entry->is_shared ?
					PMAP_NULL
					: src_map->pmap),
					VM_MAP_PAGE_SIZE(src_map),
					src_entry->vme_start,
					prot);

				assert(tmp_entry->wired_count == 0);
				tmp_entry->needs_copy = TRUE;
			}

			/*
			 *	The map has never been unlocked, so it's safe
			 *	to move to the next entry rather than doing
			 *	another lookup.
			 */

			goto CopySuccessful;
		}

		entry_was_shared = tmp_entry->is_shared;

		/*
		 *	Take an object reference, so that we may
		 *	release the map lock(s).
		 */

		assert(src_object != VM_OBJECT_NULL);
		vm_object_reference(src_object);

		/*
		 *	Record the timestamp for later verification.
		 *	Unlock the map.
		 */

		version.main_timestamp = src_map->timestamp;
		vm_map_unlock(src_map); /* Increments timestamp once! */
		saved_src_entry = src_entry;
		tmp_entry = VM_MAP_ENTRY_NULL;
		src_entry = VM_MAP_ENTRY_NULL;

		/*
		 *	Perform the copy
		 */

		if (was_wired ||
		    (src_object->copy_strategy == MEMORY_OBJECT_COPY_DELAY_FORK &&
		    !(flags & VM_MAP_COPYIN_FORK)) ||
		    (debug4k_no_cow_copyin &&
		    VM_MAP_PAGE_SHIFT(src_map) < PAGE_SHIFT)) {
CopySlowly:
			vm_object_lock(src_object);
			result = vm_object_copy_slowly(
				src_object,
				src_offset,
				src_size,
				THREAD_UNINT,
				&new_copy_object);
			/* VME_OBJECT_SET will reset used_for_jit|tpro, so preserve it. */
			saved_used_for_jit = new_entry->used_for_jit;
			VME_OBJECT_SET(new_entry, new_copy_object, false, 0);
			new_entry->used_for_jit = saved_used_for_jit;
			VME_OFFSET_SET(new_entry,
			    src_offset - vm_object_trunc_page(src_offset));
			new_entry->needs_copy = FALSE;
		} else if (src_object->copy_strategy == MEMORY_OBJECT_COPY_SYMMETRIC &&
		    (entry_was_shared || map_share)) {
			vm_object_t new_object;

			vm_object_lock_shared(src_object);
			new_object = vm_object_copy_delayed(
				src_object,
				src_offset,
				src_size,
				TRUE);
			if (new_object == VM_OBJECT_NULL) {
				goto CopySlowly;
			}

			VME_OBJECT_SET(new_entry, new_object, false, 0);
			assert(new_entry->wired_count == 0);
			new_entry->needs_copy = TRUE;
			assert(!new_entry->iokit_acct);
			assert(new_object->purgable == VM_PURGABLE_DENY);
			assertf(new_entry->use_pmap, "src_map %p new_entry %p\n", src_map, new_entry);
			result = KERN_SUCCESS;
		} else {
			vm_object_offset_t new_offset;
			new_offset = VME_OFFSET(new_entry);
			result = vm_object_copy_strategically(src_object,
			    src_offset,
			    src_size,
			    (flags & VM_MAP_COPYIN_FORK),
			    &new_copy_object,
			    &new_offset,
			    &new_entry_needs_copy);
			/* VME_OBJECT_SET will reset used_for_jit, so preserve it. */
			saved_used_for_jit = new_entry->used_for_jit;
			VME_OBJECT_SET(new_entry, new_copy_object, false, 0);
			new_entry->used_for_jit = saved_used_for_jit;
			if (new_offset != VME_OFFSET(new_entry)) {
				VME_OFFSET_SET(new_entry, new_offset);
			}

			new_entry->needs_copy = new_entry_needs_copy;
		}

		if (result == KERN_SUCCESS &&
		    ((preserve_purgeable &&
		    src_object->purgable != VM_PURGABLE_DENY) ||
		    new_entry->used_for_jit)) {
			/*
			 * Purgeable objects should be COPY_NONE, true share;
			 * this should be propogated to the copy.
			 *
			 * Also force mappings the pmap specially protects to
			 * be COPY_NONE; trying to COW these mappings would
			 * change the effective protections, which could have
			 * side effects if the pmap layer relies on the
			 * specified protections.
			 */

			vm_object_t     new_object;

			new_object = VME_OBJECT(new_entry);
			assert(new_object != src_object);
			vm_object_lock(new_object);
			assert(new_object->ref_count == 1);
			assert(new_object->shadow == VM_OBJECT_NULL);
			assert(new_object->vo_copy == VM_OBJECT_NULL);
			assert(new_object->vo_owner == NULL);

			new_object->copy_strategy = MEMORY_OBJECT_COPY_NONE;

			if (preserve_purgeable &&
			    src_object->purgable != VM_PURGABLE_DENY) {
				VM_OBJECT_SET_TRUE_SHARE(new_object, TRUE);

				/* start as non-volatile with no owner... */
				VM_OBJECT_SET_PURGABLE(new_object, VM_PURGABLE_NONVOLATILE);
				vm_purgeable_nonvolatile_enqueue(new_object, NULL);
				/* ... and move to src_object's purgeable state */
				if (src_object->purgable != VM_PURGABLE_NONVOLATILE) {
					int state;
					state = src_object->purgable;
					vm_object_purgable_control(
						new_object,
						VM_PURGABLE_SET_STATE_FROM_KERNEL,
						&state);
				}
				/* no pmap accounting for purgeable objects */
				new_entry->use_pmap = FALSE;
			}

			vm_object_unlock(new_object);
			new_object = VM_OBJECT_NULL;
		}

		if (result != KERN_SUCCESS &&
		    result != KERN_MEMORY_RESTART_COPY) {
			vm_map_lock(src_map);
			RETURN(result);
		}

		/*
		 *	Throw away the extra reference
		 */

		vm_object_deallocate(src_object);

		/*
		 *	Verify that the map has not substantially
		 *	changed while the copy was being made.
		 */

		vm_map_lock(src_map);

		if ((version.main_timestamp + 1) == src_map->timestamp) {
			/* src_map hasn't changed: src_entry is still valid */
			src_entry = saved_src_entry;
			goto VerificationSuccessful;
		}

		/*
		 *	Simple version comparison failed.
		 *
		 *	Retry the lookup and verify that the
		 *	same object/offset are still present.
		 *
		 *	[Note: a memory manager that colludes with
		 *	the calling task can detect that we have
		 *	cheated.  While the map was unlocked, the
		 *	mapping could have been changed and restored.]
		 */

		if (!vm_map_lookup_entry(src_map, src_start, &tmp_entry)) {
			if (result != KERN_MEMORY_RESTART_COPY) {
				vm_object_deallocate(VME_OBJECT(new_entry));
				VME_OBJECT_SET(new_entry, VM_OBJECT_NULL, false, 0);
				/* reset accounting state */
				new_entry->iokit_acct = FALSE;
				new_entry->use_pmap = TRUE;
			}
			RETURN(KERN_INVALID_ADDRESS);
		}

		src_entry = tmp_entry;
		vm_map_clip_start(src_map, src_entry, src_start);

		if ((((src_entry->protection & VM_PROT_READ) == VM_PROT_NONE) &&
		    !use_maxprot) ||
		    ((src_entry->max_protection & VM_PROT_READ) == 0)) {
			goto VerificationFailed;
		}

		if (src_entry->vme_end < new_entry->vme_end) {
			/*
			 * This entry might have been shortened
			 * (vm_map_clip_end) or been replaced with
			 * an entry that ends closer to "src_start"
			 * than before.
			 * Adjust "new_entry" accordingly; copying
			 * less memory would be correct but we also
			 * redo the copy (see below) if the new entry
			 * no longer points at the same object/offset.
			 */
			assert(VM_MAP_PAGE_ALIGNED(src_entry->vme_end,
			    VM_MAP_COPY_PAGE_MASK(copy)));
			new_entry->vme_end = src_entry->vme_end;
			src_size = new_entry->vme_end - src_start;
		} else if (src_entry->vme_end > new_entry->vme_end) {
			/*
			 * This entry might have been extended
			 * (vm_map_entry_simplify() or coalesce)
			 * or been replaced with an entry that ends farther
			 * from "src_start" than before.
			 *
			 * We've called vm_object_copy_*() only on
			 * the previous <start:end> range, so we can't
			 * just extend new_entry.  We have to re-do
			 * the copy based on the new entry as if it was
			 * pointing at a different object/offset (see
			 * "Verification failed" below).
			 */
		}

		if ((VME_OBJECT(src_entry) != src_object) ||
		    (VME_OFFSET(src_entry) != src_offset) ||
		    (src_entry->vme_end > new_entry->vme_end)) {
			/*
			 *	Verification failed.
			 *
			 *	Start over with this top-level entry.
			 */

VerificationFailed:     ;

			vm_object_deallocate(VME_OBJECT(new_entry));
			tmp_entry = src_entry;
			continue;
		}

		/*
		 *	Verification succeeded.
		 */

VerificationSuccessful:;

		if (result == KERN_MEMORY_RESTART_COPY) {
			goto RestartCopy;
		}

		/*
		 *	Copy succeeded.
		 */

CopySuccessful: ;

		/*
		 *	Link in the new copy entry.
		 */

		vm_map_copy_entry_link(copy, vm_map_copy_last_entry(copy),
		    new_entry);

		/*
		 *	Determine whether the entire region
		 *	has been copied.
		 */
		src_base = src_start;
		src_start = new_entry->vme_end;
		new_entry = VM_MAP_ENTRY_NULL;
		while ((src_start >= src_end) && (src_end != 0)) {
			submap_map_t    *ptr;

			if (src_map == base_map) {
				/* back to the top */
				break;
			}

			ptr = parent_maps;
			assert(ptr != NULL);
			parent_maps = parent_maps->next;

			/* fix up the damage we did in that submap */
			vm_map_simplify_range(src_map,
			    src_base,
			    src_end);

			vm_map_unlock(src_map);
			vm_map_deallocate(src_map);
			vm_map_lock(ptr->parent_map);
			src_map = ptr->parent_map;
			src_base = ptr->base_start;
			src_start = ptr->base_start + ptr->base_len;
			src_end = ptr->base_end;
			if (!vm_map_lookup_entry(src_map,
			    src_start,
			    &tmp_entry) &&
			    (src_end > src_start)) {
				RETURN(KERN_INVALID_ADDRESS);
			}
			kfree_type(submap_map_t, ptr);
			if (parent_maps == NULL) {
				map_share = FALSE;
			}
			src_entry = tmp_entry->vme_prev;
		}

		if ((VM_MAP_PAGE_SHIFT(src_map) != PAGE_SHIFT) &&
		    (src_start >= src_addr + len) &&
		    (src_addr + len != 0)) {
			/*
			 * Stop copying now, even though we haven't reached
			 * "src_end".  We'll adjust the end of the last copy
			 * entry at the end, if needed.
			 *
			 * If src_map's aligment is different from the
			 * system's page-alignment, there could be
			 * extra non-map-aligned map entries between
			 * the original (non-rounded) "src_addr + len"
			 * and the rounded "src_end".
			 * We do not want to copy those map entries since
			 * they're not part of the copied range.
			 */
			break;
		}

		if ((src_start >= src_end) && (src_end != 0)) {
			break;
		}

		/*
		 *	Verify that there are no gaps in the region
		 */

		tmp_entry = src_entry->vme_next;
		if ((tmp_entry->vme_start != src_start) ||
		    (tmp_entry == vm_map_to_entry(src_map))) {
			RETURN(KERN_INVALID_ADDRESS);
		}
	}

	/*
	 * If the source should be destroyed, do it now, since the
	 * copy was successful.
	 */
	if (src_destroy) {
		vmr_flags_t remove_flags = VM_MAP_REMOVE_NO_FLAGS;

		if (src_map == kernel_map) {
			remove_flags |= VM_MAP_REMOVE_KUNWIRE;
		}
		(void)vm_map_remove_and_unlock(src_map,
		    vm_map_trunc_page(src_addr, VM_MAP_PAGE_MASK(src_map)),
		    src_end,
		    remove_flags,
		    KMEM_GUARD_NONE);
	} else {
		/* fix up the damage we did in the base map */
		vm_map_simplify_range(
			src_map,
			vm_map_trunc_page(src_addr,
			VM_MAP_PAGE_MASK(src_map)),
			vm_map_round_page(src_end,
			VM_MAP_PAGE_MASK(src_map)));
		vm_map_unlock(src_map);
	}

	tmp_entry = VM_MAP_ENTRY_NULL;

	if (VM_MAP_PAGE_SHIFT(src_map) > PAGE_SHIFT &&
	    VM_MAP_PAGE_SHIFT(src_map) != VM_MAP_COPY_PAGE_SHIFT(copy)) {
		vm_map_offset_t original_start, original_offset, original_end;

		assert(VM_MAP_COPY_PAGE_MASK(copy) == PAGE_MASK);

		/* adjust alignment of first copy_entry's "vme_start" */
		tmp_entry = vm_map_copy_first_entry(copy);
		if (tmp_entry != vm_map_copy_to_entry(copy)) {
			vm_map_offset_t adjustment;

			original_start = tmp_entry->vme_start;
			original_offset = VME_OFFSET(tmp_entry);

			/* map-align the start of the first copy entry... */
			adjustment = (tmp_entry->vme_start -
			    vm_map_trunc_page(
				    tmp_entry->vme_start,
				    VM_MAP_PAGE_MASK(src_map)));
			tmp_entry->vme_start -= adjustment;
			VME_OFFSET_SET(tmp_entry,
			    VME_OFFSET(tmp_entry) - adjustment);
			copy_addr -= adjustment;
			assert(tmp_entry->vme_start < tmp_entry->vme_end);
			/* ... adjust for mis-aligned start of copy range */
			adjustment =
			    (vm_map_trunc_page(copy->offset,
			    PAGE_MASK) -
			    vm_map_trunc_page(copy->offset,
			    VM_MAP_PAGE_MASK(src_map)));
			if (adjustment) {
				assert(page_aligned(adjustment));
				assert(adjustment < VM_MAP_PAGE_SIZE(src_map));
				tmp_entry->vme_start += adjustment;
				VME_OFFSET_SET(tmp_entry,
				    (VME_OFFSET(tmp_entry) +
				    adjustment));
				copy_addr += adjustment;
				assert(tmp_entry->vme_start < tmp_entry->vme_end);
			}

			/*
			 * Assert that the adjustments haven't exposed
			 * more than was originally copied...
			 */
			assert(tmp_entry->vme_start >= original_start);
			assert(VME_OFFSET(tmp_entry) >= original_offset);
			/*
			 * ... and that it did not adjust outside of a
			 * a single 16K page.
			 */
			assert(vm_map_trunc_page(tmp_entry->vme_start,
			    VM_MAP_PAGE_MASK(src_map)) ==
			    vm_map_trunc_page(original_start,
			    VM_MAP_PAGE_MASK(src_map)));
		}

		/* adjust alignment of last copy_entry's "vme_end" */
		tmp_entry = vm_map_copy_last_entry(copy);
		if (tmp_entry != vm_map_copy_to_entry(copy)) {
			vm_map_offset_t adjustment;

			original_end = tmp_entry->vme_end;

			/* map-align the end of the last copy entry... */
			tmp_entry->vme_end =
			    vm_map_round_page(tmp_entry->vme_end,
			    VM_MAP_PAGE_MASK(src_map));
			/* ... adjust for mis-aligned end of copy range */
			adjustment =
			    (vm_map_round_page((copy->offset +
			    copy->size),
			    VM_MAP_PAGE_MASK(src_map)) -
			    vm_map_round_page((copy->offset +
			    copy->size),
			    PAGE_MASK));
			if (adjustment) {
				assert(page_aligned(adjustment));
				assert(adjustment < VM_MAP_PAGE_SIZE(src_map));
				tmp_entry->vme_end -= adjustment;
				assert(tmp_entry->vme_start < tmp_entry->vme_end);
			}

			/*
			 * Assert that the adjustments haven't exposed
			 * more than was originally copied...
			 */
			assert(tmp_entry->vme_end <= original_end);
			/*
			 * ... and that it did not adjust outside of a
			 * a single 16K page.
			 */
			assert(vm_map_round_page(tmp_entry->vme_end,
			    VM_MAP_PAGE_MASK(src_map)) ==
			    vm_map_round_page(original_end,
			    VM_MAP_PAGE_MASK(src_map)));
		}
	}

	/* Fix-up start and end points in copy.  This is necessary */
	/* when the various entries in the copy object were picked */
	/* up from different sub-maps */

	tmp_entry = vm_map_copy_first_entry(copy);
	copy_size = 0; /* compute actual size */
	while (tmp_entry != vm_map_copy_to_entry(copy)) {
		assert(VM_MAP_PAGE_ALIGNED(
			    copy_addr + (tmp_entry->vme_end -
			    tmp_entry->vme_start),
			    MIN(VM_MAP_COPY_PAGE_MASK(copy), PAGE_MASK)));
		assert(VM_MAP_PAGE_ALIGNED(
			    copy_addr,
			    MIN(VM_MAP_COPY_PAGE_MASK(copy), PAGE_MASK)));

		/*
		 * The copy_entries will be injected directly into the
		 * destination map and might not be "map aligned" there...
		 */
		tmp_entry->map_aligned = FALSE;

		tmp_entry->vme_end = copy_addr +
		    (tmp_entry->vme_end - tmp_entry->vme_start);
		tmp_entry->vme_start = copy_addr;
		assert(tmp_entry->vme_start < tmp_entry->vme_end);
		copy_addr += tmp_entry->vme_end - tmp_entry->vme_start;
		copy_size += tmp_entry->vme_end - tmp_entry->vme_start;
		tmp_entry = (struct vm_map_entry *)tmp_entry->vme_next;
	}

	if (VM_MAP_PAGE_SHIFT(src_map) != PAGE_SHIFT &&
	    copy_size < copy->size) {
		/*
		 * The actual size of the VM map copy is smaller than what
		 * was requested by the caller.  This must be because some
		 * PAGE_SIZE-sized pages are missing at the end of the last
		 * VM_MAP_PAGE_SIZE(src_map)-sized chunk of the range.
		 * The caller might not have been aware of those missing
		 * pages and might not want to be aware of it, which is
		 * fine as long as they don't try to access (and crash on)
		 * those missing pages.
		 * Let's adjust the size of the "copy", to avoid failing
		 * in vm_map_copyout() or vm_map_copy_overwrite().
		 */
		assert(vm_map_round_page(copy_size,
		    VM_MAP_PAGE_MASK(src_map)) ==
		    vm_map_round_page(copy->size,
		    VM_MAP_PAGE_MASK(src_map)));
		copy->size = copy_size;
	}

	*copy_result = copy;
	return KERN_SUCCESS;

#undef  RETURN
}

kern_return_t
vm_map_copy_extract(
	vm_map_t                src_map,
	vm_map_address_t        src_addr,
	vm_map_size_t           len,
	boolean_t               do_copy,
	vm_map_copy_t           *copy_result,   /* OUT */
	vm_prot_t               *cur_prot,      /* IN/OUT */
	vm_prot_t               *max_prot,      /* IN/OUT */
	vm_inherit_t            inheritance,
	vm_map_kernel_flags_t   vmk_flags)
{
	vm_map_copy_t   copy;
	kern_return_t   kr;
	vm_prot_t required_cur_prot, required_max_prot;

	/*
	 *	Check for copies of zero bytes.
	 */

	if (len == 0) {
		*copy_result = VM_MAP_COPY_NULL;
		return KERN_SUCCESS;
	}

	/*
	 *	Check that the end address doesn't overflow
	 */
	if (src_addr + len < src_addr) {
		return KERN_INVALID_ADDRESS;
	}
	if (__improbable(vm_map_range_overflows(src_map, src_addr, len))) {
		return KERN_INVALID_ADDRESS;
	}

	if (VM_MAP_PAGE_SIZE(src_map) < PAGE_SIZE) {
		DEBUG4K_SHARE("src_map %p src_addr 0x%llx src_end 0x%llx\n", src_map, (uint64_t)src_addr, (uint64_t)(src_addr + len));
	}

	required_cur_prot = *cur_prot;
	required_max_prot = *max_prot;

	/*
	 *	Allocate a header element for the list.
	 *
	 *	Use the start and end in the header to
	 *	remember the endpoints prior to rounding.
	 */

	copy = vm_map_copy_allocate(VM_MAP_COPY_ENTRY_LIST);
	copy->cpy_hdr.entries_pageable = vmk_flags.vmkf_copy_pageable;
	copy->offset = 0;
	copy->size = len;

	kr = vm_map_remap_extract(src_map,
	    src_addr,
	    len,
	    do_copy,             /* copy */
	    copy,
	    cur_prot,            /* IN/OUT */
	    max_prot,            /* IN/OUT */
	    inheritance,
	    vmk_flags);
	if (kr != KERN_SUCCESS) {
		vm_map_copy_discard(copy);
		return kr;
	}
	if (required_cur_prot != VM_PROT_NONE) {
		assert((*cur_prot & required_cur_prot) == required_cur_prot);
		assert((*max_prot & required_max_prot) == required_max_prot);
	}

	*copy_result = copy;
	return KERN_SUCCESS;
}

static void
vm_map_fork_share(
	vm_map_t        old_map,
	vm_map_entry_t  old_entry,
	vm_map_t        new_map)
{
	vm_object_t     object;
	vm_map_entry_t  new_entry;

	/*
	 *	New sharing code.  New map entry
	 *	references original object.  Internal
	 *	objects use asynchronous copy algorithm for
	 *	future copies.  First make sure we have
	 *	the right object.  If we need a shadow,
	 *	or someone else already has one, then
	 *	make a new shadow and share it.
	 */

	if (!old_entry->is_sub_map) {
		object = VME_OBJECT(old_entry);
	}

	if (old_entry->is_sub_map) {
		assert(old_entry->wired_count == 0);
#ifndef NO_NESTED_PMAP
#if !PMAP_FORK_NEST
		if (old_entry->use_pmap) {
			kern_return_t   result;

			result = pmap_nest(new_map->pmap,
			    (VME_SUBMAP(old_entry))->pmap,
			    (addr64_t)old_entry->vme_start,
			    (uint64_t)(old_entry->vme_end - old_entry->vme_start));
			if (result) {
				panic("vm_map_fork_share: pmap_nest failed!");
			}
		}
#endif /* !PMAP_FORK_NEST */
#endif  /* NO_NESTED_PMAP */
	} else if (object == VM_OBJECT_NULL) {
		object = vm_object_allocate((vm_map_size_t)(old_entry->vme_end -
		    old_entry->vme_start));
		VME_OFFSET_SET(old_entry, 0);
		VME_OBJECT_SET(old_entry, object, false, 0);
		old_entry->use_pmap = TRUE;
//		assert(!old_entry->needs_copy);
	} else if (object->copy_strategy !=
	    MEMORY_OBJECT_COPY_SYMMETRIC) {
		/*
		 *	We are already using an asymmetric
		 *	copy, and therefore we already have
		 *	the right object.
		 */

		assert(!old_entry->needs_copy);
	} else if (old_entry->needs_copy ||       /* case 1 */
	    object->shadowed ||                 /* case 2 */
	    (!object->true_share &&             /* case 3 */
	    !old_entry->is_shared &&
	    (object->vo_size >
	    (vm_map_size_t)(old_entry->vme_end -
	    old_entry->vme_start)))) {
		bool is_writable;

		/*
		 *	We need to create a shadow.
		 *	There are three cases here.
		 *	In the first case, we need to
		 *	complete a deferred symmetrical
		 *	copy that we participated in.
		 *	In the second and third cases,
		 *	we need to create the shadow so
		 *	that changes that we make to the
		 *	object do not interfere with
		 *	any symmetrical copies which
		 *	have occured (case 2) or which
		 *	might occur (case 3).
		 *
		 *	The first case is when we had
		 *	deferred shadow object creation
		 *	via the entry->needs_copy mechanism.
		 *	This mechanism only works when
		 *	only one entry points to the source
		 *	object, and we are about to create
		 *	a second entry pointing to the
		 *	same object. The problem is that
		 *	there is no way of mapping from
		 *	an object to the entries pointing
		 *	to it. (Deferred shadow creation
		 *	works with one entry because occurs
		 *	at fault time, and we walk from the
		 *	entry to the object when handling
		 *	the fault.)
		 *
		 *	The second case is when the object
		 *	to be shared has already been copied
		 *	with a symmetric copy, but we point
		 *	directly to the object without
		 *	needs_copy set in our entry. (This
		 *	can happen because different ranges
		 *	of an object can be pointed to by
		 *	different entries. In particular,
		 *	a single entry pointing to an object
		 *	can be split by a call to vm_inherit,
		 *	which, combined with task_create, can
		 *	result in the different entries
		 *	having different needs_copy values.)
		 *	The shadowed flag in the object allows
		 *	us to detect this case. The problem
		 *	with this case is that if this object
		 *	has or will have shadows, then we
		 *	must not perform an asymmetric copy
		 *	of this object, since such a copy
		 *	allows the object to be changed, which
		 *	will break the previous symmetrical
		 *	copies (which rely upon the object
		 *	not changing). In a sense, the shadowed
		 *	flag says "don't change this object".
		 *	We fix this by creating a shadow
		 *	object for this object, and sharing
		 *	that. This works because we are free
		 *	to change the shadow object (and thus
		 *	to use an asymmetric copy strategy);
		 *	this is also semantically correct,
		 *	since this object is temporary, and
		 *	therefore a copy of the object is
		 *	as good as the object itself. (This
		 *	is not true for permanent objects,
		 *	since the pager needs to see changes,
		 *	which won't happen if the changes
		 *	are made to a copy.)
		 *
		 *	The third case is when the object
		 *	to be shared has parts sticking
		 *	outside of the entry we're working
		 *	with, and thus may in the future
		 *	be subject to a symmetrical copy.
		 *	(This is a preemptive version of
		 *	case 2.)
		 */
		VME_OBJECT_SHADOW(old_entry,
		    (vm_map_size_t) (old_entry->vme_end -
		    old_entry->vme_start),
		    vm_map_always_shadow(old_map));

		/*
		 *	If we're making a shadow for other than
		 *	copy on write reasons, then we have
		 *	to remove write permission.
		 */

		is_writable = false;
		if (old_entry->protection & VM_PROT_WRITE) {
			is_writable = true;
#if __arm64e__
		} else if (old_entry->used_for_tpro) {
			is_writable = true;
#endif /* __arm64e__ */
		}
		if (!old_entry->needs_copy && is_writable) {
			vm_prot_t prot;

			if (pmap_has_prot_policy(old_map->pmap, old_entry->translated_allow_execute, old_entry->protection)) {
				panic("%s: map %p pmap %p entry %p 0x%llx:0x%llx prot 0x%x",
				    __FUNCTION__, old_map, old_map->pmap,
				    old_entry,
				    (uint64_t)old_entry->vme_start,
				    (uint64_t)old_entry->vme_end,
				    old_entry->protection);
			}

			prot = old_entry->protection & ~VM_PROT_WRITE;

			if (pmap_has_prot_policy(old_map->pmap, old_entry->translated_allow_execute, prot)) {
				panic("%s: map %p pmap %p entry %p 0x%llx:0x%llx prot 0x%x",
				    __FUNCTION__, old_map, old_map->pmap,
				    old_entry,
				    (uint64_t)old_entry->vme_start,
				    (uint64_t)old_entry->vme_end,
				    prot);
			}

			if (override_nx(old_map, VME_ALIAS(old_entry)) && prot) {
				prot |= VM_PROT_EXECUTE;
			}


			if (old_map->mapped_in_other_pmaps) {
				vm_object_pmap_protect(
					VME_OBJECT(old_entry),
					VME_OFFSET(old_entry),
					(old_entry->vme_end -
					old_entry->vme_start),
					PMAP_NULL,
					PAGE_SIZE,
					old_entry->vme_start,
					prot);
			} else {
				pmap_protect(old_map->pmap,
				    old_entry->vme_start,
				    old_entry->vme_end,
				    prot);
			}
		}

		old_entry->needs_copy = FALSE;
		object = VME_OBJECT(old_entry);
	}


	/*
	 *	If object was using a symmetric copy strategy,
	 *	change its copy strategy to the default
	 *	asymmetric copy strategy, which is copy_delay
	 *	in the non-norma case and copy_call in the
	 *	norma case. Bump the reference count for the
	 *	new entry.
	 */

	if (old_entry->is_sub_map) {
		vm_map_reference(VME_SUBMAP(old_entry));
	} else {
		vm_object_lock(object);
		vm_object_reference_locked(object);
		if (object->copy_strategy == MEMORY_OBJECT_COPY_SYMMETRIC) {
			object->copy_strategy = MEMORY_OBJECT_COPY_DELAY;
		}
		vm_object_unlock(object);
	}

	/*
	 *	Clone the entry, using object ref from above.
	 *	Mark both entries as shared.
	 */

	new_entry = vm_map_entry_create(new_map); /* Never the kernel map or descendants */
	vm_map_entry_copy(old_map, new_entry, old_entry);
	old_entry->is_shared = TRUE;
	new_entry->is_shared = TRUE;

	/*
	 * We're dealing with a shared mapping, so the resulting mapping
	 * should inherit some of the original mapping's accounting settings.
	 * "iokit_acct" should have been cleared in vm_map_entry_copy().
	 * "use_pmap" should stay the same as before (if it hasn't been reset
	 * to TRUE when we cleared "iokit_acct").
	 */
	assert(!new_entry->iokit_acct);

	/*
	 *	If old entry's inheritence is VM_INHERIT_NONE,
	 *	the new entry is for corpse fork, remove the
	 *	write permission from the new entry.
	 */
	if (old_entry->inheritance == VM_INHERIT_NONE) {
		new_entry->protection &= ~VM_PROT_WRITE;
		new_entry->max_protection &= ~VM_PROT_WRITE;
	}

	/*
	 *	Insert the entry into the new map -- we
	 *	know we're inserting at the end of the new
	 *	map.
	 */

	vm_map_store_entry_link(new_map, vm_map_last_entry(new_map), new_entry,
	    VM_MAP_KERNEL_FLAGS_NONE);

	/*
	 *	Update the physical map
	 */

	if (old_entry->is_sub_map) {
		/* Bill Angell pmap support goes here */
	} else {
		pmap_copy(new_map->pmap, old_map->pmap, new_entry->vme_start,
		    old_entry->vme_end - old_entry->vme_start,
		    old_entry->vme_start);
	}
}

static boolean_t
vm_map_fork_copy(
	vm_map_t        old_map,
	vm_map_entry_t  *old_entry_p,
	vm_map_t        new_map,
	int             vm_map_copyin_flags)
{
	vm_map_entry_t old_entry = *old_entry_p;
	vm_map_size_t entry_size = old_entry->vme_end - old_entry->vme_start;
	vm_map_offset_t start = old_entry->vme_start;
	vm_map_copy_t copy;
	vm_map_entry_t last = vm_map_last_entry(new_map);

	vm_map_unlock(old_map);
	/*
	 *	Use maxprot version of copyin because we
	 *	care about whether this memory can ever
	 *	be accessed, not just whether it's accessible
	 *	right now.
	 */
	vm_map_copyin_flags |= VM_MAP_COPYIN_USE_MAXPROT;
	if (vm_map_copyin_internal(old_map, start, entry_size,
	    vm_map_copyin_flags, &copy)
	    != KERN_SUCCESS) {
		/*
		 *	The map might have changed while it
		 *	was unlocked, check it again.  Skip
		 *	any blank space or permanently
		 *	unreadable region.
		 */
		vm_map_lock(old_map);
		if (!vm_map_lookup_entry(old_map, start, &last) ||
		    (last->max_protection & VM_PROT_READ) == VM_PROT_NONE) {
			last = last->vme_next;
		}
		*old_entry_p = last;

		/*
		 * XXX	For some error returns, want to
		 * XXX	skip to the next element.  Note
		 *	that INVALID_ADDRESS and
		 *	PROTECTION_FAILURE are handled above.
		 */

		return FALSE;
	}

	/*
	 * Assert that the vm_map_copy is coming from the right
	 * zone and hasn't been forged
	 */
	vm_map_copy_require(copy);

	/*
	 *	Insert the copy into the new map
	 */
	vm_map_copy_insert(new_map, last, copy);

	/*
	 *	Pick up the traversal at the end of
	 *	the copied region.
	 */

	vm_map_lock(old_map);
	start += entry_size;
	if (!vm_map_lookup_entry(old_map, start, &last)) {
		last = last->vme_next;
	} else {
		if (last->vme_start == start) {
			/*
			 * No need to clip here and we don't
			 * want to cause any unnecessary
			 * unnesting...
			 */
		} else {
			vm_map_clip_start(old_map, last, start);
		}
	}
	*old_entry_p = last;

	return TRUE;
}

#if PMAP_FORK_NEST
#define PMAP_FORK_NEST_DEBUG 0
static inline void
vm_map_fork_unnest(
	pmap_t new_pmap,
	vm_map_offset_t pre_nested_start,
	vm_map_offset_t pre_nested_end,
	vm_map_offset_t start,
	vm_map_offset_t end)
{
	kern_return_t kr;
	vm_map_offset_t nesting_mask, start_unnest, end_unnest;

	assertf(pre_nested_start <= pre_nested_end,
	    "pre_nested start 0x%llx end 0x%llx",
	    (uint64_t)pre_nested_start, (uint64_t)pre_nested_end);
	assertf(start <= end,
	    "start 0x%llx end 0x%llx",
	    (uint64_t) start, (uint64_t)end);

	if (pre_nested_start == pre_nested_end) {
		/* nothing was pre-nested: done */
		return;
	}
	if (end <= pre_nested_start) {
		/* fully before pre-nested range: done */
		return;
	}
	if (start >= pre_nested_end) {
		/* fully after pre-nested range: done */
		return;
	}
	/* ignore parts of range outside of pre_nested range */
	if (start < pre_nested_start) {
		start = pre_nested_start;
	}
	if (end > pre_nested_end) {
		end = pre_nested_end;
	}
	nesting_mask = pmap_shared_region_size_min(new_pmap) - 1;
	start_unnest = start & ~nesting_mask;
	end_unnest = (end + nesting_mask) & ~nesting_mask;
	kr = pmap_unnest(new_pmap,
	    (addr64_t)start_unnest,
	    (uint64_t)(end_unnest - start_unnest));
#if PMAP_FORK_NEST_DEBUG
	printf("PMAP_FORK_NEST %s:%d new_pmap %p 0x%llx:0x%llx -> pmap_unnest 0x%llx:0x%llx kr 0x%x\n", __FUNCTION__, __LINE__, new_pmap, (uint64_t)start, (uint64_t)end, (uint64_t)start_unnest, (uint64_t)end_unnest, kr);
#endif /* PMAP_FORK_NEST_DEBUG */
	assertf(kr == KERN_SUCCESS,
	    "0x%llx 0x%llx pmap_unnest(%p, 0x%llx, 0x%llx) -> 0x%x",
	    (uint64_t)start, (uint64_t)end, new_pmap,
	    (uint64_t)start_unnest, (uint64_t)(end_unnest - start_unnest),
	    kr);
}
#endif /* PMAP_FORK_NEST */

void
vm_map_inherit_limits(vm_map_t new_map, const struct _vm_map *old_map)
{
	new_map->size_limit = old_map->size_limit;
	new_map->data_limit = old_map->data_limit;
	new_map->user_wire_limit = old_map->user_wire_limit;
	new_map->reserved_regions = old_map->reserved_regions;
}

/*
 *	vm_map_fork:
 *
 *	Create and return a new map based on the old
 *	map, according to the inheritance values on the
 *	regions in that map and the options.
 *
 *	The source map must not be locked.
 */
vm_map_t
vm_map_fork(
	ledger_t        ledger,
	vm_map_t        old_map,
	int             options)
{
	pmap_t          new_pmap;
	vm_map_t        new_map;
	vm_map_entry_t  old_entry;
	vm_map_size_t   new_size = 0, entry_size;
	vm_map_entry_t  new_entry;
	boolean_t       src_needs_copy;
	boolean_t       new_entry_needs_copy;
	boolean_t       pmap_is64bit;
	int             vm_map_copyin_flags;
	vm_inherit_t    old_entry_inheritance;
	int             map_create_options;
	kern_return_t   footprint_collect_kr;

	if (options & ~(VM_MAP_FORK_SHARE_IF_INHERIT_NONE |
	    VM_MAP_FORK_PRESERVE_PURGEABLE |
	    VM_MAP_FORK_CORPSE_FOOTPRINT)) {
		/* unsupported option */
		return VM_MAP_NULL;
	}

	pmap_is64bit =
#if defined(__i386__) || defined(__x86_64__)
	    old_map->pmap->pm_task_map != TASK_MAP_32BIT;
#elif defined(__arm64__)
	    old_map->pmap->is_64bit;
#else
#error Unknown architecture.
#endif

	unsigned int pmap_flags = 0;
	pmap_flags |= pmap_is64bit ? PMAP_CREATE_64BIT : 0;
#if defined(HAS_APPLE_PAC)
	pmap_flags |= old_map->pmap->disable_jop ? PMAP_CREATE_DISABLE_JOP : 0;
#endif
#if CONFIG_ROSETTA
	pmap_flags |= old_map->pmap->is_rosetta ? PMAP_CREATE_ROSETTA : 0;
#endif
#if PMAP_CREATE_FORCE_4K_PAGES
	if (VM_MAP_PAGE_SIZE(old_map) == FOURK_PAGE_SIZE &&
	    PAGE_SIZE != FOURK_PAGE_SIZE) {
		pmap_flags |= PMAP_CREATE_FORCE_4K_PAGES;
	}
#endif /* PMAP_CREATE_FORCE_4K_PAGES */
	new_pmap = pmap_create_options(ledger, (vm_map_size_t) 0, pmap_flags);
	if (new_pmap == NULL) {
		return VM_MAP_NULL;
	}

	vm_map_reference(old_map);
	vm_map_lock(old_map);

	map_create_options = 0;
	if (old_map->hdr.entries_pageable) {
		map_create_options |= VM_MAP_CREATE_PAGEABLE;
	}
	if (options & VM_MAP_FORK_CORPSE_FOOTPRINT) {
		map_create_options |= VM_MAP_CREATE_CORPSE_FOOTPRINT;
		footprint_collect_kr = KERN_SUCCESS;
	}
	new_map = vm_map_create_options(new_pmap,
	    old_map->min_offset,
	    old_map->max_offset,
	    map_create_options);

	/* inherit cs_enforcement */
	vm_map_cs_enforcement_set(new_map, old_map->cs_enforcement);

	vm_map_lock(new_map);
	vm_commit_pagezero_status(new_map);
	/* inherit the parent map's page size */
	vm_map_set_page_shift(new_map, VM_MAP_PAGE_SHIFT(old_map));

	/* inherit the parent rlimits */
	vm_map_inherit_limits(new_map, old_map);

#if CONFIG_MAP_RANGES
	/* inherit the parent map's VM ranges */
	vm_map_range_fork(new_map, old_map);
#endif

#if CODE_SIGNING_MONITOR
	/* Prepare the monitor for the fork */
	csm_fork_prepare(old_map->pmap, new_pmap);
#endif

#if PMAP_FORK_NEST
	/*
	 * Pre-nest the shared region's pmap.
	 */
	vm_map_offset_t pre_nested_start = 0, pre_nested_end = 0;
	pmap_fork_nest(old_map->pmap, new_pmap,
	    &pre_nested_start, &pre_nested_end);
#if PMAP_FORK_NEST_DEBUG
	printf("PMAP_FORK_NEST %s:%d old %p new %p pre_nested start 0x%llx end 0x%llx\n", __FUNCTION__, __LINE__, old_map->pmap, new_pmap, (uint64_t)pre_nested_start, (uint64_t)pre_nested_end);
#endif /* PMAP_FORK_NEST_DEBUG */
#endif /* PMAP_FORK_NEST */

	for (old_entry = vm_map_first_entry(old_map); old_entry != vm_map_to_entry(old_map);) {
		/*
		 * Abort any corpse collection if the system is shutting down.
		 */
		if ((options & VM_MAP_FORK_CORPSE_FOOTPRINT) &&
		    get_system_inshutdown()) {
#if PMAP_FORK_NEST
			new_entry = vm_map_last_entry(new_map);
			if (new_entry == vm_map_to_entry(new_map)) {
				/* unnest all that was pre-nested */
				vm_map_fork_unnest(new_pmap,
				    pre_nested_start, pre_nested_end,
				    vm_map_min(new_map), vm_map_max(new_map));
			} else if (new_entry->vme_end < vm_map_max(new_map)) {
				/* unnest hole at the end, if pre-nested */
				vm_map_fork_unnest(new_pmap,
				    pre_nested_start, pre_nested_end,
				    new_entry->vme_end, vm_map_max(new_map));
			}
#endif /* PMAP_FORK_NEST */
			vm_map_corpse_footprint_collect_done(new_map);
			vm_map_unlock(new_map);
			vm_map_unlock(old_map);
			vm_map_deallocate(new_map);
			vm_map_deallocate(old_map);
			printf("Aborting corpse map due to system shutdown\n");
			return VM_MAP_NULL;
		}

		entry_size = old_entry->vme_end - old_entry->vme_start;

#if PMAP_FORK_NEST
		/*
		 * Undo any unnecessary pre-nesting.
		 */
		vm_map_offset_t prev_end;
		if (old_entry == vm_map_first_entry(old_map)) {
			prev_end = vm_map_min(old_map);
		} else {
			prev_end = old_entry->vme_prev->vme_end;
		}
		if (prev_end < old_entry->vme_start) {
			/* unnest hole before this entry, if pre-nested */
			vm_map_fork_unnest(new_pmap,
			    pre_nested_start, pre_nested_end,
			    prev_end, old_entry->vme_start);
		}
		if (old_entry->is_sub_map && old_entry->use_pmap) {
			/* keep this entry nested in the child */
#if PMAP_FORK_NEST_DEBUG
			printf("PMAP_FORK_NEST %s:%d new_pmap %p keeping 0x%llx:0x%llx nested\n", __FUNCTION__, __LINE__, new_pmap, (uint64_t)old_entry->vme_start, (uint64_t)old_entry->vme_end);
#endif /* PMAP_FORK_NEST_DEBUG */
		} else {
			/* undo nesting for this entry, if pre-nested */
			vm_map_fork_unnest(new_pmap,
			    pre_nested_start, pre_nested_end,
			    old_entry->vme_start, old_entry->vme_end);
		}
#endif /* PMAP_FORK_NEST */

		old_entry_inheritance = old_entry->inheritance;
		/*
		 * If caller used the VM_MAP_FORK_SHARE_IF_INHERIT_NONE option
		 * share VM_INHERIT_NONE entries that are not backed by a
		 * device pager.
		 */
		if (old_entry_inheritance == VM_INHERIT_NONE &&
		    (options & VM_MAP_FORK_SHARE_IF_INHERIT_NONE) &&
		    (old_entry->protection & VM_PROT_READ) &&
		    !(!old_entry->is_sub_map &&
		    VME_OBJECT(old_entry) != NULL &&
		    VME_OBJECT(old_entry)->pager != NULL &&
		    is_device_pager_ops(
			    VME_OBJECT(old_entry)->pager->mo_pager_ops))) {
			old_entry_inheritance = VM_INHERIT_SHARE;
		}

		if (old_entry_inheritance != VM_INHERIT_NONE &&
		    (options & VM_MAP_FORK_CORPSE_FOOTPRINT) &&
		    footprint_collect_kr == KERN_SUCCESS) {
			/*
			 * The corpse won't have old_map->pmap to query
			 * footprint information, so collect that data now
			 * and store it in new_map->vmmap_corpse_footprint
			 * for later autopsy.
			 */
			footprint_collect_kr =
			    vm_map_corpse_footprint_collect(old_map,
			    old_entry,
			    new_map);
		}

		switch (old_entry_inheritance) {
		case VM_INHERIT_NONE:
			break;

		case VM_INHERIT_SHARE:
			vm_map_fork_share(old_map, old_entry, new_map);
			new_size += entry_size;
			break;

		case VM_INHERIT_COPY:

			/*
			 *	Inline the copy_quickly case;
			 *	upon failure, fall back on call
			 *	to vm_map_fork_copy.
			 */

			if (old_entry->is_sub_map) {
				break;
			}
			if ((old_entry->wired_count != 0) ||
			    ((VME_OBJECT(old_entry) != NULL) &&
			    (VME_OBJECT(old_entry)->true_share))) {
				goto slow_vm_map_fork_copy;
			}

			new_entry = vm_map_entry_create(new_map); /* never the kernel map or descendants */
			vm_map_entry_copy(old_map, new_entry, old_entry);
			if (old_entry->vme_permanent) {
				/* inherit "permanent" on fork() */
				new_entry->vme_permanent = TRUE;
			}

			if (new_entry->used_for_jit == TRUE && new_map->jit_entry_exists == FALSE) {
				new_map->jit_entry_exists = TRUE;
			}

			if (new_entry->is_sub_map) {
				/* clear address space specifics */
				new_entry->use_pmap = FALSE;
			} else {
				/*
				 * We're dealing with a copy-on-write operation,
				 * so the resulting mapping should not inherit
				 * the original mapping's accounting settings.
				 * "iokit_acct" should have been cleared in
				 * vm_map_entry_copy().
				 * "use_pmap" should be reset to its default
				 * (TRUE) so that the new mapping gets
				 * accounted for in the task's memory footprint.
				 */
				assert(!new_entry->iokit_acct);
				new_entry->use_pmap = TRUE;
			}

			if (!vm_object_copy_quickly(
				    VME_OBJECT(new_entry),
				    VME_OFFSET(old_entry),
				    (old_entry->vme_end -
				    old_entry->vme_start),
				    &src_needs_copy,
				    &new_entry_needs_copy)) {
				vm_map_entry_dispose(new_entry);
				goto slow_vm_map_fork_copy;
			}

			/*
			 *	Handle copy-on-write obligations
			 */

			if (src_needs_copy && !old_entry->needs_copy) {
				vm_prot_t prot;

				if (pmap_has_prot_policy(old_map->pmap, old_entry->translated_allow_execute, old_entry->protection)) {
					panic("%s: map %p pmap %p entry %p 0x%llx:0x%llx prot 0x%x",
					    __FUNCTION__,
					    old_map, old_map->pmap, old_entry,
					    (uint64_t)old_entry->vme_start,
					    (uint64_t)old_entry->vme_end,
					    old_entry->protection);
				}

				prot = old_entry->protection & ~VM_PROT_WRITE;

				if (override_nx(old_map, VME_ALIAS(old_entry))
				    && prot) {
					prot |= VM_PROT_EXECUTE;
				}

				if (pmap_has_prot_policy(old_map->pmap, old_entry->translated_allow_execute, prot)) {
					panic("%s: map %p pmap %p entry %p 0x%llx:0x%llx prot 0x%x",
					    __FUNCTION__,
					    old_map, old_map->pmap, old_entry,
					    (uint64_t)old_entry->vme_start,
					    (uint64_t)old_entry->vme_end,
					    prot);
				}

				vm_object_pmap_protect(
					VME_OBJECT(old_entry),
					VME_OFFSET(old_entry),
					(old_entry->vme_end -
					old_entry->vme_start),
					((old_entry->is_shared
					|| old_map->mapped_in_other_pmaps)
					? PMAP_NULL :
					old_map->pmap),
					VM_MAP_PAGE_SIZE(old_map),
					old_entry->vme_start,
					prot);

				assert(old_entry->wired_count == 0);
				old_entry->needs_copy = TRUE;
			}
			new_entry->needs_copy = new_entry_needs_copy;

			/*
			 *	Insert the entry at the end
			 *	of the map.
			 */

			vm_map_store_entry_link(new_map,
			    vm_map_last_entry(new_map),
			    new_entry,
			    VM_MAP_KERNEL_FLAGS_NONE);
			new_size += entry_size;
			break;

slow_vm_map_fork_copy:
			vm_map_copyin_flags = VM_MAP_COPYIN_FORK;
			if (options & VM_MAP_FORK_PRESERVE_PURGEABLE) {
				vm_map_copyin_flags |=
				    VM_MAP_COPYIN_PRESERVE_PURGEABLE;
			}
			if (vm_map_fork_copy(old_map,
			    &old_entry,
			    new_map,
			    vm_map_copyin_flags)) {
				new_size += entry_size;
			}
			continue;
		}
		old_entry = old_entry->vme_next;
	}

#if PMAP_FORK_NEST
	new_entry = vm_map_last_entry(new_map);
	if (new_entry == vm_map_to_entry(new_map)) {
		/* unnest all that was pre-nested */
		vm_map_fork_unnest(new_pmap,
		    pre_nested_start, pre_nested_end,
		    vm_map_min(new_map), vm_map_max(new_map));
	} else if (new_entry->vme_end < vm_map_max(new_map)) {
		/* unnest hole at the end, if pre-nested */
		vm_map_fork_unnest(new_pmap,
		    pre_nested_start, pre_nested_end,
		    new_entry->vme_end, vm_map_max(new_map));
	}
#endif /* PMAP_FORK_NEST */

#if defined(__arm64__)
	pmap_insert_commpage(new_map->pmap);
#endif /* __arm64__ */

	new_map->size = new_size;

	if (options & VM_MAP_FORK_CORPSE_FOOTPRINT) {
		vm_map_corpse_footprint_collect_done(new_map);
	}

	/* Propagate JIT entitlement for the pmap layer. */
	if (pmap_get_jit_entitled(old_map->pmap)) {
		/* Tell the pmap that it supports JIT. */
		pmap_set_jit_entitled(new_map->pmap);
	}

	/* Propagate TPRO settings for the pmap layer */
	if (pmap_get_tpro(old_map->pmap)) {
		/* Tell the pmap that it supports TPRO */
		pmap_set_tpro(new_map->pmap);
	}


	vm_map_unlock(new_map);
	vm_map_unlock(old_map);
	vm_map_deallocate(old_map);

	return new_map;
}

/*
 * vm_map_exec:
 *
 *      Setup the "new_map" with the proper execution environment according
 *	to the type of executable (platform, 64bit, chroot environment).
 *	Map the comm page and shared region, etc...
 */
kern_return_t
vm_map_exec(
	vm_map_t        new_map,
	task_t          task,
	boolean_t       is64bit,
	void            *fsroot,
	cpu_type_t      cpu,
	cpu_subtype_t   cpu_subtype,
	boolean_t       reslide,
	boolean_t       is_driverkit,
	uint32_t        rsr_version)
{
	SHARED_REGION_TRACE_DEBUG(
		("shared_region: task %p: vm_map_exec(%p,%p,%p,0x%x,0x%x): ->\n",
		(void *)VM_KERNEL_ADDRPERM(current_task()),
		(void *)VM_KERNEL_ADDRPERM(new_map),
		(void *)VM_KERNEL_ADDRPERM(task),
		(void *)VM_KERNEL_ADDRPERM(fsroot),
		cpu,
		cpu_subtype));
	(void) vm_commpage_enter(new_map, task, is64bit);

	(void) vm_shared_region_enter(new_map, task, is64bit, fsroot, cpu, cpu_subtype, reslide, is_driverkit, rsr_version);

	SHARED_REGION_TRACE_DEBUG(
		("shared_region: task %p: vm_map_exec(%p,%p,%p,0x%x,0x%x): <-\n",
		(void *)VM_KERNEL_ADDRPERM(current_task()),
		(void *)VM_KERNEL_ADDRPERM(new_map),
		(void *)VM_KERNEL_ADDRPERM(task),
		(void *)VM_KERNEL_ADDRPERM(fsroot),
		cpu,
		cpu_subtype));

	/*
	 * Some devices have region(s) of memory that shouldn't get allocated by
	 * user processes. The following code creates dummy vm_map_entry_t's for each
	 * of the regions that needs to be reserved to prevent any allocations in
	 * those regions.
	 */
	kern_return_t kr = KERN_FAILURE;
	vm_map_kernel_flags_t vmk_flags = VM_MAP_KERNEL_FLAGS_FIXED_PERMANENT();
	vmk_flags.vmkf_beyond_max = true;

	const struct vm_reserved_region *regions = NULL;
	size_t num_regions = ml_get_vm_reserved_regions(is64bit, &regions);
	assert((num_regions == 0) || (num_regions > 0 && regions != NULL));

	for (size_t i = 0; i < num_regions; ++i) {
		vm_map_offset_t address = regions[i].vmrr_addr;

		kr = vm_map_enter(
			new_map,
			&address,
			regions[i].vmrr_size,
			(vm_map_offset_t)0,
			vmk_flags,
			VM_OBJECT_NULL,
			(vm_object_offset_t)0,
			FALSE,
			VM_PROT_NONE,
			VM_PROT_NONE,
			VM_INHERIT_COPY);

		if (kr != KERN_SUCCESS) {
			panic("Failed to reserve %s region in user map %p %d", regions[i].vmrr_name, new_map, kr);
		}
	}

	new_map->reserved_regions = (num_regions ? TRUE : FALSE);

	return KERN_SUCCESS;
}

uint64_t vm_map_lookup_and_lock_object_copy_slowly_count = 0;
uint64_t vm_map_lookup_and_lock_object_copy_slowly_size = 0;
uint64_t vm_map_lookup_and_lock_object_copy_slowly_max = 0;
uint64_t vm_map_lookup_and_lock_object_copy_slowly_restart = 0;
uint64_t vm_map_lookup_and_lock_object_copy_slowly_error = 0;
uint64_t vm_map_lookup_and_lock_object_copy_strategically_count = 0;
uint64_t vm_map_lookup_and_lock_object_copy_strategically_size = 0;
uint64_t vm_map_lookup_and_lock_object_copy_strategically_max = 0;
uint64_t vm_map_lookup_and_lock_object_copy_strategically_restart = 0;
uint64_t vm_map_lookup_and_lock_object_copy_strategically_error = 0;
uint64_t vm_map_lookup_and_lock_object_copy_shadow_count = 0;
uint64_t vm_map_lookup_and_lock_object_copy_shadow_size = 0;
uint64_t vm_map_lookup_and_lock_object_copy_shadow_max = 0;
/*
 *	vm_map_lookup_and_lock_object:
 *
 *	Finds the VM object, offset, and
 *	protection for a given virtual address in the
 *	specified map, assuming a page fault of the
 *	type specified.
 *
 *	Returns the (object, offset, protection) for
 *	this address, whether it is wired down, and whether
 *	this map has the only reference to the data in question.
 *	In order to later verify this lookup, a "version"
 *	is returned.
 *	If contended != NULL, *contended will be set to
 *	true iff the thread had to spin or block to acquire
 *	an exclusive lock.
 *
 *	The map MUST be locked by the caller and WILL be
 *	locked on exit.  In order to guarantee the
 *	existence of the returned object, it is returned
 *	locked.
 *
 *	If a lookup is requested with "write protection"
 *	specified, the map may be changed to perform virtual
 *	copying operations, although the data referenced will
 *	remain the same.
 */
kern_return_t
vm_map_lookup_and_lock_object(
	vm_map_t                *var_map,       /* IN/OUT */
	vm_map_offset_t         vaddr,
	vm_prot_t               fault_type,
	int                     object_lock_type,
	vm_map_version_t        *out_version,   /* OUT */
	vm_object_t             *object,        /* OUT */
	vm_object_offset_t      *offset,        /* OUT */
	vm_prot_t               *out_prot,      /* OUT */
	boolean_t               *wired,         /* OUT */
	vm_object_fault_info_t  fault_info,     /* OUT */
	vm_map_t                *real_map,      /* OUT */
	bool                    *contended)     /* OUT */
{
	vm_map_entry_t                  entry;
	vm_map_t                        map = *var_map;
	vm_map_t                        old_map = *var_map;
	vm_map_t                        cow_sub_map_parent = VM_MAP_NULL;
	vm_map_offset_t                 cow_parent_vaddr = 0;
	vm_map_offset_t                 old_start = 0;
	vm_map_offset_t                 old_end = 0;
	vm_prot_t                       prot;
	boolean_t                       mask_protections;
	boolean_t                       force_copy;
	boolean_t                       no_force_copy_if_executable;
	boolean_t                       submap_needed_copy;
	vm_prot_t                       original_fault_type;
	vm_map_size_t                   fault_page_mask;

	/*
	 * VM_PROT_MASK means that the caller wants us to use "fault_type"
	 * as a mask against the mapping's actual protections, not as an
	 * absolute value.
	 */
	mask_protections = (fault_type & VM_PROT_IS_MASK) ? TRUE : FALSE;
	force_copy = (fault_type & VM_PROT_COPY) ? TRUE : FALSE;
	no_force_copy_if_executable = (fault_type & VM_PROT_COPY_FAIL_IF_EXECUTABLE) ? TRUE : FALSE;
	fault_type &= VM_PROT_ALL;
	original_fault_type = fault_type;
	if (contended) {
		*contended = false;
	}

	*real_map = map;

	fault_page_mask = MIN(VM_MAP_PAGE_MASK(map), PAGE_MASK);
	vaddr = VM_MAP_TRUNC_PAGE(vaddr, fault_page_mask);

RetryLookup:
	fault_type = original_fault_type;

	/*
	 *	If the map has an interesting hint, try it before calling
	 *	full blown lookup routine.
	 */
	entry = map->hint;

	if ((entry == vm_map_to_entry(map)) ||
	    (vaddr < entry->vme_start) || (vaddr >= entry->vme_end)) {
		vm_map_entry_t  tmp_entry;

		/*
		 *	Entry was either not a valid hint, or the vaddr
		 *	was not contained in the entry, so do a full lookup.
		 */
		if (!vm_map_lookup_entry(map, vaddr, &tmp_entry)) {
			if ((cow_sub_map_parent) && (cow_sub_map_parent != map)) {
				vm_map_unlock(cow_sub_map_parent);
			}
			if ((*real_map != map)
			    && (*real_map != cow_sub_map_parent)) {
				vm_map_unlock(*real_map);
			}
			return KERN_INVALID_ADDRESS;
		}

		entry = tmp_entry;
	}
	if (map == old_map) {
		old_start = entry->vme_start;
		old_end = entry->vme_end;
	}

	/*
	 *	Handle submaps.  Drop lock on upper map, submap is
	 *	returned locked.
	 */

	submap_needed_copy = FALSE;
submap_recurse:
	if (entry->is_sub_map) {
		vm_map_offset_t         local_vaddr;
		vm_map_offset_t         end_delta;
		vm_map_offset_t         start_delta;
		vm_map_offset_t         top_entry_saved_start;
		vm_object_offset_t      top_entry_saved_offset;
		vm_map_entry_t          submap_entry, saved_submap_entry;
		vm_object_offset_t      submap_entry_offset;
		vm_object_size_t        submap_entry_size;
		vm_prot_t               subentry_protection;
		vm_prot_t               subentry_max_protection;
		boolean_t               subentry_no_copy_on_read;
		boolean_t               subentry_permanent;
		boolean_t               subentry_csm_associated;
#if __arm64e__
		boolean_t               subentry_used_for_tpro;
#endif /* __arm64e__ */
		boolean_t               mapped_needs_copy = FALSE;
		vm_map_version_t        version;

		assertf(VM_MAP_PAGE_SHIFT(VME_SUBMAP(entry)) >= VM_MAP_PAGE_SHIFT(map),
		    "map %p (%d) entry %p submap %p (%d)\n",
		    map, VM_MAP_PAGE_SHIFT(map), entry,
		    VME_SUBMAP(entry), VM_MAP_PAGE_SHIFT(VME_SUBMAP(entry)));

		local_vaddr = vaddr;
		top_entry_saved_start = entry->vme_start;
		top_entry_saved_offset = VME_OFFSET(entry);

		if ((entry->use_pmap &&
		    !((fault_type & VM_PROT_WRITE) ||
		    force_copy))) {
			/* if real_map equals map we unlock below */
			if ((*real_map != map) &&
			    (*real_map != cow_sub_map_parent)) {
				vm_map_unlock(*real_map);
			}
			*real_map = VME_SUBMAP(entry);
		}

		if (entry->needs_copy &&
		    ((fault_type & VM_PROT_WRITE) ||
		    force_copy)) {
			if (!mapped_needs_copy) {
				if (vm_map_lock_read_to_write(map)) {
					vm_map_lock_read(map);
					*real_map = map;
					goto RetryLookup;
				}
				vm_map_lock_read(VME_SUBMAP(entry));
				*var_map = VME_SUBMAP(entry);
				cow_sub_map_parent = map;
				/* reset base to map before cow object */
				/* this is the map which will accept   */
				/* the new cow object */
				old_start = entry->vme_start;
				old_end = entry->vme_end;
				cow_parent_vaddr = vaddr;
				mapped_needs_copy = TRUE;
			} else {
				vm_map_lock_read(VME_SUBMAP(entry));
				*var_map = VME_SUBMAP(entry);
				if ((cow_sub_map_parent != map) &&
				    (*real_map != map)) {
					vm_map_unlock(map);
				}
			}
		} else {
			if (entry->needs_copy) {
				submap_needed_copy = TRUE;
			}
			vm_map_lock_read(VME_SUBMAP(entry));
			*var_map = VME_SUBMAP(entry);
			/* leave map locked if it is a target */
			/* cow sub_map above otherwise, just  */
			/* follow the maps down to the object */
			/* here we unlock knowing we are not  */
			/* revisiting the map.  */
			if ((*real_map != map) && (map != cow_sub_map_parent)) {
				vm_map_unlock_read(map);
			}
		}

		entry = NULL;
		map = *var_map;

		/* calculate the offset in the submap for vaddr */
		local_vaddr = (local_vaddr - top_entry_saved_start) + top_entry_saved_offset;
		assertf(VM_MAP_PAGE_ALIGNED(local_vaddr, fault_page_mask),
		    "local_vaddr 0x%llx entry->vme_start 0x%llx fault_page_mask 0x%llx\n",
		    (uint64_t)local_vaddr, (uint64_t)top_entry_saved_start, (uint64_t)fault_page_mask);

RetrySubMap:
		if (!vm_map_lookup_entry(map, local_vaddr, &submap_entry)) {
			if ((cow_sub_map_parent) && (cow_sub_map_parent != map)) {
				vm_map_unlock(cow_sub_map_parent);
			}
			if ((*real_map != map)
			    && (*real_map != cow_sub_map_parent)) {
				vm_map_unlock(*real_map);
			}
			*real_map = map;
			return KERN_INVALID_ADDRESS;
		}

		/* find the attenuated shadow of the underlying object */
		/* on our target map */

		/* in english the submap object may extend beyond the     */
		/* region mapped by the entry or, may only fill a portion */
		/* of it.  For our purposes, we only care if the object   */
		/* doesn't fill.  In this case the area which will        */
		/* ultimately be clipped in the top map will only need    */
		/* to be as big as the portion of the underlying entry    */
		/* which is mapped */
		start_delta = submap_entry->vme_start > top_entry_saved_offset ?
		    submap_entry->vme_start - top_entry_saved_offset : 0;

		end_delta =
		    (top_entry_saved_offset + start_delta + (old_end - old_start)) <=
		    submap_entry->vme_end ?
		    0 : (top_entry_saved_offset +
		    (old_end - old_start))
		    - submap_entry->vme_end;

		old_start += start_delta;
		old_end -= end_delta;

		if (submap_entry->is_sub_map) {
			entry = submap_entry;
			vaddr = local_vaddr;
			goto submap_recurse;
		}

		if (((fault_type & VM_PROT_WRITE) ||
		    force_copy)
		    && cow_sub_map_parent) {
			vm_object_t     sub_object, copy_object;
			vm_object_offset_t copy_offset;
			vm_map_offset_t local_start;
			vm_map_offset_t local_end;
			boolean_t       object_copied = FALSE;
			vm_object_offset_t object_copied_offset = 0;
			boolean_t       object_copied_needs_copy = FALSE;
			kern_return_t   kr = KERN_SUCCESS;

			if (vm_map_lock_read_to_write(map)) {
				vm_map_lock_read(map);
				old_start -= start_delta;
				old_end += end_delta;
				goto RetrySubMap;
			}


			sub_object = VME_OBJECT(submap_entry);
			if (sub_object == VM_OBJECT_NULL) {
				sub_object =
				    vm_object_allocate(
					(vm_map_size_t)
					(submap_entry->vme_end -
					submap_entry->vme_start));
				VME_OBJECT_SET(submap_entry, sub_object, false, 0);
				VME_OFFSET_SET(submap_entry, 0);
				assert(!submap_entry->is_sub_map);
				assert(submap_entry->use_pmap);
			}
			local_start =  local_vaddr -
			    (cow_parent_vaddr - old_start);
			local_end = local_vaddr +
			    (old_end - cow_parent_vaddr);
			vm_map_clip_start(map, submap_entry, local_start);
			vm_map_clip_end(map, submap_entry, local_end);
			if (submap_entry->is_sub_map) {
				/* unnesting was done when clipping */
				assert(!submap_entry->use_pmap);
			}

			/* This is the COW case, lets connect */
			/* an entry in our space to the underlying */
			/* object in the submap, bypassing the  */
			/* submap. */
			submap_entry_offset = VME_OFFSET(submap_entry);
			submap_entry_size = submap_entry->vme_end - submap_entry->vme_start;

			if ((submap_entry->wired_count != 0 ||
			    sub_object->copy_strategy != MEMORY_OBJECT_COPY_SYMMETRIC) &&
			    (submap_entry->protection & VM_PROT_EXECUTE) &&
			    no_force_copy_if_executable) {
//				printf("FBDP map %p entry %p start 0x%llx end 0x%llx wired %d strat %d\n", map, submap_entry, (uint64_t)local_start, (uint64_t)local_end, submap_entry->wired_count, sub_object->copy_strategy);
				if ((cow_sub_map_parent) && (cow_sub_map_parent != map)) {
					vm_map_unlock(cow_sub_map_parent);
				}
				if ((*real_map != map)
				    && (*real_map != cow_sub_map_parent)) {
					vm_map_unlock(*real_map);
				}
				*real_map = map;
				ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_SUBMAP_NO_COW_ON_EXECUTABLE), 0 /* arg */);
				vm_map_lock_write_to_read(map);
				kr = KERN_PROTECTION_FAILURE;
				DTRACE_VM4(submap_no_copy_executable,
				    vm_map_t, map,
				    vm_object_offset_t, submap_entry_offset,
				    vm_object_size_t, submap_entry_size,
				    int, kr);
				return kr;
			}

			if (submap_entry->wired_count != 0) {
				vm_object_reference(sub_object);

				assertf(VM_MAP_PAGE_ALIGNED(VME_OFFSET(submap_entry), VM_MAP_PAGE_MASK(map)),
				    "submap_entry %p offset 0x%llx\n",
				    submap_entry, VME_OFFSET(submap_entry));

				DTRACE_VM6(submap_copy_slowly,
				    vm_map_t, cow_sub_map_parent,
				    vm_map_offset_t, vaddr,
				    vm_map_t, map,
				    vm_object_size_t, submap_entry_size,
				    int, submap_entry->wired_count,
				    int, sub_object->copy_strategy);

				saved_submap_entry = submap_entry;
				version.main_timestamp = map->timestamp;
				vm_map_unlock(map); /* Increments timestamp by 1 */
				submap_entry = VM_MAP_ENTRY_NULL;

				vm_object_lock(sub_object);
				kr = vm_object_copy_slowly(sub_object,
				    submap_entry_offset,
				    submap_entry_size,
				    FALSE,
				    &copy_object);
				object_copied = TRUE;
				object_copied_offset = 0;
				/* 4k: account for extra offset in physical page */
				object_copied_offset += submap_entry_offset - vm_object_trunc_page(submap_entry_offset);
				object_copied_needs_copy = FALSE;
				vm_object_deallocate(sub_object);

				vm_map_lock(map);

				if (kr != KERN_SUCCESS &&
				    kr != KERN_MEMORY_RESTART_COPY) {
					if ((cow_sub_map_parent) && (cow_sub_map_parent != map)) {
						vm_map_unlock(cow_sub_map_parent);
					}
					if ((*real_map != map)
					    && (*real_map != cow_sub_map_parent)) {
						vm_map_unlock(*real_map);
					}
					*real_map = map;
					vm_object_deallocate(copy_object);
					copy_object = VM_OBJECT_NULL;
					ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_SUBMAP_COPY_SLOWLY_FAILED), 0 /* arg */);
					vm_map_lock_write_to_read(map);
					DTRACE_VM4(submap_copy_error_slowly,
					    vm_object_t, sub_object,
					    vm_object_offset_t, submap_entry_offset,
					    vm_object_size_t, submap_entry_size,
					    int, kr);
					vm_map_lookup_and_lock_object_copy_slowly_error++;
					return kr;
				}

				if ((kr == KERN_SUCCESS) &&
				    (version.main_timestamp + 1) == map->timestamp) {
					submap_entry = saved_submap_entry;
				} else {
					saved_submap_entry = NULL;
					old_start -= start_delta;
					old_end += end_delta;
					vm_object_deallocate(copy_object);
					copy_object = VM_OBJECT_NULL;
					vm_map_lock_write_to_read(map);
					vm_map_lookup_and_lock_object_copy_slowly_restart++;
					goto RetrySubMap;
				}
				vm_map_lookup_and_lock_object_copy_slowly_count++;
				vm_map_lookup_and_lock_object_copy_slowly_size += submap_entry_size;
				if (submap_entry_size > vm_map_lookup_and_lock_object_copy_slowly_max) {
					vm_map_lookup_and_lock_object_copy_slowly_max = submap_entry_size;
				}
			} else if (sub_object->copy_strategy != MEMORY_OBJECT_COPY_SYMMETRIC) {
				submap_entry_offset = VME_OFFSET(submap_entry);
				copy_object = VM_OBJECT_NULL;
				object_copied_offset = submap_entry_offset;
				object_copied_needs_copy = FALSE;
				DTRACE_VM6(submap_copy_strategically,
				    vm_map_t, cow_sub_map_parent,
				    vm_map_offset_t, vaddr,
				    vm_map_t, map,
				    vm_object_size_t, submap_entry_size,
				    int, submap_entry->wired_count,
				    int, sub_object->copy_strategy);
				kr = vm_object_copy_strategically(
					sub_object,
					submap_entry_offset,
					submap_entry->vme_end - submap_entry->vme_start,
					false, /* forking */
					&copy_object,
					&object_copied_offset,
					&object_copied_needs_copy);
				if (kr == KERN_MEMORY_RESTART_COPY) {
					old_start -= start_delta;
					old_end += end_delta;
					vm_object_deallocate(copy_object);
					copy_object = VM_OBJECT_NULL;
					vm_map_lock_write_to_read(map);
					vm_map_lookup_and_lock_object_copy_strategically_restart++;
					goto RetrySubMap;
				}
				if (kr != KERN_SUCCESS) {
					if ((cow_sub_map_parent) && (cow_sub_map_parent != map)) {
						vm_map_unlock(cow_sub_map_parent);
					}
					if ((*real_map != map)
					    && (*real_map != cow_sub_map_parent)) {
						vm_map_unlock(*real_map);
					}
					*real_map = map;
					vm_object_deallocate(copy_object);
					copy_object = VM_OBJECT_NULL;
					ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_SUBMAP_COPY_STRAT_FAILED), 0 /* arg */);
					vm_map_lock_write_to_read(map);
					DTRACE_VM4(submap_copy_error_strategically,
					    vm_object_t, sub_object,
					    vm_object_offset_t, submap_entry_offset,
					    vm_object_size_t, submap_entry_size,
					    int, kr);
					vm_map_lookup_and_lock_object_copy_strategically_error++;
					return kr;
				}
				assert(copy_object != VM_OBJECT_NULL);
				assert(copy_object != sub_object);
				object_copied = TRUE;
				vm_map_lookup_and_lock_object_copy_strategically_count++;
				vm_map_lookup_and_lock_object_copy_strategically_size += submap_entry_size;
				if (submap_entry_size > vm_map_lookup_and_lock_object_copy_strategically_max) {
					vm_map_lookup_and_lock_object_copy_strategically_max = submap_entry_size;
				}
			} else {
				/* set up shadow object */
				object_copied = FALSE;
				copy_object = sub_object;
				vm_object_lock(sub_object);
				vm_object_reference_locked(sub_object);
				VM_OBJECT_SET_SHADOWED(sub_object, TRUE);
				vm_object_unlock(sub_object);

				assert(submap_entry->wired_count == 0);
				submap_entry->needs_copy = TRUE;

				prot = submap_entry->protection;
				if (pmap_has_prot_policy(map->pmap, submap_entry->translated_allow_execute, prot)) {
					panic("%s: map %p pmap %p entry %p 0x%llx:0x%llx prot 0x%x",
					    __FUNCTION__,
					    map, map->pmap, submap_entry,
					    (uint64_t)submap_entry->vme_start,
					    (uint64_t)submap_entry->vme_end,
					    prot);
				}
				prot = prot & ~VM_PROT_WRITE;
				if (pmap_has_prot_policy(map->pmap, submap_entry->translated_allow_execute, prot)) {
					panic("%s: map %p pmap %p entry %p 0x%llx:0x%llx prot 0x%x",
					    __FUNCTION__,
					    map, map->pmap, submap_entry,
					    (uint64_t)submap_entry->vme_start,
					    (uint64_t)submap_entry->vme_end,
					    prot);
				}

				if (override_nx(old_map,
				    VME_ALIAS(submap_entry))
				    && prot) {
					prot |= VM_PROT_EXECUTE;
				}

				vm_object_pmap_protect(
					sub_object,
					VME_OFFSET(submap_entry),
					submap_entry->vme_end -
					submap_entry->vme_start,
					(submap_entry->is_shared
					|| map->mapped_in_other_pmaps) ?
					PMAP_NULL : map->pmap,
					VM_MAP_PAGE_SIZE(map),
					submap_entry->vme_start,
					prot);
				vm_map_lookup_and_lock_object_copy_shadow_count++;
				vm_map_lookup_and_lock_object_copy_shadow_size += submap_entry_size;
				if (submap_entry_size > vm_map_lookup_and_lock_object_copy_shadow_max) {
					vm_map_lookup_and_lock_object_copy_shadow_max = submap_entry_size;
				}
			}

			/*
			 * Adjust the fault offset to the submap entry.
			 */
			copy_offset = (local_vaddr -
			    submap_entry->vme_start +
			    VME_OFFSET(submap_entry));

			/* This works diffently than the   */
			/* normal submap case. We go back  */
			/* to the parent of the cow map and*/
			/* clip out the target portion of  */
			/* the sub_map, substituting the   */
			/* new copy object,                */

			subentry_protection = submap_entry->protection;
			subentry_max_protection = submap_entry->max_protection;
			subentry_no_copy_on_read = submap_entry->vme_no_copy_on_read;
			subentry_permanent = submap_entry->vme_permanent;
			subentry_csm_associated = submap_entry->csm_associated;
#if __arm64e__
			subentry_used_for_tpro = submap_entry->used_for_tpro;
#endif // __arm64e__
			vm_map_unlock(map);
			submap_entry = NULL; /* not valid after map unlock */

			local_start = old_start;
			local_end = old_end;
			map = cow_sub_map_parent;
			*var_map = cow_sub_map_parent;
			vaddr = cow_parent_vaddr;
			cow_sub_map_parent = NULL;

			if (!vm_map_lookup_entry(map,
			    vaddr, &entry)) {
				if ((cow_sub_map_parent) && (cow_sub_map_parent != map)) {
					vm_map_unlock(cow_sub_map_parent);
				}
				if ((*real_map != map)
				    && (*real_map != cow_sub_map_parent)) {
					vm_map_unlock(*real_map);
				}
				*real_map = map;
				vm_object_deallocate(
					copy_object);
				copy_object = VM_OBJECT_NULL;
				vm_map_lock_write_to_read(map);
				DTRACE_VM4(submap_lookup_post_unlock,
				    uint64_t, (uint64_t)entry->vme_start,
				    uint64_t, (uint64_t)entry->vme_end,
				    vm_map_offset_t, vaddr,
				    int, object_copied);
				return KERN_INVALID_ADDRESS;
			}

			/* clip out the portion of space */
			/* mapped by the sub map which   */
			/* corresponds to the underlying */
			/* object */

			/*
			 * Clip (and unnest) the smallest nested chunk
			 * possible around the faulting address...
			 */
			local_start = vaddr & ~(pmap_shared_region_size_min(map->pmap) - 1);
			local_end = local_start + pmap_shared_region_size_min(map->pmap);
			/*
			 * ... but don't go beyond the "old_start" to "old_end"
			 * range, to avoid spanning over another VM region
			 * with a possibly different VM object and/or offset.
			 */
			if (local_start < old_start) {
				local_start = old_start;
			}
			if (local_end > old_end) {
				local_end = old_end;
			}
			/*
			 * Adjust copy_offset to the start of the range.
			 */
			copy_offset -= (vaddr - local_start);

			vm_map_clip_start(map, entry, local_start);
			vm_map_clip_end(map, entry, local_end);
			if (entry->is_sub_map) {
				/* unnesting was done when clipping */
				assert(!entry->use_pmap);
			}

			/* substitute copy object for */
			/* shared map entry           */
			vm_map_deallocate(VME_SUBMAP(entry));
			assert(!entry->iokit_acct);
			entry->use_pmap = TRUE;
			VME_OBJECT_SET(entry, copy_object, false, 0);

			/* propagate the submap entry's protections */
			if (entry->protection != VM_PROT_READ) {
				/*
				 * Someone has already altered the top entry's
				 * protections via vm_protect(VM_PROT_COPY).
				 * Respect these new values and ignore the
				 * submap entry's protections.
				 */
			} else {
				/*
				 * Regular copy-on-write: propagate the submap
				 * entry's protections to the top map entry.
				 */
				entry->protection |= subentry_protection;
			}
			entry->max_protection |= subentry_max_protection;
			/* propagate some attributes from subentry */
			entry->vme_no_copy_on_read = subentry_no_copy_on_read;
			entry->vme_permanent = subentry_permanent;
			entry->csm_associated = subentry_csm_associated;
#if __arm64e__
			/* propagate TPRO iff the destination map has TPRO enabled */
			if (subentry_used_for_tpro && vm_map_tpro(map)) {
				entry->used_for_tpro = subentry_used_for_tpro;
			}
#endif /* __arm64e */
			if ((entry->protection & VM_PROT_WRITE) &&
			    (entry->protection & VM_PROT_EXECUTE) &&
#if XNU_TARGET_OS_OSX
			    map->pmap != kernel_pmap &&
			    (vm_map_cs_enforcement(map)
#if __arm64__
			    || !VM_MAP_IS_EXOTIC(map)
#endif /* __arm64__ */
			    ) &&
#endif /* XNU_TARGET_OS_OSX */
#if CODE_SIGNING_MONITOR
			    (csm_address_space_exempt(map->pmap) != KERN_SUCCESS) &&
#endif
			    !(entry->used_for_jit) &&
			    VM_MAP_POLICY_WX_STRIP_X(map)) {
				DTRACE_VM3(cs_wx,
				    uint64_t, (uint64_t)entry->vme_start,
				    uint64_t, (uint64_t)entry->vme_end,
				    vm_prot_t, entry->protection);
				printf("CODE SIGNING: %d[%s] %s:%d(0x%llx,0x%llx,0x%x) can't have both write and exec at the same time\n",
				    proc_selfpid(),
				    (get_bsdtask_info(current_task())
				    ? proc_name_address(get_bsdtask_info(current_task()))
				    : "?"),
				    __FUNCTION__, __LINE__,
#if DEVELOPMENT || DEBUG
				    (uint64_t)entry->vme_start,
				    (uint64_t)entry->vme_end,
#else /* DEVELOPMENT || DEBUG */
				    (uint64_t)0,
				    (uint64_t)0,
#endif /* DEVELOPMENT || DEBUG */
				    entry->protection);
				entry->protection &= ~VM_PROT_EXECUTE;
			}

			if (object_copied) {
				VME_OFFSET_SET(entry, local_start - old_start + object_copied_offset);
				entry->needs_copy = object_copied_needs_copy;
				entry->is_shared = FALSE;
			} else {
				assert(VME_OBJECT(entry) != VM_OBJECT_NULL);
				assert(VME_OBJECT(entry)->copy_strategy == MEMORY_OBJECT_COPY_SYMMETRIC);
				assert(entry->wired_count == 0);
				VME_OFFSET_SET(entry, copy_offset);
				entry->needs_copy = TRUE;
				if (map != old_map) {
					entry->is_shared = TRUE;
				}
			}
			if (entry->inheritance == VM_INHERIT_SHARE) {
				entry->inheritance = VM_INHERIT_COPY;
			}

			vm_map_lock_write_to_read(map);
		} else {
			if ((cow_sub_map_parent)
			    && (cow_sub_map_parent != *real_map)
			    && (cow_sub_map_parent != map)) {
				vm_map_unlock(cow_sub_map_parent);
			}
			entry = submap_entry;
			vaddr = local_vaddr;
		}
	}

	/*
	 *	Check whether this task is allowed to have
	 *	this page.
	 */

	prot = entry->protection;

	if (override_nx(old_map, VME_ALIAS(entry)) && prot) {
		/*
		 * HACK -- if not a stack, then allow execution
		 */
		prot |= VM_PROT_EXECUTE;
	}

#if __arm64e__
	/*
	 * If the entry we're dealing with is TPRO and we have a write
	 * fault, inject VM_PROT_WRITE into protections. This allows us
	 * to maintain RO permissions when not marked as TPRO.
	 */
	if (entry->used_for_tpro && (fault_type & VM_PROT_WRITE)) {
		prot |= VM_PROT_WRITE;
	}
#endif /* __arm64e__ */
	if (mask_protections) {
		fault_type &= prot;
		if (fault_type == VM_PROT_NONE) {
			goto protection_failure;
		}
	}
	if (((fault_type & prot) != fault_type)
#if __arm64__
	    /* prefetch abort in execute-only page */
	    && !(prot == VM_PROT_EXECUTE && fault_type == (VM_PROT_READ | VM_PROT_EXECUTE))
#elif defined(__x86_64__)
	    /* Consider the UEXEC bit when handling an EXECUTE fault */
	    && !((fault_type & VM_PROT_EXECUTE) && !(prot & VM_PROT_EXECUTE) && (prot & VM_PROT_UEXEC))
#endif
	    ) {
protection_failure:
		if (*real_map != map) {
			vm_map_unlock(*real_map);
		}
		*real_map = map;

		if ((fault_type & VM_PROT_EXECUTE) && prot) {
			log_stack_execution_failure((addr64_t)vaddr, prot);
		}

		DTRACE_VM2(prot_fault, int, 1, (uint64_t *), NULL);
		DTRACE_VM3(prot_fault_detailed, vm_prot_t, fault_type, vm_prot_t, prot, void *, vaddr);
		/*
		 * Noisy (esp. internally) and can be inferred from CrashReports. So OFF for now.
		 *
		 * ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_PROTECTION_FAILURE), 0);
		 */
		return KERN_PROTECTION_FAILURE;
	}

	/*
	 *	If this page is not pageable, we have to get
	 *	it for all possible accesses.
	 */

	*wired = (entry->wired_count != 0);
	if (*wired) {
		fault_type = prot;
	}

	/*
	 *	If the entry was copy-on-write, we either ...
	 */

	if (entry->needs_copy) {
		/*
		 *	If we want to write the page, we may as well
		 *	handle that now since we've got the map locked.
		 *
		 *	If we don't need to write the page, we just
		 *	demote the permissions allowed.
		 */

		if ((fault_type & VM_PROT_WRITE) || *wired || force_copy) {
			/*
			 *	Make a new object, and place it in the
			 *	object chain.  Note that no new references
			 *	have appeared -- one just moved from the
			 *	map to the new object.
			 */

			if (vm_map_lock_read_to_write(map)) {
				vm_map_lock_read(map);
				goto RetryLookup;
			}

			if (VME_OBJECT(entry)->shadowed == FALSE) {
				vm_object_lock(VME_OBJECT(entry));
				VM_OBJECT_SET_SHADOWED(VME_OBJECT(entry), TRUE);
				vm_object_unlock(VME_OBJECT(entry));
			}
			VME_OBJECT_SHADOW(entry,
			    (vm_map_size_t) (entry->vme_end -
			    entry->vme_start),
			    vm_map_always_shadow(map));
			entry->needs_copy = FALSE;

			vm_map_lock_write_to_read(map);
		}
		if ((fault_type & VM_PROT_WRITE) == 0 && *wired == 0) {
			/*
			 *	We're attempting to read a copy-on-write
			 *	page -- don't allow writes.
			 */

			prot &= (~VM_PROT_WRITE);
		}
	}

	if (submap_needed_copy && (prot & VM_PROT_WRITE)) {
		/*
		 * We went through a "needs_copy" submap without triggering
		 * a copy, so granting write access to the page would bypass
		 * that submap's "needs_copy".
		 */
		assert(!(fault_type & VM_PROT_WRITE));
		assert(!*wired);
		assert(!force_copy);
		// printf("FBDP %d[%s] submap_needed_copy for %p 0x%llx\n", proc_selfpid(), proc_name_address(current_task()->bsd_info), map, vaddr);
		prot &= ~VM_PROT_WRITE;
	}

	/*
	 *	Create an object if necessary.
	 */
	if (VME_OBJECT(entry) == VM_OBJECT_NULL) {
		if (vm_map_lock_read_to_write(map)) {
			vm_map_lock_read(map);
			goto RetryLookup;
		}

		VME_OBJECT_SET(entry,
		    vm_object_allocate(
			    (vm_map_size_t)(entry->vme_end -
			    entry->vme_start)), false, 0);
		VME_OFFSET_SET(entry, 0);
		assert(entry->use_pmap);
		vm_map_lock_write_to_read(map);
	}

	/*
	 *	Return the object/offset from this entry.  If the entry
	 *	was copy-on-write or empty, it has been fixed up.  Also
	 *	return the protection.
	 */

	*offset = (vaddr - entry->vme_start) + VME_OFFSET(entry);
	*object = VME_OBJECT(entry);
	*out_prot = prot;
	KDBG_FILTERED(MACHDBG_CODE(DBG_MACH_WORKINGSET, VM_MAP_LOOKUP_OBJECT), VM_KERNEL_UNSLIDE_OR_PERM(*object), (unsigned long) VME_ALIAS(entry), 0, 0);

	if (fault_info) {
		fault_info->interruptible = THREAD_UNINT; /* for now... */
		/* ... the caller will change "interruptible" if needed */
		fault_info->cluster_size = 0;
		fault_info->user_tag = VME_ALIAS(entry);
		fault_info->pmap_options = 0;
		if (entry->iokit_acct ||
		    (!entry->is_sub_map && !entry->use_pmap)) {
			fault_info->pmap_options |= PMAP_OPTIONS_ALT_ACCT;
		}
		fault_info->behavior = entry->behavior;
		fault_info->lo_offset = VME_OFFSET(entry);
		fault_info->hi_offset =
		    (entry->vme_end - entry->vme_start) + VME_OFFSET(entry);
		fault_info->no_cache  = entry->no_cache;
		fault_info->stealth = FALSE;
		fault_info->io_sync = FALSE;
		if (entry->used_for_jit ||
#if CODE_SIGNING_MONITOR
		    (csm_address_space_exempt(map->pmap) == KERN_SUCCESS) ||
#endif
		    entry->vme_resilient_codesign) {
			fault_info->cs_bypass = TRUE;
		} else {
			fault_info->cs_bypass = FALSE;
		}
		fault_info->csm_associated = FALSE;
#if CODE_SIGNING_MONITOR
		if (entry->csm_associated) {
			/*
			 * The pmap layer will validate this page
			 * before allowing it to be executed from.
			 */
			fault_info->csm_associated = TRUE;
		}
#endif
		fault_info->mark_zf_absent = FALSE;
		fault_info->batch_pmap_op = FALSE;
		fault_info->resilient_media = entry->vme_resilient_media;
		fault_info->fi_xnu_user_debug = entry->vme_xnu_user_debug;
		fault_info->no_copy_on_read = entry->vme_no_copy_on_read;
#if __arm64e__
		fault_info->fi_used_for_tpro = entry->used_for_tpro;
#else /* __arm64e__ */
		fault_info->fi_used_for_tpro = FALSE;
#endif
		if (entry->translated_allow_execute) {
			fault_info->pmap_options |= PMAP_OPTIONS_TRANSLATED_ALLOW_EXECUTE;
		}
	}

	/*
	 *	Lock the object to prevent it from disappearing
	 */
	if (object_lock_type == OBJECT_LOCK_EXCLUSIVE) {
		if (contended == NULL) {
			vm_object_lock(*object);
		} else {
			*contended = vm_object_lock_check_contended(*object);
		}
	} else {
		vm_object_lock_shared(*object);
	}

	/*
	 *	Save the version number
	 */

	out_version->main_timestamp = map->timestamp;

	return KERN_SUCCESS;
}


/*
 *	vm_map_verify:
 *
 *	Verifies that the map in question has not changed
 *	since the given version. The map has to be locked
 *	("shared" mode is fine) before calling this function
 *	and it will be returned locked too.
 */
boolean_t
vm_map_verify(
	vm_map_t                map,
	vm_map_version_t        *version)       /* REF */
{
	boolean_t       result;

	vm_map_lock_assert_held(map);
	result = (map->timestamp == version->main_timestamp);

	return result;
}

/*
 *	TEMPORARYTEMPORARYTEMPORARYTEMPORARYTEMPORARYTEMPORARY
 *	Goes away after regular vm_region_recurse function migrates to
 *	64 bits
 *	vm_region_recurse: A form of vm_region which follows the
 *	submaps in a target map
 *
 */

kern_return_t
vm_map_region_recurse_64(
	vm_map_t                 map,
	vm_map_offset_t *address,               /* IN/OUT */
	vm_map_size_t           *size,                  /* OUT */
	natural_t               *nesting_depth, /* IN/OUT */
	vm_region_submap_info_64_t      submap_info,    /* IN/OUT */
	mach_msg_type_number_t  *count) /* IN/OUT */
{
	mach_msg_type_number_t  original_count;
	vm_region_extended_info_data_t  extended;
	vm_map_entry_t                  tmp_entry;
	vm_map_offset_t                 user_address;
	unsigned int                    user_max_depth;

	/*
	 * "curr_entry" is the VM map entry preceding or including the
	 * address we're looking for.
	 * "curr_map" is the map or sub-map containing "curr_entry".
	 * "curr_address" is the equivalent of the top map's "user_address"
	 * in the current map.
	 * "curr_offset" is the cumulated offset of "curr_map" in the
	 * target task's address space.
	 * "curr_depth" is the depth of "curr_map" in the chain of
	 * sub-maps.
	 *
	 * "curr_max_below" and "curr_max_above" limit the range (around
	 * "curr_address") we should take into account in the current (sub)map.
	 * They limit the range to what's visible through the map entries
	 * we've traversed from the top map to the current map.
	 *
	 */
	vm_map_entry_t                  curr_entry;
	vm_map_address_t                curr_address;
	vm_map_offset_t                 curr_offset;
	vm_map_t                        curr_map;
	unsigned int                    curr_depth;
	vm_map_offset_t                 curr_max_below, curr_max_above;
	vm_map_offset_t                 curr_skip;

	/*
	 * "next_" is the same as "curr_" but for the VM region immediately
	 * after the address we're looking for.  We need to keep track of this
	 * too because we want to return info about that region if the
	 * address we're looking for is not mapped.
	 */
	vm_map_entry_t                  next_entry;
	vm_map_offset_t                 next_offset;
	vm_map_offset_t                 next_address;
	vm_map_t                        next_map;
	unsigned int                    next_depth;
	vm_map_offset_t                 next_max_below, next_max_above;
	vm_map_offset_t                 next_skip;

	boolean_t                       look_for_pages;
	vm_region_submap_short_info_64_t short_info;
	boolean_t                       do_region_footprint;
	int                             effective_page_size, effective_page_shift;
	boolean_t                       submap_needed_copy;

	if (map == VM_MAP_NULL) {
		/* no address space to work on */
		return KERN_INVALID_ARGUMENT;
	}

	effective_page_shift = vm_self_region_page_shift(map);
	effective_page_size = (1 << effective_page_shift);

	if (*count < VM_REGION_SUBMAP_SHORT_INFO_COUNT_64) {
		/*
		 * "info" structure is not big enough and
		 * would overflow
		 */
		return KERN_INVALID_ARGUMENT;
	}

	do_region_footprint = task_self_region_footprint();
	original_count = *count;

	if (original_count < VM_REGION_SUBMAP_INFO_V0_COUNT_64) {
		*count = VM_REGION_SUBMAP_SHORT_INFO_COUNT_64;
		look_for_pages = FALSE;
		short_info = (vm_region_submap_short_info_64_t) submap_info;
		submap_info = NULL;
	} else {
		look_for_pages = TRUE;
		*count = VM_REGION_SUBMAP_INFO_V0_COUNT_64;
		short_info = NULL;

		if (original_count >= VM_REGION_SUBMAP_INFO_V1_COUNT_64) {
			*count = VM_REGION_SUBMAP_INFO_V1_COUNT_64;
		}
		if (original_count >= VM_REGION_SUBMAP_INFO_V2_COUNT_64) {
			*count = VM_REGION_SUBMAP_INFO_V2_COUNT_64;
		}
	}

	user_address = *address;
	user_max_depth = *nesting_depth;
	submap_needed_copy = FALSE;

	if (not_in_kdp) {
		vm_map_lock_read(map);
	}

recurse_again:
	curr_entry = NULL;
	curr_map = map;
	curr_address = user_address;
	curr_offset = 0;
	curr_skip = 0;
	curr_depth = 0;
	curr_max_above = ((vm_map_offset_t) -1) - curr_address;
	curr_max_below = curr_address;

	next_entry = NULL;
	next_map = NULL;
	next_address = 0;
	next_offset = 0;
	next_skip = 0;
	next_depth = 0;
	next_max_above = (vm_map_offset_t) -1;
	next_max_below = (vm_map_offset_t) -1;

	for (;;) {
		if (vm_map_lookup_entry(curr_map,
		    curr_address,
		    &tmp_entry)) {
			/* tmp_entry contains the address we're looking for */
			curr_entry = tmp_entry;
		} else {
			vm_map_offset_t skip;
			/*
			 * The address is not mapped.  "tmp_entry" is the
			 * map entry preceding the address.  We want the next
			 * one, if it exists.
			 */
			curr_entry = tmp_entry->vme_next;

			if (curr_entry == vm_map_to_entry(curr_map) ||
			    (curr_entry->vme_start >=
			    curr_address + curr_max_above)) {
				/* no next entry at this level: stop looking */
				if (not_in_kdp) {
					vm_map_unlock_read(curr_map);
				}
				curr_entry = NULL;
				curr_map = NULL;
				curr_skip = 0;
				curr_offset = 0;
				curr_depth = 0;
				curr_max_above = 0;
				curr_max_below = 0;
				break;
			}

			/* adjust current address and offset */
			skip = curr_entry->vme_start - curr_address;
			curr_address = curr_entry->vme_start;
			curr_skip += skip;
			curr_offset += skip;
			curr_max_above -= skip;
			curr_max_below = 0;
		}

		/*
		 * Is the next entry at this level closer to the address (or
		 * deeper in the submap chain) than the one we had
		 * so far ?
		 */
		tmp_entry = curr_entry->vme_next;
		if (tmp_entry == vm_map_to_entry(curr_map)) {
			/* no next entry at this level */
		} else if (tmp_entry->vme_start >=
		    curr_address + curr_max_above) {
			/*
			 * tmp_entry is beyond the scope of what we mapped of
			 * this submap in the upper level: ignore it.
			 */
		} else if ((next_entry == NULL) ||
		    (tmp_entry->vme_start + curr_offset <=
		    next_entry->vme_start + next_offset)) {
			/*
			 * We didn't have a "next_entry" or this one is
			 * closer to the address we're looking for:
			 * use this "tmp_entry" as the new "next_entry".
			 */
			if (next_entry != NULL) {
				/* unlock the last "next_map" */
				if (next_map != curr_map && not_in_kdp) {
					vm_map_unlock_read(next_map);
				}
			}
			next_entry = tmp_entry;
			next_map = curr_map;
			next_depth = curr_depth;
			next_address = next_entry->vme_start;
			next_skip = curr_skip;
			next_skip += (next_address - curr_address);
			next_offset = curr_offset;
			next_offset += (next_address - curr_address);
			next_max_above = MIN(next_max_above, curr_max_above);
			next_max_above = MIN(next_max_above,
			    next_entry->vme_end - next_address);
			next_max_below = MIN(next_max_below, curr_max_below);
			next_max_below = MIN(next_max_below,
			    next_address - next_entry->vme_start);
		}

		/*
		 * "curr_max_{above,below}" allow us to keep track of the
		 * portion of the submap that is actually mapped at this level:
		 * the rest of that submap is irrelevant to us, since it's not
		 * mapped here.
		 * The relevant portion of the map starts at
		 * "VME_OFFSET(curr_entry)" up to the size of "curr_entry".
		 */
		curr_max_above = MIN(curr_max_above,
		    curr_entry->vme_end - curr_address);
		curr_max_below = MIN(curr_max_below,
		    curr_address - curr_entry->vme_start);

		if (!curr_entry->is_sub_map ||
		    curr_depth >= user_max_depth) {
			/*
			 * We hit a leaf map or we reached the maximum depth
			 * we could, so stop looking.  Keep the current map
			 * locked.
			 */
			break;
		}

		/*
		 * Get down to the next submap level.
		 */

		if (curr_entry->needs_copy) {
			/* everything below this is effectively copy-on-write */
			submap_needed_copy = TRUE;
		}

		/*
		 * Lock the next level and unlock the current level,
		 * unless we need to keep it locked to access the "next_entry"
		 * later.
		 */
		if (not_in_kdp) {
			vm_map_lock_read(VME_SUBMAP(curr_entry));
		}
		if (curr_map == next_map) {
			/* keep "next_map" locked in case we need it */
		} else {
			/* release this map */
			if (not_in_kdp) {
				vm_map_unlock_read(curr_map);
			}
		}

		/*
		 * Adjust the offset.  "curr_entry" maps the submap
		 * at relative address "curr_entry->vme_start" in the
		 * curr_map but skips the first "VME_OFFSET(curr_entry)"
		 * bytes of the submap.
		 * "curr_offset" always represents the offset of a virtual
		 * address in the curr_map relative to the absolute address
		 * space (i.e. the top-level VM map).
		 */
		curr_offset +=
		    (VME_OFFSET(curr_entry) - curr_entry->vme_start);
		curr_address = user_address + curr_offset;
		/* switch to the submap */
		curr_map = VME_SUBMAP(curr_entry);
		curr_depth++;
		curr_entry = NULL;
	}

// LP64todo: all the current tools are 32bit, obviously never worked for 64b
// so probably should be a real 32b ID vs. ptr.
// Current users just check for equality

	if (curr_entry == NULL) {
		/* no VM region contains the address... */

		if (do_region_footprint && /* we want footprint numbers */
		    next_entry == NULL && /* & there are no more regions */
		    /* & we haven't already provided our fake region: */
		    user_address <= vm_map_last_entry(map)->vme_end) {
			ledger_amount_t ledger_resident, ledger_compressed;

			/*
			 * Add a fake memory region to account for
			 * purgeable and/or ledger-tagged memory that
			 * counts towards this task's memory footprint,
			 * i.e. the resident/compressed pages of non-volatile
			 * objects owned by that task.
			 */
			task_ledgers_footprint(map->pmap->ledger,
			    &ledger_resident,
			    &ledger_compressed);
			if (ledger_resident + ledger_compressed == 0) {
				/* no purgeable memory usage to report */
				return KERN_INVALID_ADDRESS;
			}
			/* fake region to show nonvolatile footprint */
			if (look_for_pages) {
				submap_info->protection = VM_PROT_DEFAULT;
				submap_info->max_protection = VM_PROT_DEFAULT;
				submap_info->inheritance = VM_INHERIT_DEFAULT;
				submap_info->offset = 0;
				submap_info->user_tag = -1;
				submap_info->pages_resident = (unsigned int) (ledger_resident / effective_page_size);
				submap_info->pages_shared_now_private = 0;
				submap_info->pages_swapped_out = (unsigned int) (ledger_compressed / effective_page_size);
				submap_info->pages_dirtied = submap_info->pages_resident;
				submap_info->ref_count = 1;
				submap_info->shadow_depth = 0;
				submap_info->external_pager = 0;
				submap_info->share_mode = SM_PRIVATE;
				if (submap_needed_copy) {
					submap_info->share_mode = SM_COW;
				}
				submap_info->is_submap = 0;
				submap_info->behavior = VM_BEHAVIOR_DEFAULT;
				submap_info->object_id = VM_OBJECT_ID_FAKE(map, task_ledgers.purgeable_nonvolatile);
				submap_info->user_wired_count = 0;
				submap_info->pages_reusable = 0;
			} else {
				short_info->user_tag = -1;
				short_info->offset = 0;
				short_info->protection = VM_PROT_DEFAULT;
				short_info->inheritance = VM_INHERIT_DEFAULT;
				short_info->max_protection = VM_PROT_DEFAULT;
				short_info->behavior = VM_BEHAVIOR_DEFAULT;
				short_info->user_wired_count = 0;
				short_info->is_submap = 0;
				short_info->object_id = VM_OBJECT_ID_FAKE(map, task_ledgers.purgeable_nonvolatile);
				short_info->external_pager = 0;
				short_info->shadow_depth = 0;
				short_info->share_mode = SM_PRIVATE;
				if (submap_needed_copy) {
					short_info->share_mode = SM_COW;
				}
				short_info->ref_count = 1;
			}
			*nesting_depth = 0;
			*size = (vm_map_size_t) (ledger_resident + ledger_compressed);
//			*address = user_address;
			*address = vm_map_last_entry(map)->vme_end;
			return KERN_SUCCESS;
		}

		if (next_entry == NULL) {
			/* ... and no VM region follows it either */
			return KERN_INVALID_ADDRESS;
		}
		/* ... gather info about the next VM region */
		curr_entry = next_entry;
		curr_map = next_map;    /* still locked ... */
		curr_address = next_address;
		curr_skip = next_skip;
		curr_offset = next_offset;
		curr_depth = next_depth;
		curr_max_above = next_max_above;
		curr_max_below = next_max_below;
	} else {
		/* we won't need "next_entry" after all */
		if (next_entry != NULL) {
			/* release "next_map" */
			if (next_map != curr_map && not_in_kdp) {
				vm_map_unlock_read(next_map);
			}
		}
	}
	next_entry = NULL;
	next_map = NULL;
	next_offset = 0;
	next_skip = 0;
	next_depth = 0;
	next_max_below = -1;
	next_max_above = -1;

	if (curr_entry->is_sub_map &&
	    curr_depth < user_max_depth) {
		/*
		 * We're not as deep as we could be:  we must have
		 * gone back up after not finding anything mapped
		 * below the original top-level map entry's.
		 * Let's move "curr_address" forward and recurse again.
		 */
		user_address = curr_address;
		goto recurse_again;
	}

	*nesting_depth = curr_depth;
	*size = curr_max_above + curr_max_below;
	*address = user_address + curr_skip - curr_max_below;

	if (look_for_pages) {
		submap_info->user_tag = VME_ALIAS(curr_entry);
		submap_info->offset = VME_OFFSET(curr_entry);
		submap_info->protection = curr_entry->protection;
		submap_info->inheritance = curr_entry->inheritance;
		submap_info->max_protection = curr_entry->max_protection;
		submap_info->behavior = curr_entry->behavior;
		submap_info->user_wired_count = curr_entry->user_wired_count;
		submap_info->is_submap = curr_entry->is_sub_map;
		if (curr_entry->is_sub_map) {
			submap_info->object_id = VM_OBJECT_ID(VME_SUBMAP(curr_entry));
		} else {
			submap_info->object_id = VM_OBJECT_ID(VME_OBJECT(curr_entry));
		}
	} else {
		short_info->user_tag = VME_ALIAS(curr_entry);
		short_info->offset = VME_OFFSET(curr_entry);
		short_info->protection = curr_entry->protection;
		short_info->inheritance = curr_entry->inheritance;
		short_info->max_protection = curr_entry->max_protection;
		short_info->behavior = curr_entry->behavior;
		short_info->user_wired_count = curr_entry->user_wired_count;
		short_info->is_submap = curr_entry->is_sub_map;
		if (curr_entry->is_sub_map) {
			short_info->object_id = VM_OBJECT_ID(VME_SUBMAP(curr_entry));
		} else {
			short_info->object_id = VM_OBJECT_ID(VME_OBJECT(curr_entry));
		}
	}

	extended.pages_resident = 0;
	extended.pages_swapped_out = 0;
	extended.pages_shared_now_private = 0;
	extended.pages_dirtied = 0;
	extended.pages_reusable = 0;
	extended.external_pager = 0;
	extended.shadow_depth = 0;
	extended.share_mode = SM_EMPTY;
	extended.ref_count = 0;

	if (not_in_kdp) {
		if (!curr_entry->is_sub_map) {
			vm_map_offset_t range_start, range_end;
			range_start = MAX((curr_address - curr_max_below),
			    curr_entry->vme_start);
			range_end = MIN((curr_address + curr_max_above),
			    curr_entry->vme_end);
			vm_map_region_walk(curr_map,
			    range_start,
			    curr_entry,
			    (VME_OFFSET(curr_entry) +
			    (range_start -
			    curr_entry->vme_start)),
			    range_end - range_start,
			    &extended,
			    look_for_pages, VM_REGION_EXTENDED_INFO_COUNT);
			if (extended.external_pager &&
			    extended.ref_count == 2 &&
			    extended.share_mode == SM_SHARED) {
				extended.share_mode = SM_PRIVATE;
			}
			if (submap_needed_copy) {
				extended.share_mode = SM_COW;
			}
		} else {
			if (curr_entry->use_pmap) {
				extended.share_mode = SM_TRUESHARED;
			} else {
				extended.share_mode = SM_PRIVATE;
			}
			extended.ref_count = os_ref_get_count_raw(&VME_SUBMAP(curr_entry)->map_refcnt);
		}
	}

	if (look_for_pages) {
		submap_info->pages_resident = extended.pages_resident;
		submap_info->pages_swapped_out = extended.pages_swapped_out;
		submap_info->pages_shared_now_private =
		    extended.pages_shared_now_private;
		submap_info->pages_dirtied = extended.pages_dirtied;
		submap_info->external_pager = extended.external_pager;
		submap_info->shadow_depth = extended.shadow_depth;
		submap_info->share_mode = extended.share_mode;
		submap_info->ref_count = extended.ref_count;

		if (original_count >= VM_REGION_SUBMAP_INFO_V1_COUNT_64) {
			submap_info->pages_reusable = extended.pages_reusable;
		}
		if (original_count >= VM_REGION_SUBMAP_INFO_V2_COUNT_64) {
			if (curr_entry->is_sub_map) {
				submap_info->object_id_full = (vm_object_id_t)VM_KERNEL_ADDRHASH(VME_SUBMAP(curr_entry));
			} else if (VME_OBJECT(curr_entry)) {
				submap_info->object_id_full = (vm_object_id_t)VM_KERNEL_ADDRHASH(VME_OBJECT(curr_entry));
			} else {
				submap_info->object_id_full = 0ull;
			}
		}
	} else {
		short_info->external_pager = extended.external_pager;
		short_info->shadow_depth = extended.shadow_depth;
		short_info->share_mode = extended.share_mode;
		short_info->ref_count = extended.ref_count;
	}

	if (not_in_kdp) {
		vm_map_unlock_read(curr_map);
	}

	return KERN_SUCCESS;
}

/*
 *	vm_region:
 *
 *	User call to obtain information about a region in
 *	a task's address map. Currently, only one flavor is
 *	supported.
 *
 *	XXX The reserved and behavior fields cannot be filled
 *	    in until the vm merge from the IK is completed, and
 *	    vm_reserve is implemented.
 */

kern_return_t
vm_map_region(
	vm_map_t                 map,
	vm_map_offset_t *address,               /* IN/OUT */
	vm_map_size_t           *size,                  /* OUT */
	vm_region_flavor_t       flavor,                /* IN */
	vm_region_info_t         info,                  /* OUT */
	mach_msg_type_number_t  *count, /* IN/OUT */
	mach_port_t             *object_name)           /* OUT */
{
	vm_map_entry_t          tmp_entry;
	vm_map_entry_t          entry;
	vm_map_offset_t         start;

	if (map == VM_MAP_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	switch (flavor) {
	case VM_REGION_BASIC_INFO:
		/* legacy for old 32-bit objects info */
	{
		vm_region_basic_info_t  basic;

		if (*count < VM_REGION_BASIC_INFO_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}

		basic = (vm_region_basic_info_t) info;
		*count = VM_REGION_BASIC_INFO_COUNT;

		vm_map_lock_read(map);

		start = *address;
		if (!vm_map_lookup_entry(map, start, &tmp_entry)) {
			if ((entry = tmp_entry->vme_next) == vm_map_to_entry(map)) {
				vm_map_unlock_read(map);
				return KERN_INVALID_ADDRESS;
			}
		} else {
			entry = tmp_entry;
		}

		start = entry->vme_start;

		basic->offset = (uint32_t)VME_OFFSET(entry);
		basic->protection = entry->protection;
		basic->inheritance = entry->inheritance;
		basic->max_protection = entry->max_protection;
		basic->behavior = entry->behavior;
		basic->user_wired_count = entry->user_wired_count;
		basic->reserved = entry->is_sub_map;
		*address = start;
		*size = (entry->vme_end - start);

		if (object_name) {
			*object_name = IP_NULL;
		}
		if (entry->is_sub_map) {
			basic->shared = FALSE;
		} else {
			basic->shared = entry->is_shared;
		}

		vm_map_unlock_read(map);
		return KERN_SUCCESS;
	}

	case VM_REGION_BASIC_INFO_64:
	{
		vm_region_basic_info_64_t       basic;

		if (*count < VM_REGION_BASIC_INFO_COUNT_64) {
			return KERN_INVALID_ARGUMENT;
		}

		basic = (vm_region_basic_info_64_t) info;
		*count = VM_REGION_BASIC_INFO_COUNT_64;

		vm_map_lock_read(map);

		start = *address;
		if (!vm_map_lookup_entry(map, start, &tmp_entry)) {
			if ((entry = tmp_entry->vme_next) == vm_map_to_entry(map)) {
				vm_map_unlock_read(map);
				return KERN_INVALID_ADDRESS;
			}
		} else {
			entry = tmp_entry;
		}

		start = entry->vme_start;

		basic->offset = VME_OFFSET(entry);
		basic->protection = entry->protection;
		basic->inheritance = entry->inheritance;
		basic->max_protection = entry->max_protection;
		basic->behavior = entry->behavior;
		basic->user_wired_count = entry->user_wired_count;
		basic->reserved = entry->is_sub_map;
		*address = start;
		*size = (entry->vme_end - start);

		if (object_name) {
			*object_name = IP_NULL;
		}
		if (entry->is_sub_map) {
			basic->shared = FALSE;
		} else {
			basic->shared = entry->is_shared;
		}

		vm_map_unlock_read(map);
		return KERN_SUCCESS;
	}
	case VM_REGION_EXTENDED_INFO:
		if (*count < VM_REGION_EXTENDED_INFO_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}
		OS_FALLTHROUGH;
	case VM_REGION_EXTENDED_INFO__legacy:
		if (*count < VM_REGION_EXTENDED_INFO_COUNT__legacy) {
			return KERN_INVALID_ARGUMENT;
		}

		{
			vm_region_extended_info_t       extended;
			mach_msg_type_number_t original_count;
			int effective_page_size, effective_page_shift;

			extended = (vm_region_extended_info_t) info;

			effective_page_shift = vm_self_region_page_shift(map);
			effective_page_size = (1 << effective_page_shift);

			vm_map_lock_read(map);

			start = *address;
			if (!vm_map_lookup_entry(map, start, &tmp_entry)) {
				if ((entry = tmp_entry->vme_next) == vm_map_to_entry(map)) {
					vm_map_unlock_read(map);
					return KERN_INVALID_ADDRESS;
				}
			} else {
				entry = tmp_entry;
			}
			start = entry->vme_start;

			extended->protection = entry->protection;
			extended->user_tag = VME_ALIAS(entry);
			extended->pages_resident = 0;
			extended->pages_swapped_out = 0;
			extended->pages_shared_now_private = 0;
			extended->pages_dirtied = 0;
			extended->external_pager = 0;
			extended->shadow_depth = 0;

			original_count = *count;
			if (flavor == VM_REGION_EXTENDED_INFO__legacy) {
				*count = VM_REGION_EXTENDED_INFO_COUNT__legacy;
			} else {
				extended->pages_reusable = 0;
				*count = VM_REGION_EXTENDED_INFO_COUNT;
			}

			vm_map_region_walk(map, start, entry, VME_OFFSET(entry), entry->vme_end - start, extended, TRUE, *count);

			if (extended->external_pager && extended->ref_count == 2 && extended->share_mode == SM_SHARED) {
				extended->share_mode = SM_PRIVATE;
			}

			if (object_name) {
				*object_name = IP_NULL;
			}
			*address = start;
			*size = (entry->vme_end - start);

			vm_map_unlock_read(map);
			return KERN_SUCCESS;
		}
	case VM_REGION_TOP_INFO:
	{
		vm_region_top_info_t    top;

		if (*count < VM_REGION_TOP_INFO_COUNT) {
			return KERN_INVALID_ARGUMENT;
		}

		top = (vm_region_top_info_t) info;
		*count = VM_REGION_TOP_INFO_COUNT;

		vm_map_lock_read(map);

		start = *address;
		if (!vm_map_lookup_entry(map, start, &tmp_entry)) {
			if ((entry = tmp_entry->vme_next) == vm_map_to_entry(map)) {
				vm_map_unlock_read(map);
				return KERN_INVALID_ADDRESS;
			}
		} else {
			entry = tmp_entry;
		}
		start = entry->vme_start;

		top->private_pages_resident = 0;
		top->shared_pages_resident = 0;

		vm_map_region_top_walk(entry, top);

		if (object_name) {
			*object_name = IP_NULL;
		}
		*address = start;
		*size = (entry->vme_end - start);

		vm_map_unlock_read(map);
		return KERN_SUCCESS;
	}
	default:
		return KERN_INVALID_ARGUMENT;
	}
}

#define OBJ_RESIDENT_COUNT(obj, entry_size)                             \
	MIN((entry_size),                                               \
	    ((obj)->all_reusable ?                                      \
	     (obj)->wired_page_count :                                  \
	     (obj)->resident_page_count - (obj)->reusable_page_count))

void
vm_map_region_top_walk(
	vm_map_entry_t             entry,
	vm_region_top_info_t       top)
{
	if (entry->is_sub_map || VME_OBJECT(entry) == 0) {
		top->share_mode = SM_EMPTY;
		top->ref_count = 0;
		top->obj_id = 0;
		return;
	}

	{
		struct  vm_object *obj, *tmp_obj;
		int             ref_count;
		uint32_t        entry_size;

		entry_size = (uint32_t) ((entry->vme_end - entry->vme_start) / PAGE_SIZE_64);

		obj = VME_OBJECT(entry);

		vm_object_lock(obj);

		if ((ref_count = obj->ref_count) > 1 && obj->paging_in_progress) {
			ref_count--;
		}

		assert(obj->reusable_page_count <= obj->resident_page_count);
		if (obj->shadow) {
			if (ref_count == 1) {
				top->private_pages_resident =
				    OBJ_RESIDENT_COUNT(obj, entry_size);
			} else {
				top->shared_pages_resident =
				    OBJ_RESIDENT_COUNT(obj, entry_size);
			}
			top->ref_count  = ref_count;
			top->share_mode = SM_COW;

			while ((tmp_obj = obj->shadow)) {
				vm_object_lock(tmp_obj);
				vm_object_unlock(obj);
				obj = tmp_obj;

				if ((ref_count = obj->ref_count) > 1 && obj->paging_in_progress) {
					ref_count--;
				}

				assert(obj->reusable_page_count <= obj->resident_page_count);
				top->shared_pages_resident +=
				    OBJ_RESIDENT_COUNT(obj, entry_size);
				top->ref_count += ref_count - 1;
			}
		} else {
			if (entry->superpage_size) {
				top->share_mode = SM_LARGE_PAGE;
				top->shared_pages_resident = 0;
				top->private_pages_resident = entry_size;
			} else if (entry->needs_copy) {
				top->share_mode = SM_COW;
				top->shared_pages_resident =
				    OBJ_RESIDENT_COUNT(obj, entry_size);
			} else {
				if (ref_count == 1 ||
				    (ref_count == 2 && obj->named)) {
					top->share_mode = SM_PRIVATE;
					top->private_pages_resident =
					    OBJ_RESIDENT_COUNT(obj,
					    entry_size);
				} else {
					top->share_mode = SM_SHARED;
					top->shared_pages_resident =
					    OBJ_RESIDENT_COUNT(obj,
					    entry_size);
				}
			}
			top->ref_count = ref_count;
		}

		vm_object_unlock(obj);

		/* XXX K64: obj_id will be truncated */
		top->obj_id = (unsigned int) (uintptr_t)VM_KERNEL_ADDRHASH(obj);
	}
}

void
vm_map_region_walk(
	vm_map_t                        map,
	vm_map_offset_t                 va,
	vm_map_entry_t                  entry,
	vm_object_offset_t              offset,
	vm_object_size_t                range,
	vm_region_extended_info_t       extended,
	boolean_t                       look_for_pages,
	mach_msg_type_number_t count)
{
	struct vm_object *obj, *tmp_obj;
	vm_map_offset_t       last_offset;
	int               i;
	int               ref_count;
	struct vm_object        *shadow_object;
	unsigned short          shadow_depth;
	boolean_t         do_region_footprint;
	int                     effective_page_size, effective_page_shift;
	vm_map_offset_t         effective_page_mask;

	do_region_footprint = task_self_region_footprint();

	if ((entry->is_sub_map) ||
	    (VME_OBJECT(entry) == 0) ||
	    (VME_OBJECT(entry)->phys_contiguous &&
	    !entry->superpage_size)) {
		extended->share_mode = SM_EMPTY;
		extended->ref_count = 0;
		return;
	}

	if (entry->superpage_size) {
		extended->shadow_depth = 0;
		extended->share_mode = SM_LARGE_PAGE;
		extended->ref_count = 1;
		extended->external_pager = 0;

		/* TODO4K: Superpage in 4k mode? */
		extended->pages_resident = (unsigned int)(range >> PAGE_SHIFT);
		extended->shadow_depth = 0;
		return;
	}

	effective_page_shift = vm_self_region_page_shift(map);
	effective_page_size = (1 << effective_page_shift);
	effective_page_mask = effective_page_size - 1;

	offset = vm_map_trunc_page(offset, effective_page_mask);

	obj = VME_OBJECT(entry);

	vm_object_lock(obj);

	if ((ref_count = obj->ref_count) > 1 && obj->paging_in_progress) {
		ref_count--;
	}

	if (look_for_pages) {
		for (last_offset = offset + range;
		    offset < last_offset;
		    offset += effective_page_size, va += effective_page_size) {
			if (do_region_footprint) {
				int disp;

				disp = 0;
				if (map->has_corpse_footprint) {
					/*
					 * Query the page info data we saved
					 * while forking the corpse.
					 */
					vm_map_corpse_footprint_query_page_info(
						map,
						va,
						&disp);
				} else {
					/*
					 * Query the pmap.
					 */
					vm_map_footprint_query_page_info(
						map,
						entry,
						va,
						&disp);
				}
				if (disp & VM_PAGE_QUERY_PAGE_PRESENT) {
					extended->pages_resident++;
				}
				if (disp & VM_PAGE_QUERY_PAGE_REUSABLE) {
					extended->pages_reusable++;
				}
				if (disp & VM_PAGE_QUERY_PAGE_DIRTY) {
					extended->pages_dirtied++;
				}
				if (disp & PMAP_QUERY_PAGE_COMPRESSED) {
					extended->pages_swapped_out++;
				}
				continue;
			}

			vm_map_region_look_for_page(map, va, obj,
			    vm_object_trunc_page(offset), ref_count,
			    0, extended, count);
		}

		if (do_region_footprint) {
			goto collect_object_info;
		}
	} else {
collect_object_info:
		shadow_object = obj->shadow;
		shadow_depth = 0;

		if (!(obj->internal)) {
			extended->external_pager = 1;
		}

		if (shadow_object != VM_OBJECT_NULL) {
			vm_object_lock(shadow_object);
			for (;
			    shadow_object != VM_OBJECT_NULL;
			    shadow_depth++) {
				vm_object_t     next_shadow;

				if (!(shadow_object->internal)) {
					extended->external_pager = 1;
				}

				next_shadow = shadow_object->shadow;
				if (next_shadow) {
					vm_object_lock(next_shadow);
				}
				vm_object_unlock(shadow_object);
				shadow_object = next_shadow;
			}
		}
		extended->shadow_depth = shadow_depth;
	}

	if (extended->shadow_depth || entry->needs_copy) {
		extended->share_mode = SM_COW;
	} else {
		if (ref_count == 1) {
			extended->share_mode = SM_PRIVATE;
		} else {
			if (obj->true_share) {
				extended->share_mode = SM_TRUESHARED;
			} else {
				extended->share_mode = SM_SHARED;
			}
		}
	}
	extended->ref_count = ref_count - extended->shadow_depth;

	for (i = 0; i < extended->shadow_depth; i++) {
		if ((tmp_obj = obj->shadow) == 0) {
			break;
		}
		vm_object_lock(tmp_obj);
		vm_object_unlock(obj);

		if ((ref_count = tmp_obj->ref_count) > 1 && tmp_obj->paging_in_progress) {
			ref_count--;
		}

		extended->ref_count += ref_count;
		obj = tmp_obj;
	}
	vm_object_unlock(obj);

	if (extended->share_mode == SM_SHARED) {
		vm_map_entry_t       cur;
		vm_map_entry_t       last;
		int      my_refs;

		obj = VME_OBJECT(entry);
		last = vm_map_to_entry(map);
		my_refs = 0;

		if ((ref_count = obj->ref_count) > 1 && obj->paging_in_progress) {
			ref_count--;
		}
		for (cur = vm_map_first_entry(map); cur != last; cur = cur->vme_next) {
			my_refs += vm_map_region_count_obj_refs(cur, obj);
		}

		if (my_refs == ref_count) {
			extended->share_mode = SM_PRIVATE_ALIASED;
		} else if (my_refs > 1) {
			extended->share_mode = SM_SHARED_ALIASED;
		}
	}
}


/* object is locked on entry and locked on return */


static void
vm_map_region_look_for_page(
	__unused vm_map_t               map,
	__unused vm_map_offset_t        va,
	vm_object_t                     object,
	vm_object_offset_t              offset,
	int                             max_refcnt,
	unsigned short                  depth,
	vm_region_extended_info_t       extended,
	mach_msg_type_number_t count)
{
	vm_page_t       p;
	vm_object_t     shadow;
	int             ref_count;
	vm_object_t     caller_object;

	shadow = object->shadow;
	caller_object = object;


	while (TRUE) {
		if (!(object->internal)) {
			extended->external_pager = 1;
		}

		if ((p = vm_page_lookup(object, offset)) != VM_PAGE_NULL) {
			if (shadow && (max_refcnt == 1)) {
				extended->pages_shared_now_private++;
			}

			if (!p->vmp_fictitious &&
			    (p->vmp_dirty || pmap_is_modified(VM_PAGE_GET_PHYS_PAGE(p)))) {
				extended->pages_dirtied++;
			} else if (count >= VM_REGION_EXTENDED_INFO_COUNT) {
				if (p->vmp_reusable || object->all_reusable) {
					extended->pages_reusable++;
				}
			}

			extended->pages_resident++;

			if (object != caller_object) {
				vm_object_unlock(object);
			}

			return;
		}
		if (object->internal &&
		    object->alive &&
		    !object->terminating &&
		    object->pager_ready) {
			if (VM_COMPRESSOR_PAGER_STATE_GET(object, offset)
			    == VM_EXTERNAL_STATE_EXISTS) {
				/* the pager has that page */
				extended->pages_swapped_out++;
				if (object != caller_object) {
					vm_object_unlock(object);
				}
				return;
			}
		}

		if (shadow) {
			vm_object_lock(shadow);

			if ((ref_count = shadow->ref_count) > 1 && shadow->paging_in_progress) {
				ref_count--;
			}

			if (++depth > extended->shadow_depth) {
				extended->shadow_depth = depth;
			}

			if (ref_count > max_refcnt) {
				max_refcnt = ref_count;
			}

			if (object != caller_object) {
				vm_object_unlock(object);
			}

			offset = offset + object->vo_shadow_offset;
			object = shadow;
			shadow = object->shadow;
			continue;
		}
		if (object != caller_object) {
			vm_object_unlock(object);
		}
		break;
	}
}

static int
vm_map_region_count_obj_refs(
	vm_map_entry_t    entry,
	vm_object_t       object)
{
	int ref_count;
	vm_object_t chk_obj;
	vm_object_t tmp_obj;

	if (entry->is_sub_map || VME_OBJECT(entry) == VM_OBJECT_NULL) {
		return 0;
	}

	ref_count = 0;
	chk_obj = VME_OBJECT(entry);
	vm_object_lock(chk_obj);

	while (chk_obj) {
		if (chk_obj == object) {
			ref_count++;
		}
		tmp_obj = chk_obj->shadow;
		if (tmp_obj) {
			vm_object_lock(tmp_obj);
		}
		vm_object_unlock(chk_obj);

		chk_obj = tmp_obj;
	}

	return ref_count;
}


/*
 *	Routine:	vm_map_simplify
 *
 *	Description:
 *		Attempt to simplify the map representation in
 *		the vicinity of the given starting address.
 *	Note:
 *		This routine is intended primarily to keep the
 *		kernel maps more compact -- they generally don't
 *		benefit from the "expand a map entry" technology
 *		at allocation time because the adjacent entry
 *		is often wired down.
 */
void
vm_map_simplify_entry(
	vm_map_t        map,
	vm_map_entry_t  this_entry)
{
	vm_map_entry_t  prev_entry;

	prev_entry = this_entry->vme_prev;

	if ((this_entry != vm_map_to_entry(map)) &&
	    (prev_entry != vm_map_to_entry(map)) &&

	    (prev_entry->vme_end == this_entry->vme_start) &&

	    (prev_entry->is_sub_map == this_entry->is_sub_map) &&
	    (prev_entry->vme_object_value == this_entry->vme_object_value) &&
	    (prev_entry->vme_kernel_object == this_entry->vme_kernel_object) &&
	    ((VME_OFFSET(prev_entry) + (prev_entry->vme_end -
	    prev_entry->vme_start))
	    == VME_OFFSET(this_entry)) &&

	    (prev_entry->behavior == this_entry->behavior) &&
	    (prev_entry->needs_copy == this_entry->needs_copy) &&
	    (prev_entry->protection == this_entry->protection) &&
	    (prev_entry->max_protection == this_entry->max_protection) &&
	    (prev_entry->inheritance == this_entry->inheritance) &&
	    (prev_entry->use_pmap == this_entry->use_pmap) &&
	    (VME_ALIAS(prev_entry) == VME_ALIAS(this_entry)) &&
	    (prev_entry->no_cache == this_entry->no_cache) &&
	    (prev_entry->vme_permanent == this_entry->vme_permanent) &&
	    (prev_entry->map_aligned == this_entry->map_aligned) &&
	    (prev_entry->zero_wired_pages == this_entry->zero_wired_pages) &&
	    (prev_entry->used_for_jit == this_entry->used_for_jit) &&
#if __arm64e__
	    (prev_entry->used_for_tpro == this_entry->used_for_tpro) &&
#endif
	    (prev_entry->csm_associated == this_entry->csm_associated) &&
	    (prev_entry->vme_xnu_user_debug == this_entry->vme_xnu_user_debug) &&
	    (prev_entry->iokit_acct == this_entry->iokit_acct) &&
	    (prev_entry->vme_resilient_codesign ==
	    this_entry->vme_resilient_codesign) &&
	    (prev_entry->vme_resilient_media ==
	    this_entry->vme_resilient_media) &&
	    (prev_entry->vme_no_copy_on_read == this_entry->vme_no_copy_on_read) &&
	    (prev_entry->translated_allow_execute == this_entry->translated_allow_execute) &&

	    (prev_entry->wired_count == this_entry->wired_count) &&
	    (prev_entry->user_wired_count == this_entry->user_wired_count) &&

	    ((prev_entry->vme_atomic == FALSE) && (this_entry->vme_atomic == FALSE)) &&
	    (prev_entry->in_transition == FALSE) &&
	    (this_entry->in_transition == FALSE) &&
	    (prev_entry->needs_wakeup == FALSE) &&
	    (this_entry->needs_wakeup == FALSE) &&
	    (prev_entry->is_shared == this_entry->is_shared) &&
	    (prev_entry->superpage_size == FALSE) &&
	    (this_entry->superpage_size == FALSE)
	    ) {
		if (prev_entry->vme_permanent) {
			assert(this_entry->vme_permanent);
			prev_entry->vme_permanent = false;
		}
		vm_map_store_entry_unlink(map, prev_entry, true);
		assert(prev_entry->vme_start < this_entry->vme_end);
		if (prev_entry->map_aligned) {
			assert(VM_MAP_PAGE_ALIGNED(prev_entry->vme_start,
			    VM_MAP_PAGE_MASK(map)));
		}
		this_entry->vme_start = prev_entry->vme_start;
		VME_OFFSET_SET(this_entry, VME_OFFSET(prev_entry));

		if (map->holelistenabled) {
			vm_map_store_update_first_free(map, this_entry, TRUE);
		}

		if (prev_entry->is_sub_map) {
			vm_map_deallocate(VME_SUBMAP(prev_entry));
		} else {
			vm_object_deallocate(VME_OBJECT(prev_entry));
		}
		vm_map_entry_dispose(prev_entry);
		SAVE_HINT_MAP_WRITE(map, this_entry);
	}
}

void
vm_map_simplify(
	vm_map_t        map,
	vm_map_offset_t start)
{
	vm_map_entry_t  this_entry;

	vm_map_lock(map);
	if (vm_map_lookup_entry(map, start, &this_entry)) {
		vm_map_simplify_entry(map, this_entry);
		vm_map_simplify_entry(map, this_entry->vme_next);
	}
	vm_map_unlock(map);
}

static void
vm_map_simplify_range(
	vm_map_t        map,
	vm_map_offset_t start,
	vm_map_offset_t end)
{
	vm_map_entry_t  entry;

	/*
	 * The map should be locked (for "write") by the caller.
	 */

	if (start >= end) {
		/* invalid address range */
		return;
	}

	start = vm_map_trunc_page(start,
	    VM_MAP_PAGE_MASK(map));
	end = vm_map_round_page(end,
	    VM_MAP_PAGE_MASK(map));

	if (!vm_map_lookup_entry(map, start, &entry)) {
		/* "start" is not mapped and "entry" ends before "start" */
		if (entry == vm_map_to_entry(map)) {
			/* start with first entry in the map */
			entry = vm_map_first_entry(map);
		} else {
			/* start with next entry */
			entry = entry->vme_next;
		}
	}

	while (entry != vm_map_to_entry(map) &&
	    entry->vme_start <= end) {
		/* try and coalesce "entry" with its previous entry */
		vm_map_simplify_entry(map, entry);
		entry = entry->vme_next;
	}
}


/*
 *	Routine:	vm_map_machine_attribute
 *	Purpose:
 *		Provide machine-specific attributes to mappings,
 *		such as cachability etc. for machines that provide
 *		them.  NUMA architectures and machines with big/strange
 *		caches will use this.
 *	Note:
 *		Responsibilities for locking and checking are handled here,
 *		everything else in the pmap module. If any non-volatile
 *		information must be kept, the pmap module should handle
 *		it itself. [This assumes that attributes do not
 *		need to be inherited, which seems ok to me]
 */
kern_return_t
vm_map_machine_attribute(
	vm_map_t                        map,
	vm_map_offset_t         start,
	vm_map_offset_t         end,
	vm_machine_attribute_t  attribute,
	vm_machine_attribute_val_t* value)              /* IN/OUT */
{
	kern_return_t   ret;
	vm_map_size_t sync_size;
	vm_map_entry_t entry;

	if (start < vm_map_min(map) || end > vm_map_max(map)) {
		return KERN_INVALID_ADDRESS;
	}
	if (__improbable(vm_map_range_overflows(map, start, end - start))) {
		return KERN_INVALID_ADDRESS;
	}

	/* Figure how much memory we need to flush (in page increments) */
	sync_size = end - start;

	vm_map_lock(map);

	if (attribute != MATTR_CACHE) {
		/* If we don't have to find physical addresses, we */
		/* don't have to do an explicit traversal here.    */
		ret = pmap_attribute(map->pmap, start, end - start,
		    attribute, value);
		vm_map_unlock(map);
		return ret;
	}

	ret = KERN_SUCCESS;                                                                             /* Assume it all worked */

	while (sync_size) {
		if (vm_map_lookup_entry(map, start, &entry)) {
			vm_map_size_t   sub_size;
			if ((entry->vme_end - start) > sync_size) {
				sub_size = sync_size;
				sync_size = 0;
			} else {
				sub_size = entry->vme_end - start;
				sync_size -= sub_size;
			}
			if (entry->is_sub_map) {
				vm_map_offset_t sub_start;
				vm_map_offset_t sub_end;

				sub_start = (start - entry->vme_start)
				    + VME_OFFSET(entry);
				sub_end = sub_start + sub_size;
				vm_map_machine_attribute(
					VME_SUBMAP(entry),
					sub_start,
					sub_end,
					attribute, value);
			} else if (VME_OBJECT(entry)) {
				vm_page_t               m;
				vm_object_t             object;
				vm_object_t             base_object;
				vm_object_t             last_object;
				vm_object_offset_t      offset;
				vm_object_offset_t      base_offset;
				vm_map_size_t           range;
				range = sub_size;
				offset = (start - entry->vme_start)
				    + VME_OFFSET(entry);
				offset = vm_object_trunc_page(offset);
				base_offset = offset;
				object = VME_OBJECT(entry);
				base_object = object;
				last_object = NULL;

				vm_object_lock(object);

				while (range) {
					m = vm_page_lookup(
						object, offset);

					if (m && !m->vmp_fictitious) {
						ret =
						    pmap_attribute_cache_sync(
							VM_PAGE_GET_PHYS_PAGE(m),
							PAGE_SIZE,
							attribute, value);
					} else if (object->shadow) {
						offset = offset + object->vo_shadow_offset;
						last_object = object;
						object = object->shadow;
						vm_object_lock(last_object->shadow);
						vm_object_unlock(last_object);
						continue;
					}
					if (range < PAGE_SIZE) {
						range = 0;
					} else {
						range -= PAGE_SIZE;
					}

					if (base_object != object) {
						vm_object_unlock(object);
						vm_object_lock(base_object);
						object = base_object;
					}
					/* Bump to the next page */
					base_offset += PAGE_SIZE;
					offset = base_offset;
				}
				vm_object_unlock(object);
			}
			start += sub_size;
		} else {
			vm_map_unlock(map);
			return KERN_FAILURE;
		}
	}

	vm_map_unlock(map);

	return ret;
}

/*
 *	vm_map_behavior_set:
 *
 *	Sets the paging reference behavior of the specified address
 *	range in the target map.  Paging reference behavior affects
 *	how pagein operations resulting from faults on the map will be
 *	clustered.
 */
kern_return_t
vm_map_behavior_set(
	vm_map_t        map,
	vm_map_offset_t start,
	vm_map_offset_t end,
	vm_behavior_t   new_behavior)
{
	vm_map_entry_t  entry;
	vm_map_entry_t  temp_entry;

	if (start > end ||
	    start < vm_map_min(map) ||
	    end > vm_map_max(map)) {
		return KERN_NO_SPACE;
	}
	if (__improbable(vm_map_range_overflows(map, start, end - start))) {
		return KERN_INVALID_ADDRESS;
	}

	switch (new_behavior) {
	/*
	 * This first block of behaviors all set a persistent state on the specified
	 * memory range.  All we have to do here is to record the desired behavior
	 * in the vm_map_entry_t's.
	 */

	case VM_BEHAVIOR_DEFAULT:
	case VM_BEHAVIOR_RANDOM:
	case VM_BEHAVIOR_SEQUENTIAL:
	case VM_BEHAVIOR_RSEQNTL:
	case VM_BEHAVIOR_ZERO_WIRED_PAGES:
		vm_map_lock(map);

		/*
		 *	The entire address range must be valid for the map.
		 *      Note that vm_map_range_check() does a
		 *	vm_map_lookup_entry() internally and returns the
		 *	entry containing the start of the address range if
		 *	the entire range is valid.
		 */
		if (vm_map_range_check(map, start, end, &temp_entry)) {
			entry = temp_entry;
			vm_map_clip_start(map, entry, start);
		} else {
			vm_map_unlock(map);
			return KERN_INVALID_ADDRESS;
		}

		while ((entry != vm_map_to_entry(map)) && (entry->vme_start < end)) {
			vm_map_clip_end(map, entry, end);
			if (entry->is_sub_map) {
				assert(!entry->use_pmap);
			}

			if (new_behavior == VM_BEHAVIOR_ZERO_WIRED_PAGES) {
				entry->zero_wired_pages = TRUE;
			} else {
				entry->behavior = new_behavior;
			}
			entry = entry->vme_next;
		}

		vm_map_unlock(map);
		break;

	/*
	 * The rest of these are different from the above in that they cause
	 * an immediate action to take place as opposed to setting a behavior that
	 * affects future actions.
	 */

	case VM_BEHAVIOR_WILLNEED:
		return vm_map_willneed(map, start, end);

	case VM_BEHAVIOR_DONTNEED:
		return vm_map_msync(map, start, end - start, VM_SYNC_DEACTIVATE | VM_SYNC_CONTIGUOUS);

	case VM_BEHAVIOR_FREE:
		return vm_map_msync(map, start, end - start, VM_SYNC_KILLPAGES | VM_SYNC_CONTIGUOUS);

	case VM_BEHAVIOR_REUSABLE:
		return vm_map_reusable_pages(map, start, end);

	case VM_BEHAVIOR_REUSE:
		return vm_map_reuse_pages(map, start, end);

	case VM_BEHAVIOR_CAN_REUSE:
		return vm_map_can_reuse(map, start, end);

#if MACH_ASSERT
	case VM_BEHAVIOR_PAGEOUT:
		return vm_map_pageout(map, start, end);
#endif /* MACH_ASSERT */

	case VM_BEHAVIOR_ZERO:
		return vm_map_zero(map, start, end);

	default:
		return KERN_INVALID_ARGUMENT;
	}

	return KERN_SUCCESS;
}


/*
 * Internals for madvise(MADV_WILLNEED) system call.
 *
 * The implementation is to do:-
 * a) read-ahead if the mapping corresponds to a mapped regular file
 * b) or, fault in the pages (zero-fill, decompress etc) if it's an anonymous mapping
 */


static kern_return_t
vm_map_willneed(
	vm_map_t        map,
	vm_map_offset_t start,
	vm_map_offset_t end
	)
{
	vm_map_entry_t                  entry;
	vm_object_t                     object;
	memory_object_t                 pager;
	struct vm_object_fault_info     fault_info = {};
	kern_return_t                   kr;
	vm_object_size_t                len;
	vm_object_offset_t              offset;

	fault_info.interruptible = THREAD_UNINT;        /* ignored value */
	fault_info.behavior      = VM_BEHAVIOR_SEQUENTIAL;
	fault_info.stealth       = TRUE;

	/*
	 * The MADV_WILLNEED operation doesn't require any changes to the
	 * vm_map_entry_t's, so the read lock is sufficient.
	 */

	vm_map_lock_read(map);

	/*
	 * The madvise semantics require that the address range be fully
	 * allocated with no holes.  Otherwise, we're required to return
	 * an error.
	 */

	if (!vm_map_range_check(map, start, end, &entry)) {
		vm_map_unlock_read(map);
		return KERN_INVALID_ADDRESS;
	}

	/*
	 * Examine each vm_map_entry_t in the range.
	 */
	for (; entry != vm_map_to_entry(map) && start < end;) {
		/*
		 * The first time through, the start address could be anywhere
		 * within the vm_map_entry we found.  So adjust the offset to
		 * correspond.  After that, the offset will always be zero to
		 * correspond to the beginning of the current vm_map_entry.
		 */
		offset = (start - entry->vme_start) + VME_OFFSET(entry);

		/*
		 * Set the length so we don't go beyond the end of the
		 * map_entry or beyond the end of the range we were given.
		 * This range could span also multiple map entries all of which
		 * map different files, so make sure we only do the right amount
		 * of I/O for each object.  Note that it's possible for there
		 * to be multiple map entries all referring to the same object
		 * but with different page permissions, but it's not worth
		 * trying to optimize that case.
		 */
		len = MIN(entry->vme_end - start, end - start);

		if ((vm_size_t) len != len) {
			/* 32-bit overflow */
			len = (vm_size_t) (0 - PAGE_SIZE);
		}
		fault_info.cluster_size = (vm_size_t) len;
		fault_info.lo_offset    = offset;
		fault_info.hi_offset    = offset + len;
		fault_info.user_tag     = VME_ALIAS(entry);
		fault_info.pmap_options = 0;
		if (entry->iokit_acct ||
		    (!entry->is_sub_map && !entry->use_pmap)) {
			fault_info.pmap_options |= PMAP_OPTIONS_ALT_ACCT;
		}
		fault_info.fi_xnu_user_debug = entry->vme_xnu_user_debug;

		/*
		 * If the entry is a submap OR there's no read permission
		 * to this mapping, then just skip it.
		 */
		if ((entry->is_sub_map) || (entry->protection & VM_PROT_READ) == 0) {
			entry = entry->vme_next;
			start = entry->vme_start;
			continue;
		}

		object = VME_OBJECT(entry);

		if (object == NULL ||
		    (object && object->internal)) {
			/*
			 * Memory range backed by anonymous memory.
			 */
			vm_size_t region_size = 0, effective_page_size = 0;
			vm_map_offset_t addr = 0, effective_page_mask = 0;

			region_size = len;
			addr = start;

			effective_page_mask = MIN(vm_map_page_mask(current_map()), PAGE_MASK);
			effective_page_size = effective_page_mask + 1;

			vm_map_unlock_read(map);

			while (region_size) {
				vm_pre_fault(
					vm_map_trunc_page(addr, effective_page_mask),
					VM_PROT_READ | VM_PROT_WRITE);

				region_size -= effective_page_size;
				addr += effective_page_size;
			}
		} else {
			/*
			 * Find the file object backing this map entry.  If there is
			 * none, then we simply ignore the "will need" advice for this
			 * entry and go on to the next one.
			 */
			if ((object = find_vnode_object(entry)) == VM_OBJECT_NULL) {
				entry = entry->vme_next;
				start = entry->vme_start;
				continue;
			}

			vm_object_paging_begin(object);
			pager = object->pager;
			vm_object_unlock(object);

			/*
			 * The data_request() could take a long time, so let's
			 * release the map lock to avoid blocking other threads.
			 */
			vm_map_unlock_read(map);

			/*
			 * Get the data from the object asynchronously.
			 *
			 * Note that memory_object_data_request() places limits on the
			 * amount of I/O it will do.  Regardless of the len we
			 * specified, it won't do more than MAX_UPL_TRANSFER_BYTES and it
			 * silently truncates the len to that size.  This isn't
			 * necessarily bad since madvise shouldn't really be used to
			 * page in unlimited amounts of data.  Other Unix variants
			 * limit the willneed case as well.  If this turns out to be an
			 * issue for developers, then we can always adjust the policy
			 * here and still be backwards compatible since this is all
			 * just "advice".
			 */
			kr = memory_object_data_request(
				pager,
				vm_object_trunc_page(offset) + object->paging_offset,
				0,      /* ignored */
				VM_PROT_READ,
				(memory_object_fault_info_t)&fault_info);

			vm_object_lock(object);
			vm_object_paging_end(object);
			vm_object_unlock(object);

			/*
			 * If we couldn't do the I/O for some reason, just give up on
			 * the madvise.  We still return success to the user since
			 * madvise isn't supposed to fail when the advice can't be
			 * taken.
			 */

			if (kr != KERN_SUCCESS) {
				return KERN_SUCCESS;
			}
		}

		start += len;
		if (start >= end) {
			/* done */
			return KERN_SUCCESS;
		}

		/* look up next entry */
		vm_map_lock_read(map);
		if (!vm_map_lookup_entry(map, start, &entry)) {
			/*
			 * There's a new hole in the address range.
			 */
			vm_map_unlock_read(map);
			return KERN_INVALID_ADDRESS;
		}
	}

	vm_map_unlock_read(map);
	return KERN_SUCCESS;
}

static boolean_t
vm_map_entry_is_reusable(
	vm_map_entry_t entry)
{
	/* Only user map entries */

	vm_object_t object;

	if (entry->is_sub_map) {
		return FALSE;
	}

	switch (VME_ALIAS(entry)) {
	case VM_MEMORY_MALLOC:
	case VM_MEMORY_MALLOC_SMALL:
	case VM_MEMORY_MALLOC_LARGE:
	case VM_MEMORY_REALLOC:
	case VM_MEMORY_MALLOC_TINY:
	case VM_MEMORY_MALLOC_LARGE_REUSABLE:
	case VM_MEMORY_MALLOC_LARGE_REUSED:
		/*
		 * This is a malloc() memory region: check if it's still
		 * in its original state and can be re-used for more
		 * malloc() allocations.
		 */
		break;
	default:
		/*
		 * Not a malloc() memory region: let the caller decide if
		 * it's re-usable.
		 */
		return TRUE;
	}

	if (/*entry->is_shared ||*/
		entry->is_sub_map ||
		entry->in_transition ||
		entry->protection != VM_PROT_DEFAULT ||
		entry->max_protection != VM_PROT_ALL ||
		entry->inheritance != VM_INHERIT_DEFAULT ||
		entry->no_cache ||
		entry->vme_permanent ||
		entry->superpage_size != FALSE ||
		entry->zero_wired_pages ||
		entry->wired_count != 0 ||
		entry->user_wired_count != 0) {
		return FALSE;
	}

	object = VME_OBJECT(entry);
	if (object == VM_OBJECT_NULL) {
		return TRUE;
	}
	if (
#if 0
		/*
		 * Let's proceed even if the VM object is potentially
		 * shared.
		 * We check for this later when processing the actual
		 * VM pages, so the contents will be safe if shared.
		 *
		 * But we can still mark this memory region as "reusable" to
		 * acknowledge that the caller did let us know that the memory
		 * could be re-used and should not be penalized for holding
		 * on to it.  This allows its "resident size" to not include
		 * the reusable range.
		 */
		object->ref_count == 1 &&
#endif
		object->vo_copy == VM_OBJECT_NULL &&
		object->shadow == VM_OBJECT_NULL &&
		object->internal &&
		object->purgable == VM_PURGABLE_DENY &&
		object->wimg_bits == VM_WIMG_USE_DEFAULT &&
		!object->code_signed) {
		return TRUE;
	}
	return FALSE;
}

static kern_return_t
vm_map_reuse_pages(
	vm_map_t        map,
	vm_map_offset_t start,
	vm_map_offset_t end)
{
	vm_map_entry_t                  entry;
	vm_object_t                     object;
	vm_object_offset_t              start_offset, end_offset;

	/*
	 * The MADV_REUSE operation doesn't require any changes to the
	 * vm_map_entry_t's, so the read lock is sufficient.
	 */

	if (VM_MAP_PAGE_SHIFT(map) < PAGE_SHIFT) {
		/*
		 * XXX TODO4K
		 * need to figure out what reusable means for a
		 * portion of a native page.
		 */
		return KERN_SUCCESS;
	}

	vm_map_lock_read(map);
	assert(map->pmap != kernel_pmap);       /* protect alias access */

	/*
	 * The madvise semantics require that the address range be fully
	 * allocated with no holes.  Otherwise, we're required to return
	 * an error.
	 */

	if (!vm_map_range_check(map, start, end, &entry)) {
		vm_map_unlock_read(map);
		vm_page_stats_reusable.reuse_pages_failure++;
		return KERN_INVALID_ADDRESS;
	}

	/*
	 * Examine each vm_map_entry_t in the range.
	 */
	for (; entry != vm_map_to_entry(map) && entry->vme_start < end;
	    entry = entry->vme_next) {
		/*
		 * Sanity check on the VM map entry.
		 */
		if (!vm_map_entry_is_reusable(entry)) {
			vm_map_unlock_read(map);
			vm_page_stats_reusable.reuse_pages_failure++;
			return KERN_INVALID_ADDRESS;
		}

		/*
		 * The first time through, the start address could be anywhere
		 * within the vm_map_entry we found.  So adjust the offset to
		 * correspond.
		 */
		if (entry->vme_start < start) {
			start_offset = start - entry->vme_start;
		} else {
			start_offset = 0;
		}
		end_offset = MIN(end, entry->vme_end) - entry->vme_start;
		start_offset += VME_OFFSET(entry);
		end_offset += VME_OFFSET(entry);

		object = VME_OBJECT(entry);
		if (object != VM_OBJECT_NULL) {
			vm_object_lock(object);
			vm_object_reuse_pages(object, start_offset, end_offset,
			    TRUE);
			vm_object_unlock(object);
		}

		if (VME_ALIAS(entry) == VM_MEMORY_MALLOC_LARGE_REUSABLE) {
			/*
			 * XXX
			 * We do not hold the VM map exclusively here.
			 * The "alias" field is not that critical, so it's
			 * safe to update it here, as long as it is the only
			 * one that can be modified while holding the VM map
			 * "shared".
			 */
			VME_ALIAS_SET(entry, VM_MEMORY_MALLOC_LARGE_REUSED);
		}
	}

	vm_map_unlock_read(map);
	vm_page_stats_reusable.reuse_pages_success++;
	return KERN_SUCCESS;
}


static kern_return_t
vm_map_reusable_pages(
	vm_map_t        map,
	vm_map_offset_t start,
	vm_map_offset_t end)
{
	vm_map_entry_t                  entry;
	vm_object_t                     object;
	vm_object_offset_t              start_offset, end_offset;
	vm_map_offset_t                 pmap_offset;

	if (VM_MAP_PAGE_SHIFT(map) < PAGE_SHIFT) {
		/*
		 * XXX TODO4K
		 * need to figure out what reusable means for a portion
		 * of a native page.
		 */
		return KERN_SUCCESS;
	}

	/*
	 * The MADV_REUSABLE operation doesn't require any changes to the
	 * vm_map_entry_t's, so the read lock is sufficient.
	 */

	vm_map_lock_read(map);
	assert(map->pmap != kernel_pmap);       /* protect alias access */

	/*
	 * The madvise semantics require that the address range be fully
	 * allocated with no holes.  Otherwise, we're required to return
	 * an error.
	 */

	if (!vm_map_range_check(map, start, end, &entry)) {
		vm_map_unlock_read(map);
		vm_page_stats_reusable.reusable_pages_failure++;
		return KERN_INVALID_ADDRESS;
	}

	/*
	 * Examine each vm_map_entry_t in the range.
	 */
	for (; entry != vm_map_to_entry(map) && entry->vme_start < end;
	    entry = entry->vme_next) {
		int kill_pages = 0;
		boolean_t reusable_no_write = FALSE;

		/*
		 * Sanity check on the VM map entry.
		 */
		if (!vm_map_entry_is_reusable(entry)) {
			vm_map_unlock_read(map);
			vm_page_stats_reusable.reusable_pages_failure++;
			return KERN_INVALID_ADDRESS;
		}

		if (!(entry->protection & VM_PROT_WRITE) && !entry->used_for_jit
#if __arm64e__
		    && !entry->used_for_tpro
#endif
		    ) {
			/* not writable: can't discard contents */
			vm_map_unlock_read(map);
			vm_page_stats_reusable.reusable_nonwritable++;
			vm_page_stats_reusable.reusable_pages_failure++;
			return KERN_PROTECTION_FAILURE;
		}

		/*
		 * The first time through, the start address could be anywhere
		 * within the vm_map_entry we found.  So adjust the offset to
		 * correspond.
		 */
		if (entry->vme_start < start) {
			start_offset = start - entry->vme_start;
			pmap_offset = start;
		} else {
			start_offset = 0;
			pmap_offset = entry->vme_start;
		}
		end_offset = MIN(end, entry->vme_end) - entry->vme_start;
		start_offset += VME_OFFSET(entry);
		end_offset += VME_OFFSET(entry);

		object = VME_OBJECT(entry);
		if (object == VM_OBJECT_NULL) {
			continue;
		}

		if (entry->protection & VM_PROT_EXECUTE) {
			/*
			 * Executable mappings might be write-protected by
			 * hardware, so do not attempt to write to these pages.
			 */
			reusable_no_write = TRUE;
		}

		if (entry->vme_xnu_user_debug) {
			/*
			 * User debug pages might be write-protected by hardware,
			 * so do not attempt to write to these pages.
			 */
			reusable_no_write = TRUE;
		}

		vm_object_lock(object);
		if (((object->ref_count == 1) ||
		    (object->copy_strategy != MEMORY_OBJECT_COPY_SYMMETRIC &&
		    object->vo_copy == VM_OBJECT_NULL)) &&
		    object->shadow == VM_OBJECT_NULL &&
		    /*
		     * "iokit_acct" entries are billed for their virtual size
		     * (rather than for their resident pages only), so they
		     * wouldn't benefit from making pages reusable, and it
		     * would be hard to keep track of pages that are both
		     * "iokit_acct" and "reusable" in the pmap stats and
		     * ledgers.
		     */
		    !(entry->iokit_acct ||
		    (!entry->is_sub_map && !entry->use_pmap))) {
			if (object->ref_count != 1) {
				vm_page_stats_reusable.reusable_shared++;
			}
			kill_pages = 1;
		} else {
			kill_pages = -1;
		}
		if (kill_pages != -1) {
			vm_object_deactivate_pages(object,
			    start_offset,
			    end_offset - start_offset,
			    kill_pages,
			    TRUE /*reusable_pages*/,
			    reusable_no_write,
			    map->pmap,
			    pmap_offset);
		} else {
			vm_page_stats_reusable.reusable_pages_shared++;
			DTRACE_VM4(vm_map_reusable_pages_shared,
			    unsigned int, VME_ALIAS(entry),
			    vm_map_t, map,
			    vm_map_entry_t, entry,
			    vm_object_t, object);
		}
		vm_object_unlock(object);

		if (VME_ALIAS(entry) == VM_MEMORY_MALLOC_LARGE ||
		    VME_ALIAS(entry) == VM_MEMORY_MALLOC_LARGE_REUSED) {
			/*
			 * XXX
			 * We do not hold the VM map exclusively here.
			 * The "alias" field is not that critical, so it's
			 * safe to update it here, as long as it is the only
			 * one that can be modified while holding the VM map
			 * "shared".
			 */
			VME_ALIAS_SET(entry, VM_MEMORY_MALLOC_LARGE_REUSABLE);
		}
	}

	vm_map_unlock_read(map);
	vm_page_stats_reusable.reusable_pages_success++;
	return KERN_SUCCESS;
}


static kern_return_t
vm_map_can_reuse(
	vm_map_t        map,
	vm_map_offset_t start,
	vm_map_offset_t end)
{
	vm_map_entry_t                  entry;

	/*
	 * The MADV_REUSABLE operation doesn't require any changes to the
	 * vm_map_entry_t's, so the read lock is sufficient.
	 */

	vm_map_lock_read(map);
	assert(map->pmap != kernel_pmap);       /* protect alias access */

	/*
	 * The madvise semantics require that the address range be fully
	 * allocated with no holes.  Otherwise, we're required to return
	 * an error.
	 */

	if (!vm_map_range_check(map, start, end, &entry)) {
		vm_map_unlock_read(map);
		vm_page_stats_reusable.can_reuse_failure++;
		return KERN_INVALID_ADDRESS;
	}

	/*
	 * Examine each vm_map_entry_t in the range.
	 */
	for (; entry != vm_map_to_entry(map) && entry->vme_start < end;
	    entry = entry->vme_next) {
		/*
		 * Sanity check on the VM map entry.
		 */
		if (!vm_map_entry_is_reusable(entry)) {
			vm_map_unlock_read(map);
			vm_page_stats_reusable.can_reuse_failure++;
			return KERN_INVALID_ADDRESS;
		}
	}

	vm_map_unlock_read(map);
	vm_page_stats_reusable.can_reuse_success++;
	return KERN_SUCCESS;
}


#if MACH_ASSERT
static kern_return_t
vm_map_pageout(
	vm_map_t        map,
	vm_map_offset_t start,
	vm_map_offset_t end)
{
	vm_map_entry_t                  entry;

	/*
	 * The MADV_PAGEOUT operation doesn't require any changes to the
	 * vm_map_entry_t's, so the read lock is sufficient.
	 */

	vm_map_lock_read(map);

	/*
	 * The madvise semantics require that the address range be fully
	 * allocated with no holes.  Otherwise, we're required to return
	 * an error.
	 */

	if (!vm_map_range_check(map, start, end, &entry)) {
		vm_map_unlock_read(map);
		return KERN_INVALID_ADDRESS;
	}

	/*
	 * Examine each vm_map_entry_t in the range.
	 */
	for (; entry != vm_map_to_entry(map) && entry->vme_start < end;
	    entry = entry->vme_next) {
		vm_object_t     object;

		/*
		 * Sanity check on the VM map entry.
		 */
		if (entry->is_sub_map) {
			vm_map_t submap;
			vm_map_offset_t submap_start;
			vm_map_offset_t submap_end;
			vm_map_entry_t submap_entry;

			submap = VME_SUBMAP(entry);
			submap_start = VME_OFFSET(entry);
			submap_end = submap_start + (entry->vme_end -
			    entry->vme_start);

			vm_map_lock_read(submap);

			if (!vm_map_range_check(submap,
			    submap_start,
			    submap_end,
			    &submap_entry)) {
				vm_map_unlock_read(submap);
				vm_map_unlock_read(map);
				return KERN_INVALID_ADDRESS;
			}

			if (submap_entry->is_sub_map) {
				vm_map_unlock_read(submap);
				continue;
			}

			object = VME_OBJECT(submap_entry);
			if (object == VM_OBJECT_NULL || !object->internal) {
				vm_map_unlock_read(submap);
				continue;
			}

			vm_object_pageout(object);

			vm_map_unlock_read(submap);
			submap = VM_MAP_NULL;
			submap_entry = VM_MAP_ENTRY_NULL;
			continue;
		}

		object = VME_OBJECT(entry);
		if (object == VM_OBJECT_NULL || !object->internal) {
			continue;
		}

		vm_object_pageout(object);
	}

	vm_map_unlock_read(map);
	return KERN_SUCCESS;
}
#endif /* MACH_ASSERT */

/*
 * This function determines if the zero operation can be run on the
 * respective entry. Additional checks on the object are in
 * vm_object_zero_preflight.
 */
static kern_return_t
vm_map_zero_entry_preflight(vm_map_entry_t entry)
{
	/*
	 * Zeroing is restricted to writable non-executable entries and non-JIT
	 * regions.
	 */
	if (!(entry->protection & VM_PROT_WRITE) ||
	    (entry->protection & VM_PROT_EXECUTE) ||
	    entry->used_for_jit ||
	    entry->vme_xnu_user_debug) {
		return KERN_PROTECTION_FAILURE;
	}

	/*
	 * Zeroing for copy on write isn't yet supported. Zeroing is also not
	 * allowed for submaps.
	 */
	if (entry->needs_copy || entry->is_sub_map) {
		return KERN_NO_ACCESS;
	}

	return KERN_SUCCESS;
}

/*
 * This function translates entry's start and end to offsets in the object
 */
static void
vm_map_get_bounds_in_object(
	vm_map_entry_t      entry,
	vm_map_offset_t     start,
	vm_map_offset_t     end,
	vm_map_offset_t    *start_offset,
	vm_map_offset_t    *end_offset)
{
	if (entry->vme_start < start) {
		*start_offset = start - entry->vme_start;
	} else {
		*start_offset = 0;
	}
	*end_offset = MIN(end, entry->vme_end) - entry->vme_start;
	*start_offset += VME_OFFSET(entry);
	*end_offset += VME_OFFSET(entry);
}

/*
 * This function iterates through the entries in the requested range
 * and zeroes any resident pages in the corresponding objects. Compressed
 * pages are dropped instead of being faulted in and zeroed.
 */
static kern_return_t
vm_map_zero(
	vm_map_t        map,
	vm_map_offset_t start,
	vm_map_offset_t end)
{
	vm_map_entry_t                  entry;
	vm_map_offset_t                 cur = start;
	kern_return_t                   ret;

	/*
	 * This operation isn't supported where the map page size is less than
	 * the hardware page size. Caller will need to handle error and
	 * explicitly zero memory if needed.
	 */
	if (VM_MAP_PAGE_SHIFT(map) < PAGE_SHIFT) {
		return KERN_NO_ACCESS;
	}

	/*
	 * The MADV_ZERO operation doesn't require any changes to the
	 * vm_map_entry_t's, so the read lock is sufficient.
	 */
	vm_map_lock_read(map);
	assert(map->pmap != kernel_pmap);       /* protect alias access */

	/*
	 * The madvise semantics require that the address range be fully
	 * allocated with no holes. Otherwise, we're required to return
	 * an error. This check needs to be redone if the map has changed.
	 */
	if (!vm_map_range_check(map, cur, end, &entry)) {
		vm_map_unlock_read(map);
		return KERN_INVALID_ADDRESS;
	}

	/*
	 * Examine each vm_map_entry_t in the range.
	 */
	while (entry != vm_map_to_entry(map) && entry->vme_start < end) {
		vm_map_offset_t cur_offset;
		vm_map_offset_t end_offset;
		unsigned int last_timestamp = map->timestamp;
		vm_object_t object = VME_OBJECT(entry);

		ret = vm_map_zero_entry_preflight(entry);
		if (ret != KERN_SUCCESS) {
			vm_map_unlock_read(map);
			return ret;
		}

		if (object == VM_OBJECT_NULL) {
			entry = entry->vme_next;
			continue;
		}

		vm_map_get_bounds_in_object(entry, cur, end, &cur_offset, &end_offset);
		vm_object_lock(object);
		/*
		 * Take a reference on the object as vm_object_zero will drop the object
		 * lock when it encounters a busy page.
		 */
		vm_object_reference_locked(object);
		vm_map_unlock_read(map);

		ret = vm_object_zero(object, cur_offset, end_offset);
		vm_object_unlock(object);
		vm_object_deallocate(object);
		if (ret != KERN_SUCCESS) {
			return ret;
		}
		/*
		 * Update cur as vm_object_zero has succeeded.
		 */
		cur += (end_offset - cur_offset);
		if (cur == end) {
			return KERN_SUCCESS;
		}

		/*
		 * If the map timestamp has changed, restart by relooking up cur in the
		 * map
		 */
		vm_map_lock_read(map);
		if (last_timestamp != map->timestamp) {
			/*
			 * Relookup cur in the map
			 */
			if (!vm_map_range_check(map, cur, end, &entry)) {
				vm_map_unlock_read(map);
				return KERN_INVALID_ADDRESS;
			}
			continue;
		}
		/*
		 * If the map hasn't changed proceed with the next entry
		 */
		entry = entry->vme_next;
	}

	vm_map_unlock_read(map);
	return KERN_SUCCESS;
}


/*
 *	Routine:	vm_map_entry_insert
 *
 *	Description:	This routine inserts a new vm_entry in a locked map.
 */
static vm_map_entry_t
vm_map_entry_insert(
	vm_map_t                map,
	vm_map_entry_t          insp_entry,
	vm_map_offset_t         start,
	vm_map_offset_t         end,
	vm_object_t             object,
	vm_object_offset_t      offset,
	vm_map_kernel_flags_t   vmk_flags,
	boolean_t               needs_copy,
	vm_prot_t               cur_protection,
	vm_prot_t               max_protection,
	vm_inherit_t            inheritance,
	boolean_t               clear_map_aligned)
{
	vm_map_entry_t  new_entry;
	boolean_t map_aligned = FALSE;

	assert(insp_entry != (vm_map_entry_t)0);
	vm_map_lock_assert_exclusive(map);

#if DEVELOPMENT || DEBUG
	vm_object_offset_t      end_offset = 0;
	assertf(!os_add_overflow(end - start, offset, &end_offset), "size 0x%llx, offset 0x%llx caused overflow", (uint64_t)(end - start), offset);
#endif /* DEVELOPMENT || DEBUG */

	if (VM_MAP_PAGE_SHIFT(map) != PAGE_SHIFT) {
		map_aligned = TRUE;
	}
	if (clear_map_aligned &&
	    (!VM_MAP_PAGE_ALIGNED(start, VM_MAP_PAGE_MASK(map)) ||
	    !VM_MAP_PAGE_ALIGNED(end, VM_MAP_PAGE_MASK(map)))) {
		map_aligned = FALSE;
	}
	if (map_aligned) {
		assert(VM_MAP_PAGE_ALIGNED(start, VM_MAP_PAGE_MASK(map)));
		assert(VM_MAP_PAGE_ALIGNED(end, VM_MAP_PAGE_MASK(map)));
	} else {
		assert(page_aligned(start));
		assert(page_aligned(end));
	}
	assert(start < end);

	new_entry = vm_map_entry_create(map);

	new_entry->vme_start = start;
	new_entry->vme_end = end;

	if (vmk_flags.vmkf_submap) {
		new_entry->vme_atomic = vmk_flags.vmkf_submap_atomic;
		VME_SUBMAP_SET(new_entry, (vm_map_t)object);
	} else {
		VME_OBJECT_SET(new_entry, object, false, 0);
	}
	VME_OFFSET_SET(new_entry, offset);
	VME_ALIAS_SET(new_entry, vmk_flags.vm_tag);

	new_entry->map_aligned = map_aligned;
	new_entry->needs_copy = needs_copy;
	new_entry->inheritance = inheritance;
	new_entry->protection = cur_protection;
	new_entry->max_protection = max_protection;
	/*
	 * submap: "use_pmap" means "nested".
	 * default: false.
	 *
	 * object: "use_pmap" means "use pmap accounting" for footprint.
	 * default: true.
	 */
	new_entry->use_pmap = !vmk_flags.vmkf_submap;
	new_entry->no_cache = vmk_flags.vmf_no_cache;
	new_entry->vme_permanent = vmk_flags.vmf_permanent;
	new_entry->translated_allow_execute = vmk_flags.vmkf_translated_allow_execute;
	new_entry->vme_no_copy_on_read = vmk_flags.vmkf_no_copy_on_read;
	new_entry->superpage_size = (vmk_flags.vmf_superpage_size != 0);

	if (vmk_flags.vmkf_map_jit) {
		if (!(map->jit_entry_exists) ||
		    VM_MAP_POLICY_ALLOW_MULTIPLE_JIT(map)) {
			new_entry->used_for_jit = TRUE;
			map->jit_entry_exists = TRUE;
		}
	}

	/*
	 *	Insert the new entry into the list.
	 */

	vm_map_store_entry_link(map, insp_entry, new_entry, vmk_flags);
	map->size += end - start;

	/*
	 *	Update the free space hint and the lookup hint.
	 */

	SAVE_HINT_MAP_WRITE(map, new_entry);
	return new_entry;
}

/*
 *	Routine:	vm_map_remap_extract
 *
 *	Description:	This routine returns a vm_entry list from a map.
 */
static kern_return_t
vm_map_remap_extract(
	vm_map_t                map,
	vm_map_offset_t         addr,
	vm_map_size_t           size,
	boolean_t               copy,
	vm_map_copy_t           map_copy,
	vm_prot_t               *cur_protection,   /* IN/OUT */
	vm_prot_t               *max_protection,   /* IN/OUT */
	/* What, no behavior? */
	vm_inherit_t            inheritance,
	vm_map_kernel_flags_t   vmk_flags)
{
	struct vm_map_header   *map_header = &map_copy->cpy_hdr;
	kern_return_t           result;
	vm_map_size_t           mapped_size;
	vm_map_size_t           tmp_size;
	vm_map_entry_t          src_entry;     /* result of last map lookup */
	vm_map_entry_t          new_entry;
	vm_object_offset_t      offset;
	vm_map_offset_t         map_address;
	vm_map_offset_t         src_start;     /* start of entry to map */
	vm_map_offset_t         src_end;       /* end of region to be mapped */
	vm_object_t             object;
	vm_map_version_t        version;
	boolean_t               src_needs_copy;
	boolean_t               new_entry_needs_copy;
	vm_map_entry_t          saved_src_entry;
	boolean_t               src_entry_was_wired;
	vm_prot_t               max_prot_for_prot_copy;
	vm_map_offset_t         effective_page_mask;
	bool                    pageable, same_map;
	boolean_t               vm_remap_legacy;
	vm_prot_t               required_cur_prot, required_max_prot;
	vm_object_t             new_copy_object;     /* vm_object_copy_* result */
	boolean_t               saved_used_for_jit;  /* Saved used_for_jit. */

	pageable = vmk_flags.vmkf_copy_pageable;
	same_map = vmk_flags.vmkf_copy_same_map;

	effective_page_mask = MIN(PAGE_MASK, VM_MAP_PAGE_MASK(map));

	assert(map != VM_MAP_NULL);
	assert(size != 0);
	assert(size == vm_map_round_page(size, effective_page_mask));
	assert(inheritance == VM_INHERIT_NONE ||
	    inheritance == VM_INHERIT_COPY ||
	    inheritance == VM_INHERIT_SHARE);
	assert(!(*cur_protection & ~(VM_PROT_ALL | VM_PROT_ALLEXEC)));
	assert(!(*max_protection & ~(VM_PROT_ALL | VM_PROT_ALLEXEC)));
	assert((*cur_protection & *max_protection) == *cur_protection);

	/*
	 *	Compute start and end of region.
	 */
	src_start = vm_map_trunc_page(addr, effective_page_mask);
	src_end = vm_map_round_page(src_start + size, effective_page_mask);

	/*
	 *	Initialize map_header.
	 */
	map_header->nentries = 0;
	map_header->entries_pageable = pageable;
//	map_header->page_shift = MIN(VM_MAP_PAGE_SHIFT(map), PAGE_SHIFT);
	map_header->page_shift = (uint16_t)VM_MAP_PAGE_SHIFT(map);
	map_header->rb_head_store.rbh_root = (void *)(int)SKIP_RB_TREE;
	vm_map_store_init(map_header);

	if (copy && vmk_flags.vmkf_remap_prot_copy) {
		/*
		 * Special case for vm_map_protect(VM_PROT_COPY):
		 * we want to set the new mappings' max protection to the
		 * specified *max_protection...
		 */
		max_prot_for_prot_copy = *max_protection & (VM_PROT_ALL | VM_PROT_ALLEXEC);
		/* ... but we want to use the vm_remap() legacy mode */
		*max_protection = VM_PROT_NONE;
		*cur_protection = VM_PROT_NONE;
	} else {
		max_prot_for_prot_copy = VM_PROT_NONE;
	}

	if (*cur_protection == VM_PROT_NONE &&
	    *max_protection == VM_PROT_NONE) {
		/*
		 * vm_remap() legacy mode:
		 * Extract all memory regions in the specified range and
		 * collect the strictest set of protections allowed on the
		 * entire range, so the caller knows what they can do with
		 * the remapped range.
		 * We start with VM_PROT_ALL and we'll remove the protections
		 * missing from each memory region.
		 */
		vm_remap_legacy = TRUE;
		*cur_protection = VM_PROT_ALL;
		*max_protection = VM_PROT_ALL;
		required_cur_prot = VM_PROT_NONE;
		required_max_prot = VM_PROT_NONE;
	} else {
		/*
		 * vm_remap_new() mode:
		 * Extract all memory regions in the specified range and
		 * ensure that they have at least the protections specified
		 * by the caller via *cur_protection and *max_protection.
		 * The resulting mapping should have these protections.
		 */
		vm_remap_legacy = FALSE;
		if (copy) {
			required_cur_prot = VM_PROT_NONE;
			required_max_prot = VM_PROT_READ;
		} else {
			required_cur_prot = *cur_protection;
			required_max_prot = *max_protection;
		}
	}

	map_address = 0;
	mapped_size = 0;
	result = KERN_SUCCESS;

	/*
	 *	The specified source virtual space might correspond to
	 *	multiple map entries, need to loop on them.
	 */
	vm_map_lock(map);

	if (map->pmap == kernel_pmap) {
		map_copy->is_kernel_range = true;
		map_copy->orig_range = kmem_addr_get_range(addr, size);
#if CONFIG_MAP_RANGES
	} else if (map->uses_user_ranges) {
		map_copy->is_user_range = true;
		map_copy->orig_range = vm_map_user_range_resolve(map, addr, size, NULL);
#endif /* CONFIG_MAP_RANGES */
	}

	if (VM_MAP_PAGE_SHIFT(map) < PAGE_SHIFT) {
		/*
		 * This address space uses sub-pages so the range might
		 * not be re-mappable in an address space with larger
		 * pages. Re-assemble any broken-up VM map entries to
		 * improve our chances of making it work.
		 */
		vm_map_simplify_range(map, src_start, src_end);
	}
	while (mapped_size != size) {
		vm_map_size_t   entry_size;

		/*
		 *	Find the beginning of the region.
		 */
		if (!vm_map_lookup_entry(map, src_start, &src_entry)) {
			result = KERN_INVALID_ADDRESS;
			break;
		}

		if (src_start < src_entry->vme_start ||
		    (mapped_size && src_start != src_entry->vme_start)) {
			result = KERN_INVALID_ADDRESS;
			break;
		}

		tmp_size = size - mapped_size;
		if (src_end > src_entry->vme_end) {
			tmp_size -= (src_end - src_entry->vme_end);
		}

		entry_size = (vm_map_size_t)(src_entry->vme_end -
		    src_entry->vme_start);

		if (src_entry->is_sub_map &&
		    vmk_flags.vmkf_copy_single_object) {
			vm_map_t submap;
			vm_map_offset_t submap_start;
			vm_map_size_t submap_size;
			boolean_t submap_needs_copy;

			/*
			 * No check for "required protection" on "src_entry"
			 * because the protections that matter are the ones
			 * on the submap's VM map entry, which will be checked
			 * during the call to vm_map_remap_extract() below.
			 */
			submap_size = src_entry->vme_end - src_start;
			if (submap_size > size) {
				submap_size = size;
			}
			submap_start = VME_OFFSET(src_entry) + src_start - src_entry->vme_start;
			submap = VME_SUBMAP(src_entry);
			if (copy) {
				/*
				 * The caller wants a copy-on-write re-mapping,
				 * so let's extract from the submap accordingly.
				 */
				submap_needs_copy = TRUE;
			} else if (src_entry->needs_copy) {
				/*
				 * The caller wants a shared re-mapping but the
				 * submap is mapped with "needs_copy", so its
				 * contents can't be shared as is. Extract the
				 * contents of the submap as "copy-on-write".
				 * The re-mapping won't be shared with the
				 * original mapping but this is equivalent to
				 * what happened with the original "remap from
				 * submap" code.
				 * The shared region is mapped "needs_copy", for
				 * example.
				 */
				submap_needs_copy = TRUE;
			} else {
				/*
				 * The caller wants a shared re-mapping and
				 * this mapping can be shared (no "needs_copy"),
				 * so let's extract from the submap accordingly.
				 * Kernel submaps are mapped without
				 * "needs_copy", for example.
				 */
				submap_needs_copy = FALSE;
			}
			vm_map_reference(submap);
			vm_map_unlock(map);
			src_entry = NULL;
			if (vm_remap_legacy) {
				*cur_protection = VM_PROT_NONE;
				*max_protection = VM_PROT_NONE;
			}

			DTRACE_VM7(remap_submap_recurse,
			    vm_map_t, map,
			    vm_map_offset_t, addr,
			    vm_map_size_t, size,
			    boolean_t, copy,
			    vm_map_offset_t, submap_start,
			    vm_map_size_t, submap_size,
			    boolean_t, submap_needs_copy);

			result = vm_map_remap_extract(submap,
			    submap_start,
			    submap_size,
			    submap_needs_copy,
			    map_copy,
			    cur_protection,
			    max_protection,
			    inheritance,
			    vmk_flags);
			vm_map_deallocate(submap);

			if (result == KERN_SUCCESS &&
			    submap_needs_copy &&
			    !copy) {
				/*
				 * We were asked for a "shared"
				 * re-mapping but had to ask for a
				 * "copy-on-write" remapping of the
				 * submap's mapping to honor the
				 * submap's "needs_copy".
				 * We now need to resolve that
				 * pending "copy-on-write" to
				 * get something we can share.
				 */
				vm_map_entry_t copy_entry;
				vm_object_offset_t copy_offset;
				vm_map_size_t copy_size;
				vm_object_t copy_object;
				copy_entry = vm_map_copy_first_entry(map_copy);
				copy_size = copy_entry->vme_end - copy_entry->vme_start;
				copy_object = VME_OBJECT(copy_entry);
				copy_offset = VME_OFFSET(copy_entry);
				if (copy_object == VM_OBJECT_NULL) {
					assert(copy_offset == 0);
					assert(!copy_entry->needs_copy);
					if (copy_entry->max_protection == VM_PROT_NONE) {
						assert(copy_entry->protection == VM_PROT_NONE);
						/* nothing to share */
					} else {
						assert(copy_offset == 0);
						copy_object = vm_object_allocate(copy_size);
						VME_OFFSET_SET(copy_entry, 0);
						VME_OBJECT_SET(copy_entry, copy_object, false, 0);
						assert(copy_entry->use_pmap);
					}
				} else if (copy_object->copy_strategy != MEMORY_OBJECT_COPY_SYMMETRIC) {
					/* already shareable */
					assert(!copy_entry->needs_copy);
				} else if (copy_entry->needs_copy ||
				    copy_object->shadowed ||
				    (object->internal &&
				    !object->true_share &&
				    !copy_entry->is_shared &&
				    copy_object->vo_size > copy_size)) {
					VME_OBJECT_SHADOW(copy_entry, copy_size, TRUE);
					assert(copy_entry->use_pmap);
					if (copy_entry->needs_copy) {
						/* already write-protected */
					} else {
						vm_prot_t prot;
						prot = copy_entry->protection & ~VM_PROT_WRITE;
						vm_object_pmap_protect(copy_object,
						    copy_offset,
						    copy_size,
						    PMAP_NULL,
						    PAGE_SIZE,
						    0,
						    prot);
					}
					copy_entry->needs_copy = FALSE;
				}
				copy_object = VME_OBJECT(copy_entry);
				copy_offset = VME_OFFSET(copy_entry);
				if (copy_object &&
				    copy_object->copy_strategy == MEMORY_OBJECT_COPY_SYMMETRIC) {
					copy_object->copy_strategy = MEMORY_OBJECT_COPY_DELAY;
					copy_object->true_share = TRUE;
				}
			}

			return result;
		}

		if (src_entry->is_sub_map) {
			/* protections for submap mapping are irrelevant here */
		} else if (((src_entry->protection & required_cur_prot) !=
		    required_cur_prot) ||
		    ((src_entry->max_protection & required_max_prot) !=
		    required_max_prot)) {
			if (vmk_flags.vmkf_copy_single_object &&
			    mapped_size != 0) {
				/*
				 * Single object extraction.
				 * We can't extract more with the required
				 * protection but we've extracted some, so
				 * stop there and declare success.
				 * The caller should check the size of
				 * the copy entry we've extracted.
				 */
				result = KERN_SUCCESS;
			} else {
				/*
				 * VM range extraction.
				 * Required proctection is not available
				 * for this part of the range: fail.
				 */
				result = KERN_PROTECTION_FAILURE;
			}
			break;
		}

		if (src_entry->is_sub_map) {
			vm_map_t submap;
			vm_map_offset_t submap_start;
			vm_map_size_t submap_size;
			vm_map_copy_t submap_copy;
			vm_prot_t submap_curprot, submap_maxprot;
			boolean_t submap_needs_copy;

			/*
			 * No check for "required protection" on "src_entry"
			 * because the protections that matter are the ones
			 * on the submap's VM map entry, which will be checked
			 * during the call to vm_map_copy_extract() below.
			 */
			object = VM_OBJECT_NULL;
			submap_copy = VM_MAP_COPY_NULL;

			/* find equivalent range in the submap */
			submap = VME_SUBMAP(src_entry);
			submap_start = VME_OFFSET(src_entry) + src_start - src_entry->vme_start;
			submap_size = tmp_size;
			if (copy) {
				/*
				 * The caller wants a copy-on-write re-mapping,
				 * so let's extract from the submap accordingly.
				 */
				submap_needs_copy = TRUE;
			} else if (src_entry->needs_copy) {
				/*
				 * The caller wants a shared re-mapping but the
				 * submap is mapped with "needs_copy", so its
				 * contents can't be shared as is. Extract the
				 * contents of the submap as "copy-on-write".
				 * The re-mapping won't be shared with the
				 * original mapping but this is equivalent to
				 * what happened with the original "remap from
				 * submap" code.
				 * The shared region is mapped "needs_copy", for
				 * example.
				 */
				submap_needs_copy = TRUE;
			} else {
				/*
				 * The caller wants a shared re-mapping and
				 * this mapping can be shared (no "needs_copy"),
				 * so let's extract from the submap accordingly.
				 * Kernel submaps are mapped without
				 * "needs_copy", for example.
				 */
				submap_needs_copy = FALSE;
			}
			/* extra ref to keep submap alive */
			vm_map_reference(submap);

			DTRACE_VM7(remap_submap_recurse,
			    vm_map_t, map,
			    vm_map_offset_t, addr,
			    vm_map_size_t, size,
			    boolean_t, copy,
			    vm_map_offset_t, submap_start,
			    vm_map_size_t, submap_size,
			    boolean_t, submap_needs_copy);

			/*
			 * The map can be safely unlocked since we
			 * already hold a reference on the submap.
			 *
			 * No timestamp since we don't care if the map
			 * gets modified while we're down in the submap.
			 * We'll resume the extraction at src_start + tmp_size
			 * anyway.
			 */
			vm_map_unlock(map);
			src_entry = NULL; /* not valid once map is unlocked */

			if (vm_remap_legacy) {
				submap_curprot = VM_PROT_NONE;
				submap_maxprot = VM_PROT_NONE;
				if (max_prot_for_prot_copy) {
					submap_maxprot = max_prot_for_prot_copy;
				}
			} else {
				assert(!max_prot_for_prot_copy);
				submap_curprot = *cur_protection;
				submap_maxprot = *max_protection;
			}
			result = vm_map_copy_extract(submap,
			    submap_start,
			    submap_size,
			    submap_needs_copy,
			    &submap_copy,
			    &submap_curprot,
			    &submap_maxprot,
			    inheritance,
			    vmk_flags);

			/* release extra ref on submap */
			vm_map_deallocate(submap);
			submap = VM_MAP_NULL;

			if (result != KERN_SUCCESS) {
				vm_map_lock(map);
				break;
			}

			/* transfer submap_copy entries to map_header */
			while (vm_map_copy_first_entry(submap_copy) !=
			    vm_map_copy_to_entry(submap_copy)) {
				vm_map_entry_t copy_entry;
				vm_map_size_t copy_entry_size;

				copy_entry = vm_map_copy_first_entry(submap_copy);

				/*
				 * Prevent kernel_object from being exposed to
				 * user space.
				 */
				if (__improbable(copy_entry->vme_kernel_object)) {
					printf("%d[%s]: rejecting attempt to extract from kernel_object\n",
					    proc_selfpid(),
					    (get_bsdtask_info(current_task())
					    ? proc_name_address(get_bsdtask_info(current_task()))
					    : "?"));
					DTRACE_VM(extract_kernel_only);
					result = KERN_INVALID_RIGHT;
					vm_map_copy_discard(submap_copy);
					submap_copy = VM_MAP_COPY_NULL;
					vm_map_lock(map);
					break;
				}

#ifdef __arm64e__
				if (vmk_flags.vmkf_tpro_enforcement_override) {
					copy_entry->used_for_tpro = FALSE;
				}
#endif /* __arm64e__ */

				vm_map_copy_entry_unlink(submap_copy, copy_entry);
				copy_entry_size = copy_entry->vme_end - copy_entry->vme_start;
				copy_entry->vme_start = map_address;
				copy_entry->vme_end = map_address + copy_entry_size;
				map_address += copy_entry_size;
				mapped_size += copy_entry_size;
				src_start += copy_entry_size;
				assert(src_start <= src_end);
				_vm_map_store_entry_link(map_header,
				    map_header->links.prev,
				    copy_entry);
			}
			/* done with submap_copy */
			vm_map_copy_discard(submap_copy);

			if (vm_remap_legacy) {
				*cur_protection &= submap_curprot;
				*max_protection &= submap_maxprot;
			}

			/* re-acquire the map lock and continue to next entry */
			vm_map_lock(map);
			continue;
		} else {
			object = VME_OBJECT(src_entry);

			/*
			 * Prevent kernel_object from being exposed to
			 * user space.
			 */
			if (__improbable(is_kernel_object(object))) {
				printf("%d[%s]: rejecting attempt to extract from kernel_object\n",
				    proc_selfpid(),
				    (get_bsdtask_info(current_task())
				    ? proc_name_address(get_bsdtask_info(current_task()))
				    : "?"));
				DTRACE_VM(extract_kernel_only);
				result = KERN_INVALID_RIGHT;
				break;
			}

			if (src_entry->iokit_acct) {
				/*
				 * This entry uses "IOKit accounting".
				 */
			} else if (object != VM_OBJECT_NULL &&
			    (object->purgable != VM_PURGABLE_DENY ||
			    object->vo_ledger_tag != VM_LEDGER_TAG_NONE)) {
				/*
				 * Purgeable objects have their own accounting:
				 * no pmap accounting for them.
				 */
				assertf(!src_entry->use_pmap,
				    "map=%p src_entry=%p [0x%llx:0x%llx] 0x%x/0x%x %d",
				    map,
				    src_entry,
				    (uint64_t)src_entry->vme_start,
				    (uint64_t)src_entry->vme_end,
				    src_entry->protection,
				    src_entry->max_protection,
				    VME_ALIAS(src_entry));
			} else {
				/*
				 * Not IOKit or purgeable:
				 * must be accounted by pmap stats.
				 */
				assertf(src_entry->use_pmap,
				    "map=%p src_entry=%p [0x%llx:0x%llx] 0x%x/0x%x %d",
				    map,
				    src_entry,
				    (uint64_t)src_entry->vme_start,
				    (uint64_t)src_entry->vme_end,
				    src_entry->protection,
				    src_entry->max_protection,
				    VME_ALIAS(src_entry));
			}

			if (object == VM_OBJECT_NULL) {
				assert(!src_entry->needs_copy);
				if (src_entry->max_protection == VM_PROT_NONE) {
					assert(src_entry->protection == VM_PROT_NONE);
					/*
					 * No VM object and no permissions:
					 * this must be a reserved range with
					 * nothing to share or copy.
					 * There could also be all sorts of
					 * pmap shenanigans within that reserved
					 * range, so let's just copy the map
					 * entry as is to remap a similar
					 * reserved range.
					 */
					offset = 0; /* no object => no offset */
					goto copy_src_entry;
				}
				object = vm_object_allocate(entry_size);
				VME_OFFSET_SET(src_entry, 0);
				VME_OBJECT_SET(src_entry, object, false, 0);
				assert(src_entry->use_pmap);
				assert(!map->mapped_in_other_pmaps);
			} else if (src_entry->wired_count ||
			    object->copy_strategy != MEMORY_OBJECT_COPY_SYMMETRIC) {
				/*
				 * A wired memory region should not have
				 * any pending copy-on-write and needs to
				 * keep pointing at the VM object that
				 * contains the wired pages.
				 * If we're sharing this memory (copy=false),
				 * we'll share this VM object.
				 * If we're copying this memory (copy=true),
				 * we'll call vm_object_copy_slowly() below
				 * and use the new VM object for the remapping.
				 *
				 * Or, we are already using an asymmetric
				 * copy, and therefore we already have
				 * the right object.
				 */
				assert(!src_entry->needs_copy);
			} else if (src_entry->needs_copy || object->shadowed ||
			    (object->internal && !object->true_share &&
			    !src_entry->is_shared &&
			    object->vo_size > entry_size)) {
				bool is_writable;

				VME_OBJECT_SHADOW(src_entry, entry_size,
				    vm_map_always_shadow(map));
				assert(src_entry->use_pmap);

				is_writable = false;
				if (src_entry->protection & VM_PROT_WRITE) {
					is_writable = true;
#if __arm64e__
				} else if (src_entry->used_for_tpro) {
					is_writable = true;
#endif /* __arm64e__ */
				}
				if (!src_entry->needs_copy && is_writable) {
					vm_prot_t prot;

					if (pmap_has_prot_policy(map->pmap, src_entry->translated_allow_execute, src_entry->protection)) {
						panic("%s: map %p pmap %p entry %p 0x%llx:0x%llx prot 0x%x",
						    __FUNCTION__,
						    map, map->pmap,
						    src_entry,
						    (uint64_t)src_entry->vme_start,
						    (uint64_t)src_entry->vme_end,
						    src_entry->protection);
					}

					prot = src_entry->protection & ~VM_PROT_WRITE;

					if (override_nx(map,
					    VME_ALIAS(src_entry))
					    && prot) {
						prot |= VM_PROT_EXECUTE;
					}

					if (pmap_has_prot_policy(map->pmap, src_entry->translated_allow_execute, prot)) {
						panic("%s: map %p pmap %p entry %p 0x%llx:0x%llx prot 0x%x",
						    __FUNCTION__,
						    map, map->pmap,
						    src_entry,
						    (uint64_t)src_entry->vme_start,
						    (uint64_t)src_entry->vme_end,
						    prot);
					}

					if (map->mapped_in_other_pmaps) {
						vm_object_pmap_protect(
							VME_OBJECT(src_entry),
							VME_OFFSET(src_entry),
							entry_size,
							PMAP_NULL,
							PAGE_SIZE,
							src_entry->vme_start,
							prot);
#if MACH_ASSERT
					} else if (__improbable(map->pmap == PMAP_NULL)) {
						extern boolean_t vm_tests_in_progress;
						assert(vm_tests_in_progress);
						/*
						 * Some VM tests (in vm_tests.c)
						 * sometimes want to use a VM
						 * map without a pmap.
						 * Otherwise, this should never
						 * happen.
						 */
#endif /* MACH_ASSERT */
					} else {
						pmap_protect(vm_map_pmap(map),
						    src_entry->vme_start,
						    src_entry->vme_end,
						    prot);
					}
				}

				object = VME_OBJECT(src_entry);
				src_entry->needs_copy = FALSE;
			}


			vm_object_lock(object);
			vm_object_reference_locked(object); /* object ref. for new entry */
			assert(!src_entry->needs_copy);
			if (object->copy_strategy ==
			    MEMORY_OBJECT_COPY_SYMMETRIC) {
				/*
				 * If we want to share this object (copy==0),
				 * it needs to be COPY_DELAY.
				 * If we want to copy this object (copy==1),
				 * we can't just set "needs_copy" on our side
				 * and expect the other side to do the same
				 * (symmetrically), so we can't let the object
				 * stay COPY_SYMMETRIC.
				 * So we always switch from COPY_SYMMETRIC to
				 * COPY_DELAY.
				 */
				object->copy_strategy =
				    MEMORY_OBJECT_COPY_DELAY;
				VM_OBJECT_SET_TRUE_SHARE(object, TRUE);
			}
			vm_object_unlock(object);
		}

		offset = (VME_OFFSET(src_entry) +
		    (src_start - src_entry->vme_start));

copy_src_entry:
		new_entry = _vm_map_entry_create(map_header);
		vm_map_entry_copy(map, new_entry, src_entry);
		if (new_entry->is_sub_map) {
			/* clr address space specifics */
			new_entry->use_pmap = FALSE;
		} else if (copy) {
			/*
			 * We're dealing with a copy-on-write operation,
			 * so the resulting mapping should not inherit the
			 * original mapping's accounting settings.
			 * "use_pmap" should be reset to its default (TRUE)
			 * so that the new mapping gets accounted for in
			 * the task's memory footprint.
			 */
			new_entry->use_pmap = TRUE;
		}
		/* "iokit_acct" was cleared in vm_map_entry_copy() */
		assert(!new_entry->iokit_acct);

		new_entry->map_aligned = FALSE;

		new_entry->vme_start = map_address;
		new_entry->vme_end = map_address + tmp_size;
		assert(new_entry->vme_start < new_entry->vme_end);
		if (copy && vmk_flags.vmkf_remap_prot_copy) {
			/* security: keep "permanent" and "csm_associated" */
			new_entry->vme_permanent = src_entry->vme_permanent;
			new_entry->csm_associated = src_entry->csm_associated;
			/*
			 * Remapping for vm_map_protect(VM_PROT_COPY)
			 * to convert a read-only mapping into a
			 * copy-on-write version of itself but
			 * with write access:
			 * keep the original inheritance but let's not
			 * add VM_PROT_WRITE to the max protection yet
			 * since we want to do more security checks against
			 * the target map.
			 */
			new_entry->inheritance = src_entry->inheritance;
			new_entry->protection &= max_prot_for_prot_copy;
		} else {
			new_entry->inheritance = inheritance;
			if (!vm_remap_legacy) {
				new_entry->protection = *cur_protection;
				new_entry->max_protection = *max_protection;
			}
		}
#ifdef __arm64e__
		if (copy && vmk_flags.vmkf_tpro_enforcement_override) {
			new_entry->used_for_tpro = FALSE;
		}
#endif /* __arm64e__ */
		VME_OFFSET_SET(new_entry, offset);

		/*
		 * The new region has to be copied now if required.
		 */
RestartCopy:
		if (!copy) {
			if (src_entry->used_for_jit == TRUE) {
				if (same_map) {
				} else if (!VM_MAP_POLICY_ALLOW_JIT_SHARING(map)) {
					/*
					 * Cannot allow an entry describing a JIT
					 * region to be shared across address spaces.
					 */
					result = KERN_INVALID_ARGUMENT;
					vm_object_deallocate(object);
					vm_map_entry_dispose(new_entry);
					new_entry = VM_MAP_ENTRY_NULL;
					break;
				}
			}

			src_entry->is_shared = TRUE;
			new_entry->is_shared = TRUE;
			if (!(new_entry->is_sub_map)) {
				new_entry->needs_copy = FALSE;
			}
		} else if (src_entry->is_sub_map) {
			/* make this a COW sub_map if not already */
			assert(new_entry->wired_count == 0);
			new_entry->needs_copy = TRUE;
			object = VM_OBJECT_NULL;
		} else if (src_entry->wired_count == 0 &&
		    !(debug4k_no_cow_copyin && VM_MAP_PAGE_SHIFT(map) < PAGE_SHIFT) &&
		    vm_object_copy_quickly(VME_OBJECT(new_entry),
		    VME_OFFSET(new_entry),
		    (new_entry->vme_end -
		    new_entry->vme_start),
		    &src_needs_copy,
		    &new_entry_needs_copy)) {
			new_entry->needs_copy = new_entry_needs_copy;
			new_entry->is_shared = FALSE;
			assertf(new_entry->use_pmap, "map %p new_entry %p\n", map, new_entry);

			/*
			 * Handle copy_on_write semantics.
			 */
			if (src_needs_copy && !src_entry->needs_copy) {
				vm_prot_t prot;

				if (pmap_has_prot_policy(map->pmap, src_entry->translated_allow_execute, src_entry->protection)) {
					panic("%s: map %p pmap %p entry %p 0x%llx:0x%llx prot 0x%x",
					    __FUNCTION__,
					    map, map->pmap, src_entry,
					    (uint64_t)src_entry->vme_start,
					    (uint64_t)src_entry->vme_end,
					    src_entry->protection);
				}

				prot = src_entry->protection & ~VM_PROT_WRITE;

				if (override_nx(map,
				    VME_ALIAS(src_entry))
				    && prot) {
					prot |= VM_PROT_EXECUTE;
				}

				if (pmap_has_prot_policy(map->pmap, src_entry->translated_allow_execute, prot)) {
					panic("%s: map %p pmap %p entry %p 0x%llx:0x%llx prot 0x%x",
					    __FUNCTION__,
					    map, map->pmap, src_entry,
					    (uint64_t)src_entry->vme_start,
					    (uint64_t)src_entry->vme_end,
					    prot);
				}

				vm_object_pmap_protect(object,
				    offset,
				    entry_size,
				    ((src_entry->is_shared
				    || map->mapped_in_other_pmaps) ?
				    PMAP_NULL : map->pmap),
				    VM_MAP_PAGE_SIZE(map),
				    src_entry->vme_start,
				    prot);

				assert(src_entry->wired_count == 0);
				src_entry->needs_copy = TRUE;
			}
			/*
			 * Throw away the old object reference of the new entry.
			 */
			vm_object_deallocate(object);
		} else {
			new_entry->is_shared = FALSE;
			assertf(new_entry->use_pmap, "map %p new_entry %p\n", map, new_entry);

			src_entry_was_wired = (src_entry->wired_count > 0);
			saved_src_entry = src_entry;
			src_entry = VM_MAP_ENTRY_NULL;

			/*
			 * The map can be safely unlocked since we
			 * already hold a reference on the object.
			 *
			 * Record the timestamp of the map for later
			 * verification, and unlock the map.
			 */
			version.main_timestamp = map->timestamp;
			vm_map_unlock(map);     /* Increments timestamp once! */

			/*
			 * Perform the copy.
			 */
			if (src_entry_was_wired > 0 ||
			    (debug4k_no_cow_copyin &&
			    VM_MAP_PAGE_SHIFT(map) < PAGE_SHIFT)) {
				vm_object_lock(object);
				result = vm_object_copy_slowly(
					object,
					offset,
					(new_entry->vme_end -
					new_entry->vme_start),
					THREAD_UNINT,
					&new_copy_object);
				/* VME_OBJECT_SET will reset used_for_jit, so preserve it. */
				saved_used_for_jit = new_entry->used_for_jit;
				VME_OBJECT_SET(new_entry, new_copy_object, false, 0);
				new_entry->used_for_jit = saved_used_for_jit;
				VME_OFFSET_SET(new_entry, offset - vm_object_trunc_page(offset));
				new_entry->needs_copy = FALSE;
			} else {
				vm_object_offset_t new_offset;

				new_offset = VME_OFFSET(new_entry);
				result = vm_object_copy_strategically(
					object,
					offset,
					(new_entry->vme_end -
					new_entry->vme_start),
					false, /* forking */
					&new_copy_object,
					&new_offset,
					&new_entry_needs_copy);
				/* VME_OBJECT_SET will reset used_for_jit, so preserve it. */
				saved_used_for_jit = new_entry->used_for_jit;
				VME_OBJECT_SET(new_entry, new_copy_object, false, 0);
				new_entry->used_for_jit = saved_used_for_jit;
				if (new_offset != VME_OFFSET(new_entry)) {
					VME_OFFSET_SET(new_entry, new_offset);
				}

				new_entry->needs_copy = new_entry_needs_copy;
			}

			/*
			 * Throw away the old object reference of the new entry.
			 */
			vm_object_deallocate(object);

			if (result != KERN_SUCCESS &&
			    result != KERN_MEMORY_RESTART_COPY) {
				vm_map_entry_dispose(new_entry);
				vm_map_lock(map);
				break;
			}

			/*
			 * Verify that the map has not substantially
			 * changed while the copy was being made.
			 */

			vm_map_lock(map);
			if (version.main_timestamp + 1 != map->timestamp) {
				/*
				 * Simple version comparison failed.
				 *
				 * Retry the lookup and verify that the
				 * same object/offset are still present.
				 */
				saved_src_entry = VM_MAP_ENTRY_NULL;
				vm_object_deallocate(VME_OBJECT(new_entry));
				vm_map_entry_dispose(new_entry);
				if (result == KERN_MEMORY_RESTART_COPY) {
					result = KERN_SUCCESS;
				}
				continue;
			}
			/* map hasn't changed: src_entry is still valid */
			src_entry = saved_src_entry;
			saved_src_entry = VM_MAP_ENTRY_NULL;

			if (result == KERN_MEMORY_RESTART_COPY) {
				vm_object_reference(object);
				goto RestartCopy;
			}
		}

		_vm_map_store_entry_link(map_header,
		    map_header->links.prev, new_entry);

		/* protections for submap mapping are irrelevant here */
		if (vm_remap_legacy && !src_entry->is_sub_map) {
			*cur_protection &= src_entry->protection;
			*max_protection &= src_entry->max_protection;
		}

		map_address += tmp_size;
		mapped_size += tmp_size;
		src_start += tmp_size;

		if (vmk_flags.vmkf_copy_single_object) {
			if (mapped_size != size) {
				DEBUG4K_SHARE("map %p addr 0x%llx size 0x%llx clipped copy at mapped_size 0x%llx\n",
				    map, (uint64_t)addr, (uint64_t)size, (uint64_t)mapped_size);
				if (src_entry->vme_next != vm_map_to_entry(map) &&
				    src_entry->vme_next->vme_object_value ==
				    src_entry->vme_object_value) {
					/* XXX TODO4K */
					DEBUG4K_ERROR("could have extended copy to next entry...\n");
				}
			}
			break;
		}
	} /* end while */

	vm_map_unlock(map);
	if (result != KERN_SUCCESS) {
		/*
		 * Free all allocated elements.
		 */
		for (src_entry = map_header->links.next;
		    src_entry != CAST_TO_VM_MAP_ENTRY(&map_header->links);
		    src_entry = new_entry) {
			new_entry = src_entry->vme_next;
			_vm_map_store_entry_unlink(map_header, src_entry, false);
			if (src_entry->is_sub_map) {
				vm_map_deallocate(VME_SUBMAP(src_entry));
			} else {
				vm_object_deallocate(VME_OBJECT(src_entry));
			}
			vm_map_entry_dispose(src_entry);
		}
	}
	return result;
}

bool
vm_map_is_exotic(
	vm_map_t map)
{
	return VM_MAP_IS_EXOTIC(map);
}

bool
vm_map_is_alien(
	vm_map_t map)
{
	return VM_MAP_IS_ALIEN(map);
}

#if XNU_TARGET_OS_OSX
void
vm_map_mark_alien(
	vm_map_t map)
{
	vm_map_lock(map);
	map->is_alien = true;
	vm_map_unlock(map);
}

void
vm_map_single_jit(
	vm_map_t map)
{
	vm_map_lock(map);
	map->single_jit = true;
	vm_map_unlock(map);
}
#endif /* XNU_TARGET_OS_OSX */


/*
 * Callers of this function must call vm_map_copy_require on
 * previously created vm_map_copy_t or pass a newly created
 * one to ensure that it hasn't been forged.
 */
static kern_return_t
vm_map_copy_to_physcopy(
	vm_map_copy_t   copy_map,
	vm_map_t        target_map)
{
	vm_map_size_t           size;
	vm_map_entry_t          entry;
	vm_map_entry_t          new_entry;
	vm_object_t             new_object;
	unsigned int            pmap_flags;
	pmap_t                  new_pmap;
	vm_map_t                new_map;
	vm_map_address_t        src_start, src_end, src_cur;
	vm_map_address_t        dst_start, dst_end, dst_cur;
	kern_return_t           kr;
	void                    *kbuf;

	/*
	 * Perform the equivalent of vm_allocate() and memcpy().
	 * Replace the mappings in "copy_map" with the newly allocated mapping.
	 */
	DEBUG4K_COPY("copy_map %p (%d %d 0x%llx 0x%llx) BEFORE\n", copy_map, copy_map->cpy_hdr.page_shift, copy_map->cpy_hdr.nentries, copy_map->offset, (uint64_t)copy_map->size);

	assert(copy_map->cpy_hdr.page_shift != VM_MAP_PAGE_MASK(target_map));

	/* create a new pmap to map "copy_map" */
	pmap_flags = 0;
	assert(copy_map->cpy_hdr.page_shift == FOURK_PAGE_SHIFT);
#if PMAP_CREATE_FORCE_4K_PAGES
	pmap_flags |= PMAP_CREATE_FORCE_4K_PAGES;
#endif /* PMAP_CREATE_FORCE_4K_PAGES */
	pmap_flags |= PMAP_CREATE_64BIT;
	new_pmap = pmap_create_options(NULL, (vm_map_size_t)0, pmap_flags);
	if (new_pmap == NULL) {
		return KERN_RESOURCE_SHORTAGE;
	}

	/* allocate new VM object */
	size = VM_MAP_ROUND_PAGE(copy_map->size, PAGE_MASK);
	new_object = vm_object_allocate(size);
	assert(new_object);

	/* allocate new VM map entry */
	new_entry = vm_map_copy_entry_create(copy_map);
	assert(new_entry);

	/* finish initializing new VM map entry */
	new_entry->protection = VM_PROT_DEFAULT;
	new_entry->max_protection = VM_PROT_DEFAULT;
	new_entry->use_pmap = TRUE;

	/* make new VM map entry point to new VM object */
	new_entry->vme_start = 0;
	new_entry->vme_end = size;
	VME_OBJECT_SET(new_entry, new_object, false, 0);
	VME_OFFSET_SET(new_entry, 0);

	/* create a new pageable VM map to map "copy_map" */
	new_map = vm_map_create_options(new_pmap, 0, MACH_VM_MAX_ADDRESS,
	    VM_MAP_CREATE_PAGEABLE);
	assert(new_map);
	vm_map_set_page_shift(new_map, copy_map->cpy_hdr.page_shift);

	/* map "copy_map" in the new VM map */
	src_start = 0;
	kr = vm_map_copyout_internal(
		new_map,
		&src_start,
		copy_map,
		copy_map->size,
		FALSE, /* consume_on_success */
		VM_PROT_DEFAULT,
		VM_PROT_DEFAULT,
		VM_INHERIT_DEFAULT);
	assert(kr == KERN_SUCCESS);
	src_end = src_start + copy_map->size;

	/* map "new_object" in the new VM map */
	vm_object_reference(new_object);
	dst_start = 0;
	kr = vm_map_enter(new_map,
	    &dst_start,
	    size,
	    0,               /* mask */
	    VM_MAP_KERNEL_FLAGS_ANYWHERE(.vm_tag = VM_KERN_MEMORY_OSFMK),
	    new_object,
	    0,               /* offset */
	    FALSE,               /* needs copy */
	    VM_PROT_DEFAULT,
	    VM_PROT_DEFAULT,
	    VM_INHERIT_DEFAULT);
	assert(kr == KERN_SUCCESS);
	dst_end = dst_start + size;

	/* get a kernel buffer */
	kbuf = kalloc_data(PAGE_SIZE, Z_WAITOK | Z_NOFAIL);

	/* physically copy "copy_map" mappings to new VM object */
	for (src_cur = src_start, dst_cur = dst_start;
	    src_cur < src_end;
	    src_cur += PAGE_SIZE, dst_cur += PAGE_SIZE) {
		vm_size_t bytes;

		bytes = PAGE_SIZE;
		if (src_cur + PAGE_SIZE > src_end) {
			/* partial copy for last page */
			bytes = src_end - src_cur;
			assert(bytes > 0 && bytes < PAGE_SIZE);
			/* rest of dst page should be zero-filled */
		}
		/* get bytes from src mapping */
		kr = copyinmap(new_map, src_cur, kbuf, bytes);
		if (kr != KERN_SUCCESS) {
			DEBUG4K_COPY("copyinmap(%p, 0x%llx, %p, 0x%llx) kr 0x%x\n", new_map, (uint64_t)src_cur, kbuf, (uint64_t)bytes, kr);
		}
		/* put bytes in dst mapping */
		assert(dst_cur < dst_end);
		assert(dst_cur + bytes <= dst_end);
		kr = copyoutmap(new_map, kbuf, dst_cur, bytes);
		if (kr != KERN_SUCCESS) {
			DEBUG4K_COPY("copyoutmap(%p, %p, 0x%llx, 0x%llx) kr 0x%x\n", new_map, kbuf, (uint64_t)dst_cur, (uint64_t)bytes, kr);
		}
	}

	/* free kernel buffer */
	kfree_data(kbuf, PAGE_SIZE);

	/* destroy new map */
	vm_map_destroy(new_map);
	new_map = VM_MAP_NULL;

	/* dispose of the old map entries in "copy_map" */
	while (vm_map_copy_first_entry(copy_map) !=
	    vm_map_copy_to_entry(copy_map)) {
		entry = vm_map_copy_first_entry(copy_map);
		vm_map_copy_entry_unlink(copy_map, entry);
		if (entry->is_sub_map) {
			vm_map_deallocate(VME_SUBMAP(entry));
		} else {
			vm_object_deallocate(VME_OBJECT(entry));
		}
		vm_map_copy_entry_dispose(entry);
	}

	/* change "copy_map"'s page_size to match "target_map" */
	copy_map->cpy_hdr.page_shift = (uint16_t)VM_MAP_PAGE_SHIFT(target_map);
	copy_map->offset = 0;
	copy_map->size = size;

	/* insert new map entry in "copy_map" */
	assert(vm_map_copy_last_entry(copy_map) == vm_map_copy_to_entry(copy_map));
	vm_map_copy_entry_link(copy_map, vm_map_copy_last_entry(copy_map), new_entry);

	DEBUG4K_COPY("copy_map %p (%d %d 0x%llx 0x%llx) AFTER\n", copy_map, copy_map->cpy_hdr.page_shift, copy_map->cpy_hdr.nentries, copy_map->offset, (uint64_t)copy_map->size);
	return KERN_SUCCESS;
}

void
vm_map_copy_adjust_get_target_copy_map(
	vm_map_copy_t   copy_map,
	vm_map_copy_t   *target_copy_map_p);
void
vm_map_copy_adjust_get_target_copy_map(
	vm_map_copy_t   copy_map,
	vm_map_copy_t   *target_copy_map_p)
{
	vm_map_copy_t   target_copy_map;
	vm_map_entry_t  entry, target_entry;

	if (*target_copy_map_p != VM_MAP_COPY_NULL) {
		/* the caller already has a "target_copy_map": use it */
		return;
	}

	/* the caller wants us to create a new copy of "copy_map" */
	assert(copy_map->type == VM_MAP_COPY_ENTRY_LIST);
	target_copy_map = vm_map_copy_allocate(copy_map->type);
	target_copy_map->offset = copy_map->offset;
	target_copy_map->size = copy_map->size;
	target_copy_map->cpy_hdr.page_shift = copy_map->cpy_hdr.page_shift;
	for (entry = vm_map_copy_first_entry(copy_map);
	    entry != vm_map_copy_to_entry(copy_map);
	    entry = entry->vme_next) {
		target_entry = vm_map_copy_entry_create(target_copy_map);
		vm_map_entry_copy_full(target_entry, entry);
		if (target_entry->is_sub_map) {
			vm_map_reference(VME_SUBMAP(target_entry));
		} else {
			vm_object_reference(VME_OBJECT(target_entry));
		}
		vm_map_copy_entry_link(
			target_copy_map,
			vm_map_copy_last_entry(target_copy_map),
			target_entry);
	}
	entry = VM_MAP_ENTRY_NULL;
	*target_copy_map_p = target_copy_map;
}

/*
 * Callers of this function must call vm_map_copy_require on
 * previously created vm_map_copy_t or pass a newly created
 * one to ensure that it hasn't been forged.
 */
static void
vm_map_copy_trim(
	vm_map_copy_t   copy_map,
	uint16_t        new_page_shift,
	vm_map_offset_t trim_start,
	vm_map_offset_t trim_end)
{
	uint16_t        copy_page_shift;
	vm_map_entry_t  entry, next_entry;

	assert(copy_map->type == VM_MAP_COPY_ENTRY_LIST);
	assert(copy_map->cpy_hdr.nentries > 0);

	trim_start += vm_map_copy_first_entry(copy_map)->vme_start;
	trim_end += vm_map_copy_first_entry(copy_map)->vme_start;

	/* use the new page_shift to do the clipping */
	copy_page_shift = VM_MAP_COPY_PAGE_SHIFT(copy_map);
	copy_map->cpy_hdr.page_shift = new_page_shift;

	for (entry = vm_map_copy_first_entry(copy_map);
	    entry != vm_map_copy_to_entry(copy_map);
	    entry = next_entry) {
		next_entry = entry->vme_next;
		if (entry->vme_end <= trim_start) {
			/* entry fully before trim range: skip */
			continue;
		}
		if (entry->vme_start >= trim_end) {
			/* entry fully after trim range: done */
			break;
		}
		/* clip entry if needed */
		vm_map_copy_clip_start(copy_map, entry, trim_start);
		vm_map_copy_clip_end(copy_map, entry, trim_end);
		/* dispose of entry */
		copy_map->size -= entry->vme_end - entry->vme_start;
		vm_map_copy_entry_unlink(copy_map, entry);
		if (entry->is_sub_map) {
			vm_map_deallocate(VME_SUBMAP(entry));
		} else {
			vm_object_deallocate(VME_OBJECT(entry));
		}
		vm_map_copy_entry_dispose(entry);
		entry = VM_MAP_ENTRY_NULL;
	}

	/* restore copy_map's original page_shift */
	copy_map->cpy_hdr.page_shift = copy_page_shift;
}

/*
 * Make any necessary adjustments to "copy_map" to allow it to be
 * mapped into "target_map".
 * If no changes were necessary, "target_copy_map" points to the
 * untouched "copy_map".
 * If changes are necessary, changes will be made to "target_copy_map".
 * If "target_copy_map" was NULL, we create a new "vm_map_copy_t" and
 * copy the original "copy_map" to it before applying the changes.
 * The caller should discard "target_copy_map" if it's not the same as
 * the original "copy_map".
 */
/* TODO4K: also adjust to sub-range in the copy_map -> add start&end? */
kern_return_t
vm_map_copy_adjust_to_target(
	vm_map_copy_t           src_copy_map,
	vm_map_offset_t         offset,
	vm_map_size_t           size,
	vm_map_t                target_map,
	boolean_t               copy,
	vm_map_copy_t           *target_copy_map_p,
	vm_map_offset_t         *overmap_start_p,
	vm_map_offset_t         *overmap_end_p,
	vm_map_offset_t         *trimmed_start_p)
{
	vm_map_copy_t           copy_map, target_copy_map;
	vm_map_size_t           target_size;
	vm_map_size_t           src_copy_map_size;
	vm_map_size_t           overmap_start, overmap_end;
	int                     misalignments;
	vm_map_entry_t          entry, target_entry;
	vm_map_offset_t         addr_adjustment;
	vm_map_offset_t         new_start, new_end;
	int                     copy_page_mask, target_page_mask;
	uint16_t                copy_page_shift, target_page_shift;
	vm_map_offset_t         trimmed_end;

	/*
	 * Assert that the vm_map_copy is coming from the right
	 * zone and hasn't been forged
	 */
	vm_map_copy_require(src_copy_map);
	assert(src_copy_map->type == VM_MAP_COPY_ENTRY_LIST);

	/*
	 * Start working with "src_copy_map" but we'll switch
	 * to "target_copy_map" as soon as we start making adjustments.
	 */
	copy_map = src_copy_map;
	src_copy_map_size = src_copy_map->size;

	copy_page_shift = VM_MAP_COPY_PAGE_SHIFT(copy_map);
	copy_page_mask = VM_MAP_COPY_PAGE_MASK(copy_map);
	target_page_shift = (uint16_t)VM_MAP_PAGE_SHIFT(target_map);
	target_page_mask = VM_MAP_PAGE_MASK(target_map);

	DEBUG4K_ADJUST("copy_map %p (%d offset 0x%llx size 0x%llx) target_map %p (%d) copy %d offset 0x%llx size 0x%llx target_copy_map %p...\n", copy_map, copy_page_shift, (uint64_t)copy_map->offset, (uint64_t)copy_map->size, target_map, target_page_shift, copy, (uint64_t)offset, (uint64_t)size, *target_copy_map_p);

	target_copy_map = *target_copy_map_p;
	if (target_copy_map != VM_MAP_COPY_NULL) {
		vm_map_copy_require(target_copy_map);
	}

	if (offset + size > copy_map->size) {
		DEBUG4K_ERROR("copy_map %p (%d->%d) copy_map->size 0x%llx offset 0x%llx size 0x%llx KERN_INVALID_ARGUMENT\n", copy_map, copy_page_shift, target_page_shift, (uint64_t)copy_map->size, (uint64_t)offset, (uint64_t)size);
		return KERN_INVALID_ARGUMENT;
	}

	/* trim the end */
	trimmed_end = 0;
	new_end = VM_MAP_ROUND_PAGE(offset + size, target_page_mask);
	if (new_end < copy_map->size) {
		trimmed_end = src_copy_map_size - new_end;
		DEBUG4K_ADJUST("copy_map %p (%d->%d) copy %d offset 0x%llx size 0x%llx target_copy_map %p... trim end from 0x%llx to 0x%llx\n", copy_map, copy_page_shift, target_page_shift, copy, (uint64_t)offset, (uint64_t)size, target_copy_map, (uint64_t)new_end, (uint64_t)copy_map->size);
		/* get "target_copy_map" if needed and adjust it */
		vm_map_copy_adjust_get_target_copy_map(copy_map,
		    &target_copy_map);
		copy_map = target_copy_map;
		vm_map_copy_trim(target_copy_map, target_page_shift,
		    new_end, copy_map->size);
	}

	/* trim the start */
	new_start = VM_MAP_TRUNC_PAGE(offset, target_page_mask);
	if (new_start != 0) {
		DEBUG4K_ADJUST("copy_map %p (%d->%d) copy %d offset 0x%llx size 0x%llx target_copy_map %p... trim start from 0x%llx to 0x%llx\n", copy_map, copy_page_shift, target_page_shift, copy, (uint64_t)offset, (uint64_t)size, target_copy_map, (uint64_t)0, (uint64_t)new_start);
		/* get "target_copy_map" if needed and adjust it */
		vm_map_copy_adjust_get_target_copy_map(copy_map,
		    &target_copy_map);
		copy_map = target_copy_map;
		vm_map_copy_trim(target_copy_map, target_page_shift,
		    0, new_start);
	}
	*trimmed_start_p = new_start;

	/* target_size starts with what's left after trimming */
	target_size = copy_map->size;
	assertf(target_size == src_copy_map_size - *trimmed_start_p - trimmed_end,
	    "target_size 0x%llx src_copy_map_size 0x%llx trimmed_start 0x%llx trimmed_end 0x%llx\n",
	    (uint64_t)target_size, (uint64_t)src_copy_map_size,
	    (uint64_t)*trimmed_start_p, (uint64_t)trimmed_end);

	/* check for misalignments but don't adjust yet */
	misalignments = 0;
	overmap_start = 0;
	overmap_end = 0;
	if (copy_page_shift < target_page_shift) {
		/*
		 * Remapping from 4K to 16K: check the VM object alignments
		 * throughout the range.
		 * If the start and end of the range are mis-aligned, we can
		 * over-map to re-align, and adjust the "overmap" start/end
		 * and "target_size" of the range accordingly.
		 * If there is any mis-alignment within the range:
		 *     if "copy":
		 *         we can do immediate-copy instead of copy-on-write,
		 *     else:
		 *         no way to remap and share; fail.
		 */
		for (entry = vm_map_copy_first_entry(copy_map);
		    entry != vm_map_copy_to_entry(copy_map);
		    entry = entry->vme_next) {
			vm_object_offset_t object_offset_start, object_offset_end;

			object_offset_start = VME_OFFSET(entry);
			object_offset_end = object_offset_start;
			object_offset_end += entry->vme_end - entry->vme_start;
			if (object_offset_start & target_page_mask) {
				if (entry == vm_map_copy_first_entry(copy_map) && !copy) {
					overmap_start++;
				} else {
					misalignments++;
				}
			}
			if (object_offset_end & target_page_mask) {
				if (entry->vme_next == vm_map_copy_to_entry(copy_map) && !copy) {
					overmap_end++;
				} else {
					misalignments++;
				}
			}
		}
	}
	entry = VM_MAP_ENTRY_NULL;

	/* decide how to deal with misalignments */
	assert(overmap_start <= 1);
	assert(overmap_end <= 1);
	if (!overmap_start && !overmap_end && !misalignments) {
		/* copy_map is properly aligned for target_map ... */
		if (*trimmed_start_p) {
			/* ... but we trimmed it, so still need to adjust */
		} else {
			/* ... and we didn't trim anything: we're done */
			if (target_copy_map == VM_MAP_COPY_NULL) {
				target_copy_map = copy_map;
			}
			*target_copy_map_p = target_copy_map;
			*overmap_start_p = 0;
			*overmap_end_p = 0;
			DEBUG4K_ADJUST("copy_map %p (%d offset 0x%llx size 0x%llx) target_map %p (%d) copy %d target_copy_map %p (%d offset 0x%llx size 0x%llx) -> trimmed 0x%llx overmap start 0x%llx end 0x%llx KERN_SUCCESS\n", copy_map, copy_page_shift, (uint64_t)copy_map->offset, (uint64_t)copy_map->size, target_map, target_page_shift, copy, *target_copy_map_p, VM_MAP_COPY_PAGE_SHIFT(*target_copy_map_p), (uint64_t)(*target_copy_map_p)->offset, (uint64_t)(*target_copy_map_p)->size, (uint64_t)*trimmed_start_p, (uint64_t)*overmap_start_p, (uint64_t)*overmap_end_p);
			return KERN_SUCCESS;
		}
	} else if (misalignments && !copy) {
		/* can't "share" if misaligned */
		DEBUG4K_ADJUST("unsupported sharing\n");
#if MACH_ASSERT
		if (debug4k_panic_on_misaligned_sharing) {
			panic("DEBUG4k %s:%d unsupported sharing", __FUNCTION__, __LINE__);
		}
#endif /* MACH_ASSERT */
		DEBUG4K_ADJUST("copy_map %p (%d) target_map %p (%d) copy %d target_copy_map %p -> KERN_NOT_SUPPORTED\n", copy_map, copy_page_shift, target_map, target_page_shift, copy, *target_copy_map_p);
		return KERN_NOT_SUPPORTED;
	} else {
		/* can't virtual-copy if misaligned (but can physical-copy) */
		DEBUG4K_ADJUST("mis-aligned copying\n");
	}

	/* get a "target_copy_map" if needed and switch to it */
	vm_map_copy_adjust_get_target_copy_map(copy_map, &target_copy_map);
	copy_map = target_copy_map;

	if (misalignments && copy) {
		vm_map_size_t target_copy_map_size;

		/*
		 * Can't do copy-on-write with misaligned mappings.
		 * Replace the mappings with a physical copy of the original
		 * mappings' contents.
		 */
		target_copy_map_size = target_copy_map->size;
		kern_return_t kr = vm_map_copy_to_physcopy(target_copy_map, target_map);
		if (kr != KERN_SUCCESS) {
			return kr;
		}
		*target_copy_map_p = target_copy_map;
		*overmap_start_p = 0;
		*overmap_end_p = target_copy_map->size - target_copy_map_size;
		DEBUG4K_ADJUST("copy_map %p (%d offset 0x%llx size 0x%llx) target_map %p (%d) copy %d target_copy_map %p (%d offset 0x%llx size 0x%llx)-> trimmed 0x%llx overmap start 0x%llx end 0x%llx PHYSCOPY\n", copy_map, copy_page_shift, (uint64_t)copy_map->offset, (uint64_t)copy_map->size, target_map, target_page_shift, copy, *target_copy_map_p, VM_MAP_COPY_PAGE_SHIFT(*target_copy_map_p), (uint64_t)(*target_copy_map_p)->offset, (uint64_t)(*target_copy_map_p)->size, (uint64_t)*trimmed_start_p, (uint64_t)*overmap_start_p, (uint64_t)*overmap_end_p);
		return KERN_SUCCESS;
	}

	/* apply the adjustments */
	misalignments = 0;
	overmap_start = 0;
	overmap_end = 0;
	/* remove copy_map->offset, so that everything starts at offset 0 */
	addr_adjustment = copy_map->offset;
	/* also remove whatever we trimmed from the start */
	addr_adjustment += *trimmed_start_p;
	for (target_entry = vm_map_copy_first_entry(target_copy_map);
	    target_entry != vm_map_copy_to_entry(target_copy_map);
	    target_entry = target_entry->vme_next) {
		vm_object_offset_t object_offset_start, object_offset_end;

		DEBUG4K_ADJUST("copy %p (%d 0x%llx 0x%llx) entry %p [ 0x%llx 0x%llx ] object %p offset 0x%llx BEFORE\n", target_copy_map, VM_MAP_COPY_PAGE_SHIFT(target_copy_map), target_copy_map->offset, (uint64_t)target_copy_map->size, target_entry, (uint64_t)target_entry->vme_start, (uint64_t)target_entry->vme_end, VME_OBJECT(target_entry), VME_OFFSET(target_entry));
		object_offset_start = VME_OFFSET(target_entry);
		if (object_offset_start & target_page_mask) {
			DEBUG4K_ADJUST("copy %p (%d 0x%llx 0x%llx) entry %p [ 0x%llx 0x%llx ] object %p offset 0x%llx misaligned at start\n", target_copy_map, VM_MAP_COPY_PAGE_SHIFT(target_copy_map), target_copy_map->offset, (uint64_t)target_copy_map->size, target_entry, (uint64_t)target_entry->vme_start, (uint64_t)target_entry->vme_end, VME_OBJECT(target_entry), VME_OFFSET(target_entry));
			if (target_entry == vm_map_copy_first_entry(target_copy_map)) {
				/*
				 * start of 1st entry is mis-aligned:
				 * re-adjust by over-mapping.
				 */
				overmap_start = object_offset_start - trunc_page_mask_64(object_offset_start, target_page_mask);
				DEBUG4K_ADJUST("entry %p offset 0x%llx copy %d -> overmap_start 0x%llx\n", target_entry, VME_OFFSET(target_entry), copy, (uint64_t)overmap_start);
				VME_OFFSET_SET(target_entry, VME_OFFSET(target_entry) - overmap_start);
			} else {
				misalignments++;
				DEBUG4K_ADJUST("entry %p offset 0x%llx copy %d -> misalignments %d\n", target_entry, VME_OFFSET(target_entry), copy, misalignments);
				assert(copy);
			}
		}

		if (target_entry == vm_map_copy_first_entry(target_copy_map)) {
			target_size += overmap_start;
		} else {
			target_entry->vme_start += overmap_start;
		}
		target_entry->vme_end += overmap_start;

		object_offset_end = VME_OFFSET(target_entry) + target_entry->vme_end - target_entry->vme_start;
		if (object_offset_end & target_page_mask) {
			DEBUG4K_ADJUST("copy %p (%d 0x%llx 0x%llx) entry %p [ 0x%llx 0x%llx ] object %p offset 0x%llx misaligned at end\n", target_copy_map, VM_MAP_COPY_PAGE_SHIFT(target_copy_map), target_copy_map->offset, (uint64_t)target_copy_map->size, target_entry, (uint64_t)target_entry->vme_start, (uint64_t)target_entry->vme_end, VME_OBJECT(target_entry), VME_OFFSET(target_entry));
			if (target_entry->vme_next == vm_map_copy_to_entry(target_copy_map)) {
				/*
				 * end of last entry is mis-aligned: re-adjust by over-mapping.
				 */
				overmap_end = round_page_mask_64(object_offset_end, target_page_mask) - object_offset_end;
				DEBUG4K_ADJUST("entry %p offset 0x%llx copy %d -> overmap_end 0x%llx\n", target_entry, VME_OFFSET(target_entry), copy, (uint64_t)overmap_end);
				target_entry->vme_end += overmap_end;
				target_size += overmap_end;
			} else {
				misalignments++;
				DEBUG4K_ADJUST("entry %p offset 0x%llx copy %d -> misalignments %d\n", target_entry, VME_OFFSET(target_entry), copy, misalignments);
				assert(copy);
			}
		}
		target_entry->vme_start -= addr_adjustment;
		target_entry->vme_end -= addr_adjustment;
		DEBUG4K_ADJUST("copy %p (%d 0x%llx 0x%llx) entry %p [ 0x%llx 0x%llx ] object %p offset 0x%llx AFTER\n", target_copy_map, VM_MAP_COPY_PAGE_SHIFT(target_copy_map), target_copy_map->offset, (uint64_t)target_copy_map->size, target_entry, (uint64_t)target_entry->vme_start, (uint64_t)target_entry->vme_end, VME_OBJECT(target_entry), VME_OFFSET(target_entry));
	}

	target_copy_map->size = target_size;
	target_copy_map->offset += overmap_start;
	target_copy_map->offset -= addr_adjustment;
	target_copy_map->cpy_hdr.page_shift = target_page_shift;

//	assert(VM_MAP_PAGE_ALIGNED(target_copy_map->size, target_page_mask));
//	assert(VM_MAP_PAGE_ALIGNED(target_copy_map->offset, FOURK_PAGE_MASK));
	assert(overmap_start < VM_MAP_PAGE_SIZE(target_map));
	assert(overmap_end < VM_MAP_PAGE_SIZE(target_map));

	*target_copy_map_p = target_copy_map;
	*overmap_start_p = overmap_start;
	*overmap_end_p = overmap_end;

	DEBUG4K_ADJUST("copy_map %p (%d offset 0x%llx size 0x%llx) target_map %p (%d) copy %d target_copy_map %p (%d offset 0x%llx size 0x%llx) -> trimmed 0x%llx overmap start 0x%llx end 0x%llx KERN_SUCCESS\n", copy_map, copy_page_shift, (uint64_t)copy_map->offset, (uint64_t)copy_map->size, target_map, target_page_shift, copy, *target_copy_map_p, VM_MAP_COPY_PAGE_SHIFT(*target_copy_map_p), (uint64_t)(*target_copy_map_p)->offset, (uint64_t)(*target_copy_map_p)->size, (uint64_t)*trimmed_start_p, (uint64_t)*overmap_start_p, (uint64_t)*overmap_end_p);
	return KERN_SUCCESS;
}

kern_return_t
vm_map_range_physical_size(
	vm_map_t         map,
	vm_map_address_t start,
	mach_vm_size_t   size,
	mach_vm_size_t * phys_size)
{
	kern_return_t   kr;
	vm_map_copy_t   copy_map, target_copy_map;
	vm_map_offset_t adjusted_start, adjusted_end;
	vm_map_size_t   adjusted_size;
	vm_prot_t       cur_prot, max_prot;
	vm_map_offset_t overmap_start, overmap_end, trimmed_start, end;
	vm_map_kernel_flags_t vmk_flags;

	if (size == 0) {
		DEBUG4K_SHARE("map %p start 0x%llx size 0x%llx -> phys_size 0!\n", map, (uint64_t)start, (uint64_t)size);
		*phys_size = 0;
		return KERN_SUCCESS;
	}

	adjusted_start = vm_map_trunc_page(start, VM_MAP_PAGE_MASK(map));
	adjusted_end = vm_map_round_page(start + size, VM_MAP_PAGE_MASK(map));
	if (__improbable(os_add_overflow(start, size, &end) ||
	    adjusted_end <= adjusted_start)) {
		/* wraparound */
		printf("%s:%d(start=0x%llx, size=0x%llx) pgmask 0x%x: wraparound\n", __FUNCTION__, __LINE__, (uint64_t)start, (uint64_t)size, VM_MAP_PAGE_MASK(map));
		*phys_size = 0;
		return KERN_INVALID_ARGUMENT;
	}
	if (__improbable(vm_map_range_overflows(map, start, size))) {
		*phys_size = 0;
		return KERN_INVALID_ADDRESS;
	}
	assert(adjusted_end > adjusted_start);
	adjusted_size = adjusted_end - adjusted_start;
	*phys_size = adjusted_size;
	if (VM_MAP_PAGE_SIZE(map) == PAGE_SIZE) {
		return KERN_SUCCESS;
	}
	if (start == 0) {
		adjusted_start = vm_map_trunc_page(start, PAGE_MASK);
		adjusted_end = vm_map_round_page(start + size, PAGE_MASK);
		if (__improbable(adjusted_end <= adjusted_start)) {
			/* wraparound */
			printf("%s:%d(start=0x%llx, size=0x%llx) pgmask 0x%x: wraparound\n", __FUNCTION__, __LINE__, (uint64_t)start, (uint64_t)size, PAGE_MASK);
			*phys_size = 0;
			return KERN_INVALID_ARGUMENT;
		}
		assert(adjusted_end > adjusted_start);
		adjusted_size = adjusted_end - adjusted_start;
		*phys_size = adjusted_size;
		return KERN_SUCCESS;
	}

	vmk_flags = VM_MAP_KERNEL_FLAGS_NONE;
	vmk_flags.vmkf_copy_pageable = TRUE;
	vmk_flags.vmkf_copy_same_map = TRUE;
	assert(adjusted_size != 0);
	cur_prot = VM_PROT_NONE; /* legacy mode */
	max_prot = VM_PROT_NONE; /* legacy mode */
	kr = vm_map_copy_extract(map, adjusted_start, adjusted_size,
	    FALSE /* copy */,
	    &copy_map,
	    &cur_prot, &max_prot, VM_INHERIT_DEFAULT,
	    vmk_flags);
	if (kr != KERN_SUCCESS) {
		DEBUG4K_ERROR("map %p start 0x%llx 0x%llx size 0x%llx 0x%llx kr 0x%x\n", map, (uint64_t)start, (uint64_t)adjusted_start, size, (uint64_t)adjusted_size, kr);
		//assert(0);
		*phys_size = 0;
		return kr;
	}
	assert(copy_map != VM_MAP_COPY_NULL);
	target_copy_map = copy_map;
	DEBUG4K_ADJUST("adjusting...\n");
	kr = vm_map_copy_adjust_to_target(
		copy_map,
		start - adjusted_start, /* offset */
		size, /* size */
		kernel_map,
		FALSE,                          /* copy */
		&target_copy_map,
		&overmap_start,
		&overmap_end,
		&trimmed_start);
	if (kr == KERN_SUCCESS) {
		if (target_copy_map->size != *phys_size) {
			DEBUG4K_ADJUST("map %p (%d) start 0x%llx size 0x%llx adjusted_start 0x%llx adjusted_end 0x%llx overmap_start 0x%llx overmap_end 0x%llx trimmed_start 0x%llx phys_size 0x%llx -> 0x%llx\n", map, VM_MAP_PAGE_SHIFT(map), (uint64_t)start, (uint64_t)size, (uint64_t)adjusted_start, (uint64_t)adjusted_end, (uint64_t)overmap_start, (uint64_t)overmap_end, (uint64_t)trimmed_start, (uint64_t)*phys_size, (uint64_t)target_copy_map->size);
		}
		*phys_size = target_copy_map->size;
	} else {
		DEBUG4K_ERROR("map %p start 0x%llx 0x%llx size 0x%llx 0x%llx kr 0x%x\n", map, (uint64_t)start, (uint64_t)adjusted_start, size, (uint64_t)adjusted_size, kr);
		//assert(0);
		*phys_size = 0;
	}
	vm_map_copy_discard(copy_map);
	copy_map = VM_MAP_COPY_NULL;

	return kr;
}


kern_return_t
memory_entry_check_for_adjustment(
	vm_map_t                        src_map,
	ipc_port_t                      port,
	vm_map_offset_t         *overmap_start,
	vm_map_offset_t         *overmap_end)
{
	kern_return_t kr = KERN_SUCCESS;
	vm_map_copy_t copy_map = VM_MAP_COPY_NULL, target_copy_map = VM_MAP_COPY_NULL;

	assert(port);
	assertf(ip_kotype(port) == IKOT_NAMED_ENTRY, "Port Type expected: %d...received:%d\n", IKOT_NAMED_ENTRY, ip_kotype(port));

	vm_named_entry_t        named_entry;

	named_entry = mach_memory_entry_from_port(port);
	named_entry_lock(named_entry);
	copy_map = named_entry->backing.copy;
	target_copy_map = copy_map;

	if (src_map && VM_MAP_PAGE_SHIFT(src_map) < PAGE_SHIFT) {
		vm_map_offset_t trimmed_start;

		trimmed_start = 0;
		DEBUG4K_ADJUST("adjusting...\n");
		kr = vm_map_copy_adjust_to_target(
			copy_map,
			0, /* offset */
			copy_map->size, /* size */
			src_map,
			FALSE, /* copy */
			&target_copy_map,
			overmap_start,
			overmap_end,
			&trimmed_start);
		assert(trimmed_start == 0);
	}
	named_entry_unlock(named_entry);

	return kr;
}


/*
 *	Routine:	vm_remap
 *
 *			Map portion of a task's address space.
 *			Mapped region must not overlap more than
 *			one vm memory object. Protections and
 *			inheritance attributes remain the same
 *			as in the original task and are	out parameters.
 *			Source and Target task can be identical
 *			Other attributes are identical as for vm_map()
 */
kern_return_t
vm_map_remap(
	vm_map_t                target_map,
	vm_map_address_t        *address,
	vm_map_size_t           size,
	vm_map_offset_t         mask,
	vm_map_kernel_flags_t   vmk_flags,
	vm_map_t                src_map,
	vm_map_offset_t         memory_address,
	boolean_t               copy,
	vm_prot_t               *cur_protection, /* IN/OUT */
	vm_prot_t               *max_protection, /* IN/OUT */
	vm_inherit_t            inheritance)
{
	kern_return_t           result;
	vm_map_entry_t          entry;
	vm_map_entry_t          insp_entry = VM_MAP_ENTRY_NULL;
	vm_map_entry_t          new_entry;
	vm_map_copy_t           copy_map;
	vm_map_offset_t         offset_in_mapping;
	vm_map_size_t           target_size = 0;
	vm_map_size_t           src_page_mask, target_page_mask;
	vm_map_offset_t         overmap_start, overmap_end, trimmed_start;
	vm_map_offset_t         initial_memory_address;
	vm_map_size_t           initial_size;
	VM_MAP_ZAP_DECLARE(zap_list);

	if (target_map == VM_MAP_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	if (__improbable(vm_map_range_overflows(src_map, memory_address, size))) {
		return KERN_INVALID_ARGUMENT;
	}

	if (__improbable((*cur_protection & *max_protection) != *cur_protection)) {
		/* cur is more permissive than max */
		return KERN_INVALID_ARGUMENT;
	}

	initial_memory_address = memory_address;
	initial_size = size;
	src_page_mask = VM_MAP_PAGE_MASK(src_map);
	target_page_mask = VM_MAP_PAGE_MASK(target_map);

	switch (inheritance) {
	case VM_INHERIT_NONE:
	case VM_INHERIT_COPY:
	case VM_INHERIT_SHARE:
		if (size != 0 && src_map != VM_MAP_NULL) {
			break;
		}
		OS_FALLTHROUGH;
	default:
		return KERN_INVALID_ARGUMENT;
	}

	if (src_page_mask != target_page_mask) {
		if (copy) {
			DEBUG4K_COPY("src_map %p pgsz 0x%x addr 0x%llx size 0x%llx copy %d -> target_map %p pgsz 0x%x\n", src_map, VM_MAP_PAGE_SIZE(src_map), (uint64_t)memory_address, (uint64_t)size, copy, target_map, VM_MAP_PAGE_SIZE(target_map));
		} else {
			DEBUG4K_SHARE("src_map %p pgsz 0x%x addr 0x%llx size 0x%llx copy %d -> target_map %p pgsz 0x%x\n", src_map, VM_MAP_PAGE_SIZE(src_map), (uint64_t)memory_address, (uint64_t)size, copy, target_map, VM_MAP_PAGE_SIZE(target_map));
		}
	}

	/*
	 * If the user is requesting that we return the address of the
	 * first byte of the data (rather than the base of the page),
	 * then we use different rounding semantics: specifically,
	 * we assume that (memory_address, size) describes a region
	 * all of whose pages we must cover, rather than a base to be truncated
	 * down and a size to be added to that base.  So we figure out
	 * the highest page that the requested region includes and make
	 * sure that the size will cover it.
	 *
	 * The key example we're worried about it is of the form:
	 *
	 *              memory_address = 0x1ff0, size = 0x20
	 *
	 * With the old semantics, we round down the memory_address to 0x1000
	 * and round up the size to 0x1000, resulting in our covering *only*
	 * page 0x1000.  With the new semantics, we'd realize that the region covers
	 * 0x1ff0-0x2010, and compute a size of 0x2000.  Thus, we cover both page
	 * 0x1000 and page 0x2000 in the region we remap.
	 */
	if (vmk_flags.vmf_return_data_addr) {
		vm_map_offset_t range_start, range_end;

		range_start = vm_map_trunc_page(memory_address, src_page_mask);
		range_end = vm_map_round_page(memory_address + size, src_page_mask);
		memory_address = range_start;
		size = range_end - range_start;
		offset_in_mapping = initial_memory_address - memory_address;
	} else {
		/*
		 * IMPORTANT:
		 * This legacy code path is broken: for the range mentioned
		 * above [ memory_address = 0x1ff0,size = 0x20 ], which spans
		 * two 4k pages, it yields [ memory_address = 0x1000,
		 * size = 0x1000 ], which covers only the first 4k page.
		 * BUT some code unfortunately depends on this bug, so we
		 * can't fix it without breaking something.
		 * New code should get automatically opted in the new
		 * behavior with the new VM_FLAGS_RETURN_DATA_ADDR flags.
		 */
		offset_in_mapping = 0;
		memory_address = vm_map_trunc_page(memory_address, src_page_mask);
		size = vm_map_round_page(size, src_page_mask);
		initial_memory_address = memory_address;
		initial_size = size;
	}


	if (size == 0) {
		return KERN_INVALID_ARGUMENT;
	}

	if (vmk_flags.vmf_resilient_media) {
		/* must be copy-on-write to be "media resilient" */
		if (!copy) {
			return KERN_INVALID_ARGUMENT;
		}
	}

	vmk_flags.vmkf_copy_pageable = target_map->hdr.entries_pageable;
	vmk_flags.vmkf_copy_same_map = (src_map == target_map);

	assert(size != 0);
	result = vm_map_copy_extract(src_map,
	    memory_address,
	    size,
	    copy, &copy_map,
	    cur_protection, /* IN/OUT */
	    max_protection, /* IN/OUT */
	    inheritance,
	    vmk_flags);
	if (result != KERN_SUCCESS) {
		return result;
	}
	assert(copy_map != VM_MAP_COPY_NULL);

	/*
	 * Handle the policy for vm map ranges
	 *
	 * If the maps differ, the target_map policy applies like for vm_map()
	 * For same mapping remaps, we preserve the range.
	 */
	if (vmk_flags.vmkf_copy_same_map) {
		vmk_flags.vmkf_range_id = copy_map->orig_range;
	} else {
		vm_map_kernel_flags_update_range_id(&vmk_flags, target_map);
	}

	overmap_start = 0;
	overmap_end = 0;
	trimmed_start = 0;
	target_size = size;
	if (src_page_mask != target_page_mask) {
		vm_map_copy_t target_copy_map;

		target_copy_map = copy_map; /* can modify "copy_map" itself */
		DEBUG4K_ADJUST("adjusting...\n");
		result = vm_map_copy_adjust_to_target(
			copy_map,
			offset_in_mapping, /* offset */
			initial_size,
			target_map,
			copy,
			&target_copy_map,
			&overmap_start,
			&overmap_end,
			&trimmed_start);
		if (result != KERN_SUCCESS) {
			DEBUG4K_COPY("failed to adjust 0x%x\n", result);
			vm_map_copy_discard(copy_map);
			return result;
		}
		if (trimmed_start == 0) {
			/* nothing trimmed: no adjustment needed */
		} else if (trimmed_start >= offset_in_mapping) {
			/* trimmed more than offset_in_mapping: nothing left */
			assert(overmap_start == 0);
			assert(overmap_end == 0);
			offset_in_mapping = 0;
		} else {
			/* trimmed some of offset_in_mapping: adjust */
			assert(overmap_start == 0);
			assert(overmap_end == 0);
			offset_in_mapping -= trimmed_start;
		}
		offset_in_mapping += overmap_start;
		target_size = target_copy_map->size;
	}

	/*
	 * Allocate/check a range of free virtual address
	 * space for the target
	 */
	*address = vm_map_trunc_page(*address, target_page_mask);
	vm_map_lock(target_map);
	target_size = vm_map_round_page(target_size, target_page_mask);
	result = vm_map_remap_range_allocate(target_map, address,
	    target_size, mask, vmk_flags,
	    &insp_entry, &zap_list);

	for (entry = vm_map_copy_first_entry(copy_map);
	    entry != vm_map_copy_to_entry(copy_map);
	    entry = new_entry) {
		new_entry = entry->vme_next;
		vm_map_copy_entry_unlink(copy_map, entry);
		if (result == KERN_SUCCESS) {
			if (vmk_flags.vmkf_remap_prot_copy) {
				/*
				 * This vm_map_remap() is for a
				 * vm_protect(VM_PROT_COPY), so the caller
				 * expects to be allowed to add write access
				 * to this new mapping.  This is done by
				 * adding VM_PROT_WRITE to each entry's
				 * max_protection... unless some security
				 * settings disallow it.
				 */
				bool allow_write = false;
				if (entry->vme_permanent) {
					/* immutable mapping... */
					if ((entry->max_protection & VM_PROT_EXECUTE) &&
					    developer_mode_state()) {
						/*
						 * ... but executable and
						 * possibly being debugged,
						 * so let's allow it to become
						 * writable, for breakpoints
						 * and dtrace probes, for
						 * example.
						 */
						allow_write = true;
					} else {
						printf("%d[%s] vm_remap(0x%llx,0x%llx) VM_PROT_COPY denied on permanent mapping prot 0x%x/0x%x developer %d\n",
						    proc_selfpid(),
						    (get_bsdtask_info(current_task())
						    ? proc_name_address(get_bsdtask_info(current_task()))
						    : "?"),
						    (uint64_t)memory_address,
						    (uint64_t)size,
						    entry->protection,
						    entry->max_protection,
						    developer_mode_state());
						DTRACE_VM6(vm_map_delete_permanent_deny_protcopy,
						    vm_map_entry_t, entry,
						    vm_map_offset_t, entry->vme_start,
						    vm_map_offset_t, entry->vme_end,
						    vm_prot_t, entry->protection,
						    vm_prot_t, entry->max_protection,
						    int, VME_ALIAS(entry));
					}
				} else {
					allow_write = true;
				}

				/*
				 * VM_PROT_COPY: allow this mapping to become
				 * writable, unless it was "permanent".
				 */
				if (allow_write) {
					entry->max_protection |= VM_PROT_WRITE;
				}
			}
			if (vmk_flags.vmf_resilient_codesign) {
				/* no codesigning -> read-only access */
				entry->max_protection = VM_PROT_READ;
				entry->protection = VM_PROT_READ;
				entry->vme_resilient_codesign = TRUE;
			}
			entry->vme_start += *address;
			entry->vme_end += *address;
			assert(!entry->map_aligned);
			if (vmk_flags.vmf_resilient_media &&
			    !entry->is_sub_map &&
			    (VME_OBJECT(entry) == VM_OBJECT_NULL ||
			    VME_OBJECT(entry)->internal)) {
				entry->vme_resilient_media = TRUE;
			}
			assert(VM_MAP_PAGE_ALIGNED(entry->vme_start, MIN(target_page_mask, PAGE_MASK)));
			assert(VM_MAP_PAGE_ALIGNED(entry->vme_end, MIN(target_page_mask, PAGE_MASK)));
			assert(VM_MAP_PAGE_ALIGNED(VME_OFFSET(entry), MIN(target_page_mask, PAGE_MASK)));
			vm_map_store_entry_link(target_map, insp_entry, entry,
			    vmk_flags);
			insp_entry = entry;
		} else {
			if (!entry->is_sub_map) {
				vm_object_deallocate(VME_OBJECT(entry));
			} else {
				vm_map_deallocate(VME_SUBMAP(entry));
			}
			vm_map_copy_entry_dispose(entry);
		}
	}

	if (vmk_flags.vmf_resilient_codesign) {
		*cur_protection = VM_PROT_READ;
		*max_protection = VM_PROT_READ;
	}

	if (result == KERN_SUCCESS) {
		target_map->size += target_size;
		SAVE_HINT_MAP_WRITE(target_map, insp_entry);
	}
	vm_map_unlock(target_map);

	vm_map_zap_dispose(&zap_list);

	if (result == KERN_SUCCESS && target_map->wiring_required) {
		result = vm_map_wire_kernel(target_map, *address,
		    *address + size, *cur_protection, VM_KERN_MEMORY_MLOCK,
		    TRUE);
	}

	/*
	 * If requested, return the address of the data pointed to by the
	 * request, rather than the base of the resulting page.
	 */
	if (vmk_flags.vmf_return_data_addr) {
		*address += offset_in_mapping;
	}

	if (src_page_mask != target_page_mask) {
		DEBUG4K_SHARE("vm_remap(%p 0x%llx 0x%llx copy=%d-> %p 0x%llx 0x%llx  result=0x%x\n", src_map, (uint64_t)memory_address, (uint64_t)size, copy, target_map, (uint64_t)*address, (uint64_t)offset_in_mapping, result);
	}
	vm_map_copy_discard(copy_map);
	copy_map = VM_MAP_COPY_NULL;

	return result;
}

/*
 *	Routine:	vm_map_remap_range_allocate
 *
 *	Description:
 *		Allocate a range in the specified virtual address map.
 *		returns the address and the map entry just before the allocated
 *		range
 *
 *	Map must be locked.
 */

static kern_return_t
vm_map_remap_range_allocate(
	vm_map_t                map,
	vm_map_address_t        *address,       /* IN/OUT */
	vm_map_size_t           size,
	vm_map_offset_t         mask,
	vm_map_kernel_flags_t   vmk_flags,
	vm_map_entry_t          *map_entry,     /* OUT */
	vm_map_zap_t            zap_list)
{
	vm_map_entry_t  entry;
	vm_map_offset_t start;
	kern_return_t   kr;

	start = *address;

	if (!vmk_flags.vmf_fixed) {
		kr = vm_map_locate_space(map, size, mask, vmk_flags,
		    &start, &entry);
		if (kr != KERN_SUCCESS) {
			return kr;
		}
		*address = start;
	} else {
		vm_map_offset_t effective_min_offset, effective_max_offset;
		vm_map_entry_t  temp_entry;
		vm_map_offset_t end;

		effective_min_offset = map->min_offset;
		effective_max_offset = map->max_offset;

		/*
		 *	Verify that:
		 *		the address doesn't itself violate
		 *		the mask requirement.
		 */

		if ((start & mask) != 0) {
			return KERN_NO_SPACE;
		}

#if CONFIG_MAP_RANGES
		if (map->uses_user_ranges) {
			struct mach_vm_range r;

			vm_map_user_range_resolve(map, start, 1, &r);
			if (r.max_address == 0) {
				return KERN_INVALID_ADDRESS;
			}

			effective_min_offset = r.min_address;
			effective_max_offset = r.max_address;
		}
#endif /* CONFIG_MAP_RANGES */
		if (map == kernel_map) {
			mach_vm_range_t r = kmem_validate_range_for_overwrite(start, size);
			effective_min_offset = r->min_address;
			effective_min_offset = r->max_address;
		}

		/*
		 *	...	the address is within bounds
		 */

		end = start + size;

		if ((start < effective_min_offset) ||
		    (end > effective_max_offset) ||
		    (start >= end)) {
			return KERN_INVALID_ADDRESS;
		}

		/*
		 * If we're asked to overwrite whatever was mapped in that
		 * range, first deallocate that range.
		 */
		if (vmk_flags.vmf_overwrite) {
			vmr_flags_t remove_flags = VM_MAP_REMOVE_NO_MAP_ALIGN;

			/*
			 * We use a "zap_list" to avoid having to unlock
			 * the "map" in vm_map_delete(), which would compromise
			 * the atomicity of the "deallocate" and then "remap"
			 * combination.
			 */
			remove_flags |= VM_MAP_REMOVE_NO_YIELD;

			if (vmk_flags.vmkf_overwrite_immutable) {
				remove_flags |= VM_MAP_REMOVE_IMMUTABLE;
			}
			if (vmk_flags.vmkf_remap_prot_copy) {
				remove_flags |= VM_MAP_REMOVE_IMMUTABLE_CODE;
			}
			kr = vm_map_delete(map, start, end, remove_flags,
			    KMEM_GUARD_NONE, zap_list).kmr_return;
			if (kr != KERN_SUCCESS) {
				/* XXX FBDP restore zap_list? */
				return kr;
			}
		}

		/*
		 *	...	the starting address isn't allocated
		 */

		if (vm_map_lookup_entry(map, start, &temp_entry)) {
			return KERN_NO_SPACE;
		}

		entry = temp_entry;

		/*
		 *	...	the next region doesn't overlap the
		 *		end point.
		 */

		if ((entry->vme_next != vm_map_to_entry(map)) &&
		    (entry->vme_next->vme_start < end)) {
			return KERN_NO_SPACE;
		}
	}
	*map_entry = entry;
	return KERN_SUCCESS;
}

/*
 *	vm_map_switch:
 *
 *	Set the address map for the current thread to the specified map
 */

vm_map_t
vm_map_switch(
	vm_map_t        map)
{
	thread_t        thread = current_thread();
	vm_map_t        oldmap = thread->map;


	/*
	 *	Deactivate the current map and activate the requested map
	 */
	mp_disable_preemption();
	PMAP_SWITCH_USER(thread, map, cpu_number());
	mp_enable_preemption();
	return oldmap;
}


/*
 *	Routine:	vm_map_write_user
 *
 *	Description:
 *		Copy out data from a kernel space into space in the
 *		destination map. The space must already exist in the
 *		destination map.
 *		NOTE:  This routine should only be called by threads
 *		which can block on a page fault. i.e. kernel mode user
 *		threads.
 *
 */
kern_return_t
vm_map_write_user(
	vm_map_t                map,
	void                    *src_p,
	vm_map_address_t        dst_addr,
	vm_size_t               size)
{
	kern_return_t   kr = KERN_SUCCESS;

	if (__improbable(vm_map_range_overflows(map, dst_addr, size))) {
		return KERN_INVALID_ADDRESS;
	}

	if (current_map() == map) {
		if (copyout(src_p, dst_addr, size)) {
			kr = KERN_INVALID_ADDRESS;
		}
	} else {
		vm_map_t        oldmap;

		/* take on the identity of the target map while doing */
		/* the transfer */

		vm_map_reference(map);
		oldmap = vm_map_switch(map);
		if (copyout(src_p, dst_addr, size)) {
			kr = KERN_INVALID_ADDRESS;
		}
		vm_map_switch(oldmap);
		vm_map_deallocate(map);
	}
	return kr;
}

/*
 *	Routine:	vm_map_read_user
 *
 *	Description:
 *		Copy in data from a user space source map into the
 *		kernel map. The space must already exist in the
 *		kernel map.
 *		NOTE:  This routine should only be called by threads
 *		which can block on a page fault. i.e. kernel mode user
 *		threads.
 *
 */
kern_return_t
vm_map_read_user(
	vm_map_t                map,
	vm_map_address_t        src_addr,
	void                    *dst_p,
	vm_size_t               size)
{
	kern_return_t   kr = KERN_SUCCESS;

	if (__improbable(vm_map_range_overflows(map, src_addr, size))) {
		return KERN_INVALID_ADDRESS;
	}

	if (current_map() == map) {
		if (copyin(src_addr, dst_p, size)) {
			kr = KERN_INVALID_ADDRESS;
		}
	} else {
		vm_map_t        oldmap;

		/* take on the identity of the target map while doing */
		/* the transfer */

		vm_map_reference(map);
		oldmap = vm_map_switch(map);
		if (copyin(src_addr, dst_p, size)) {
			kr = KERN_INVALID_ADDRESS;
		}
		vm_map_switch(oldmap);
		vm_map_deallocate(map);
	}
	return kr;
}


/*
 *	vm_map_check_protection:
 *
 *	Assert that the target map allows the specified
 *	privilege on the entire address region given.
 *	The entire region must be allocated.
 */
boolean_t
vm_map_check_protection(vm_map_t map, vm_map_offset_t start,
    vm_map_offset_t end, vm_prot_t protection)
{
	vm_map_entry_t entry;
	vm_map_entry_t tmp_entry;

	if (__improbable(vm_map_range_overflows(map, start, end - start))) {
		return FALSE;
	}

	vm_map_lock(map);

	if (start < vm_map_min(map) || end > vm_map_max(map) || start > end) {
		vm_map_unlock(map);
		return FALSE;
	}

	if (!vm_map_lookup_entry(map, start, &tmp_entry)) {
		vm_map_unlock(map);
		return FALSE;
	}

	entry = tmp_entry;

	while (start < end) {
		if (entry == vm_map_to_entry(map)) {
			vm_map_unlock(map);
			return FALSE;
		}

		/*
		 *	No holes allowed!
		 */

		if (start < entry->vme_start) {
			vm_map_unlock(map);
			return FALSE;
		}

		/*
		 * Check protection associated with entry.
		 */

		if ((entry->protection & protection) != protection) {
			vm_map_unlock(map);
			return FALSE;
		}

		/* go to next entry */

		start = entry->vme_end;
		entry = entry->vme_next;
	}
	vm_map_unlock(map);
	return TRUE;
}

kern_return_t
vm_map_purgable_control(
	vm_map_t                map,
	vm_map_offset_t         address,
	vm_purgable_t           control,
	int                     *state)
{
	vm_map_entry_t          entry;
	vm_object_t             object;
	kern_return_t           kr;
	boolean_t               was_nonvolatile;

	/*
	 * Vet all the input parameters and current type and state of the
	 * underlaying object.  Return with an error if anything is amiss.
	 */
	if (map == VM_MAP_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	if (control != VM_PURGABLE_SET_STATE &&
	    control != VM_PURGABLE_GET_STATE &&
	    control != VM_PURGABLE_PURGE_ALL &&
	    control != VM_PURGABLE_SET_STATE_FROM_KERNEL) {
		return KERN_INVALID_ARGUMENT;
	}

	if (control == VM_PURGABLE_PURGE_ALL) {
		vm_purgeable_object_purge_all();
		return KERN_SUCCESS;
	}

	if ((control == VM_PURGABLE_SET_STATE ||
	    control == VM_PURGABLE_SET_STATE_FROM_KERNEL) &&
	    (((*state & ~(VM_PURGABLE_ALL_MASKS)) != 0) ||
	    ((*state & VM_PURGABLE_STATE_MASK) > VM_PURGABLE_STATE_MASK))) {
		return KERN_INVALID_ARGUMENT;
	}

	vm_map_lock_read(map);

	if (!vm_map_lookup_entry(map, address, &entry) || entry->is_sub_map) {
		/*
		 * Must pass a valid non-submap address.
		 */
		vm_map_unlock_read(map);
		return KERN_INVALID_ADDRESS;
	}

	if ((entry->protection & VM_PROT_WRITE) == 0 &&
	    control != VM_PURGABLE_GET_STATE) {
		/*
		 * Can't apply purgable controls to something you can't write.
		 */
		vm_map_unlock_read(map);
		return KERN_PROTECTION_FAILURE;
	}

	object = VME_OBJECT(entry);
	if (object == VM_OBJECT_NULL ||
	    object->purgable == VM_PURGABLE_DENY) {
		/*
		 * Object must already be present and be purgeable.
		 */
		vm_map_unlock_read(map);
		return KERN_INVALID_ARGUMENT;
	}

	vm_object_lock(object);

#if 00
	if (VME_OFFSET(entry) != 0 ||
	    entry->vme_end - entry->vme_start != object->vo_size) {
		/*
		 * Can only apply purgable controls to the whole (existing)
		 * object at once.
		 */
		vm_map_unlock_read(map);
		vm_object_unlock(object);
		return KERN_INVALID_ARGUMENT;
	}
#endif

	assert(!entry->is_sub_map);
	assert(!entry->use_pmap); /* purgeable has its own accounting */

	vm_map_unlock_read(map);

	was_nonvolatile = (object->purgable == VM_PURGABLE_NONVOLATILE);

	kr = vm_object_purgable_control(object, control, state);

	if (was_nonvolatile &&
	    object->purgable != VM_PURGABLE_NONVOLATILE &&
	    map->pmap == kernel_pmap) {
#if DEBUG
		object->vo_purgeable_volatilizer = kernel_task;
#endif /* DEBUG */
	}

	vm_object_unlock(object);

	return kr;
}

void
vm_map_footprint_query_page_info(
	vm_map_t        map,
	vm_map_entry_t  map_entry,
	vm_map_offset_t curr_s_offset,
	int             *disposition_p)
{
	int             pmap_disp;
	vm_object_t     object = VM_OBJECT_NULL;
	int             disposition;
	int             effective_page_size;

	vm_map_lock_assert_held(map);
	assert(!map->has_corpse_footprint);
	assert(curr_s_offset >= map_entry->vme_start);
	assert(curr_s_offset < map_entry->vme_end);

	if (map_entry->is_sub_map) {
		if (!map_entry->use_pmap) {
			/* nested pmap: no footprint */
			*disposition_p = 0;
			return;
		}
	} else {
		object = VME_OBJECT(map_entry);
		if (object == VM_OBJECT_NULL) {
			/* nothing mapped here: no need to ask */
			*disposition_p = 0;
			return;
		}
	}

	effective_page_size = MIN(PAGE_SIZE, VM_MAP_PAGE_SIZE(map));

	pmap_disp = 0;

	/*
	 * Query the pmap.
	 */
	pmap_query_page_info(map->pmap, curr_s_offset, &pmap_disp);

	/*
	 * Compute this page's disposition.
	 */
	disposition = 0;

	/* deal with "alternate accounting" first */
	if (!map_entry->is_sub_map &&
	    object->vo_no_footprint) {
		/* does not count in footprint */
		assertf(!map_entry->use_pmap, "offset 0x%llx map_entry %p", (uint64_t) curr_s_offset, map_entry);
	} else if (!map_entry->is_sub_map &&
	    (object->purgable == VM_PURGABLE_NONVOLATILE ||
	    (object->purgable == VM_PURGABLE_DENY &&
	    object->vo_ledger_tag)) &&
	    VM_OBJECT_OWNER(object) != NULL &&
	    VM_OBJECT_OWNER(object)->map == map) {
		assertf(!map_entry->use_pmap, "offset 0x%llx map_entry %p", (uint64_t) curr_s_offset, map_entry);
		if ((((curr_s_offset
		    - map_entry->vme_start
		    + VME_OFFSET(map_entry))
		    / effective_page_size) <
		    (object->resident_page_count +
		    vm_compressor_pager_get_count(object->pager)))) {
			/*
			 * Non-volatile purgeable object owned
			 * by this task: report the first
			 * "#resident + #compressed" pages as
			 * "resident" (to show that they
			 * contribute to the footprint) but not
			 * "dirty" (to avoid double-counting
			 * with the fake "non-volatile" region
			 * we'll report at the end of the
			 * address space to account for all
			 * (mapped or not) non-volatile memory
			 * owned by this task.
			 */
			disposition |= VM_PAGE_QUERY_PAGE_PRESENT;
		}
	} else if (!map_entry->is_sub_map &&
	    (object->purgable == VM_PURGABLE_VOLATILE ||
	    object->purgable == VM_PURGABLE_EMPTY) &&
	    VM_OBJECT_OWNER(object) != NULL &&
	    VM_OBJECT_OWNER(object)->map == map) {
		assertf(!map_entry->use_pmap, "offset 0x%llx map_entry %p", (uint64_t) curr_s_offset, map_entry);
		if ((((curr_s_offset
		    - map_entry->vme_start
		    + VME_OFFSET(map_entry))
		    / effective_page_size) <
		    object->wired_page_count)) {
			/*
			 * Volatile|empty purgeable object owned
			 * by this task: report the first
			 * "#wired" pages as "resident" (to
			 * show that they contribute to the
			 * footprint) but not "dirty" (to avoid
			 * double-counting with the fake
			 * "non-volatile" region we'll report
			 * at the end of the address space to
			 * account for all (mapped or not)
			 * non-volatile memory owned by this
			 * task.
			 */
			disposition |= VM_PAGE_QUERY_PAGE_PRESENT;
		}
	} else if (!map_entry->is_sub_map &&
	    map_entry->iokit_acct &&
	    object->internal &&
	    object->purgable == VM_PURGABLE_DENY) {
		/*
		 * Non-purgeable IOKit memory: phys_footprint
		 * includes the entire virtual mapping.
		 */
		assertf(!map_entry->use_pmap, "offset 0x%llx map_entry %p", (uint64_t) curr_s_offset, map_entry);
		disposition |= VM_PAGE_QUERY_PAGE_PRESENT;
		disposition |= VM_PAGE_QUERY_PAGE_DIRTY;
	} else if (pmap_disp & (PMAP_QUERY_PAGE_ALTACCT |
	    PMAP_QUERY_PAGE_COMPRESSED_ALTACCT)) {
		/* alternate accounting */
#if __arm64__ && (DEVELOPMENT || DEBUG)
		if (map->pmap->footprint_was_suspended) {
			/*
			 * The assertion below can fail if dyld
			 * suspended footprint accounting
			 * while doing some adjustments to
			 * this page;  the mapping would say
			 * "use pmap accounting" but the page
			 * would be marked "alternate
			 * accounting".
			 */
		} else
#endif /* __arm64__ && (DEVELOPMENT || DEBUG) */
		{
			assertf(!map_entry->use_pmap, "offset 0x%llx map_entry %p", (uint64_t) curr_s_offset, map_entry);
		}
		disposition = 0;
	} else {
		if (pmap_disp & PMAP_QUERY_PAGE_PRESENT) {
			assertf(map_entry->use_pmap, "offset 0x%llx map_entry %p", (uint64_t) curr_s_offset, map_entry);
			disposition |= VM_PAGE_QUERY_PAGE_PRESENT;
			disposition |= VM_PAGE_QUERY_PAGE_REF;
			if (pmap_disp & PMAP_QUERY_PAGE_INTERNAL) {
				disposition |= VM_PAGE_QUERY_PAGE_DIRTY;
			} else {
				disposition |= VM_PAGE_QUERY_PAGE_EXTERNAL;
			}
			if (pmap_disp & PMAP_QUERY_PAGE_REUSABLE) {
				disposition |= VM_PAGE_QUERY_PAGE_REUSABLE;
			}
		} else if (pmap_disp & PMAP_QUERY_PAGE_COMPRESSED) {
			assertf(map_entry->use_pmap, "offset 0x%llx map_entry %p", (uint64_t) curr_s_offset, map_entry);
			disposition |= VM_PAGE_QUERY_PAGE_PAGED_OUT;
		}
	}

	*disposition_p = disposition;
}

kern_return_t
vm_map_page_query_internal(
	vm_map_t        target_map,
	vm_map_offset_t offset,
	int             *disposition,
	int             *ref_count)
{
	kern_return_t                   kr;
	vm_page_info_basic_data_t       info;
	mach_msg_type_number_t          count;

	count = VM_PAGE_INFO_BASIC_COUNT;
	kr = vm_map_page_info(target_map,
	    offset,
	    VM_PAGE_INFO_BASIC,
	    (vm_page_info_t) &info,
	    &count);
	if (kr == KERN_SUCCESS) {
		*disposition = info.disposition;
		*ref_count = info.ref_count;
	} else {
		*disposition = 0;
		*ref_count = 0;
	}

	return kr;
}

kern_return_t
vm_map_page_info(
	vm_map_t                map,
	vm_map_offset_t         offset,
	vm_page_info_flavor_t   flavor,
	vm_page_info_t          info,
	mach_msg_type_number_t  *count)
{
	return vm_map_page_range_info_internal(map,
	           offset, /* start of range */
	           (offset + 1), /* this will get rounded in the call to the page boundary */
	           (int)-1, /* effective_page_shift: unspecified */
	           flavor,
	           info,
	           count);
}

kern_return_t
vm_map_page_range_info_internal(
	vm_map_t                map,
	vm_map_offset_t         start_offset,
	vm_map_offset_t         end_offset,
	int                     effective_page_shift,
	vm_page_info_flavor_t   flavor,
	vm_page_info_t          info,
	mach_msg_type_number_t  *count)
{
	vm_map_entry_t          map_entry = VM_MAP_ENTRY_NULL;
	vm_object_t             object = VM_OBJECT_NULL, curr_object = VM_OBJECT_NULL;
	vm_page_t               m = VM_PAGE_NULL;
	kern_return_t           retval = KERN_SUCCESS;
	int                     disposition = 0;
	int                     ref_count = 0;
	int                     depth = 0, info_idx = 0;
	vm_page_info_basic_t    basic_info = 0;
	vm_map_offset_t         offset_in_page = 0, offset_in_object = 0, curr_offset_in_object = 0;
	vm_map_offset_t         start = 0, end = 0, curr_s_offset = 0, curr_e_offset = 0;
	boolean_t               do_region_footprint;
	ledger_amount_t         ledger_resident, ledger_compressed;
	int                     effective_page_size;
	vm_map_offset_t         effective_page_mask;

	switch (flavor) {
	case VM_PAGE_INFO_BASIC:
		if (*count != VM_PAGE_INFO_BASIC_COUNT) {
			/*
			 * The "vm_page_info_basic_data" structure was not
			 * properly padded, so allow the size to be off by
			 * one to maintain backwards binary compatibility...
			 */
			if (*count != VM_PAGE_INFO_BASIC_COUNT - 1) {
				return KERN_INVALID_ARGUMENT;
			}
		}
		break;
	default:
		return KERN_INVALID_ARGUMENT;
	}

	if (effective_page_shift == -1) {
		effective_page_shift = vm_self_region_page_shift_safely(map);
		if (effective_page_shift == -1) {
			return KERN_INVALID_ARGUMENT;
		}
	}
	effective_page_size = (1 << effective_page_shift);
	effective_page_mask = effective_page_size - 1;

	do_region_footprint = task_self_region_footprint();
	disposition = 0;
	ref_count = 0;
	depth = 0;
	info_idx = 0; /* Tracks the next index within the info structure to be filled.*/
	retval = KERN_SUCCESS;

	if (__improbable(vm_map_range_overflows(map, start_offset, end_offset - start_offset))) {
		return KERN_INVALID_ADDRESS;
	}

	offset_in_page = start_offset & effective_page_mask;
	start = vm_map_trunc_page(start_offset, effective_page_mask);
	end = vm_map_round_page(end_offset, effective_page_mask);

	if (end < start) {
		return KERN_INVALID_ARGUMENT;
	}

	assert((end - start) <= MAX_PAGE_RANGE_QUERY);

	vm_map_lock_read(map);

	task_ledgers_footprint(map->pmap->ledger, &ledger_resident, &ledger_compressed);

	for (curr_s_offset = start; curr_s_offset < end;) {
		/*
		 * New lookup needs reset of these variables.
		 */
		curr_object = object = VM_OBJECT_NULL;
		offset_in_object = 0;
		ref_count = 0;
		depth = 0;

		if (do_region_footprint &&
		    curr_s_offset >= vm_map_last_entry(map)->vme_end) {
			/*
			 * Request for "footprint" info about a page beyond
			 * the end of address space: this must be for
			 * the fake region vm_map_region_recurse_64()
			 * reported to account for non-volatile purgeable
			 * memory owned by this task.
			 */
			disposition = 0;

			if (curr_s_offset - vm_map_last_entry(map)->vme_end <=
			    (unsigned) ledger_compressed) {
				/*
				 * We haven't reported all the "non-volatile
				 * compressed" pages yet, so report this fake
				 * page as "compressed".
				 */
				disposition |= VM_PAGE_QUERY_PAGE_PAGED_OUT;
			} else {
				/*
				 * We've reported all the non-volatile
				 * compressed page but not all the non-volatile
				 * pages , so report this fake page as
				 * "resident dirty".
				 */
				disposition |= VM_PAGE_QUERY_PAGE_PRESENT;
				disposition |= VM_PAGE_QUERY_PAGE_DIRTY;
				disposition |= VM_PAGE_QUERY_PAGE_REF;
			}
			switch (flavor) {
			case VM_PAGE_INFO_BASIC:
				basic_info = (vm_page_info_basic_t) (((uintptr_t) info) + (info_idx * sizeof(struct vm_page_info_basic)));
				basic_info->disposition = disposition;
				basic_info->ref_count = 1;
				basic_info->object_id = VM_OBJECT_ID_FAKE(map, task_ledgers.purgeable_nonvolatile);
				basic_info->offset = 0;
				basic_info->depth = 0;

				info_idx++;
				break;
			}
			curr_s_offset += effective_page_size;
			continue;
		}

		/*
		 * First, find the map entry covering "curr_s_offset", going down
		 * submaps if necessary.
		 */
		if (!vm_map_lookup_entry(map, curr_s_offset, &map_entry)) {
			/* no entry -> no object -> no page */

			if (curr_s_offset < vm_map_min(map)) {
				/*
				 * Illegal address that falls below map min.
				 */
				curr_e_offset = MIN(end, vm_map_min(map));
			} else if (curr_s_offset >= vm_map_max(map)) {
				/*
				 * Illegal address that falls on/after map max.
				 */
				curr_e_offset = end;
			} else if (map_entry == vm_map_to_entry(map)) {
				/*
				 * Hit a hole.
				 */
				if (map_entry->vme_next == vm_map_to_entry(map)) {
					/*
					 * Empty map.
					 */
					curr_e_offset = MIN(map->max_offset, end);
				} else {
					/*
					 * Hole at start of the map.
					 */
					curr_e_offset = MIN(map_entry->vme_next->vme_start, end);
				}
			} else {
				if (map_entry->vme_next == vm_map_to_entry(map)) {
					/*
					 * Hole at the end of the map.
					 */
					curr_e_offset = MIN(map->max_offset, end);
				} else {
					curr_e_offset = MIN(map_entry->vme_next->vme_start, end);
				}
			}

			assert(curr_e_offset >= curr_s_offset);

			uint64_t num_pages = (curr_e_offset - curr_s_offset) >> effective_page_shift;

			void *info_ptr = (void*) (((uintptr_t) info) + (info_idx * sizeof(struct vm_page_info_basic)));

			bzero(info_ptr, num_pages * sizeof(struct vm_page_info_basic));

			curr_s_offset = curr_e_offset;

			info_idx += num_pages;

			continue;
		}

		/* compute offset from this map entry's start */
		offset_in_object = curr_s_offset - map_entry->vme_start;

		/* compute offset into this map entry's object (or submap) */
		offset_in_object += VME_OFFSET(map_entry);

		if (map_entry->is_sub_map) {
			vm_map_t sub_map = VM_MAP_NULL;
			vm_page_info_t submap_info = 0;
			vm_map_offset_t submap_s_offset = 0, submap_e_offset = 0, range_len = 0;

			range_len = MIN(map_entry->vme_end, end) - curr_s_offset;

			submap_s_offset = offset_in_object;
			submap_e_offset = submap_s_offset + range_len;

			sub_map = VME_SUBMAP(map_entry);

			vm_map_reference(sub_map);
			vm_map_unlock_read(map);

			submap_info = (vm_page_info_t) (((uintptr_t) info) + (info_idx * sizeof(struct vm_page_info_basic)));

			assertf(VM_MAP_PAGE_SHIFT(sub_map) >= VM_MAP_PAGE_SHIFT(map),
			    "Submap page size (%d) differs from current map (%d)\n", VM_MAP_PAGE_SIZE(sub_map), VM_MAP_PAGE_SIZE(map));

			retval = vm_map_page_range_info_internal(sub_map,
			    submap_s_offset,
			    submap_e_offset,
			    effective_page_shift,
			    VM_PAGE_INFO_BASIC,
			    (vm_page_info_t) submap_info,
			    count);

			assert(retval == KERN_SUCCESS);

			vm_map_lock_read(map);
			vm_map_deallocate(sub_map);

			/* Move the "info" index by the number of pages we inspected.*/
			info_idx += range_len >> effective_page_shift;

			/* Move our current offset by the size of the range we inspected.*/
			curr_s_offset += range_len;

			continue;
		}

		object = VME_OBJECT(map_entry);

		if (object == VM_OBJECT_NULL) {
			/*
			 * We don't have an object here and, hence,
			 * no pages to inspect. We'll fill up the
			 * info structure appropriately.
			 */

			curr_e_offset = MIN(map_entry->vme_end, end);

			uint64_t num_pages = (curr_e_offset - curr_s_offset) >> effective_page_shift;

			void *info_ptr = (void*) (((uintptr_t) info) + (info_idx * sizeof(struct vm_page_info_basic)));

			bzero(info_ptr, num_pages * sizeof(struct vm_page_info_basic));

			curr_s_offset = curr_e_offset;

			info_idx += num_pages;

			continue;
		}

		if (do_region_footprint) {
			disposition = 0;
			if (map->has_corpse_footprint) {
				/*
				 * Query the page info data we saved
				 * while forking the corpse.
				 */
				vm_map_corpse_footprint_query_page_info(
					map,
					curr_s_offset,
					&disposition);
			} else {
				/*
				 * Query the live pmap for footprint info
				 * about this page.
				 */
				vm_map_footprint_query_page_info(
					map,
					map_entry,
					curr_s_offset,
					&disposition);
			}
			switch (flavor) {
			case VM_PAGE_INFO_BASIC:
				basic_info = (vm_page_info_basic_t) (((uintptr_t) info) + (info_idx * sizeof(struct vm_page_info_basic)));
				basic_info->disposition = disposition;
				basic_info->ref_count = 1;
				basic_info->object_id = VM_OBJECT_ID_FAKE(map, task_ledgers.purgeable_nonvolatile);
				basic_info->offset = 0;
				basic_info->depth = 0;

				info_idx++;
				break;
			}
			curr_s_offset += effective_page_size;
			continue;
		}

		vm_object_reference(object);
		/*
		 * Shared mode -- so we can allow other readers
		 * to grab the lock too.
		 */
		vm_object_lock_shared(object);

		curr_e_offset = MIN(map_entry->vme_end, end);

		vm_map_unlock_read(map);

		map_entry = NULL; /* map is unlocked, the entry is no longer valid. */

		curr_object = object;

		for (; curr_s_offset < curr_e_offset;) {
			if (object == curr_object) {
				ref_count = curr_object->ref_count - 1; /* account for our object reference above. */
			} else {
				ref_count = curr_object->ref_count;
			}

			curr_offset_in_object = offset_in_object;

			for (;;) {
				m = vm_page_lookup(curr_object, vm_object_trunc_page(curr_offset_in_object));

				if (m != VM_PAGE_NULL) {
					disposition |= VM_PAGE_QUERY_PAGE_PRESENT;
					break;
				} else {
					if (curr_object->internal &&
					    curr_object->alive &&
					    !curr_object->terminating &&
					    curr_object->pager_ready) {
						if (VM_COMPRESSOR_PAGER_STATE_GET(curr_object, vm_object_trunc_page(curr_offset_in_object))
						    == VM_EXTERNAL_STATE_EXISTS) {
							/* the pager has that page */
							disposition |= VM_PAGE_QUERY_PAGE_PAGED_OUT;
							break;
						}
					}

					/*
					 * Go down the VM object shadow chain until we find the page
					 * we're looking for.
					 */

					if (curr_object->shadow != VM_OBJECT_NULL) {
						vm_object_t shadow = VM_OBJECT_NULL;

						curr_offset_in_object += curr_object->vo_shadow_offset;
						shadow = curr_object->shadow;

						vm_object_lock_shared(shadow);
						vm_object_unlock(curr_object);

						curr_object = shadow;
						depth++;
						continue;
					} else {
						break;
					}
				}
			}

			/* The ref_count is not strictly accurate, it measures the number   */
			/* of entities holding a ref on the object, they may not be mapping */
			/* the object or may not be mapping the section holding the         */
			/* target page but its still a ball park number and though an over- */
			/* count, it picks up the copy-on-write cases                       */

			/* We could also get a picture of page sharing from pmap_attributes */
			/* but this would under count as only faulted-in mappings would     */
			/* show up.							    */

			if ((curr_object == object) && curr_object->shadow) {
				disposition |= VM_PAGE_QUERY_PAGE_COPIED;
			}

			if (!curr_object->internal) {
				disposition |= VM_PAGE_QUERY_PAGE_EXTERNAL;
			}

			if (m != VM_PAGE_NULL) {
				if (m->vmp_fictitious) {
					disposition |= VM_PAGE_QUERY_PAGE_FICTITIOUS;
				} else {
					if (m->vmp_dirty || pmap_is_modified(VM_PAGE_GET_PHYS_PAGE(m))) {
						disposition |= VM_PAGE_QUERY_PAGE_DIRTY;
					}

					if (m->vmp_reference || pmap_is_referenced(VM_PAGE_GET_PHYS_PAGE(m))) {
						disposition |= VM_PAGE_QUERY_PAGE_REF;
					}

					if (m->vmp_q_state == VM_PAGE_ON_SPECULATIVE_Q) {
						disposition |= VM_PAGE_QUERY_PAGE_SPECULATIVE;
					}

					/*
					 * XXX TODO4K:
					 * when this routine deals with 4k
					 * pages, check the appropriate CS bit
					 * here.
					 */
					if (m->vmp_cs_validated) {
						disposition |= VM_PAGE_QUERY_PAGE_CS_VALIDATED;
					}
					if (m->vmp_cs_tainted) {
						disposition |= VM_PAGE_QUERY_PAGE_CS_TAINTED;
					}
					if (m->vmp_cs_nx) {
						disposition |= VM_PAGE_QUERY_PAGE_CS_NX;
					}
					if (m->vmp_reusable || curr_object->all_reusable) {
						disposition |= VM_PAGE_QUERY_PAGE_REUSABLE;
					}
				}
			}

			switch (flavor) {
			case VM_PAGE_INFO_BASIC:
				basic_info = (vm_page_info_basic_t) (((uintptr_t) info) + (info_idx * sizeof(struct vm_page_info_basic)));
				basic_info->disposition = disposition;
				basic_info->ref_count = ref_count;
				basic_info->object_id = (vm_object_id_t) (uintptr_t)
				    VM_KERNEL_ADDRHASH(curr_object);
				basic_info->offset =
				    (memory_object_offset_t) curr_offset_in_object + offset_in_page;
				basic_info->depth = depth;

				info_idx++;
				break;
			}

			disposition = 0;
			offset_in_page = 0; // This doesn't really make sense for any offset other than the starting offset.

			/*
			 * Move to next offset in the range and in our object.
			 */
			curr_s_offset += effective_page_size;
			offset_in_object += effective_page_size;
			curr_offset_in_object = offset_in_object;

			if (curr_object != object) {
				vm_object_unlock(curr_object);

				curr_object = object;

				vm_object_lock_shared(curr_object);
			} else {
				vm_object_lock_yield_shared(curr_object);
			}
		}

		vm_object_unlock(curr_object);
		vm_object_deallocate(curr_object);

		vm_map_lock_read(map);
	}

	vm_map_unlock_read(map);
	return retval;
}

/*
 *	vm_map_msync
 *
 *	Synchronises the memory range specified with its backing store
 *	image by either flushing or cleaning the contents to the appropriate
 *	memory manager engaging in a memory object synchronize dialog with
 *	the manager.  The client doesn't return until the manager issues
 *	m_o_s_completed message.  MIG Magically converts user task parameter
 *	to the task's address map.
 *
 *	interpretation of sync_flags
 *	VM_SYNC_INVALIDATE	- discard pages, only return precious
 *				  pages to manager.
 *
 *	VM_SYNC_INVALIDATE & (VM_SYNC_SYNCHRONOUS | VM_SYNC_ASYNCHRONOUS)
 *				- discard pages, write dirty or precious
 *				  pages back to memory manager.
 *
 *	VM_SYNC_SYNCHRONOUS | VM_SYNC_ASYNCHRONOUS
 *				- write dirty or precious pages back to
 *				  the memory manager.
 *
 *	VM_SYNC_CONTIGUOUS	- does everything normally, but if there
 *				  is a hole in the region, and we would
 *				  have returned KERN_SUCCESS, return
 *				  KERN_INVALID_ADDRESS instead.
 *
 *	NOTE
 *	The memory object attributes have not yet been implemented, this
 *	function will have to deal with the invalidate attribute
 *
 *	RETURNS
 *	KERN_INVALID_TASK		Bad task parameter
 *	KERN_INVALID_ARGUMENT		both sync and async were specified.
 *	KERN_SUCCESS			The usual.
 *	KERN_INVALID_ADDRESS		There was a hole in the region.
 */

kern_return_t
vm_map_msync(
	vm_map_t                map,
	vm_map_address_t        address,
	vm_map_size_t           size,
	vm_sync_t               sync_flags)
{
	vm_map_entry_t          entry;
	vm_map_size_t           amount_left;
	vm_object_offset_t      offset;
	vm_object_offset_t      start_offset, end_offset;
	boolean_t               do_sync_req;
	boolean_t               had_hole = FALSE;
	vm_map_offset_t         pmap_offset;

	if ((sync_flags & VM_SYNC_ASYNCHRONOUS) &&
	    (sync_flags & VM_SYNC_SYNCHRONOUS)) {
		return KERN_INVALID_ARGUMENT;
	}

	if (__improbable(vm_map_range_overflows(map, address, size))) {
		return KERN_INVALID_ADDRESS;
	}

	if (VM_MAP_PAGE_MASK(map) < PAGE_MASK) {
		DEBUG4K_SHARE("map %p address 0x%llx size 0x%llx flags 0x%x\n", map, (uint64_t)address, (uint64_t)size, sync_flags);
	}

	/*
	 * align address and size on page boundaries
	 */
	size = (vm_map_round_page(address + size,
	    VM_MAP_PAGE_MASK(map)) -
	    vm_map_trunc_page(address,
	    VM_MAP_PAGE_MASK(map)));
	address = vm_map_trunc_page(address,
	    VM_MAP_PAGE_MASK(map));

	if (map == VM_MAP_NULL) {
		return KERN_INVALID_TASK;
	}

	if (size == 0) {
		return KERN_SUCCESS;
	}

	amount_left = size;

	while (amount_left > 0) {
		vm_object_size_t        flush_size;
		vm_object_t             object;

		vm_map_lock(map);
		if (!vm_map_lookup_entry(map,
		    address,
		    &entry)) {
			vm_map_size_t   skip;

			/*
			 * hole in the address map.
			 */
			had_hole = TRUE;

			if (sync_flags & VM_SYNC_KILLPAGES) {
				/*
				 * For VM_SYNC_KILLPAGES, there should be
				 * no holes in the range, since we couldn't
				 * prevent someone else from allocating in
				 * that hole and we wouldn't want to "kill"
				 * their pages.
				 */
				vm_map_unlock(map);
				break;
			}

			/*
			 * Check for empty map.
			 */
			if (entry == vm_map_to_entry(map) &&
			    entry->vme_next == entry) {
				vm_map_unlock(map);
				break;
			}
			/*
			 * Check that we don't wrap and that
			 * we have at least one real map entry.
			 */
			if ((map->hdr.nentries == 0) ||
			    (entry->vme_next->vme_start < address)) {
				vm_map_unlock(map);
				break;
			}
			/*
			 * Move up to the next entry if needed
			 */
			skip = (entry->vme_next->vme_start - address);
			if (skip >= amount_left) {
				amount_left = 0;
			} else {
				amount_left -= skip;
			}
			address = entry->vme_next->vme_start;
			vm_map_unlock(map);
			continue;
		}

		offset = address - entry->vme_start;
		pmap_offset = address;

		/*
		 * do we have more to flush than is contained in this
		 * entry ?
		 */
		if (amount_left + entry->vme_start + offset > entry->vme_end) {
			flush_size = entry->vme_end -
			    (entry->vme_start + offset);
		} else {
			flush_size = amount_left;
		}
		amount_left -= flush_size;
		address += flush_size;

		if (entry->is_sub_map == TRUE) {
			vm_map_t        local_map;
			vm_map_offset_t local_offset;

			local_map = VME_SUBMAP(entry);
			local_offset = VME_OFFSET(entry);
			vm_map_reference(local_map);
			vm_map_unlock(map);
			if (vm_map_msync(
				    local_map,
				    local_offset,
				    flush_size,
				    sync_flags) == KERN_INVALID_ADDRESS) {
				had_hole = TRUE;
			}
			vm_map_deallocate(local_map);
			continue;
		}
		object = VME_OBJECT(entry);

		/*
		 * We can't sync this object if the object has not been
		 * created yet
		 */
		if (object == VM_OBJECT_NULL) {
			vm_map_unlock(map);
			continue;
		}
		offset += VME_OFFSET(entry);

		vm_object_lock(object);

		if (sync_flags & (VM_SYNC_KILLPAGES | VM_SYNC_DEACTIVATE)) {
			int kill_pages = 0;

			if (VM_MAP_PAGE_MASK(map) < PAGE_MASK) {
				/*
				 * This is a destructive operation and so we
				 * err on the side of limiting the range of
				 * the operation.
				 */
				start_offset = vm_object_round_page(offset);
				end_offset = vm_object_trunc_page(offset + flush_size);

				if (end_offset <= start_offset) {
					vm_object_unlock(object);
					vm_map_unlock(map);
					continue;
				}

				pmap_offset += start_offset - offset;
			} else {
				start_offset = offset;
				end_offset = offset + flush_size;
			}

			if (sync_flags & VM_SYNC_KILLPAGES) {
				if (((object->ref_count == 1) ||
				    ((object->copy_strategy !=
				    MEMORY_OBJECT_COPY_SYMMETRIC) &&
				    (object->vo_copy == VM_OBJECT_NULL))) &&
				    (object->shadow == VM_OBJECT_NULL)) {
					if (object->ref_count != 1) {
						vm_page_stats_reusable.free_shared++;
					}
					kill_pages = 1;
				} else {
					kill_pages = -1;
				}
			}
			if (kill_pages != -1) {
				vm_object_deactivate_pages(
					object,
					start_offset,
					(vm_object_size_t) (end_offset - start_offset),
					kill_pages,
					FALSE, /* reusable_pages */
					FALSE, /* reusable_no_write */
					map->pmap,
					pmap_offset);
			}
			vm_object_unlock(object);
			vm_map_unlock(map);
			continue;
		}
		/*
		 * We can't sync this object if there isn't a pager.
		 * Don't bother to sync internal objects, since there can't
		 * be any "permanent" storage for these objects anyway.
		 */
		if ((object->pager == MEMORY_OBJECT_NULL) ||
		    (object->internal) || (object->private)) {
			vm_object_unlock(object);
			vm_map_unlock(map);
			continue;
		}
		/*
		 * keep reference on the object until syncing is done
		 */
		vm_object_reference_locked(object);
		vm_object_unlock(object);

		vm_map_unlock(map);

		if (VM_MAP_PAGE_MASK(map) < PAGE_MASK) {
			start_offset = vm_object_trunc_page(offset);
			end_offset = vm_object_round_page(offset + flush_size);
		} else {
			start_offset = offset;
			end_offset = offset + flush_size;
		}

		do_sync_req = vm_object_sync(object,
		    start_offset,
		    (end_offset - start_offset),
		    sync_flags & VM_SYNC_INVALIDATE,
		    ((sync_flags & VM_SYNC_SYNCHRONOUS) ||
		    (sync_flags & VM_SYNC_ASYNCHRONOUS)),
		    sync_flags & VM_SYNC_SYNCHRONOUS);

		if ((sync_flags & VM_SYNC_INVALIDATE) && object->resident_page_count == 0) {
			/*
			 * clear out the clustering and read-ahead hints
			 */
			vm_object_lock(object);

			object->pages_created = 0;
			object->pages_used = 0;
			object->sequential = 0;
			object->last_alloc = 0;

			vm_object_unlock(object);
		}
		vm_object_deallocate(object);
	} /* while */

	/* for proper msync() behaviour */
	if (had_hole == TRUE && (sync_flags & VM_SYNC_CONTIGUOUS)) {
		return KERN_INVALID_ADDRESS;
	}

	return KERN_SUCCESS;
}/* vm_msync */

void
vm_named_entry_associate_vm_object(
	vm_named_entry_t        named_entry,
	vm_object_t             object,
	vm_object_offset_t      offset,
	vm_object_size_t        size,
	vm_prot_t               prot)
{
	vm_map_copy_t copy;
	vm_map_entry_t copy_entry;

	assert(!named_entry->is_sub_map);
	assert(!named_entry->is_copy);
	assert(!named_entry->is_object);
	assert(!named_entry->internal);
	assert(named_entry->backing.copy == VM_MAP_COPY_NULL);

	copy = vm_map_copy_allocate(VM_MAP_COPY_ENTRY_LIST);
	copy->offset = offset;
	copy->size = size;
	copy->cpy_hdr.page_shift = (uint16_t)PAGE_SHIFT;

	copy_entry = vm_map_copy_entry_create(copy);
	copy_entry->protection = prot;
	copy_entry->max_protection = prot;
	copy_entry->use_pmap = TRUE;
	copy_entry->vme_start = VM_MAP_TRUNC_PAGE(offset, PAGE_MASK);
	copy_entry->vme_end = VM_MAP_ROUND_PAGE(offset + size, PAGE_MASK);
	VME_OBJECT_SET(copy_entry, object, false, 0);
	VME_OFFSET_SET(copy_entry, vm_object_trunc_page(offset));
	vm_map_copy_entry_link(copy, vm_map_copy_last_entry(copy), copy_entry);

	named_entry->backing.copy = copy;
	named_entry->is_object = TRUE;
	if (object->internal) {
		named_entry->internal = TRUE;
	}

	DEBUG4K_MEMENTRY("named_entry %p copy %p object %p offset 0x%llx size 0x%llx prot 0x%x\n",
	    named_entry, copy, object, offset, size, prot);
}

vm_object_t
vm_named_entry_to_vm_object(
	vm_named_entry_t named_entry)
{
	vm_map_copy_t   copy;
	vm_map_entry_t  copy_entry;
	vm_object_t     object;

	assert(!named_entry->is_sub_map);
	assert(!named_entry->is_copy);
	assert(named_entry->is_object);
	copy = named_entry->backing.copy;
	assert(copy != VM_MAP_COPY_NULL);
	/*
	 * Assert that the vm_map_copy is coming from the right
	 * zone and hasn't been forged
	 */
	vm_map_copy_require(copy);
	assert(copy->cpy_hdr.nentries == 1);
	copy_entry = vm_map_copy_first_entry(copy);
	object = VME_OBJECT(copy_entry);

	DEBUG4K_MEMENTRY("%p -> %p -> %p [0x%llx 0x%llx 0x%llx 0x%x/0x%x ] -> %p offset 0x%llx size 0x%llx prot 0x%x\n", named_entry, copy, copy_entry, (uint64_t)copy_entry->vme_start, (uint64_t)copy_entry->vme_end, copy_entry->vme_offset, copy_entry->protection, copy_entry->max_protection, object, named_entry->offset, named_entry->size, named_entry->protection);

	return object;
}

/*
 *	Routine:	convert_port_entry_to_map
 *	Purpose:
 *		Convert from a port specifying an entry or a task
 *		to a map. Doesn't consume the port ref; produces a map ref,
 *		which may be null.  Unlike convert_port_to_map, the
 *		port may be task or a named entry backed.
 *	Conditions:
 *		Nothing locked.
 */

vm_map_t
convert_port_entry_to_map(
	ipc_port_t      port)
{
	vm_map_t map = VM_MAP_NULL;
	vm_named_entry_t named_entry;

	if (!IP_VALID(port)) {
		return VM_MAP_NULL;
	}

	if (ip_kotype(port) != IKOT_NAMED_ENTRY) {
		return convert_port_to_map(port);
	}

	named_entry = mach_memory_entry_from_port(port);

	if ((named_entry->is_sub_map) &&
	    (named_entry->protection & VM_PROT_WRITE)) {
		map = named_entry->backing.map;
		if (map->pmap != PMAP_NULL) {
			if (map->pmap == kernel_pmap) {
				panic("userspace has access "
				    "to a kernel map %p", map);
			}
			pmap_require(map->pmap);
		}
		vm_map_reference(map);
	}

	return map;
}

/*
 * Export routines to other components for the things we access locally through
 * macros.
 */
#undef current_map
vm_map_t
current_map(void)
{
	return current_map_fast();
}

/*
 *	vm_map_reference:
 *
 *	Takes a reference on the specified map.
 */
void
vm_map_reference(
	vm_map_t        map)
{
	if (__probable(map != VM_MAP_NULL)) {
		vm_map_require(map);
		os_ref_retain_raw(&map->map_refcnt, &map_refgrp);
	}
}

/*
 *	vm_map_deallocate:
 *
 *	Removes a reference from the specified map,
 *	destroying it if no references remain.
 *	The map should not be locked.
 */
void
vm_map_deallocate(
	vm_map_t        map)
{
	if (__probable(map != VM_MAP_NULL)) {
		vm_map_require(map);
		if (os_ref_release_raw(&map->map_refcnt, &map_refgrp) == 0) {
			vm_map_destroy(map);
		}
	}
}

void
vm_map_inspect_deallocate(
	vm_map_inspect_t      map)
{
	vm_map_deallocate((vm_map_t)map);
}

void
vm_map_read_deallocate(
	vm_map_read_t      map)
{
	vm_map_deallocate((vm_map_t)map);
}


void
vm_map_disable_NX(vm_map_t map)
{
	if (map == NULL) {
		return;
	}
	if (map->pmap == NULL) {
		return;
	}

	pmap_disable_NX(map->pmap);
}

void
vm_map_disallow_data_exec(vm_map_t map)
{
	if (map == NULL) {
		return;
	}

	map->map_disallow_data_exec = TRUE;
}

/* XXX Consider making these constants (VM_MAX_ADDRESS and MACH_VM_MAX_ADDRESS)
 * more descriptive.
 */
void
vm_map_set_32bit(vm_map_t map)
{
#if defined(__arm64__)
	map->max_offset = pmap_max_offset(FALSE, ARM_PMAP_MAX_OFFSET_DEVICE);
#else
	map->max_offset = (vm_map_offset_t)VM_MAX_ADDRESS;
#endif
}


void
vm_map_set_64bit(vm_map_t map)
{
#if defined(__arm64__)
	map->max_offset = pmap_max_offset(TRUE, ARM_PMAP_MAX_OFFSET_DEVICE);
#else
	map->max_offset = (vm_map_offset_t)MACH_VM_MAX_ADDRESS;
#endif
}

/*
 * Expand the maximum size of an existing map to the maximum supported.
 */
void
vm_map_set_jumbo(vm_map_t map)
{
#if defined (__arm64__) && !XNU_TARGET_OS_OSX
	vm_map_set_max_addr(map, ~0);
#else /* arm64 */
	(void) map;
#endif
}

/*
 * This map has a JIT entitlement
 */
void
vm_map_set_jit_entitled(vm_map_t map)
{
#if defined (__arm64__)
	pmap_set_jit_entitled(map->pmap);
#else /* arm64 */
	(void) map;
#endif
}

/*
 * Get status of this maps TPRO flag
 */
boolean_t
vm_map_tpro(vm_map_t map)
{
#if defined (__arm64e__)
	return pmap_get_tpro(map->pmap);
#else /* arm64e */
	(void) map;
	return FALSE;
#endif
}

/*
 * This map has TPRO enabled
 */
void
vm_map_set_tpro(vm_map_t map)
{
#if defined (__arm64e__)
	pmap_set_tpro(map->pmap);
#else /* arm64e */
	(void) map;
#endif
}

/*
 * Does this map have TPRO enforcement enabled
 */
boolean_t
vm_map_tpro_enforcement(vm_map_t map)
{
	return map->tpro_enforcement;
}

/*
 * Set TPRO enforcement for this map
 */
void
vm_map_set_tpro_enforcement(vm_map_t map)
{
	if (vm_map_tpro(map)) {
		vm_map_lock(map);
		map->tpro_enforcement = TRUE;
		vm_map_unlock(map);
	}
}

/*
 * Enable TPRO on the requested region
 *
 * Note:
 *     This routine is primarily intended to be called during/soon after map
 *     creation before the associated task has been released to run. It is only
 *     currently safe when we have no resident pages.
 */
boolean_t
vm_map_set_tpro_range(
	__unused vm_map_t map,
	__unused vm_map_address_t start,
	__unused vm_map_address_t end)
{
	return TRUE;
}

/*
 * Expand the maximum size of an existing map.
 */
void
vm_map_set_max_addr(vm_map_t map, vm_map_offset_t new_max_offset)
{
#if defined(__arm64__)
	vm_map_offset_t max_supported_offset;
	vm_map_offset_t old_max_offset;

	vm_map_lock(map);

	old_max_offset = map->max_offset;
	max_supported_offset = pmap_max_offset(vm_map_is_64bit(map), ARM_PMAP_MAX_OFFSET_JUMBO);

	new_max_offset = trunc_page(new_max_offset);

	/* The address space cannot be shrunk using this routine. */
	if (old_max_offset >= new_max_offset) {
		vm_map_unlock(map);
		return;
	}

	if (max_supported_offset < new_max_offset) {
		new_max_offset = max_supported_offset;
	}

	map->max_offset = new_max_offset;

	if (map->holelistenabled) {
		if (map->holes_list->prev->vme_end == old_max_offset) {
			/*
			 * There is already a hole at the end of the map; simply make it bigger.
			 */
			map->holes_list->prev->vme_end = map->max_offset;
		} else {
			/*
			 * There is no hole at the end, so we need to create a new hole
			 * for the new empty space we're creating.
			 */
			struct vm_map_links *new_hole;

			new_hole = zalloc_id(ZONE_ID_VM_MAP_HOLES, Z_WAITOK | Z_NOFAIL);
			new_hole->start = old_max_offset;
			new_hole->end = map->max_offset;
			new_hole->prev = map->holes_list->prev;
			new_hole->next = (struct vm_map_entry *)map->holes_list;
			map->holes_list->prev->vme_next = (struct vm_map_entry *)new_hole;
			map->holes_list->prev = (struct vm_map_entry *)new_hole;
		}
	}

	vm_map_unlock(map);
#else
	(void)map;
	(void)new_max_offset;
#endif
}

vm_map_offset_t
vm_compute_max_offset(boolean_t is64)
{
#if defined(__arm64__)
	return pmap_max_offset(is64, ARM_PMAP_MAX_OFFSET_DEVICE);
#else
	return is64 ? (vm_map_offset_t)MACH_VM_MAX_ADDRESS : (vm_map_offset_t)VM_MAX_ADDRESS;
#endif
}

void
vm_map_get_max_aslr_slide_section(
	vm_map_t                map __unused,
	int64_t                 *max_sections,
	int64_t                 *section_size)
{
#if defined(__arm64__)
	*max_sections = 3;
	*section_size = ARM_TT_TWIG_SIZE;
#else
	*max_sections = 1;
	*section_size = 0;
#endif
}

uint64_t
vm_map_get_max_aslr_slide_pages(vm_map_t map)
{
#if defined(__arm64__)
	/* Limit arm64 slide to 16MB to conserve contiguous VA space in the more
	 * limited embedded address space; this is also meant to minimize pmap
	 * memory usage on 16KB page systems.
	 */
	return 1 << (24 - VM_MAP_PAGE_SHIFT(map));
#else
	return 1 << (vm_map_is_64bit(map) ? 16 : 8);
#endif
}

uint64_t
vm_map_get_max_loader_aslr_slide_pages(vm_map_t map)
{
#if defined(__arm64__)
	/* We limit the loader slide to 4MB, in order to ensure at least 8 bits
	 * of independent entropy on 16KB page systems.
	 */
	return 1 << (22 - VM_MAP_PAGE_SHIFT(map));
#else
	return 1 << (vm_map_is_64bit(map) ? 16 : 8);
#endif
}

boolean_t
vm_map_is_64bit(
	vm_map_t map)
{
	return map->max_offset > ((vm_map_offset_t)VM_MAX_ADDRESS);
}

boolean_t
vm_map_has_hard_pagezero(
	vm_map_t        map,
	vm_map_offset_t pagezero_size)
{
	/*
	 * XXX FBDP
	 * We should lock the VM map (for read) here but we can get away
	 * with it for now because there can't really be any race condition:
	 * the VM map's min_offset is changed only when the VM map is created
	 * and when the zero page is established (when the binary gets loaded),
	 * and this routine gets called only when the task terminates and the
	 * VM map is being torn down, and when a new map is created via
	 * load_machfile()/execve().
	 */
	return map->min_offset >= pagezero_size;
}

/*
 * Raise a VM map's maximun offset.
 */
kern_return_t
vm_map_raise_max_offset(
	vm_map_t        map,
	vm_map_offset_t new_max_offset)
{
	kern_return_t   ret;

	vm_map_lock(map);
	ret = KERN_INVALID_ADDRESS;

	if (new_max_offset >= map->max_offset) {
		if (!vm_map_is_64bit(map)) {
			if (new_max_offset <= (vm_map_offset_t)VM_MAX_ADDRESS) {
				map->max_offset = new_max_offset;
				ret = KERN_SUCCESS;
			}
		} else {
			if (new_max_offset <= (vm_map_offset_t)MACH_VM_MAX_ADDRESS) {
				map->max_offset = new_max_offset;
				ret = KERN_SUCCESS;
			}
		}
	}

	vm_map_unlock(map);
	return ret;
}


/*
 * Raise a VM map's minimum offset.
 * To strictly enforce "page zero" reservation.
 */
kern_return_t
vm_map_raise_min_offset(
	vm_map_t        map,
	vm_map_offset_t new_min_offset)
{
	vm_map_entry_t  first_entry;

	new_min_offset = vm_map_round_page(new_min_offset,
	    VM_MAP_PAGE_MASK(map));

	vm_map_lock(map);

	if (new_min_offset < map->min_offset) {
		/*
		 * Can't move min_offset backwards, as that would expose
		 * a part of the address space that was previously, and for
		 * possibly good reasons, inaccessible.
		 */
		vm_map_unlock(map);
		return KERN_INVALID_ADDRESS;
	}
	if (new_min_offset >= map->max_offset) {
		/* can't go beyond the end of the address space */
		vm_map_unlock(map);
		return KERN_INVALID_ADDRESS;
	}

	first_entry = vm_map_first_entry(map);
	if (first_entry != vm_map_to_entry(map) &&
	    first_entry->vme_start < new_min_offset) {
		/*
		 * Some memory was already allocated below the new
		 * minimun offset.  It's too late to change it now...
		 */
		vm_map_unlock(map);
		return KERN_NO_SPACE;
	}

	map->min_offset = new_min_offset;

	if (map->holelistenabled) {
		assert(map->holes_list);
		map->holes_list->start = new_min_offset;
		assert(new_min_offset < map->holes_list->end);
	}

	vm_map_unlock(map);

	return KERN_SUCCESS;
}

/*
 * Set the limit on the maximum amount of address space and user wired memory allowed for this map.
 * This is basically a copy of the RLIMIT_AS and RLIMIT_MEMLOCK rlimit value maintained by the BSD
 * side of the kernel. The limits are checked in the mach VM side, so we keep a copy so we don't
 * have to reach over to the BSD data structures.
 */

uint64_t vm_map_set_size_limit_count = 0;
kern_return_t
vm_map_set_size_limit(vm_map_t map, uint64_t new_size_limit)
{
	kern_return_t kr;

	vm_map_lock(map);
	if (new_size_limit < map->size) {
		/* new limit should not be lower than its current size */
		DTRACE_VM2(vm_map_set_size_limit_fail,
		    vm_map_size_t, map->size,
		    uint64_t, new_size_limit);
		kr = KERN_FAILURE;
	} else if (new_size_limit == map->size_limit) {
		/* no change */
		kr = KERN_SUCCESS;
	} else {
		/* set new limit */
		DTRACE_VM2(vm_map_set_size_limit,
		    vm_map_size_t, map->size,
		    uint64_t, new_size_limit);
		if (new_size_limit != RLIM_INFINITY) {
			vm_map_set_size_limit_count++;
		}
		map->size_limit = new_size_limit;
		kr = KERN_SUCCESS;
	}
	vm_map_unlock(map);
	return kr;
}

uint64_t vm_map_set_data_limit_count = 0;
kern_return_t
vm_map_set_data_limit(vm_map_t map, uint64_t new_data_limit)
{
	kern_return_t kr;

	vm_map_lock(map);
	if (new_data_limit < map->size) {
		/* new limit should not be lower than its current size */
		DTRACE_VM2(vm_map_set_data_limit_fail,
		    vm_map_size_t, map->size,
		    uint64_t, new_data_limit);
		kr = KERN_FAILURE;
	} else if (new_data_limit == map->data_limit) {
		/* no change */
		kr = KERN_SUCCESS;
	} else {
		/* set new limit */
		DTRACE_VM2(vm_map_set_data_limit,
		    vm_map_size_t, map->size,
		    uint64_t, new_data_limit);
		if (new_data_limit != RLIM_INFINITY) {
			vm_map_set_data_limit_count++;
		}
		map->data_limit = new_data_limit;
		kr = KERN_SUCCESS;
	}
	vm_map_unlock(map);
	return kr;
}

void
vm_map_set_user_wire_limit(vm_map_t     map,
    vm_size_t    limit)
{
	vm_map_lock(map);
	map->user_wire_limit = limit;
	vm_map_unlock(map);
}


void
vm_map_switch_protect(vm_map_t     map,
    boolean_t    val)
{
	vm_map_lock(map);
	map->switch_protect = val;
	vm_map_unlock(map);
}

extern int cs_process_enforcement_enable;
boolean_t
vm_map_cs_enforcement(
	vm_map_t map)
{
	if (cs_process_enforcement_enable) {
		return TRUE;
	}
	return map->cs_enforcement;
}

kern_return_t
vm_map_cs_wx_enable(
	__unused vm_map_t map)
{
#if CODE_SIGNING_MONITOR
	kern_return_t ret = csm_allow_invalid_code(vm_map_pmap(map));
	if ((ret == KERN_SUCCESS) || (ret == KERN_NOT_SUPPORTED)) {
		return KERN_SUCCESS;
	}
	return ret;
#else
	/* The VM manages WX memory entirely on its own */
	return KERN_SUCCESS;
#endif
}

kern_return_t
vm_map_csm_allow_jit(
	__unused vm_map_t map)
{
#if CODE_SIGNING_MONITOR
	return csm_allow_jit_region(vm_map_pmap(map));
#else
	/* No code signing monitor to enforce JIT policy */
	return KERN_SUCCESS;
#endif
}

void
vm_map_cs_debugged_set(
	vm_map_t map,
	boolean_t val)
{
	vm_map_lock(map);
	map->cs_debugged = val;
	vm_map_unlock(map);
}

void
vm_map_cs_enforcement_set(
	vm_map_t map,
	boolean_t val)
{
	vm_map_lock(map);
	map->cs_enforcement = val;
	pmap_set_vm_map_cs_enforced(map->pmap, val);
	vm_map_unlock(map);
}

/*
 * IOKit has mapped a region into this map; adjust the pmap's ledgers appropriately.
 * phys_footprint is a composite limit consisting of iokit + physmem, so we need to
 * bump both counters.
 */
void
vm_map_iokit_mapped_region(vm_map_t map, vm_size_t bytes)
{
	pmap_t pmap = vm_map_pmap(map);

	ledger_credit(pmap->ledger, task_ledgers.iokit_mapped, bytes);
	ledger_credit(pmap->ledger, task_ledgers.phys_footprint, bytes);
}

void
vm_map_iokit_unmapped_region(vm_map_t map, vm_size_t bytes)
{
	pmap_t pmap = vm_map_pmap(map);

	ledger_debit(pmap->ledger, task_ledgers.iokit_mapped, bytes);
	ledger_debit(pmap->ledger, task_ledgers.phys_footprint, bytes);
}

/* Add (generate) code signature for memory range */
#if CONFIG_DYNAMIC_CODE_SIGNING
kern_return_t
vm_map_sign(vm_map_t map,
    vm_map_offset_t start,
    vm_map_offset_t end)
{
	vm_map_entry_t entry;
	vm_page_t m;
	vm_object_t object;

	/*
	 * Vet all the input parameters and current type and state of the
	 * underlaying object.  Return with an error if anything is amiss.
	 */
	if (map == VM_MAP_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	if (__improbable(vm_map_range_overflows(map, start, end - start))) {
		return KERN_INVALID_ADDRESS;
	}

	vm_map_lock_read(map);

	if (!vm_map_lookup_entry(map, start, &entry) || entry->is_sub_map) {
		/*
		 * Must pass a valid non-submap address.
		 */
		vm_map_unlock_read(map);
		return KERN_INVALID_ADDRESS;
	}

	if ((entry->vme_start > start) || (entry->vme_end < end)) {
		/*
		 * Map entry doesn't cover the requested range. Not handling
		 * this situation currently.
		 */
		vm_map_unlock_read(map);
		return KERN_INVALID_ARGUMENT;
	}

	object = VME_OBJECT(entry);
	if (object == VM_OBJECT_NULL) {
		/*
		 * Object must already be present or we can't sign.
		 */
		vm_map_unlock_read(map);
		return KERN_INVALID_ARGUMENT;
	}

	vm_object_lock(object);
	vm_map_unlock_read(map);

	while (start < end) {
		uint32_t refmod;

		m = vm_page_lookup(object,
		    start - entry->vme_start + VME_OFFSET(entry));
		if (m == VM_PAGE_NULL) {
			/* shoud we try to fault a page here? we can probably
			 * demand it exists and is locked for this request */
			vm_object_unlock(object);
			return KERN_FAILURE;
		}
		/* deal with special page status */
		if (m->vmp_busy ||
		    (m->vmp_unusual && (VMP_ERROR_GET(m) || m->vmp_restart || m->vmp_private || m->vmp_absent))) {
			vm_object_unlock(object);
			return KERN_FAILURE;
		}

		/* Page is OK... now "validate" it */
		/* This is the place where we'll call out to create a code
		 * directory, later */
		/* XXX TODO4K: deal with 4k subpages individually? */
		m->vmp_cs_validated = VMP_CS_ALL_TRUE;

		/* The page is now "clean" for codesigning purposes. That means
		 * we don't consider it as modified (wpmapped) anymore. But
		 * we'll disconnect the page so we note any future modification
		 * attempts. */
		m->vmp_wpmapped = FALSE;
		refmod = pmap_disconnect(VM_PAGE_GET_PHYS_PAGE(m));

		/* Pull the dirty status from the pmap, since we cleared the
		 * wpmapped bit */
		if ((refmod & VM_MEM_MODIFIED) && !m->vmp_dirty) {
			SET_PAGE_DIRTY(m, FALSE);
		}

		/* On to the next page */
		start += PAGE_SIZE;
	}
	vm_object_unlock(object);

	return KERN_SUCCESS;
}
#endif

kern_return_t
vm_map_partial_reap(vm_map_t map, unsigned int *reclaimed_resident, unsigned int *reclaimed_compressed)
{
	vm_map_entry_t  entry = VM_MAP_ENTRY_NULL;
	vm_map_entry_t  next_entry;
	kern_return_t   kr = KERN_SUCCESS;
	VM_MAP_ZAP_DECLARE(zap_list);

	vm_map_lock(map);

	for (entry = vm_map_first_entry(map);
	    entry != vm_map_to_entry(map);
	    entry = next_entry) {
		next_entry = entry->vme_next;

		if (!entry->is_sub_map &&
		    VME_OBJECT(entry) &&
		    (VME_OBJECT(entry)->internal == TRUE) &&
		    (VME_OBJECT(entry)->ref_count == 1)) {
			*reclaimed_resident += VME_OBJECT(entry)->resident_page_count;
			*reclaimed_compressed += vm_compressor_pager_get_count(VME_OBJECT(entry)->pager);

			(void)vm_map_delete(map, entry->vme_start,
			    entry->vme_end, VM_MAP_REMOVE_NO_YIELD,
			    KMEM_GUARD_NONE, &zap_list);
		}
	}

	vm_map_unlock(map);

	vm_map_zap_dispose(&zap_list);

	return kr;
}


#if DEVELOPMENT || DEBUG

int
vm_map_disconnect_page_mappings(
	vm_map_t map,
	boolean_t do_unnest)
{
	vm_map_entry_t entry;
	ledger_amount_t byte_count = 0;

	if (do_unnest == TRUE) {
#ifndef NO_NESTED_PMAP
		vm_map_lock(map);

		for (entry = vm_map_first_entry(map);
		    entry != vm_map_to_entry(map);
		    entry = entry->vme_next) {
			if (entry->is_sub_map && entry->use_pmap) {
				/*
				 * Make sure the range between the start of this entry and
				 * the end of this entry is no longer nested, so that
				 * we will only remove mappings from the pmap in use by this
				 * this task
				 */
				vm_map_clip_unnest(map, entry, entry->vme_start, entry->vme_end);
			}
		}
		vm_map_unlock(map);
#endif
	}
	vm_map_lock_read(map);

	ledger_get_balance(map->pmap->ledger, task_ledgers.phys_mem, &byte_count);

	for (entry = vm_map_first_entry(map);
	    entry != vm_map_to_entry(map);
	    entry = entry->vme_next) {
		if (!entry->is_sub_map && ((VME_OBJECT(entry) == 0) ||
		    (VME_OBJECT(entry)->phys_contiguous))) {
			continue;
		}
		if (entry->is_sub_map) {
			assert(!entry->use_pmap);
		}

		pmap_remove_options(map->pmap, entry->vme_start, entry->vme_end, 0);
	}
	vm_map_unlock_read(map);

	return (int) (byte_count / VM_MAP_PAGE_SIZE(map));
}

kern_return_t
vm_map_inject_error(vm_map_t map, vm_map_offset_t vaddr)
{
	vm_object_t object = NULL;
	vm_object_offset_t offset;
	vm_prot_t prot;
	boolean_t wired;
	vm_map_version_t version;
	vm_map_t real_map;
	int result = KERN_FAILURE;

	vaddr = vm_map_trunc_page(vaddr, PAGE_MASK);
	vm_map_lock(map);

	result = vm_map_lookup_and_lock_object(&map, vaddr, VM_PROT_READ,
	    OBJECT_LOCK_EXCLUSIVE, &version, &object, &offset, &prot, &wired,
	    NULL, &real_map, NULL);
	if (object == NULL) {
		result = KERN_MEMORY_ERROR;
	} else if (object->pager) {
		result = vm_compressor_pager_inject_error(object->pager,
		    offset);
	} else {
		result = KERN_MEMORY_PRESENT;
	}

	if (object != NULL) {
		vm_object_unlock(object);
	}

	if (real_map != map) {
		vm_map_unlock(real_map);
	}
	vm_map_unlock(map);

	return result;
}

#endif


#if CONFIG_FREEZE


extern struct freezer_context freezer_context_global;
AbsoluteTime c_freezer_last_yield_ts = 0;

extern unsigned int memorystatus_freeze_private_shared_pages_ratio;
extern unsigned int memorystatus_freeze_shared_mb_per_process_max;

kern_return_t
vm_map_freeze(
	task_t       task,
	unsigned int *purgeable_count,
	unsigned int *wired_count,
	unsigned int *clean_count,
	unsigned int *dirty_count,
	unsigned int dirty_budget,
	unsigned int *shared_count,
	int          *freezer_error_code,
	boolean_t    eval_only)
{
	vm_map_entry_t  entry2 = VM_MAP_ENTRY_NULL;
	kern_return_t   kr = KERN_SUCCESS;
	boolean_t       evaluation_phase = TRUE;
	vm_object_t     cur_shared_object = NULL;
	int             cur_shared_obj_ref_cnt = 0;
	unsigned int    dirty_private_count = 0, dirty_shared_count = 0, obj_pages_snapshot = 0;

	*purgeable_count = *wired_count = *clean_count = *dirty_count = *shared_count = 0;

	/*
	 * We need the exclusive lock here so that we can
	 * block any page faults or lookups while we are
	 * in the middle of freezing this vm map.
	 */
	vm_map_t map = task->map;

	vm_map_lock(map);

	assert(VM_CONFIG_COMPRESSOR_IS_PRESENT);

	if (vm_compressor_low_on_space() || vm_swap_low_on_space()) {
		if (vm_compressor_low_on_space()) {
			*freezer_error_code = FREEZER_ERROR_NO_COMPRESSOR_SPACE;
		}

		if (vm_swap_low_on_space()) {
			*freezer_error_code = FREEZER_ERROR_NO_SWAP_SPACE;
		}

		kr = KERN_NO_SPACE;
		goto done;
	}

	if (VM_CONFIG_FREEZER_SWAP_IS_ACTIVE == FALSE) {
		/*
		 * In-memory compressor backing the freezer. No disk.
		 * So no need to do the evaluation phase.
		 */
		evaluation_phase = FALSE;

		if (eval_only == TRUE) {
			/*
			 * We don't support 'eval_only' mode
			 * in this non-swap config.
			 */
			*freezer_error_code = FREEZER_ERROR_GENERIC;
			kr = KERN_INVALID_ARGUMENT;
			goto done;
		}

		freezer_context_global.freezer_ctx_uncompressed_pages = 0;
		clock_get_uptime(&c_freezer_last_yield_ts);
	}
again:

	for (entry2 = vm_map_first_entry(map);
	    entry2 != vm_map_to_entry(map);
	    entry2 = entry2->vme_next) {
		vm_object_t src_object;

		if (entry2->is_sub_map) {
			continue;
		}

		src_object = VME_OBJECT(entry2);
		if (!src_object ||
		    src_object->phys_contiguous ||
		    !src_object->internal) {
			continue;
		}

		/* If eligible, scan the entry, moving eligible pages over to our parent object */

		if (VM_CONFIG_FREEZER_SWAP_IS_ACTIVE) {
			/*
			 * We skip purgeable objects during evaluation phase only.
			 * If we decide to freeze this process, we'll explicitly
			 * purge these objects before we go around again with
			 * 'evaluation_phase' set to FALSE.
			 */

			if ((src_object->purgable == VM_PURGABLE_EMPTY) || (src_object->purgable == VM_PURGABLE_VOLATILE)) {
				/*
				 * We want to purge objects that may not belong to this task but are mapped
				 * in this task alone. Since we already purged this task's purgeable memory
				 * at the end of a successful evaluation phase, we want to avoid doing no-op calls
				 * on this task's purgeable objects. Hence the check for only volatile objects.
				 */
				if (evaluation_phase ||
				    src_object->purgable != VM_PURGABLE_VOLATILE ||
				    src_object->ref_count != 1) {
					continue;
				}
				vm_object_lock(src_object);
				if (src_object->purgable == VM_PURGABLE_VOLATILE &&
				    src_object->ref_count == 1) {
					purgeable_q_t old_queue;

					/* object should be on a purgeable queue */
					assert(src_object->objq.next != NULL &&
					    src_object->objq.prev != NULL);
					/* move object from its volatile queue to the nonvolatile queue */
					old_queue = vm_purgeable_object_remove(src_object);
					assert(old_queue);
					if (src_object->purgeable_when_ripe) {
						/* remove a token from that volatile queue */
						vm_page_lock_queues();
						vm_purgeable_token_delete_first(old_queue);
						vm_page_unlock_queues();
					}
					/* purge the object */
					vm_object_purge(src_object, 0);
				}
				vm_object_unlock(src_object);
				continue;
			}

			/*
			 * Pages belonging to this object could be swapped to disk.
			 * Make sure it's not a shared object because we could end
			 * up just bringing it back in again.
			 *
			 * We try to optimize somewhat by checking for objects that are mapped
			 * more than once within our own map. But we don't do full searches,
			 * we just look at the entries following our current entry.
			 */

			if (src_object->ref_count > 1) {
				if (src_object != cur_shared_object) {
					obj_pages_snapshot = (src_object->resident_page_count - src_object->wired_page_count) + vm_compressor_pager_get_count(src_object->pager);
					dirty_shared_count += obj_pages_snapshot;

					cur_shared_object = src_object;
					cur_shared_obj_ref_cnt = 1;
					continue;
				} else {
					cur_shared_obj_ref_cnt++;
					if (src_object->ref_count == cur_shared_obj_ref_cnt) {
						/*
						 * Fall through to below and treat this object as private.
						 * So deduct its pages from our shared total and add it to the
						 * private total.
						 */

						dirty_shared_count -= obj_pages_snapshot;
						dirty_private_count += obj_pages_snapshot;
					} else {
						continue;
					}
				}
			}


			if (src_object->ref_count == 1) {
				dirty_private_count += (src_object->resident_page_count - src_object->wired_page_count) + vm_compressor_pager_get_count(src_object->pager);
			}

			if (evaluation_phase == TRUE) {
				continue;
			}
		}

		uint32_t paged_out_count = vm_object_compressed_freezer_pageout(src_object, dirty_budget);
		*wired_count += src_object->wired_page_count;

		if (vm_compressor_low_on_space() || vm_swap_low_on_space()) {
			if (vm_compressor_low_on_space()) {
				*freezer_error_code = FREEZER_ERROR_NO_COMPRESSOR_SPACE;
			}

			if (vm_swap_low_on_space()) {
				*freezer_error_code = FREEZER_ERROR_NO_SWAP_SPACE;
			}

			kr = KERN_NO_SPACE;
			break;
		}
		if (paged_out_count >= dirty_budget) {
			break;
		}
		dirty_budget -= paged_out_count;
	}

	*shared_count = (unsigned int) ((dirty_shared_count * PAGE_SIZE_64) / (1024 * 1024ULL));
	if (evaluation_phase) {
		unsigned int shared_pages_threshold = (memorystatus_freeze_shared_mb_per_process_max * 1024 * 1024ULL) / PAGE_SIZE_64;

		if (dirty_shared_count > shared_pages_threshold) {
			*freezer_error_code = FREEZER_ERROR_EXCESS_SHARED_MEMORY;
			kr = KERN_FAILURE;
			goto done;
		}

		if (dirty_shared_count &&
		    ((dirty_private_count / dirty_shared_count) < memorystatus_freeze_private_shared_pages_ratio)) {
			*freezer_error_code = FREEZER_ERROR_LOW_PRIVATE_SHARED_RATIO;
			kr = KERN_FAILURE;
			goto done;
		}

		evaluation_phase = FALSE;
		dirty_shared_count = dirty_private_count = 0;

		freezer_context_global.freezer_ctx_uncompressed_pages = 0;
		clock_get_uptime(&c_freezer_last_yield_ts);

		if (eval_only) {
			kr = KERN_SUCCESS;
			goto done;
		}

		vm_purgeable_purge_task_owned(task);

		goto again;
	} else {
		kr = KERN_SUCCESS;
	}

done:
	vm_map_unlock(map);

	if ((eval_only == FALSE) && (kr == KERN_SUCCESS)) {
		vm_object_compressed_freezer_done();
	}
	return kr;
}

#endif

/*
 * vm_map_entry_should_cow_for_true_share:
 *
 * Determines if the map entry should be clipped and setup for copy-on-write
 * to avoid applying "true_share" to a large VM object when only a subset is
 * targeted.
 *
 * For now, we target only the map entries created for the Objective C
 * Garbage Collector, which initially have the following properties:
 *	- alias == VM_MEMORY_MALLOC
 *      - wired_count == 0
 *      - !needs_copy
 * and a VM object with:
 *      - internal
 *      - copy_strategy == MEMORY_OBJECT_COPY_SYMMETRIC
 *      - !true_share
 *      - vo_size == ANON_CHUNK_SIZE
 *
 * Only non-kernel map entries.
 */
boolean_t
vm_map_entry_should_cow_for_true_share(
	vm_map_entry_t  entry)
{
	vm_object_t     object;

	if (entry->is_sub_map) {
		/* entry does not point at a VM object */
		return FALSE;
	}

	if (entry->needs_copy) {
		/* already set for copy_on_write: done! */
		return FALSE;
	}

	if (VME_ALIAS(entry) != VM_MEMORY_MALLOC &&
	    VME_ALIAS(entry) != VM_MEMORY_MALLOC_SMALL) {
		/* not a malloc heap or Obj-C Garbage Collector heap */
		return FALSE;
	}

	if (entry->wired_count) {
		/* wired: can't change the map entry... */
		vm_counters.should_cow_but_wired++;
		return FALSE;
	}

	object = VME_OBJECT(entry);

	if (object == VM_OBJECT_NULL) {
		/* no object yet... */
		return FALSE;
	}

	if (!object->internal) {
		/* not an internal object */
		return FALSE;
	}

	if (object->copy_strategy != MEMORY_OBJECT_COPY_SYMMETRIC) {
		/* not the default copy strategy */
		return FALSE;
	}

	if (object->true_share) {
		/* already true_share: too late to avoid it */
		return FALSE;
	}

	if (VME_ALIAS(entry) == VM_MEMORY_MALLOC &&
	    object->vo_size != ANON_CHUNK_SIZE) {
		/* ... not an object created for the ObjC Garbage Collector */
		return FALSE;
	}

	if (VME_ALIAS(entry) == VM_MEMORY_MALLOC_SMALL &&
	    object->vo_size != 2048 * 4096) {
		/* ... not a "MALLOC_SMALL" heap */
		return FALSE;
	}

	/*
	 * All the criteria match: we have a large object being targeted for "true_share".
	 * To limit the adverse side-effects linked with "true_share", tell the caller to
	 * try and avoid setting up the entire object for "true_share" by clipping the
	 * targeted range and setting it up for copy-on-write.
	 */
	return TRUE;
}

uint64_t vm_map_range_overflows_count = 0;
TUNABLE_WRITEABLE(boolean_t, vm_map_range_overflows_log, "vm_map_range_overflows_log", FALSE);
bool
vm_map_range_overflows(
	vm_map_t map,
	vm_map_offset_t addr,
	vm_map_size_t size)
{
	vm_map_offset_t start, end, sum;
	vm_map_offset_t pgmask;

	if (size == 0) {
		/* empty range -> no overflow */
		return false;
	}
	pgmask = vm_map_page_mask(map);
	start = vm_map_trunc_page_mask(addr, pgmask);
	end = vm_map_round_page_mask(addr + size, pgmask);
	if (__improbable(os_add_overflow(addr, size, &sum) || end <= start)) {
		vm_map_range_overflows_count++;
		if (vm_map_range_overflows_log) {
			printf("%d[%s] vm_map_range_overflows addr 0x%llx size 0x%llx pgmask 0x%llx\n",
			    proc_selfpid(),
			    proc_best_name(current_proc()),
			    (uint64_t)addr,
			    (uint64_t)size,
			    (uint64_t)pgmask);
		}
		DTRACE_VM4(vm_map_range_overflows,
		    vm_map_t, map,
		    uint32_t, pgmask,
		    uint64_t, (uint64_t)addr,
		    uint64_t, (uint64_t)size);
		return true;
	}
	return false;
}

vm_map_offset_t
vm_map_round_page_mask(
	vm_map_offset_t offset,
	vm_map_offset_t mask)
{
	return VM_MAP_ROUND_PAGE(offset, mask);
}

vm_map_offset_t
vm_map_trunc_page_mask(
	vm_map_offset_t offset,
	vm_map_offset_t mask)
{
	return VM_MAP_TRUNC_PAGE(offset, mask);
}

boolean_t
vm_map_page_aligned(
	vm_map_offset_t offset,
	vm_map_offset_t mask)
{
	return ((offset) & mask) == 0;
}

int
vm_map_page_shift(
	vm_map_t map)
{
	return VM_MAP_PAGE_SHIFT(map);
}

int
vm_map_page_size(
	vm_map_t map)
{
	return VM_MAP_PAGE_SIZE(map);
}

vm_map_offset_t
vm_map_page_mask(
	vm_map_t map)
{
	return VM_MAP_PAGE_MASK(map);
}

kern_return_t
vm_map_set_page_shift(
	vm_map_t        map,
	int             pageshift)
{
	if (map->hdr.nentries != 0) {
		/* too late to change page size */
		return KERN_FAILURE;
	}

	map->hdr.page_shift = (uint16_t)pageshift;

	return KERN_SUCCESS;
}

kern_return_t
vm_map_query_volatile(
	vm_map_t        map,
	mach_vm_size_t  *volatile_virtual_size_p,
	mach_vm_size_t  *volatile_resident_size_p,
	mach_vm_size_t  *volatile_compressed_size_p,
	mach_vm_size_t  *volatile_pmap_size_p,
	mach_vm_size_t  *volatile_compressed_pmap_size_p)
{
	mach_vm_size_t  volatile_virtual_size;
	mach_vm_size_t  volatile_resident_count;
	mach_vm_size_t  volatile_compressed_count;
	mach_vm_size_t  volatile_pmap_count;
	mach_vm_size_t  volatile_compressed_pmap_count;
	mach_vm_size_t  resident_count;
	vm_map_entry_t  entry;
	vm_object_t     object;

	/* map should be locked by caller */

	volatile_virtual_size = 0;
	volatile_resident_count = 0;
	volatile_compressed_count = 0;
	volatile_pmap_count = 0;
	volatile_compressed_pmap_count = 0;

	for (entry = vm_map_first_entry(map);
	    entry != vm_map_to_entry(map);
	    entry = entry->vme_next) {
		mach_vm_size_t  pmap_resident_bytes, pmap_compressed_bytes;

		if (entry->is_sub_map) {
			continue;
		}
		if (!(entry->protection & VM_PROT_WRITE)) {
			continue;
		}
		object = VME_OBJECT(entry);
		if (object == VM_OBJECT_NULL) {
			continue;
		}
		if (object->purgable != VM_PURGABLE_VOLATILE &&
		    object->purgable != VM_PURGABLE_EMPTY) {
			continue;
		}
		if (VME_OFFSET(entry)) {
			/*
			 * If the map entry has been split and the object now
			 * appears several times in the VM map, we don't want
			 * to count the object's resident_page_count more than
			 * once.  We count it only for the first one, starting
			 * at offset 0 and ignore the other VM map entries.
			 */
			continue;
		}
		resident_count = object->resident_page_count;
		if ((VME_OFFSET(entry) / PAGE_SIZE) >= resident_count) {
			resident_count = 0;
		} else {
			resident_count -= (VME_OFFSET(entry) / PAGE_SIZE);
		}

		volatile_virtual_size += entry->vme_end - entry->vme_start;
		volatile_resident_count += resident_count;
		if (object->pager) {
			volatile_compressed_count +=
			    vm_compressor_pager_get_count(object->pager);
		}
		pmap_compressed_bytes = 0;
		pmap_resident_bytes =
		    pmap_query_resident(map->pmap,
		    entry->vme_start,
		    entry->vme_end,
		    &pmap_compressed_bytes);
		volatile_pmap_count += (pmap_resident_bytes / PAGE_SIZE);
		volatile_compressed_pmap_count += (pmap_compressed_bytes
		    / PAGE_SIZE);
	}

	/* map is still locked on return */

	*volatile_virtual_size_p = volatile_virtual_size;
	*volatile_resident_size_p = volatile_resident_count * PAGE_SIZE;
	*volatile_compressed_size_p = volatile_compressed_count * PAGE_SIZE;
	*volatile_pmap_size_p = volatile_pmap_count * PAGE_SIZE;
	*volatile_compressed_pmap_size_p = volatile_compressed_pmap_count * PAGE_SIZE;

	return KERN_SUCCESS;
}

void
vm_map_sizes(vm_map_t map,
    vm_map_size_t * psize,
    vm_map_size_t * pfree,
    vm_map_size_t * plargest_free)
{
	vm_map_entry_t  entry;
	vm_map_offset_t prev;
	vm_map_size_t   free, total_free, largest_free;
	boolean_t       end;

	if (!map) {
		*psize = *pfree = *plargest_free = 0;
		return;
	}
	total_free = largest_free = 0;

	vm_map_lock_read(map);
	if (psize) {
		*psize = map->max_offset - map->min_offset;
	}

	prev = map->min_offset;
	for (entry = vm_map_first_entry(map);; entry = entry->vme_next) {
		end = (entry == vm_map_to_entry(map));

		if (end) {
			free = entry->vme_end   - prev;
		} else {
			free = entry->vme_start - prev;
		}

		total_free += free;
		if (free > largest_free) {
			largest_free = free;
		}

		if (end) {
			break;
		}
		prev = entry->vme_end;
	}
	vm_map_unlock_read(map);
	if (pfree) {
		*pfree = total_free;
	}
	if (plargest_free) {
		*plargest_free = largest_free;
	}
}

#if VM_SCAN_FOR_SHADOW_CHAIN
int vm_map_shadow_max(vm_map_t map);
int
vm_map_shadow_max(
	vm_map_t map)
{
	int             shadows, shadows_max;
	vm_map_entry_t  entry;
	vm_object_t     object, next_object;

	if (map == NULL) {
		return 0;
	}

	shadows_max = 0;

	vm_map_lock_read(map);

	for (entry = vm_map_first_entry(map);
	    entry != vm_map_to_entry(map);
	    entry = entry->vme_next) {
		if (entry->is_sub_map) {
			continue;
		}
		object = VME_OBJECT(entry);
		if (object == NULL) {
			continue;
		}
		vm_object_lock_shared(object);
		for (shadows = 0;
		    object->shadow != NULL;
		    shadows++, object = next_object) {
			next_object = object->shadow;
			vm_object_lock_shared(next_object);
			vm_object_unlock(object);
		}
		vm_object_unlock(object);
		if (shadows > shadows_max) {
			shadows_max = shadows;
		}
	}

	vm_map_unlock_read(map);

	return shadows_max;
}
#endif /* VM_SCAN_FOR_SHADOW_CHAIN */

void
vm_commit_pagezero_status(vm_map_t lmap)
{
	pmap_advise_pagezero_range(lmap->pmap, lmap->min_offset);
}

#if __x86_64__
void
vm_map_set_high_start(
	vm_map_t        map,
	vm_map_offset_t high_start)
{
	map->vmmap_high_start = high_start;
}
#endif /* __x86_64__ */

#if CODE_SIGNING_MONITOR

kern_return_t
vm_map_entry_cs_associate(
	vm_map_t                map,
	vm_map_entry_t          entry,
	vm_map_kernel_flags_t   vmk_flags)
{
	vm_object_t cs_object, cs_shadow, backing_object;
	vm_object_offset_t cs_offset, backing_offset;
	void *cs_blobs;
	struct vnode *cs_vnode;
	kern_return_t cs_ret;

	if (map->pmap == NULL ||
	    entry->is_sub_map || /* XXX FBDP: recurse on sub-range? */
	    (csm_address_space_exempt(map->pmap) == KERN_SUCCESS) ||
	    VME_OBJECT(entry) == VM_OBJECT_NULL) {
		return KERN_SUCCESS;
	}

	if (!(entry->protection & VM_PROT_EXECUTE)) {
		/*
		 * This memory region is not executable, so the code-signing
		 * monitor would usually not care about it...
		 */
		if (vmk_flags.vmkf_remap_prot_copy &&
		    (entry->max_protection & VM_PROT_EXECUTE)) {
			/*
			 * ... except if the memory region is being remapped
			 * from r-x/r-x to rw-/rwx via vm_protect(VM_PROT_COPY)
			 * which is what a debugger or dtrace would be doing
			 * to prepare to modify an executable page to insert
			 * a breakpoint or activate a probe.
			 * In that case, fall through so that we can mark
			 * this region as being "debugged" and no longer
			 * strictly code-signed.
			 */
		} else {
			/*
			 * Really not executable, so no need to tell the
			 * code-signing monitor.
			 */
			return KERN_SUCCESS;
		}
	}

	vm_map_lock_assert_exclusive(map);

	/*
	 * Check for a debug association mapping before we check for used_for_jit. This
	 * allows non-RWX JIT on macOS systems to masquerade their mappings as USER_DEBUG
	 * pages instead of USER_JIT. These non-RWX JIT pages cannot be marked as USER_JIT
	 * since they are mapped with RW or RX permissions, which the page table monitor
	 * denies on USER_JIT pages. Given that, if they're not mapped as USER_DEBUG,
	 * they will be mapped as USER_EXEC, and that will cause another page table monitor
	 * violation when those USER_EXEC pages are mapped as RW.
	 *
	 * Since these pages switch between RW and RX through mprotect, they mimic what
	 * we expect a debugger to do. As the code signing monitor does not enforce mappings
	 * on macOS systems, this works in our favor here and allows us to continue to
	 * support these legacy-programmed applications without sacrificing security on
	 * the page table or the code signing monitor. We don't need to explicitly check
	 * for entry_for_jit here and the mapping permissions. If the initial mapping is
	 * created with RX, then the application must map it as RW in order to first write
	 * to the page (MAP_JIT mappings must be private and anonymous). The switch to
	 * RX will cause vm_map_protect to mark the entry as vmkf_remap_prot_copy.
	 * Similarly, if the mapping was created as RW, and then switched to RX,
	 * vm_map_protect will again mark the entry as a copy, and both these cases
	 * lead to this if-statement being entered.
	 *
	 * For more information: rdar://115313336.
	 */
	if (vmk_flags.vmkf_remap_prot_copy) {
		cs_ret = csm_associate_debug_region(
			map->pmap,
			entry->vme_start,
			entry->vme_end - entry->vme_start);

		/*
		 * csm_associate_debug_region returns not supported when the code signing
		 * monitor is disabled. This is intentional, since cs_ret is checked towards
		 * the end of the function, and if it is not supported, then we still want the
		 * VM to perform code-signing enforcement on this entry. That said, if we don't
		 * mark this as a xnu_user_debug page when the code-signing monitor is disabled,
		 * then it never gets retyped to XNU_USER_DEBUG frame type, which then causes
		 * an issue with debugging (since it'll be mapped in as XNU_USER_EXEC in some
		 * cases, which will cause a violation when attempted to be mapped as writable).
		 */
		if ((cs_ret == KERN_SUCCESS) || (cs_ret == KERN_NOT_SUPPORTED)) {
			entry->vme_xnu_user_debug = TRUE;
		}
#if DEVELOPMENT || DEBUG
		if (vm_log_xnu_user_debug) {
			printf("FBDP %d[%s] %s:%d map %p entry %p [ 0x%llx 0x%llx ]  vme_xnu_user_debug=%d cs_ret %d\n",
			    proc_selfpid(),
			    (get_bsdtask_info(current_task()) ? proc_name_address(get_bsdtask_info(current_task())) : "?"),
			    __FUNCTION__, __LINE__,
			    map, entry,
			    (uint64_t)entry->vme_start, (uint64_t)entry->vme_end,
			    entry->vme_xnu_user_debug,
			    cs_ret);
		}
#endif /* DEVELOPMENT || DEBUG */
		goto done;
	}

	if (entry->used_for_jit) {
		cs_ret = csm_associate_jit_region(
			map->pmap,
			entry->vme_start,
			entry->vme_end - entry->vme_start);
		goto done;
	}

	cs_object = VME_OBJECT(entry);
	vm_object_lock_shared(cs_object);
	cs_offset = VME_OFFSET(entry);

	/* find the VM object backed by the code-signed vnode */
	for (;;) {
		/* go to the bottom of cs_object's shadow chain */
		for (;
		    cs_object->shadow != VM_OBJECT_NULL;
		    cs_object = cs_shadow) {
			cs_shadow = cs_object->shadow;
			cs_offset += cs_object->vo_shadow_offset;
			vm_object_lock_shared(cs_shadow);
			vm_object_unlock(cs_object);
		}
		if (cs_object->internal ||
		    cs_object->pager == MEMORY_OBJECT_NULL) {
			vm_object_unlock(cs_object);
			return KERN_SUCCESS;
		}

		cs_offset += cs_object->paging_offset;

		/*
		 * cs_object could be backed by a:
		 *      vnode_pager
		 *	apple_protect_pager
		 *      shared_region_pager
		 *	fourk_pager (multiple backing objects -> fail?)
		 * ask the pager if it has a backing VM object
		 */
		if (!memory_object_backing_object(cs_object->pager,
		    cs_offset,
		    &backing_object,
		    &backing_offset)) {
			/* no backing object: cs_object is it */
			break;
		}

		/* look down the backing object's shadow chain */
		vm_object_lock_shared(backing_object);
		vm_object_unlock(cs_object);
		cs_object = backing_object;
		cs_offset = backing_offset;
	}

	cs_vnode = vnode_pager_lookup_vnode(cs_object->pager);
	if (cs_vnode == NULL) {
		/* no vnode, no code signatures to associate */
		cs_ret = KERN_SUCCESS;
	} else {
		cs_ret = vnode_pager_get_cs_blobs(cs_vnode,
		    &cs_blobs);
		assert(cs_ret == KERN_SUCCESS);
		cs_ret = cs_associate_blob_with_mapping(map->pmap,
		    entry->vme_start,
		    (entry->vme_end - entry->vme_start),
		    cs_offset,
		    cs_blobs);
	}
	vm_object_unlock(cs_object);
	cs_object = VM_OBJECT_NULL;

done:
	if (cs_ret == KERN_SUCCESS) {
		DTRACE_VM2(vm_map_entry_cs_associate_success,
		    vm_map_offset_t, entry->vme_start,
		    vm_map_offset_t, entry->vme_end);
		if (vm_map_executable_immutable) {
			/*
			 * Prevent this executable
			 * mapping from being unmapped
			 * or modified.
			 */
			entry->vme_permanent = TRUE;
		}
		/*
		 * pmap says it will validate the
		 * code-signing validity of pages
		 * faulted in via this mapping, so
		 * this map entry should be marked so
		 * that vm_fault() bypasses code-signing
		 * validation for faults coming through
		 * this mapping.
		 */
		entry->csm_associated = TRUE;
	} else if (cs_ret == KERN_NOT_SUPPORTED) {
		/*
		 * pmap won't check the code-signing
		 * validity of pages faulted in via
		 * this mapping, so VM should keep
		 * doing it.
		 */
		DTRACE_VM3(vm_map_entry_cs_associate_off,
		    vm_map_offset_t, entry->vme_start,
		    vm_map_offset_t, entry->vme_end,
		    int, cs_ret);
	} else {
		/*
		 * A real error: do not allow
		 * execution in this mapping.
		 */
		DTRACE_VM3(vm_map_entry_cs_associate_failure,
		    vm_map_offset_t, entry->vme_start,
		    vm_map_offset_t, entry->vme_end,
		    int, cs_ret);
		if (vmk_flags.vmkf_overwrite_immutable) {
			/*
			 * We can get here when we remap an apple_protect pager
			 * on top of an already cs_associated executable mapping
			 * with the same code signatures, so we don't want to
			 * lose VM_PROT_EXECUTE in that case...
			 */
		} else {
			entry->protection &= ~VM_PROT_ALLEXEC;
			entry->max_protection &= ~VM_PROT_ALLEXEC;
		}
	}

	return cs_ret;
}

#endif /* CODE_SIGNING_MONITOR */

inline bool
vm_map_is_corpse_source(vm_map_t map)
{
	bool status = false;
	if (map) {
		vm_map_lock_read(map);
		status = map->corpse_source;
		vm_map_unlock_read(map);
	}
	return status;
}

inline void
vm_map_set_corpse_source(vm_map_t map)
{
	if (map) {
		vm_map_lock(map);
		map->corpse_source = true;
		vm_map_unlock(map);
	}
}

inline void
vm_map_unset_corpse_source(vm_map_t map)
{
	if (map) {
		vm_map_lock(map);
		map->corpse_source = false;
		vm_map_unlock(map);
	}
}
/*
 * FORKED CORPSE FOOTPRINT
 *
 * A forked corpse gets a copy of the original VM map but its pmap is mostly
 * empty since it never ran and never got to fault in any pages.
 * Collecting footprint info (via "sysctl vm.self_region_footprint") for
 * a forked corpse would therefore return very little information.
 *
 * When forking a corpse, we can pass the VM_MAP_FORK_CORPSE_FOOTPRINT option
 * to vm_map_fork() to collect footprint information from the original VM map
 * and its pmap, and store it in the forked corpse's VM map.  That information
 * is stored in place of the VM map's "hole list" since we'll never need to
 * lookup for holes in the corpse's map.
 *
 * The corpse's footprint info looks like this:
 *
 * vm_map->vmmap_corpse_footprint points to pageable kernel memory laid out
 * as follows:
 *                     +---------------------------------------+
 *            header-> | cf_size                               |
 *                     +-------------------+-------------------+
 *                     | cf_last_region    | cf_last_zeroes    |
 *                     +-------------------+-------------------+
 *           region1-> | cfr_vaddr                             |
 *                     +-------------------+-------------------+
 *                     | cfr_num_pages     | d0 | d1 | d2 | d3 |
 *                     +---------------------------------------+
 *                     | d4 | d5 | ...                         |
 *                     +---------------------------------------+
 *                     | ...                                   |
 *                     +-------------------+-------------------+
 *                     | dy | dz | na | na | cfr_vaddr...      | <-region2
 *                     +-------------------+-------------------+
 *                     | cfr_vaddr (ctd)   | cfr_num_pages     |
 *                     +---------------------------------------+
 *                     | d0 | d1 ...                           |
 *                     +---------------------------------------+
 *                       ...
 *                     +---------------------------------------+
 *       last region-> | cfr_vaddr                             |
 *                     +---------------------------------------+
 *                     + cfr_num_pages     | d0 | d1 | d2 | d3 |
 *                     +---------------------------------------+
 *                       ...
 *                     +---------------------------------------+
 *                     | dx | dy | dz | na | na | na | na | na |
 *                     +---------------------------------------+
 *
 * where:
 *      cf_size:	total size of the buffer (rounded to page size)
 *      cf_last_region:	offset in the buffer of the last "region" sub-header
 *	cf_last_zeroes: number of trailing "zero" dispositions at the end
 *			of last region
 *	cfr_vaddr:	virtual address of the start of the covered "region"
 *	cfr_num_pages:	number of pages in the covered "region"
 *	d*:		disposition of the page at that virtual address
 * Regions in the buffer are word-aligned.
 *
 * We estimate the size of the buffer based on the number of memory regions
 * and the virtual size of the address space.  While copying each memory region
 * during vm_map_fork(), we also collect the footprint info for that region
 * and store it in the buffer, packing it as much as possible (coalescing
 * contiguous memory regions to avoid having too many region headers and
 * avoiding long streaks of "zero" page dispositions by splitting footprint
 * "regions", so the number of regions in the footprint buffer might not match
 * the number of memory regions in the address space.
 *
 * We also have to copy the original task's "nonvolatile" ledgers since that's
 * part of the footprint and will need to be reported to any tool asking for
 * the footprint information of the forked corpse.
 */

uint64_t vm_map_corpse_footprint_count = 0;
uint64_t vm_map_corpse_footprint_size_avg = 0;
uint64_t vm_map_corpse_footprint_size_max = 0;
uint64_t vm_map_corpse_footprint_full = 0;
uint64_t vm_map_corpse_footprint_no_buf = 0;

struct vm_map_corpse_footprint_header {
	vm_size_t       cf_size;        /* allocated buffer size */
	uint32_t        cf_last_region; /* offset of last region in buffer */
	union {
		uint32_t cfu_last_zeroes; /* during creation:
		                           * number of "zero" dispositions at
		                           * end of last region */
		uint32_t cfu_hint_region; /* during lookup:
		                           * offset of last looked up region */
#define cf_last_zeroes cfu.cfu_last_zeroes
#define cf_hint_region cfu.cfu_hint_region
	} cfu;
};
typedef uint8_t cf_disp_t;
struct vm_map_corpse_footprint_region {
	vm_map_offset_t cfr_vaddr;      /* region start virtual address */
	uint32_t        cfr_num_pages;  /* number of pages in this "region" */
	cf_disp_t   cfr_disposition[0]; /* disposition of each page */
} __attribute__((packed));

static cf_disp_t
vm_page_disposition_to_cf_disp(
	int disposition)
{
	assert(sizeof(cf_disp_t) == 1);
	/* relocate bits that don't fit in a "uint8_t" */
	if (disposition & VM_PAGE_QUERY_PAGE_REUSABLE) {
		disposition |= VM_PAGE_QUERY_PAGE_FICTITIOUS;
	}
	/* cast gets rid of extra bits */
	return (cf_disp_t) disposition;
}

static int
vm_page_cf_disp_to_disposition(
	cf_disp_t cf_disp)
{
	int disposition;

	assert(sizeof(cf_disp_t) == 1);
	disposition = (int) cf_disp;
	/* move relocated bits back in place */
	if (cf_disp & VM_PAGE_QUERY_PAGE_FICTITIOUS) {
		disposition |= VM_PAGE_QUERY_PAGE_REUSABLE;
		disposition &= ~VM_PAGE_QUERY_PAGE_FICTITIOUS;
	}
	return disposition;
}

/*
 * vm_map_corpse_footprint_new_region:
 *      closes the current footprint "region" and creates a new one
 *
 * Returns NULL if there's not enough space in the buffer for a new region.
 */
static struct vm_map_corpse_footprint_region *
vm_map_corpse_footprint_new_region(
	struct vm_map_corpse_footprint_header *footprint_header)
{
	uintptr_t       footprint_edge;
	uint32_t        new_region_offset;
	struct vm_map_corpse_footprint_region *footprint_region;
	struct vm_map_corpse_footprint_region *new_footprint_region;

	footprint_edge = ((uintptr_t)footprint_header +
	    footprint_header->cf_size);
	footprint_region = ((struct vm_map_corpse_footprint_region *)
	    ((char *)footprint_header +
	    footprint_header->cf_last_region));
	assert((uintptr_t)footprint_region + sizeof(*footprint_region) <=
	    footprint_edge);

	/* get rid of trailing zeroes in the last region */
	assert(footprint_region->cfr_num_pages >=
	    footprint_header->cf_last_zeroes);
	footprint_region->cfr_num_pages -=
	    footprint_header->cf_last_zeroes;
	footprint_header->cf_last_zeroes = 0;

	/* reuse this region if it's now empty */
	if (footprint_region->cfr_num_pages == 0) {
		return footprint_region;
	}

	/* compute offset of new region */
	new_region_offset = footprint_header->cf_last_region;
	new_region_offset += sizeof(*footprint_region);
	new_region_offset += (footprint_region->cfr_num_pages * sizeof(cf_disp_t));
	new_region_offset = roundup(new_region_offset, sizeof(int));

	/* check if we're going over the edge */
	if (((uintptr_t)footprint_header +
	    new_region_offset +
	    sizeof(*footprint_region)) >=
	    footprint_edge) {
		/* over the edge: no new region */
		return NULL;
	}

	/* adjust offset of last region in header */
	footprint_header->cf_last_region = new_region_offset;

	new_footprint_region = (struct vm_map_corpse_footprint_region *)
	    ((char *)footprint_header +
	    footprint_header->cf_last_region);
	new_footprint_region->cfr_vaddr = 0;
	new_footprint_region->cfr_num_pages = 0;
	/* caller needs to initialize new region */

	return new_footprint_region;
}

/*
 * vm_map_corpse_footprint_collect:
 *	collect footprint information for "old_entry" in "old_map" and
 *	stores it in "new_map"'s vmmap_footprint_info.
 */
kern_return_t
vm_map_corpse_footprint_collect(
	vm_map_t        old_map,
	vm_map_entry_t  old_entry,
	vm_map_t        new_map)
{
	vm_map_offset_t va;
	kern_return_t   kr;
	struct vm_map_corpse_footprint_header *footprint_header;
	struct vm_map_corpse_footprint_region *footprint_region;
	struct vm_map_corpse_footprint_region *new_footprint_region;
	cf_disp_t       *next_disp_p;
	uintptr_t       footprint_edge;
	uint32_t        num_pages_tmp;
	int             effective_page_size;

	effective_page_size = MIN(PAGE_SIZE, VM_MAP_PAGE_SIZE(old_map));

	va = old_entry->vme_start;

	vm_map_lock_assert_exclusive(old_map);
	vm_map_lock_assert_exclusive(new_map);

	assert(new_map->has_corpse_footprint);
	assert(!old_map->has_corpse_footprint);
	if (!new_map->has_corpse_footprint ||
	    old_map->has_corpse_footprint) {
		/*
		 * This can only transfer footprint info from a
		 * map with a live pmap to a map with a corpse footprint.
		 */
		return KERN_NOT_SUPPORTED;
	}

	if (new_map->vmmap_corpse_footprint == NULL) {
		vm_offset_t     buf;
		vm_size_t       buf_size;

		buf = 0;
		buf_size = (sizeof(*footprint_header) +
		    (old_map->hdr.nentries
		    *
		    (sizeof(*footprint_region) +
		    +3))            /* potential alignment for each region */
		    +
		    ((old_map->size / effective_page_size)
		    *
		    sizeof(cf_disp_t)));      /* disposition for each page */
//		printf("FBDP corpse map %p guestimate footprint size 0x%llx\n", new_map, (uint64_t) buf_size);
		buf_size = round_page(buf_size);

		/* limit buffer to 1 page to validate overflow detection */
//		buf_size = PAGE_SIZE;

		/* limit size to a somewhat sane amount */
#if XNU_TARGET_OS_OSX
#define VM_MAP_CORPSE_FOOTPRINT_INFO_MAX_SIZE   (8*1024*1024)   /* 8MB */
#else /* XNU_TARGET_OS_OSX */
#define VM_MAP_CORPSE_FOOTPRINT_INFO_MAX_SIZE   (256*1024)      /* 256KB */
#endif /* XNU_TARGET_OS_OSX */
		if (buf_size > VM_MAP_CORPSE_FOOTPRINT_INFO_MAX_SIZE) {
			buf_size = VM_MAP_CORPSE_FOOTPRINT_INFO_MAX_SIZE;
		}

		/*
		 * Allocate the pageable buffer (with a trailing guard page).
		 * It will be zero-filled on demand.
		 */
		kr = kmem_alloc(kernel_map, &buf, buf_size + PAGE_SIZE,
		    KMA_DATA | KMA_PAGEABLE | KMA_GUARD_LAST,
		    VM_KERN_MEMORY_DIAG);
		if (kr != KERN_SUCCESS) {
			vm_map_corpse_footprint_no_buf++;
			return kr;
		}

		/* initialize header and 1st region */
		footprint_header = (struct vm_map_corpse_footprint_header *)buf;
		new_map->vmmap_corpse_footprint = footprint_header;

		footprint_header->cf_size = buf_size;
		footprint_header->cf_last_region =
		    sizeof(*footprint_header);
		footprint_header->cf_last_zeroes = 0;

		footprint_region = (struct vm_map_corpse_footprint_region *)
		    ((char *)footprint_header +
		    footprint_header->cf_last_region);
		footprint_region->cfr_vaddr = 0;
		footprint_region->cfr_num_pages = 0;
	} else {
		/* retrieve header and last region */
		footprint_header = (struct vm_map_corpse_footprint_header *)
		    new_map->vmmap_corpse_footprint;
		footprint_region = (struct vm_map_corpse_footprint_region *)
		    ((char *)footprint_header +
		    footprint_header->cf_last_region);
	}
	footprint_edge = ((uintptr_t)footprint_header +
	    footprint_header->cf_size);

	if ((footprint_region->cfr_vaddr +
	    (((vm_map_offset_t)footprint_region->cfr_num_pages) *
	    effective_page_size))
	    != old_entry->vme_start) {
		uint64_t num_pages_delta, num_pages_delta_size;
		uint32_t region_offset_delta_size;

		/*
		 * Not the next contiguous virtual address:
		 * start a new region or store "zero" dispositions for
		 * the missing pages?
		 */
		/* size of gap in actual page dispositions */
		num_pages_delta = ((old_entry->vme_start -
		    footprint_region->cfr_vaddr) / effective_page_size)
		    - footprint_region->cfr_num_pages;
		num_pages_delta_size = num_pages_delta * sizeof(cf_disp_t);
		/* size of gap as a new footprint region header */
		region_offset_delta_size =
		    (sizeof(*footprint_region) +
		    roundup(((footprint_region->cfr_num_pages -
		    footprint_header->cf_last_zeroes) * sizeof(cf_disp_t)),
		    sizeof(int)) -
		    ((footprint_region->cfr_num_pages -
		    footprint_header->cf_last_zeroes) * sizeof(cf_disp_t)));
//		printf("FBDP %s:%d region 0x%x 0x%llx 0x%x vme_start 0x%llx pages_delta 0x%llx region_delta 0x%x\n", __FUNCTION__, __LINE__, footprint_header->cf_last_region, footprint_region->cfr_vaddr, footprint_region->cfr_num_pages, old_entry->vme_start, num_pages_delta, region_offset_delta);
		if (region_offset_delta_size < num_pages_delta_size ||
		    os_add3_overflow(footprint_region->cfr_num_pages,
		    (uint32_t) num_pages_delta,
		    1,
		    &num_pages_tmp)) {
			/*
			 * Storing data for this gap would take more space
			 * than inserting a new footprint region header:
			 * let's start a new region and save space. If it's a
			 * tie, let's avoid using a new region, since that
			 * would require more region hops to find the right
			 * range during lookups.
			 *
			 * If the current region's cfr_num_pages would overflow
			 * if we added "zero" page dispositions for the gap,
			 * no choice but to start a new region.
			 */
//			printf("FBDP %s:%d new region\n", __FUNCTION__, __LINE__);
			new_footprint_region =
			    vm_map_corpse_footprint_new_region(footprint_header);
			/* check that we're not going over the edge */
			if (new_footprint_region == NULL) {
				goto over_the_edge;
			}
			footprint_region = new_footprint_region;
			/* initialize new region as empty */
			footprint_region->cfr_vaddr = old_entry->vme_start;
			footprint_region->cfr_num_pages = 0;
		} else {
			/*
			 * Store "zero" page dispositions for the missing
			 * pages.
			 */
//			printf("FBDP %s:%d zero gap\n", __FUNCTION__, __LINE__);
			for (; num_pages_delta > 0; num_pages_delta--) {
				next_disp_p = (cf_disp_t *)
				    ((uintptr_t) footprint_region +
				    sizeof(*footprint_region));
				next_disp_p += footprint_region->cfr_num_pages;
				/* check that we're not going over the edge */
				if ((uintptr_t)next_disp_p >= footprint_edge) {
					goto over_the_edge;
				}
				/* store "zero" disposition for this gap page */
				footprint_region->cfr_num_pages++;
				*next_disp_p = (cf_disp_t) 0;
				footprint_header->cf_last_zeroes++;
			}
		}
	}

	for (va = old_entry->vme_start;
	    va < old_entry->vme_end;
	    va += effective_page_size) {
		int             disposition;
		cf_disp_t       cf_disp;

		vm_map_footprint_query_page_info(old_map,
		    old_entry,
		    va,
		    &disposition);
		cf_disp = vm_page_disposition_to_cf_disp(disposition);

//		if (va < SHARED_REGION_BASE_ARM64) printf("FBDP collect map %p va 0x%llx disp 0x%x\n", new_map, va, disp);

		if (cf_disp == 0 && footprint_region->cfr_num_pages == 0) {
			/*
			 * Ignore "zero" dispositions at start of
			 * region: just move start of region.
			 */
			footprint_region->cfr_vaddr += effective_page_size;
			continue;
		}

		/* would region's cfr_num_pages overflow? */
		if (os_add_overflow(footprint_region->cfr_num_pages, 1,
		    &num_pages_tmp)) {
			/* overflow: create a new region */
			new_footprint_region =
			    vm_map_corpse_footprint_new_region(
				footprint_header);
			if (new_footprint_region == NULL) {
				goto over_the_edge;
			}
			footprint_region = new_footprint_region;
			footprint_region->cfr_vaddr = va;
			footprint_region->cfr_num_pages = 0;
		}

		next_disp_p = (cf_disp_t *) ((uintptr_t) footprint_region +
		    sizeof(*footprint_region));
		next_disp_p += footprint_region->cfr_num_pages;
		/* check that we're not going over the edge */
		if ((uintptr_t)next_disp_p >= footprint_edge) {
			goto over_the_edge;
		}
		/* store this dispostion */
		*next_disp_p = cf_disp;
		footprint_region->cfr_num_pages++;

		if (cf_disp != 0) {
			/* non-zero disp: break the current zero streak */
			footprint_header->cf_last_zeroes = 0;
			/* done */
			continue;
		}

		/* zero disp: add to the current streak of zeroes */
		footprint_header->cf_last_zeroes++;
		if ((footprint_header->cf_last_zeroes +
		    roundup(((footprint_region->cfr_num_pages -
		    footprint_header->cf_last_zeroes) * sizeof(cf_disp_t)) &
		    (sizeof(int) - 1),
		    sizeof(int))) <
		    (sizeof(*footprint_header))) {
			/*
			 * There are not enough trailing "zero" dispositions
			 * (+ the extra padding we would need for the previous
			 * region); creating a new region would not save space
			 * at this point, so let's keep this "zero" disposition
			 * in this region and reconsider later.
			 */
			continue;
		}
		/*
		 * Create a new region to avoid having too many consecutive
		 * "zero" dispositions.
		 */
		new_footprint_region =
		    vm_map_corpse_footprint_new_region(footprint_header);
		if (new_footprint_region == NULL) {
			goto over_the_edge;
		}
		footprint_region = new_footprint_region;
		/* initialize the new region as empty ... */
		footprint_region->cfr_num_pages = 0;
		/* ... and skip this "zero" disp */
		footprint_region->cfr_vaddr = va + effective_page_size;
	}

	return KERN_SUCCESS;

over_the_edge:
//	printf("FBDP map %p footprint was full for va 0x%llx\n", new_map, va);
	vm_map_corpse_footprint_full++;
	return KERN_RESOURCE_SHORTAGE;
}

/*
 * vm_map_corpse_footprint_collect_done:
 *	completes the footprint collection by getting rid of any remaining
 *	trailing "zero" dispositions and trimming the unused part of the
 *	kernel buffer
 */
void
vm_map_corpse_footprint_collect_done(
	vm_map_t        new_map)
{
	struct vm_map_corpse_footprint_header *footprint_header;
	struct vm_map_corpse_footprint_region *footprint_region;
	vm_size_t       buf_size, actual_size;
	kern_return_t   kr;

	assert(new_map->has_corpse_footprint);
	if (!new_map->has_corpse_footprint ||
	    new_map->vmmap_corpse_footprint == NULL) {
		return;
	}

	footprint_header = (struct vm_map_corpse_footprint_header *)
	    new_map->vmmap_corpse_footprint;
	buf_size = footprint_header->cf_size;

	footprint_region = (struct vm_map_corpse_footprint_region *)
	    ((char *)footprint_header +
	    footprint_header->cf_last_region);

	/* get rid of trailing zeroes in last region */
	assert(footprint_region->cfr_num_pages >= footprint_header->cf_last_zeroes);
	footprint_region->cfr_num_pages -= footprint_header->cf_last_zeroes;
	footprint_header->cf_last_zeroes = 0;

	actual_size = (vm_size_t)(footprint_header->cf_last_region +
	    sizeof(*footprint_region) +
	    (footprint_region->cfr_num_pages * sizeof(cf_disp_t)));

//	printf("FBDP map %p buf_size 0x%llx actual_size 0x%llx\n", new_map, (uint64_t) buf_size, (uint64_t) actual_size);
	vm_map_corpse_footprint_size_avg =
	    (((vm_map_corpse_footprint_size_avg *
	    vm_map_corpse_footprint_count) +
	    actual_size) /
	    (vm_map_corpse_footprint_count + 1));
	vm_map_corpse_footprint_count++;
	if (actual_size > vm_map_corpse_footprint_size_max) {
		vm_map_corpse_footprint_size_max = actual_size;
	}

	actual_size = round_page(actual_size);
	if (buf_size > actual_size) {
		kr = vm_deallocate(kernel_map,
		    ((vm_address_t)footprint_header +
		    actual_size +
		    PAGE_SIZE),                 /* trailing guard page */
		    (buf_size - actual_size));
		assertf(kr == KERN_SUCCESS,
		    "trim: footprint_header %p buf_size 0x%llx actual_size 0x%llx kr=0x%x\n",
		    footprint_header,
		    (uint64_t) buf_size,
		    (uint64_t) actual_size,
		    kr);
		kr = vm_protect(kernel_map,
		    ((vm_address_t)footprint_header +
		    actual_size),
		    PAGE_SIZE,
		    FALSE,             /* set_maximum */
		    VM_PROT_NONE);
		assertf(kr == KERN_SUCCESS,
		    "guard: footprint_header %p buf_size 0x%llx actual_size 0x%llx kr=0x%x\n",
		    footprint_header,
		    (uint64_t) buf_size,
		    (uint64_t) actual_size,
		    kr);
	}

	footprint_header->cf_size = actual_size;
}

/*
 * vm_map_corpse_footprint_query_page_info:
 *	retrieves the disposition of the page at virtual address "vaddr"
 *	in the forked corpse's VM map
 *
 * This is the equivalent of vm_map_footprint_query_page_info() for a forked corpse.
 */
kern_return_t
vm_map_corpse_footprint_query_page_info(
	vm_map_t        map,
	vm_map_offset_t va,
	int             *disposition_p)
{
	struct vm_map_corpse_footprint_header *footprint_header;
	struct vm_map_corpse_footprint_region *footprint_region;
	uint32_t        footprint_region_offset;
	vm_map_offset_t region_start, region_end;
	int             disp_idx;
	kern_return_t   kr;
	int             effective_page_size;
	cf_disp_t       cf_disp;

	if (!map->has_corpse_footprint) {
		*disposition_p = 0;
		kr = KERN_INVALID_ARGUMENT;
		goto done;
	}

	footprint_header = map->vmmap_corpse_footprint;
	if (footprint_header == NULL) {
		*disposition_p = 0;
//		if (va < SHARED_REGION_BASE_ARM64) printf("FBDP %d query map %p va 0x%llx disp 0x%x\n", __LINE__, map, va, *disposition_p);
		kr = KERN_INVALID_ARGUMENT;
		goto done;
	}

	/* start looking at the hint ("cf_hint_region") */
	footprint_region_offset = footprint_header->cf_hint_region;

	effective_page_size = MIN(PAGE_SIZE, VM_MAP_PAGE_SIZE(map));

lookup_again:
	if (footprint_region_offset < sizeof(*footprint_header)) {
		/* hint too low: start from 1st region */
		footprint_region_offset = sizeof(*footprint_header);
	}
	if (footprint_region_offset >= footprint_header->cf_last_region) {
		/* hint too high: re-start from 1st region */
		footprint_region_offset = sizeof(*footprint_header);
	}
	footprint_region = (struct vm_map_corpse_footprint_region *)
	    ((char *)footprint_header + footprint_region_offset);
	region_start = footprint_region->cfr_vaddr;
	region_end = (region_start +
	    ((vm_map_offset_t)(footprint_region->cfr_num_pages) *
	    effective_page_size));
	if (va < region_start &&
	    footprint_region_offset != sizeof(*footprint_header)) {
		/* our range starts before the hint region */

		/* reset the hint (in a racy way...) */
		footprint_header->cf_hint_region = sizeof(*footprint_header);
		/* lookup "va" again from 1st region */
		footprint_region_offset = sizeof(*footprint_header);
		goto lookup_again;
	}

	while (va >= region_end) {
		if (footprint_region_offset >= footprint_header->cf_last_region) {
			break;
		}
		/* skip the region's header */
		footprint_region_offset += sizeof(*footprint_region);
		/* skip the region's page dispositions */
		footprint_region_offset += (footprint_region->cfr_num_pages * sizeof(cf_disp_t));
		/* align to next word boundary */
		footprint_region_offset =
		    roundup(footprint_region_offset,
		    sizeof(int));
		footprint_region = (struct vm_map_corpse_footprint_region *)
		    ((char *)footprint_header + footprint_region_offset);
		region_start = footprint_region->cfr_vaddr;
		region_end = (region_start +
		    ((vm_map_offset_t)(footprint_region->cfr_num_pages) *
		    effective_page_size));
	}
	if (va < region_start || va >= region_end) {
		/* page not found */
		*disposition_p = 0;
//		if (va < SHARED_REGION_BASE_ARM64) printf("FBDP %d query map %p va 0x%llx disp 0x%x\n", __LINE__, map, va, *disposition_p);
		kr = KERN_SUCCESS;
		goto done;
	}

	/* "va" found: set the lookup hint for next lookup (in a racy way...) */
	footprint_header->cf_hint_region = footprint_region_offset;

	/* get page disposition for "va" in this region */
	disp_idx = (int) ((va - footprint_region->cfr_vaddr) / effective_page_size);
	cf_disp = footprint_region->cfr_disposition[disp_idx];
	*disposition_p = vm_page_cf_disp_to_disposition(cf_disp);
	kr = KERN_SUCCESS;
done:
//	if (va < SHARED_REGION_BASE_ARM64) printf("FBDP %d query map %p va 0x%llx disp 0x%x\n", __LINE__, map, va, *disposition_p);
	/* dtrace -n 'vminfo:::footprint_query_page_info { printf("map 0x%p va 0x%llx disp 0x%x kr 0x%x", arg0, arg1, arg2, arg3); }' */
	DTRACE_VM4(footprint_query_page_info,
	    vm_map_t, map,
	    vm_map_offset_t, va,
	    int, *disposition_p,
	    kern_return_t, kr);

	return kr;
}

void
vm_map_corpse_footprint_destroy(
	vm_map_t        map)
{
	if (map->has_corpse_footprint &&
	    map->vmmap_corpse_footprint != 0) {
		struct vm_map_corpse_footprint_header *footprint_header;
		vm_size_t buf_size;
		kern_return_t kr;

		footprint_header = map->vmmap_corpse_footprint;
		buf_size = footprint_header->cf_size;
		kr = vm_deallocate(kernel_map,
		    (vm_offset_t) map->vmmap_corpse_footprint,
		    ((vm_size_t) buf_size
		    + PAGE_SIZE));                 /* trailing guard page */
		assertf(kr == KERN_SUCCESS, "kr=0x%x\n", kr);
		map->vmmap_corpse_footprint = 0;
		map->has_corpse_footprint = FALSE;
	}
}

/*
 * vm_map_copy_footprint_ledgers:
 *	copies any ledger that's relevant to the memory footprint of "old_task"
 *	into the forked corpse's task ("new_task")
 */
void
vm_map_copy_footprint_ledgers(
	task_t  old_task,
	task_t  new_task)
{
	vm_map_copy_ledger(old_task, new_task, task_ledgers.phys_footprint);
	vm_map_copy_ledger(old_task, new_task, task_ledgers.purgeable_nonvolatile);
	vm_map_copy_ledger(old_task, new_task, task_ledgers.purgeable_nonvolatile_compressed);
	vm_map_copy_ledger(old_task, new_task, task_ledgers.internal);
	vm_map_copy_ledger(old_task, new_task, task_ledgers.internal_compressed);
	vm_map_copy_ledger(old_task, new_task, task_ledgers.iokit_mapped);
	vm_map_copy_ledger(old_task, new_task, task_ledgers.alternate_accounting);
	vm_map_copy_ledger(old_task, new_task, task_ledgers.alternate_accounting_compressed);
	vm_map_copy_ledger(old_task, new_task, task_ledgers.page_table);
	vm_map_copy_ledger(old_task, new_task, task_ledgers.tagged_footprint);
	vm_map_copy_ledger(old_task, new_task, task_ledgers.tagged_footprint_compressed);
	vm_map_copy_ledger(old_task, new_task, task_ledgers.network_nonvolatile);
	vm_map_copy_ledger(old_task, new_task, task_ledgers.network_nonvolatile_compressed);
	vm_map_copy_ledger(old_task, new_task, task_ledgers.media_footprint);
	vm_map_copy_ledger(old_task, new_task, task_ledgers.media_footprint_compressed);
	vm_map_copy_ledger(old_task, new_task, task_ledgers.graphics_footprint);
	vm_map_copy_ledger(old_task, new_task, task_ledgers.graphics_footprint_compressed);
	vm_map_copy_ledger(old_task, new_task, task_ledgers.neural_footprint);
	vm_map_copy_ledger(old_task, new_task, task_ledgers.neural_footprint_compressed);
	vm_map_copy_ledger(old_task, new_task, task_ledgers.wired_mem);
}

/*
 * vm_map_copy_ledger:
 *	copy a single ledger from "old_task" to "new_task"
 */
void
vm_map_copy_ledger(
	task_t  old_task,
	task_t  new_task,
	int     ledger_entry)
{
	ledger_amount_t old_balance, new_balance, delta;

	assert(new_task->map->has_corpse_footprint);
	if (!new_task->map->has_corpse_footprint) {
		return;
	}

	/* turn off sanity checks for the ledger we're about to mess with */
	ledger_disable_panic_on_negative(new_task->ledger,
	    ledger_entry);

	/* adjust "new_task" to match "old_task" */
	ledger_get_balance(old_task->ledger,
	    ledger_entry,
	    &old_balance);
	ledger_get_balance(new_task->ledger,
	    ledger_entry,
	    &new_balance);
	if (new_balance == old_balance) {
		/* new == old: done */
	} else if (new_balance > old_balance) {
		/* new > old ==> new -= new - old */
		delta = new_balance - old_balance;
		ledger_debit(new_task->ledger,
		    ledger_entry,
		    delta);
	} else {
		/* new < old ==> new += old - new */
		delta = old_balance - new_balance;
		ledger_credit(new_task->ledger,
		    ledger_entry,
		    delta);
	}
}

/*
 * vm_map_get_pmap:
 * returns the pmap associated with the vm_map
 */
pmap_t
vm_map_get_pmap(vm_map_t map)
{
	return vm_map_pmap(map);
}

#if CONFIG_MAP_RANGES
static bitmap_t vm_map_user_range_heap_map[BITMAP_LEN(VM_MEMORY_COUNT)];

static_assert(UMEM_RANGE_ID_DEFAULT == MACH_VM_RANGE_DEFAULT);
static_assert(UMEM_RANGE_ID_HEAP == MACH_VM_RANGE_DATA);

/*
 * vm_map_range_map_init:
 *  initializes the VM range ID map to enable index lookup
 *  of user VM ranges based on VM tag from userspace.
 */
static void
vm_map_range_map_init(void)
{
	/*
	 * VM_MEMORY_MALLOC{,_NANO} are skipped on purpose:
	 * - the former is malloc metadata which should be kept separate
	 * - the latter has its own ranges
	 */
	bitmap_set(vm_map_user_range_heap_map, VM_MEMORY_MALLOC_HUGE);
	bitmap_set(vm_map_user_range_heap_map, VM_MEMORY_MALLOC_LARGE);
	bitmap_set(vm_map_user_range_heap_map, VM_MEMORY_MALLOC_LARGE_REUSED);
	bitmap_set(vm_map_user_range_heap_map, VM_MEMORY_MALLOC_MEDIUM);
	bitmap_set(vm_map_user_range_heap_map, VM_MEMORY_MALLOC_PROB_GUARD);
	bitmap_set(vm_map_user_range_heap_map, VM_MEMORY_MALLOC_SMALL);
	bitmap_set(vm_map_user_range_heap_map, VM_MEMORY_MALLOC_TINY);
	bitmap_set(vm_map_user_range_heap_map, VM_MEMORY_TCMALLOC);
	bitmap_set(vm_map_user_range_heap_map, VM_MEMORY_LIBNETWORK);
	bitmap_set(vm_map_user_range_heap_map, VM_MEMORY_IOACCELERATOR);
	bitmap_set(vm_map_user_range_heap_map, VM_MEMORY_IOSURFACE);
	bitmap_set(vm_map_user_range_heap_map, VM_MEMORY_IMAGEIO);
	bitmap_set(vm_map_user_range_heap_map, VM_MEMORY_COREGRAPHICS);
	bitmap_set(vm_map_user_range_heap_map, VM_MEMORY_CORESERVICES);
	bitmap_set(vm_map_user_range_heap_map, VM_MEMORY_COREDATA);
	bitmap_set(vm_map_user_range_heap_map, VM_MEMORY_LAYERKIT);
}

static struct mach_vm_range
vm_map_range_random_uniform(
	vm_map_size_t           req_size,
	vm_map_offset_t         min_addr,
	vm_map_offset_t         max_addr,
	vm_map_offset_t         offmask)
{
	vm_map_offset_t random_addr;
	struct mach_vm_range alloc;

	req_size = (req_size + offmask) & ~offmask;
	min_addr = (min_addr + offmask) & ~offmask;
	max_addr = max_addr & ~offmask;

	read_random(&random_addr, sizeof(random_addr));
	random_addr %= (max_addr - req_size - min_addr);
	random_addr &= ~offmask;

	alloc.min_address = min_addr + random_addr;
	alloc.max_address = min_addr + random_addr + req_size;
	return alloc;
}

static vm_map_offset_t
vm_map_range_offmask(void)
{
	uint32_t pte_depth;

	/*
	 * PTE optimizations
	 *
	 *
	 * 16k pages systems
	 * ~~~~~~~~~~~~~~~~~
	 *
	 * A single L1 (sub-)page covers the address space.
	 * - L2 pages cover 64G,
	 * - L3 pages cover 32M.
	 *
	 * On embedded, the dynamic VA range is 64G and uses a single L2 page.
	 * As a result, we really only need to align the ranges to 32M to avoid
	 * partial L3 pages.
	 *
	 * On macOS, the usage of L2 pages will increase, so as a result we will
	 * want to align ranges to 64G in order to utilize them fully.
	 *
	 *
	 * 4k pages systems
	 * ~~~~~~~~~~~~~~~~
	 *
	 * A single L0 (sub-)page covers the address space.
	 * - L1 pages cover 512G,
	 * - L2 pages cover 1G,
	 * - L3 pages cover 2M.
	 *
	 * The long tail of processes on a system will tend to have a VA usage
	 * (ignoring the shared regions) in the 100s of MB order of magnitnude.
	 * This is achievable with a single L1 and a few L2s without
	 * randomization.
	 *
	 * However once randomization is introduced, the system will immediately
	 * need several L1s and many more L2s. As a result:
	 *
	 * - on embedded devices, the cost of these extra pages isn't
	 *   sustainable, and we just disable the feature entirely,
	 *
	 * - on macOS we align ranges to a 512G boundary so that the extra L1
	 *   pages can be used to their full potential.
	 */

	/*
	 * note, this function assumes _non exotic mappings_
	 * which is why it uses the native kernel's PAGE_SHIFT.
	 */
#if XNU_PLATFORM_MacOSX
	pte_depth = PAGE_SHIFT > 12 ? 2 : 3;
#else /* !XNU_PLATFORM_MacOSX */
	pte_depth = PAGE_SHIFT > 12 ? 1 : 0;
#endif /* !XNU_PLATFORM_MacOSX */

	if (pte_depth == 0) {
		return 0;
	}

	return (1ull << ((PAGE_SHIFT - 3) * pte_depth + PAGE_SHIFT)) - 1;
}

/*
 * vm_map_range_configure:
 *	configures the user vm_map ranges by increasing the maximum VA range of
 *  the map and carving out a range at the end of VA space (searching backwards
 *  in the newly expanded map).
 */
kern_return_t
vm_map_range_configure(vm_map_t map)
{
	const vm_map_offset_t offmask = vm_map_range_offmask();
	struct mach_vm_range data_range;
	vm_map_offset_t default_end;
	kern_return_t kr;

	if (!vm_map_is_64bit(map) || vm_map_is_exotic(map) || offmask == 0) {
		/*
		 * No point doing vm ranges in a 32bit address space.
		 */
		return KERN_NOT_SUPPORTED;
	}

	/* Should not be applying ranges to kernel map or kernel map submaps */
	assert(vm_map_pmap(map) != kernel_pmap);

#if XNU_PLATFORM_MacOSX

	/*
	 * on macOS, the address space is a massive 47 bits (128T),
	 * with several carve outs that processes can't use:
	 * - the shared region
	 * - the commpage region
	 * - the GPU carve out (if applicable)
	 *
	 * and when nano-malloc is in use it desires memory at the 96T mark.
	 *
	 * However, their location is architecture dependent:
	 * - On intel, the shared region and commpage are
	 *   at the very end of the usable address space (above +127T),
	 *   and there is no GPU carve out, and pthread wants to place
	 *   threads at the 112T mark (0x70T).
	 *
	 * - On arm64, these are in the same spot as on embedded devices:
	 *   o shared region:   [ 6G,  10G)  [ will likely grow over time ]
	 *   o commpage region: [63G,  64G)
	 *   o GPU carve out:   [64G, 448G)
	 *
	 * This is conveninent because the mappings at the end of the address
	 * space (when they exist) are made by the kernel.
	 *
	 * The policy is to allocate a random 1T for the data heap
	 * in the end of the address-space in the:
	 * - [0x71, 0x7f) range on Intel (to leave space for pthread stacks)
	 * - [0x61, 0x7f) range on ASM (to leave space for Nano malloc).
	 */

	/* see NANOZONE_SIGNATURE in libmalloc */
#if __x86_64__
	default_end = 0x71ull << 40;
#else
	default_end = 0x61ull << 40;
#endif
	data_range  = vm_map_range_random_uniform(1ull << 40,
	        default_end, 0x7full << 40, offmask);

#else /* !XNU_PLATFORM_MacOSX */

	/*
	 * Embedded devices:
	 *
	 *   The default VA Size scales with the device physical memory.
	 *
	 *   Out of that:
	 *   - the "zero" page typically uses 4G + some slide
	 *   - the shared region uses SHARED_REGION_SIZE bytes (4G)
	 *
	 *   Without the use of jumbo or any adjustment to the address space,
	 *   a default VM map typically looks like this:
	 *
	 *       0G -->╒════════════╕
	 *             │  pagezero  │
	 *             │  + slide   │
	 *      ~4G -->╞════════════╡<-- vm_map_min(map)
	 *             │            │
	 *       6G -->├────────────┤
	 *             │   shared   │
	 *             │   region   │
	 *      10G -->├────────────┤
	 *             │            │
	 *   max_va -->├────────────┤<-- vm_map_max(map)
	 *             │            │
	 *             ╎   jumbo    ╎
	 *             ╎            ╎
	 *             │            │
	 *      63G -->╞════════════╡<-- MACH_VM_MAX_ADDRESS
	 *             │  commpage  │
	 *      64G -->├────────────┤<-- MACH_VM_MIN_GPU_CARVEOUT_ADDRESS
	 *             │            │
	 *             ╎    GPU     ╎
	 *             ╎  carveout  ╎
	 *             │            │
	 *     448G -->├────────────┤<-- MACH_VM_MAX_GPU_CARVEOUT_ADDRESS
	 *             │            │
	 *             ╎            ╎
	 *             ╎            ╎
	 *             │            │
	 *     512G -->╘════════════╛<-- (1ull << ARM_16K_TT_L1_SHIFT)
	 *
	 *   When this drawing was made, "max_va" was smaller than
	 *   ARM64_MAX_OFFSET_DEVICE_LARGE (~15.5G), leaving shy of
	 *   12G of address space for the zero-page, slide, files,
	 *   binaries, heap ...
	 *
	 *   We will want to make a "heap/data" carve out inside
	 *   the jumbo range of half of that usable space, assuming
	 *   that this is less than a forth of the jumbo range.
	 *
	 *   The assert below intends to catch when max_va grows
	 *   too large for this heuristic.
	 */

	vm_map_lock_read(map);
	default_end = vm_map_max(map);
	vm_map_unlock_read(map);

	/*
	 * Check that we're not already jumbo'd,
	 * or our address space was somehow modified.
	 *
	 * If so we cannot guarantee that we can set up the ranges
	 * safely without interfering with the existing map.
	 */
	if (default_end > vm_compute_max_offset(true)) {
		return KERN_NO_SPACE;
	}

	if (pmap_max_offset(true, ARM_PMAP_MAX_OFFSET_DEFAULT)) {
		/*
		 * an override boot-arg was set, disable user-ranges
		 *
		 * XXX: this is problematic because it means these boot-args
		 *      no longer test the behavior changing the value
		 *      of ARM64_MAX_OFFSET_DEVICE_* would have.
		 */
		return KERN_NOT_SUPPORTED;
	}

	/* expand the default VM space to the largest possible address */
	vm_map_set_jumbo(map);

	assert3u(7 * GiB(10) / 2, <=, vm_map_max(map) - default_end);
	data_range = vm_map_range_random_uniform(GiB(10),
	    default_end + PAGE_SIZE, vm_map_max(map), offmask);

#endif /* !XNU_PLATFORM_MacOSX */

	/*
	 * Poke holes so that ASAN or people listing regions
	 * do not think this space is free.
	 */

	if (default_end != data_range.min_address) {
		kr = vm_map_enter(map, &default_end,
		    data_range.min_address - default_end,
		    0, VM_MAP_KERNEL_FLAGS_FIXED_PERMANENT(), VM_OBJECT_NULL,
		    0, FALSE, VM_PROT_NONE, VM_PROT_NONE, VM_INHERIT_DEFAULT);
		assert(kr == KERN_SUCCESS);
	}

	if (data_range.max_address != vm_map_max(map)) {
		vm_map_entry_t entry;
		vm_size_t size;

		vm_map_lock_read(map);
		vm_map_lookup_entry_or_next(map, data_range.max_address, &entry);
		if (entry != vm_map_to_entry(map)) {
			size = vm_map_max(map) - data_range.max_address;
		} else {
			size = entry->vme_start - data_range.max_address;
		}
		vm_map_unlock_read(map);

		kr = vm_map_enter(map, &data_range.max_address, size,
		    0, VM_MAP_KERNEL_FLAGS_FIXED_PERMANENT(), VM_OBJECT_NULL,
		    0, FALSE, VM_PROT_NONE, VM_PROT_NONE, VM_INHERIT_DEFAULT);
		assert(kr == KERN_SUCCESS);
	}

	vm_map_lock(map);
	map->default_range.min_address = vm_map_min(map);
	map->default_range.max_address = default_end;
	map->data_range = data_range;
	map->uses_user_ranges = true;
	vm_map_unlock(map);

	return KERN_SUCCESS;
}

/*
 * vm_map_range_fork:
 *	clones the array of ranges from old_map to new_map in support
 *  of a VM map fork.
 */
void
vm_map_range_fork(vm_map_t new_map, vm_map_t old_map)
{
	if (!old_map->uses_user_ranges) {
		/* nothing to do */
		return;
	}

	new_map->default_range = old_map->default_range;
	new_map->data_range = old_map->data_range;

	if (old_map->extra_ranges_count) {
		vm_map_user_range_t otable, ntable;
		uint16_t count;

		otable = old_map->extra_ranges;
		count  = old_map->extra_ranges_count;
		ntable = kalloc_data(count * sizeof(struct vm_map_user_range),
		    Z_WAITOK | Z_ZERO | Z_NOFAIL);
		memcpy(ntable, otable,
		    count * sizeof(struct vm_map_user_range));

		new_map->extra_ranges_count = count;
		new_map->extra_ranges = ntable;
	}

	new_map->uses_user_ranges = true;
}

/*
 * vm_map_get_user_range:
 *	copy the VM user range for the given VM map and range ID.
 */
kern_return_t
vm_map_get_user_range(
	vm_map_t                map,
	vm_map_range_id_t       range_id,
	mach_vm_range_t         range)
{
	if (map == NULL || !map->uses_user_ranges || range == NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	switch (range_id) {
	case UMEM_RANGE_ID_DEFAULT:
		*range = map->default_range;
		return KERN_SUCCESS;

	case UMEM_RANGE_ID_HEAP:
		*range = map->data_range;
		return KERN_SUCCESS;

	default:
		return KERN_INVALID_ARGUMENT;
	}
}

static vm_map_range_id_t
vm_map_user_range_resolve(
	vm_map_t                map,
	mach_vm_address_t       addr,
	mach_vm_size_t          size,
	mach_vm_range_t         range)
{
	struct mach_vm_range tmp;

	vm_map_lock_assert_held(map);

	static_assert(UMEM_RANGE_ID_DEFAULT == MACH_VM_RANGE_DEFAULT);
	static_assert(UMEM_RANGE_ID_HEAP == MACH_VM_RANGE_DATA);

	if (mach_vm_range_contains(&map->default_range, addr, size)) {
		if (range) {
			*range = map->default_range;
		}
		return UMEM_RANGE_ID_DEFAULT;
	}

	if (mach_vm_range_contains(&map->data_range, addr, size)) {
		if (range) {
			*range = map->data_range;
		}
		return UMEM_RANGE_ID_HEAP;
	}

	for (size_t i = 0; i < map->extra_ranges_count; i++) {
		vm_map_user_range_t r = &map->extra_ranges[i];

		tmp.min_address = r->vmur_min_address;
		tmp.max_address = r->vmur_max_address;

		if (mach_vm_range_contains(&tmp, addr, size)) {
			if (range) {
				*range = tmp;
			}
			return r->vmur_range_id;
		}
	}

	if (range) {
		range->min_address = range->max_address = 0;
	}
	return UMEM_RANGE_ID_DEFAULT;
}

static int
vm_map_user_range_cmp(const void *e1, const void *e2)
{
	const struct vm_map_user_range *r1 = e1;
	const struct vm_map_user_range *r2 = e2;

	if (r1->vmur_min_address != r2->vmur_min_address) {
		return r1->vmur_min_address < r2->vmur_min_address ? -1 : 1;
	}

	return 0;
}

static int
mach_vm_range_recipe_v1_cmp(const void *e1, const void *e2)
{
	const mach_vm_range_recipe_v1_t *r1 = e1;
	const mach_vm_range_recipe_v1_t *r2 = e2;

	if (r1->range.min_address != r2->range.min_address) {
		return r1->range.min_address < r2->range.min_address ? -1 : 1;
	}

	return 0;
}

/*!
 * @function mach_vm_range_create_v1()
 *
 * @brief
 * Handle the backend for mach_vm_range_create() for the
 * MACH_VM_RANGE_FLAVOR_V1 flavor.
 *
 * @description
 * This call allows to create "ranges" in the map of a task
 * that have special semantics/policies around placement of
 * new allocations (in the vm_map_locate_space() sense).
 *
 * @returns
 * - KERN_SUCCESS on success
 * - KERN_INVALID_ARGUMENT for incorrect arguments
 * - KERN_NO_SPACE if the maximum amount of ranges would be exceeded
 * - KERN_MEMORY_PRESENT if any of the requested ranges
 *   overlaps with existing ranges or allocations in the map.
 */
static kern_return_t
mach_vm_range_create_v1(
	vm_map_t                map,
	mach_vm_range_recipe_v1_t *recipe,
	uint32_t                new_count)
{
	const vm_offset_t mask = VM_MAP_PAGE_MASK(map);
	vm_map_user_range_t table;
	kern_return_t kr = KERN_SUCCESS;
	uint16_t count;

	struct mach_vm_range void1 = {
		.min_address = map->default_range.max_address,
		.max_address = map->data_range.min_address,
	};
	struct mach_vm_range void2 = {
		.min_address = map->data_range.max_address,
		.max_address = vm_map_max(map),
	};

	qsort(recipe, new_count, sizeof(mach_vm_range_recipe_v1_t),
	    mach_vm_range_recipe_v1_cmp);

	/*
	 * Step 1: Validate that the recipes have no intersections.
	 */

	for (size_t i = 0; i < new_count; i++) {
		mach_vm_range_t r = &recipe[i].range;
		mach_vm_size_t s;

		if (recipe[i].flags) {
			return KERN_INVALID_ARGUMENT;
		}

		static_assert(UMEM_RANGE_ID_FIXED == MACH_VM_RANGE_FIXED);
		switch (recipe[i].range_tag) {
		case MACH_VM_RANGE_FIXED:
			break;
		default:
			return KERN_INVALID_ARGUMENT;
		}

		if (!VM_MAP_PAGE_ALIGNED(r->min_address, mask) ||
		    !VM_MAP_PAGE_ALIGNED(r->max_address, mask) ||
		    r->min_address >= r->max_address) {
			return KERN_INVALID_ARGUMENT;
		}

		s = mach_vm_range_size(r);
		if (!mach_vm_range_contains(&void1, r->min_address, s) &&
		    !mach_vm_range_contains(&void2, r->min_address, s)) {
			return KERN_INVALID_ARGUMENT;
		}

		if (i > 0 && recipe[i - 1].range.max_address >
		    recipe[i].range.min_address) {
			return KERN_INVALID_ARGUMENT;
		}
	}

	vm_map_lock(map);

	table = map->extra_ranges;
	count = map->extra_ranges_count;

	if (count + new_count > VM_MAP_EXTRA_RANGES_MAX) {
		kr = KERN_NO_SPACE;
		goto out_unlock;
	}

	/*
	 * Step 2: Check that there is no intersection with existing ranges.
	 */

	for (size_t i = 0, j = 0; i < new_count && j < count;) {
		mach_vm_range_t     r1 = &recipe[i].range;
		vm_map_user_range_t r2 = &table[j];

		if (r1->max_address <= r2->vmur_min_address) {
			i++;
		} else if (r2->vmur_max_address <= r1->min_address) {
			j++;
		} else {
			kr = KERN_MEMORY_PRESENT;
			goto out_unlock;
		}
	}

	/*
	 * Step 4: commit the new ranges.
	 */

	static_assert(VM_MAP_EXTRA_RANGES_MAX * sizeof(struct vm_map_user_range) <=
	    KALLOC_SAFE_ALLOC_SIZE);

	table = krealloc_data(table,
	    count * sizeof(struct vm_map_user_range),
	    (count + new_count) * sizeof(struct vm_map_user_range),
	    Z_ZERO | Z_WAITOK | Z_NOFAIL);

	for (size_t i = 0; i < new_count; i++) {
		static_assert(MACH_VM_MAX_ADDRESS < (1ull << 56));

		table[count + i] = (struct vm_map_user_range){
			.vmur_min_address = recipe[i].range.min_address,
			.vmur_max_address = recipe[i].range.max_address,
			.vmur_range_id    = (vm_map_range_id_t)recipe[i].range_tag,
		};
	}

	qsort(table, count + new_count,
	    sizeof(struct vm_map_user_range), vm_map_user_range_cmp);

	map->extra_ranges_count += new_count;
	map->extra_ranges = table;

out_unlock:
	vm_map_unlock(map);

	if (kr == KERN_SUCCESS) {
		for (size_t i = 0; i < new_count; i++) {
			vm_map_kernel_flags_t vmk_flags = {
				.vmf_fixed = true,
				.vmf_overwrite = true,
				.vmkf_overwrite_immutable = true,
				.vm_tag = recipe[i].vm_tag,
			};
			__assert_only kern_return_t kr2;

			kr2 = vm_map_enter(map, &recipe[i].range.min_address,
			    mach_vm_range_size(&recipe[i].range),
			    0, vmk_flags, VM_OBJECT_NULL, 0, FALSE,
			    VM_PROT_NONE, VM_PROT_ALL,
			    VM_INHERIT_DEFAULT);
			assert(kr2 == KERN_SUCCESS);
		}
	}
	return kr;
}

kern_return_t
mach_vm_range_create(
	vm_map_t                map,
	mach_vm_range_flavor_t  flavor,
	mach_vm_range_recipes_raw_t recipe,
	natural_t               size)
{
	if (map != current_map()) {
		return KERN_INVALID_ARGUMENT;
	}

	if (!map->uses_user_ranges) {
		return KERN_NOT_SUPPORTED;
	}

	if (size == 0) {
		return KERN_SUCCESS;
	}

	if (flavor == MACH_VM_RANGE_FLAVOR_V1) {
		mach_vm_range_recipe_v1_t *array;

		if (size % sizeof(mach_vm_range_recipe_v1_t)) {
			return KERN_INVALID_ARGUMENT;
		}

		size /= sizeof(mach_vm_range_recipe_v1_t);
		if (size > VM_MAP_EXTRA_RANGES_MAX) {
			return KERN_NO_SPACE;
		}

		array = (mach_vm_range_recipe_v1_t *)recipe;
		return mach_vm_range_create_v1(map, array, size);
	}

	return KERN_INVALID_ARGUMENT;
}

#else /* !CONFIG_MAP_RANGES */

kern_return_t
mach_vm_range_create(
	vm_map_t                map,
	mach_vm_range_flavor_t  flavor,
	mach_vm_range_recipes_raw_t recipe,
	natural_t               size)
{
#pragma unused(map, flavor, recipe, size)
	return KERN_NOT_SUPPORTED;
}

#endif /* !CONFIG_MAP_RANGES */

void
vm_map_kernel_flags_update_range_id(vm_map_kernel_flags_t *vmkf, vm_map_t map)
{
	if (map == kernel_map) {
		if (vmkf->vmkf_range_id == KMEM_RANGE_ID_NONE) {
			vmkf->vmkf_range_id = KMEM_RANGE_ID_DATA;
		}
#if CONFIG_MAP_RANGES
	} else if (vmkf->vm_tag < VM_MEMORY_COUNT &&
	    vmkf->vmkf_range_id == UMEM_RANGE_ID_DEFAULT &&
	    bitmap_test(vm_map_user_range_heap_map, vmkf->vm_tag)) {
		vmkf->vmkf_range_id = UMEM_RANGE_ID_HEAP;
#endif /* CONFIG_MAP_RANGES */
	}
}

/*
 * vm_map_entry_has_device_pager:
 * Check if the vm map entry specified by the virtual address has a device pager.
 * If the vm map entry does not exist or if the map is NULL, this returns FALSE.
 */
boolean_t
vm_map_entry_has_device_pager(vm_map_t map, vm_map_offset_t vaddr)
{
	vm_map_entry_t entry;
	vm_object_t object;
	boolean_t result;

	if (map == NULL) {
		return FALSE;
	}

	vm_map_lock(map);
	while (TRUE) {
		if (!vm_map_lookup_entry(map, vaddr, &entry)) {
			result = FALSE;
			break;
		}
		if (entry->is_sub_map) {
			// Check the submap
			vm_map_t submap = VME_SUBMAP(entry);
			assert(submap != NULL);
			vm_map_lock(submap);
			vm_map_unlock(map);
			map = submap;
			continue;
		}
		object = VME_OBJECT(entry);
		if (object != NULL && object->pager != NULL && is_device_pager_ops(object->pager->mo_pager_ops)) {
			result = TRUE;
			break;
		}
		result = FALSE;
		break;
	}

	vm_map_unlock(map);
	return result;
}


#if MACH_ASSERT

extern int pmap_ledgers_panic;
extern int pmap_ledgers_panic_leeway;

#define LEDGER_DRIFT(__LEDGER)                    \
	int             __LEDGER##_over;          \
	ledger_amount_t __LEDGER##_over_total;    \
	ledger_amount_t __LEDGER##_over_max;      \
	int             __LEDGER##_under;         \
	ledger_amount_t __LEDGER##_under_total;   \
	ledger_amount_t __LEDGER##_under_max

struct {
	uint64_t        num_pmaps_checked;

	LEDGER_DRIFT(phys_footprint);
	LEDGER_DRIFT(internal);
	LEDGER_DRIFT(internal_compressed);
	LEDGER_DRIFT(external);
	LEDGER_DRIFT(reusable);
	LEDGER_DRIFT(iokit_mapped);
	LEDGER_DRIFT(alternate_accounting);
	LEDGER_DRIFT(alternate_accounting_compressed);
	LEDGER_DRIFT(page_table);
	LEDGER_DRIFT(purgeable_volatile);
	LEDGER_DRIFT(purgeable_nonvolatile);
	LEDGER_DRIFT(purgeable_volatile_compressed);
	LEDGER_DRIFT(purgeable_nonvolatile_compressed);
	LEDGER_DRIFT(tagged_nofootprint);
	LEDGER_DRIFT(tagged_footprint);
	LEDGER_DRIFT(tagged_nofootprint_compressed);
	LEDGER_DRIFT(tagged_footprint_compressed);
	LEDGER_DRIFT(network_volatile);
	LEDGER_DRIFT(network_nonvolatile);
	LEDGER_DRIFT(network_volatile_compressed);
	LEDGER_DRIFT(network_nonvolatile_compressed);
	LEDGER_DRIFT(media_nofootprint);
	LEDGER_DRIFT(media_footprint);
	LEDGER_DRIFT(media_nofootprint_compressed);
	LEDGER_DRIFT(media_footprint_compressed);
	LEDGER_DRIFT(graphics_nofootprint);
	LEDGER_DRIFT(graphics_footprint);
	LEDGER_DRIFT(graphics_nofootprint_compressed);
	LEDGER_DRIFT(graphics_footprint_compressed);
	LEDGER_DRIFT(neural_nofootprint);
	LEDGER_DRIFT(neural_footprint);
	LEDGER_DRIFT(neural_nofootprint_compressed);
	LEDGER_DRIFT(neural_footprint_compressed);
} pmap_ledgers_drift;

void
vm_map_pmap_check_ledgers(
	pmap_t          pmap,
	ledger_t        ledger,
	int             pid,
	char            *procname)
{
	ledger_amount_t bal;
	boolean_t       do_panic;

	do_panic = FALSE;

	pmap_ledgers_drift.num_pmaps_checked++;

#define LEDGER_CHECK_BALANCE(__LEDGER)                                  \
MACRO_BEGIN                                                             \
	int panic_on_negative = TRUE;                                   \
	ledger_get_balance(ledger,                                      \
	                   task_ledgers.__LEDGER,                       \
	                   &bal);                                       \
	ledger_get_panic_on_negative(ledger,                            \
	                             task_ledgers.__LEDGER,             \
	                             &panic_on_negative);               \
	if (bal != 0) {                                                 \
	        if (panic_on_negative ||                                \
	            (pmap_ledgers_panic &&                              \
	             pmap_ledgers_panic_leeway > 0 &&                   \
	             (bal > (pmap_ledgers_panic_leeway * PAGE_SIZE) ||  \
	              bal < (-pmap_ledgers_panic_leeway * PAGE_SIZE)))) { \
	                do_panic = TRUE;                                \
	        }                                                       \
	        printf("LEDGER BALANCE proc %d (%s) "                   \
	               "\"%s\" = %lld\n",                               \
	               pid, procname, #__LEDGER, bal);                  \
	        if (bal > 0) {                                          \
	                pmap_ledgers_drift.__LEDGER##_over++;           \
	                pmap_ledgers_drift.__LEDGER##_over_total += bal; \
	                if (bal > pmap_ledgers_drift.__LEDGER##_over_max) { \
	                        pmap_ledgers_drift.__LEDGER##_over_max = bal; \
	                }                                               \
	        } else if (bal < 0) {                                   \
	                pmap_ledgers_drift.__LEDGER##_under++;          \
	                pmap_ledgers_drift.__LEDGER##_under_total += bal; \
	                if (bal < pmap_ledgers_drift.__LEDGER##_under_max) { \
	                        pmap_ledgers_drift.__LEDGER##_under_max = bal; \
	                }                                               \
	        }                                                       \
	}                                                               \
MACRO_END

	LEDGER_CHECK_BALANCE(phys_footprint);
	LEDGER_CHECK_BALANCE(internal);
	LEDGER_CHECK_BALANCE(internal_compressed);
	LEDGER_CHECK_BALANCE(external);
	LEDGER_CHECK_BALANCE(reusable);
	LEDGER_CHECK_BALANCE(iokit_mapped);
	LEDGER_CHECK_BALANCE(alternate_accounting);
	LEDGER_CHECK_BALANCE(alternate_accounting_compressed);
	LEDGER_CHECK_BALANCE(page_table);
	LEDGER_CHECK_BALANCE(purgeable_volatile);
	LEDGER_CHECK_BALANCE(purgeable_nonvolatile);
	LEDGER_CHECK_BALANCE(purgeable_volatile_compressed);
	LEDGER_CHECK_BALANCE(purgeable_nonvolatile_compressed);
	LEDGER_CHECK_BALANCE(tagged_nofootprint);
	LEDGER_CHECK_BALANCE(tagged_footprint);
	LEDGER_CHECK_BALANCE(tagged_nofootprint_compressed);
	LEDGER_CHECK_BALANCE(tagged_footprint_compressed);
	LEDGER_CHECK_BALANCE(network_volatile);
	LEDGER_CHECK_BALANCE(network_nonvolatile);
	LEDGER_CHECK_BALANCE(network_volatile_compressed);
	LEDGER_CHECK_BALANCE(network_nonvolatile_compressed);
	LEDGER_CHECK_BALANCE(media_nofootprint);
	LEDGER_CHECK_BALANCE(media_footprint);
	LEDGER_CHECK_BALANCE(media_nofootprint_compressed);
	LEDGER_CHECK_BALANCE(media_footprint_compressed);
	LEDGER_CHECK_BALANCE(graphics_nofootprint);
	LEDGER_CHECK_BALANCE(graphics_footprint);
	LEDGER_CHECK_BALANCE(graphics_nofootprint_compressed);
	LEDGER_CHECK_BALANCE(graphics_footprint_compressed);
	LEDGER_CHECK_BALANCE(neural_nofootprint);
	LEDGER_CHECK_BALANCE(neural_footprint);
	LEDGER_CHECK_BALANCE(neural_nofootprint_compressed);
	LEDGER_CHECK_BALANCE(neural_footprint_compressed);

	if (do_panic) {
		if (pmap_ledgers_panic) {
			panic("pmap_destroy(%p) %d[%s] has imbalanced ledgers",
			    pmap, pid, procname);
		} else {
			printf("pmap_destroy(%p) %d[%s] has imbalanced ledgers\n",
			    pmap, pid, procname);
		}
	}
}

void
vm_map_pmap_set_process(
	vm_map_t map,
	int pid,
	char *procname)
{
	pmap_set_process(vm_map_pmap(map), pid, procname);
}

#endif /* MACH_ASSERT */
