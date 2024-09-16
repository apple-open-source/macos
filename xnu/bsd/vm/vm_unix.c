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
 * Mach Operating System
 * Copyright (c) 1987 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/*
 * NOTICE: This file was modified by SPARTA, Inc. in 2006 to introduce
 * support for mandatory and extensible security protections.  This notice
 * is included in support of clause 2.2 (b) of the Apple Public License,
 * Version 2.0.
 */
#include <vm/vm_options.h>

#include <kern/ecc.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/debug.h>
#include <kern/extmod_statistics.h>
#include <mach/mach_traps.h>
#include <mach/port.h>
#include <mach/sdt.h>
#include <mach/task.h>
#include <mach/task_access.h>
#include <mach/task_special_ports.h>
#include <mach/time_value.h>
#include <mach/vm_map.h>
#include <mach/vm_param.h>
#include <mach/vm_prot.h>
#include <machine/machine_routines.h>

#include <sys/file_internal.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/dir.h>
#include <sys/namei.h>
#include <sys/proc_internal.h>
#include <sys/kauth.h>
#include <sys/vm.h>
#include <sys/file.h>
#include <sys/vnode_internal.h>
#include <sys/mount.h>
#include <sys/xattr.h>
#include <sys/trace.h>
#include <sys/kernel.h>
#include <sys/ubc_internal.h>
#include <sys/user.h>
#include <sys/syslog.h>
#include <sys/stat.h>
#include <sys/sysproto.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <sys/cprotect.h>
#include <sys/kpi_socket.h>
#include <sys/kas_info.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/random.h>
#include <sys/code_signing.h>
#if NECP
#include <net/necp.h>
#endif /* NECP */
#if SKYWALK
#include <skywalk/os_channel.h>
#endif /* SKYWALK */

#include <security/audit/audit.h>
#include <security/mac.h>
#include <bsm/audit_kevents.h>

#include <kern/kalloc.h>
#include <vm/vm_map_internal.h>
#include <vm/vm_kern_xnu.h>
#include <vm/vm_pageout_xnu.h>

#include <mach/shared_region.h>
#include <vm/vm_shared_region_internal.h>

#include <vm/vm_dyld_pager_internal.h>
#include <vm/vm_protos_internal.h>
#if DEVELOPMENT || DEBUG
#include <vm/vm_compressor_info.h>         /* for c_segment_info */
#include <vm/vm_compressor_xnu.h>          /* for vm_compressor_serialize_segment_debug_info() */
#endif
#include <vm/vm_reclaim_xnu.h>

#include <sys/kern_memorystatus.h>
#include <sys/kern_memorystatus_freeze.h>
#include <sys/proc_internal.h>

#include <mach-o/fixup-chains.h>

#if CONFIG_MACF
#include <security/mac_framework.h>
#endif

#include <kern/bits.h>

#if CONFIG_CSR
#include <sys/csr.h>
#endif /* CONFIG_CSR */
#include <sys/trust_caches.h>
#include <libkern/amfi/amfi.h>
#include <IOKit/IOBSD.h>

#if VM_MAP_DEBUG_APPLE_PROTECT
SYSCTL_INT(_vm, OID_AUTO, map_debug_apple_protect, CTLFLAG_RW | CTLFLAG_LOCKED, &vm_map_debug_apple_protect, 0, "");
#endif /* VM_MAP_DEBUG_APPLE_PROTECT */

#if DEVELOPMENT || DEBUG

static int
sysctl_kmem_alloc_contig SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2)
	vm_offset_t     kaddr;
	kern_return_t   kr;
	int     error = 0;
	int     size = 0;

	error = sysctl_handle_int(oidp, &size, 0, req);
	if (error || !req->newptr) {
		return error;
	}

	kr = kmem_alloc_contig(kernel_map, &kaddr, (vm_size_t)size,
	    0, 0, 0, KMA_DATA, VM_KERN_MEMORY_IOKIT);

	if (kr == KERN_SUCCESS) {
		kmem_free(kernel_map, kaddr, size);
	}

	return error;
}

SYSCTL_PROC(_vm, OID_AUTO, kmem_alloc_contig, CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_LOCKED | CTLFLAG_MASKED,
    0, 0, &sysctl_kmem_alloc_contig, "I", "");

extern int vm_region_footprint;
SYSCTL_INT(_vm, OID_AUTO, region_footprint, CTLFLAG_RW | CTLFLAG_ANYBODY | CTLFLAG_LOCKED, &vm_region_footprint, 0, "");

static int
sysctl_kmem_gobj_stats SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2, oidp)
	kmem_gobj_stats stats = kmem_get_gobj_stats();

	return SYSCTL_OUT(req, &stats, sizeof(stats));
}

SYSCTL_PROC(_vm, OID_AUTO, kmem_gobj_stats,
    CTLTYPE_STRUCT | CTLFLAG_RD | CTLFLAG_LOCKED | CTLFLAG_MASKED,
    0, 0, &sysctl_kmem_gobj_stats, "S,kmem_gobj_stats", "");

#endif /* DEVELOPMENT || DEBUG */

static int
sysctl_vm_self_region_footprint SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2, oidp)
	int     error = 0;
	int     value;

	value = task_self_region_footprint();
	error = SYSCTL_OUT(req, &value, sizeof(int));
	if (error) {
		return error;
	}

	if (!req->newptr) {
		return 0;
	}

	error = SYSCTL_IN(req, &value, sizeof(int));
	if (error) {
		return error;
	}
	task_self_region_footprint_set(value);
	return 0;
}
SYSCTL_PROC(_vm, OID_AUTO, self_region_footprint, CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_ANYBODY | CTLFLAG_LOCKED | CTLFLAG_MASKED, 0, 0, &sysctl_vm_self_region_footprint, "I", "");

static int
sysctl_vm_self_region_page_size SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2, oidp)
	int     error = 0;
	int     value;

	value = (1 << thread_self_region_page_shift());
	error = SYSCTL_OUT(req, &value, sizeof(int));
	if (error) {
		return error;
	}

	if (!req->newptr) {
		return 0;
	}

	error = SYSCTL_IN(req, &value, sizeof(int));
	if (error) {
		return error;
	}

	if (value != 0 && value != 4096 && value != 16384) {
		return EINVAL;
	}

#if !__ARM_MIXED_PAGE_SIZE__
	if (value != vm_map_page_size(current_map())) {
		return EINVAL;
	}
#endif /* !__ARM_MIXED_PAGE_SIZE__ */

	thread_self_region_page_shift_set(bit_first(value));
	return 0;
}
SYSCTL_PROC(_vm, OID_AUTO, self_region_page_size, CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_ANYBODY | CTLFLAG_LOCKED | CTLFLAG_MASKED, 0, 0, &sysctl_vm_self_region_page_size, "I", "");

static int
sysctl_vm_self_region_info_flags SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2, oidp)
	int     error = 0;
	int     value;
	kern_return_t kr;

	value = task_self_region_info_flags();
	error = SYSCTL_OUT(req, &value, sizeof(int));
	if (error) {
		return error;
	}

	if (!req->newptr) {
		return 0;
	}

	error = SYSCTL_IN(req, &value, sizeof(int));
	if (error) {
		return error;
	}

	kr = task_self_region_info_flags_set(value);
	if (kr != KERN_SUCCESS) {
		return EINVAL;
	}

	return 0;
}
SYSCTL_PROC(_vm, OID_AUTO, self_region_info_flags, CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_ANYBODY | CTLFLAG_LOCKED | CTLFLAG_MASKED, 0, 0, &sysctl_vm_self_region_info_flags, "I", "");


#if DEVELOPMENT || DEBUG
extern int panic_on_unsigned_execute;
SYSCTL_INT(_vm, OID_AUTO, panic_on_unsigned_execute, CTLFLAG_RW | CTLFLAG_LOCKED, &panic_on_unsigned_execute, 0, "");

extern int vm_log_xnu_user_debug;
SYSCTL_INT(_vm, OID_AUTO, log_xnu_user_debug, CTLFLAG_RW | CTLFLAG_LOCKED, &vm_log_xnu_user_debug, 0, "");
#endif /* DEVELOPMENT || DEBUG */

extern int cs_executable_create_upl;
extern int cs_executable_wire;
SYSCTL_INT(_vm, OID_AUTO, cs_executable_create_upl, CTLFLAG_RD | CTLFLAG_LOCKED, &cs_executable_create_upl, 0, "");
SYSCTL_INT(_vm, OID_AUTO, cs_executable_wire, CTLFLAG_RD | CTLFLAG_LOCKED, &cs_executable_wire, 0, "");

extern int apple_protect_pager_count;
extern int apple_protect_pager_count_mapped;
extern unsigned int apple_protect_pager_cache_limit;
SYSCTL_INT(_vm, OID_AUTO, apple_protect_pager_count, CTLFLAG_RD | CTLFLAG_LOCKED, &apple_protect_pager_count, 0, "");
SYSCTL_INT(_vm, OID_AUTO, apple_protect_pager_count_mapped, CTLFLAG_RD | CTLFLAG_LOCKED, &apple_protect_pager_count_mapped, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, apple_protect_pager_cache_limit, CTLFLAG_RW | CTLFLAG_LOCKED, &apple_protect_pager_cache_limit, 0, "");

#if DEVELOPMENT || DEBUG
extern int radar_20146450;
SYSCTL_INT(_vm, OID_AUTO, radar_20146450, CTLFLAG_RW | CTLFLAG_LOCKED, &radar_20146450, 0, "");

extern int macho_printf;
SYSCTL_INT(_vm, OID_AUTO, macho_printf, CTLFLAG_RW | CTLFLAG_LOCKED, &macho_printf, 0, "");

extern int apple_protect_pager_data_request_debug;
SYSCTL_INT(_vm, OID_AUTO, apple_protect_pager_data_request_debug, CTLFLAG_RW | CTLFLAG_LOCKED, &apple_protect_pager_data_request_debug, 0, "");

#if __arm64__
/* These are meant to support the page table accounting unit test. */
extern unsigned int arm_hardware_page_size;
extern unsigned int arm_pt_desc_size;
extern unsigned int arm_pt_root_size;
extern unsigned int inuse_user_tteroot_count;
extern unsigned int inuse_kernel_tteroot_count;
extern unsigned int inuse_user_ttepages_count;
extern unsigned int inuse_kernel_ttepages_count;
extern unsigned int inuse_user_ptepages_count;
extern unsigned int inuse_kernel_ptepages_count;
SYSCTL_UINT(_vm, OID_AUTO, native_hw_pagesize, CTLFLAG_RD | CTLFLAG_LOCKED, &arm_hardware_page_size, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, arm_pt_desc_size, CTLFLAG_RD | CTLFLAG_LOCKED, &arm_pt_desc_size, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, arm_pt_root_size, CTLFLAG_RD | CTLFLAG_LOCKED, &arm_pt_root_size, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, user_tte_root, CTLFLAG_RD | CTLFLAG_LOCKED, &inuse_user_tteroot_count, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, kernel_tte_root, CTLFLAG_RD | CTLFLAG_LOCKED, &inuse_kernel_tteroot_count, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, user_tte_pages, CTLFLAG_RD | CTLFLAG_LOCKED, &inuse_user_ttepages_count, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, kernel_tte_pages, CTLFLAG_RD | CTLFLAG_LOCKED, &inuse_kernel_ttepages_count, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, user_pte_pages, CTLFLAG_RD | CTLFLAG_LOCKED, &inuse_user_ptepages_count, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, kernel_pte_pages, CTLFLAG_RD | CTLFLAG_LOCKED, &inuse_kernel_ptepages_count, 0, "");
#if !CONFIG_SPTM
extern unsigned int free_page_size_tt_count;
extern unsigned int free_tt_count;
SYSCTL_UINT(_vm, OID_AUTO, free_1page_tte_root, CTLFLAG_RD | CTLFLAG_LOCKED, &free_page_size_tt_count, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, free_tte_root, CTLFLAG_RD | CTLFLAG_LOCKED, &free_tt_count, 0, "");
#endif
#if DEVELOPMENT || DEBUG
extern unsigned long pmap_asid_flushes;
SYSCTL_ULONG(_vm, OID_AUTO, pmap_asid_flushes, CTLFLAG_RD | CTLFLAG_LOCKED, &pmap_asid_flushes, "");
extern unsigned long pmap_asid_hits;
SYSCTL_ULONG(_vm, OID_AUTO, pmap_asid_hits, CTLFLAG_RD | CTLFLAG_LOCKED, &pmap_asid_hits, "");
extern unsigned long pmap_asid_misses;
SYSCTL_ULONG(_vm, OID_AUTO, pmap_asid_misses, CTLFLAG_RD | CTLFLAG_LOCKED, &pmap_asid_misses, "");
extern unsigned long pmap_speculation_restrictions;
SYSCTL_ULONG(_vm, OID_AUTO, pmap_speculation_restrictions, CTLFLAG_RD | CTLFLAG_LOCKED, &pmap_speculation_restrictions, "");
#endif
#endif /* __arm64__ */
#endif /* DEVELOPMENT || DEBUG */

SYSCTL_INT(_vm, OID_AUTO, vm_do_collapse_compressor, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_counters.do_collapse_compressor, 0, "");
SYSCTL_INT(_vm, OID_AUTO, vm_do_collapse_compressor_pages, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_counters.do_collapse_compressor_pages, 0, "");
SYSCTL_INT(_vm, OID_AUTO, vm_do_collapse_terminate, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_counters.do_collapse_terminate, 0, "");
SYSCTL_INT(_vm, OID_AUTO, vm_do_collapse_terminate_failure, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_counters.do_collapse_terminate_failure, 0, "");
SYSCTL_INT(_vm, OID_AUTO, vm_should_cow_but_wired, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_counters.should_cow_but_wired, 0, "");
SYSCTL_INT(_vm, OID_AUTO, vm_create_upl_extra_cow, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_counters.create_upl_extra_cow, 0, "");
SYSCTL_INT(_vm, OID_AUTO, vm_create_upl_extra_cow_pages, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_counters.create_upl_extra_cow_pages, 0, "");
SYSCTL_INT(_vm, OID_AUTO, vm_create_upl_lookup_failure_write, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_counters.create_upl_lookup_failure_write, 0, "");
SYSCTL_INT(_vm, OID_AUTO, vm_create_upl_lookup_failure_copy, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_counters.create_upl_lookup_failure_copy, 0, "");
#if VM_SCAN_FOR_SHADOW_CHAIN
static int vm_shadow_max_enabled = 0;    /* Disabled by default */
extern int proc_shadow_max(void);
static int
vm_shadow_max SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2, oidp)
	int value = 0;

	if (vm_shadow_max_enabled) {
		value = proc_shadow_max();
	}

	return SYSCTL_OUT(req, &value, sizeof(value));
}
SYSCTL_PROC(_vm, OID_AUTO, vm_shadow_max, CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_LOCKED,
    0, 0, &vm_shadow_max, "I", "");

SYSCTL_INT(_vm, OID_AUTO, vm_shadow_max_enabled, CTLFLAG_RW | CTLFLAG_LOCKED, &vm_shadow_max_enabled, 0, "");

#endif /* VM_SCAN_FOR_SHADOW_CHAIN */

SYSCTL_INT(_vm, OID_AUTO, vm_debug_events, CTLFLAG_RW | CTLFLAG_LOCKED, &vm_debug_events, 0, "");

/*
 * Sysctl's related to data/stack execution.  See osfmk/vm/vm_map.c
 */

#if DEVELOPMENT || DEBUG
extern int allow_stack_exec, allow_data_exec;

SYSCTL_INT(_vm, OID_AUTO, allow_stack_exec, CTLFLAG_RW | CTLFLAG_LOCKED, &allow_stack_exec, 0, "");
SYSCTL_INT(_vm, OID_AUTO, allow_data_exec, CTLFLAG_RW | CTLFLAG_LOCKED, &allow_data_exec, 0, "");

#endif /* DEVELOPMENT || DEBUG */

static const char *prot_values[] = {
	"none",
	"read-only",
	"write-only",
	"read-write",
	"execute-only",
	"read-execute",
	"write-execute",
	"read-write-execute"
};

void
log_stack_execution_failure(addr64_t vaddr, vm_prot_t prot)
{
	printf("Data/Stack execution not permitted: %s[pid %d] at virtual address 0x%qx, protections were %s\n",
	    current_proc()->p_comm, proc_getpid(current_proc()), vaddr, prot_values[prot & VM_PROT_ALL]);
}

/*
 * shared_region_unnest_logging: level of logging of unnesting events
 * 0	- no logging
 * 1	- throttled logging of unexpected unnesting events (default)
 * 2	- unthrottled logging of unexpected unnesting events
 * 3+	- unthrottled logging of all unnesting events
 */
int shared_region_unnest_logging = 1;

SYSCTL_INT(_vm, OID_AUTO, shared_region_unnest_logging, CTLFLAG_RW | CTLFLAG_LOCKED,
    &shared_region_unnest_logging, 0, "");

int vm_shared_region_unnest_log_interval = 10;
int shared_region_unnest_log_count_threshold = 5;


#if XNU_TARGET_OS_OSX

#if defined (__x86_64__)
static int scdir_enforce = 1;
#else /* defined (__x86_64__) */
static int scdir_enforce = 0;   /* AOT caches live elsewhere */
#endif /* defined (__x86_64__) */

static char *scdir_path[] = {
	"/System/Library/dyld/",
	"/System/Volumes/Preboot/Cryptexes/OS/System/Library/dyld",
	"/System/Cryptexes/OS/System/Library/dyld",
	NULL
};

#else /* XNU_TARGET_OS_OSX */

static int scdir_enforce = 0;
static char *scdir_path[] = {
	"/System/Library/Caches/com.apple.dyld/",
	"/private/preboot/Cryptexes/OS/System/Library/Caches/com.apple.dyld",
	"/System/Cryptexes/OS/System/Library/Caches/com.apple.dyld",
	NULL
};

#endif /* XNU_TARGET_OS_OSX */

static char *driverkit_scdir_path[] = {
	"/System/DriverKit/System/Library/dyld/",
#if XNU_TARGET_OS_OSX
	"/System/Volumes/Preboot/Cryptexes/OS/System/DriverKit/System/Library/dyld",
#else
	"/private/preboot/Cryptexes/OS/System/DriverKit/System/Library/dyld",
#endif /* XNU_TARGET_OS_OSX */
	"/System/Cryptexes/OS/System/DriverKit/System/Library/dyld",
	NULL
};

#ifndef SECURE_KERNEL
static int sysctl_scdir_enforce SYSCTL_HANDLER_ARGS
{
#if CONFIG_CSR
	if (csr_check(CSR_ALLOW_UNRESTRICTED_FS) != 0) {
		printf("Failed attempt to set vm.enforce_shared_cache_dir sysctl\n");
		return EPERM;
	}
#endif /* CONFIG_CSR */
	return sysctl_handle_int(oidp, arg1, arg2, req);
}

SYSCTL_PROC(_vm, OID_AUTO, enforce_shared_cache_dir, CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED, &scdir_enforce, 0, sysctl_scdir_enforce, "I", "");
#endif

/* These log rate throttling state variables aren't thread safe, but
 * are sufficient unto the task.
 */
static int64_t last_unnest_log_time = 0;
static int shared_region_unnest_log_count = 0;

void
log_unnest_badness(
	vm_map_t        m,
	vm_map_offset_t s,
	vm_map_offset_t e,
	boolean_t       is_nested_map,
	vm_map_offset_t lowest_unnestable_addr)
{
	struct timeval  tv;

	if (shared_region_unnest_logging == 0) {
		return;
	}

	if (shared_region_unnest_logging <= 2 &&
	    is_nested_map &&
	    s >= lowest_unnestable_addr) {
		/*
		 * Unnesting of writable map entries is fine.
		 */
		return;
	}

	if (shared_region_unnest_logging <= 1) {
		microtime(&tv);
		if ((tv.tv_sec - last_unnest_log_time) <
		    vm_shared_region_unnest_log_interval) {
			if (shared_region_unnest_log_count++ >
			    shared_region_unnest_log_count_threshold) {
				return;
			}
		} else {
			last_unnest_log_time = tv.tv_sec;
			shared_region_unnest_log_count = 0;
		}
	}

	DTRACE_VM4(log_unnest_badness,
	    vm_map_t, m,
	    vm_map_offset_t, s,
	    vm_map_offset_t, e,
	    vm_map_offset_t, lowest_unnestable_addr);
	printf("%s[%d] triggered unnest of range 0x%qx->0x%qx of DYLD shared region in VM map %p. While not abnormal for debuggers, this increases system memory footprint until the target exits.\n", current_proc()->p_comm, proc_getpid(current_proc()), (uint64_t)s, (uint64_t)e, (void *) VM_KERNEL_ADDRPERM(m));
}

uint64_t
vm_purge_filebacked_pagers(void)
{
	uint64_t pages_purged;

	pages_purged = 0;
	pages_purged += apple_protect_pager_purge_all();
	pages_purged += shared_region_pager_purge_all();
	pages_purged += dyld_pager_purge_all();
#if DEVELOPMENT || DEBUG
	printf("%s:%d pages purged: %llu\n", __FUNCTION__, __LINE__, pages_purged);
#endif /* DEVELOPMENT || DEBUG */
	return pages_purged;
}

int
useracc(
	user_addr_t     addr,
	user_size_t     len,
	int     prot)
{
	vm_map_t        map;

	map = current_map();
	return vm_map_check_protection(
		map,
		vm_map_trunc_page(addr,
		vm_map_page_mask(map)),
		vm_map_round_page(addr + len,
		vm_map_page_mask(map)),
		prot == B_READ ? VM_PROT_READ : VM_PROT_WRITE);
}

#if XNU_PLATFORM_MacOSX
static inline kern_return_t
vslock_sanitize(
	vm_map_t                map,
	user_addr_ut            addr_u,
	user_size_ut            len_u,
	vm_sanitize_caller_t    vm_sanitize_caller,
	vm_map_offset_t        *start,
	vm_map_offset_t        *end,
	vm_map_size_t          *size)
{
	return vm_sanitize_addr_size(addr_u, len_u, vm_sanitize_caller,
	           map,
	           VM_SANITIZE_FLAGS_SIZE_ZERO_SUCCEEDS, start, end,
	           size);
}
#endif /* XNU_PLATFORM_MacOSX */

int
vslock(user_addr_ut addr, user_size_ut len)
{
	kern_return_t kret;

#if XNU_PLATFORM_MacOSX
	/*
	 * Preserve previous behavior on macOS for overflows due to bin
	 * compatibility i.e. return success for overflows without doing
	 * anything. Error compatibility returns VM_ERR_RETURN_NOW (on macOS)
	 * for overflow errors which gets converted to KERN_SUCCESS by
	 * vm_sanitize_get_kr.
	 */
	vm_map_offset_t start, end;
	vm_map_size_t   size;

	kret = vslock_sanitize(current_map(),
	    addr,
	    len,
	    VM_SANITIZE_CALLER_VSLOCK,
	    &start,
	    &end,
	    &size);
	if (__improbable(kret != KERN_SUCCESS)) {
		switch (vm_sanitize_get_kr(kret)) {
		case KERN_SUCCESS:
			return 0;
		case KERN_INVALID_ADDRESS:
		case KERN_NO_SPACE:
			return ENOMEM;
		case KERN_PROTECTION_FAILURE:
			return EACCES;
		default:
			return EINVAL;
		}
	}
#endif /* XNU_PLATFORM_MacOSX */

	kret = vm_map_wire_kernel(current_map(), addr,
	    vm_sanitize_compute_unsafe_end(addr, len),
	    vm_sanitize_wrap_prot(VM_PROT_READ | VM_PROT_WRITE),
	    VM_KERN_MEMORY_BSD,
	    FALSE);

	switch (kret) {
	case KERN_SUCCESS:
		return 0;
	case KERN_INVALID_ADDRESS:
	case KERN_NO_SPACE:
		return ENOMEM;
	case KERN_PROTECTION_FAILURE:
		return EACCES;
	default:
		return EINVAL;
	}
}

int
vsunlock(user_addr_ut addr, user_size_ut len, __unused int dirtied)
{
#if FIXME  /* [ */
	pmap_t          pmap;
	vm_page_t       pg;
	vm_map_offset_t vaddr;
	ppnum_t         paddr;
#endif  /* FIXME ] */
	kern_return_t   kret;
	vm_map_t        map;

	map = current_map();

#if FIXME  /* [ */
	if (dirtied) {
		pmap = get_task_pmap(current_task());
		for (vaddr = vm_map_trunc_page(addr, PAGE_MASK);
		    vaddr < vm_map_round_page(addr + len, PAGE_MASK);
		    vaddr += PAGE_SIZE) {
			paddr = pmap_find_phys(pmap, vaddr);
			pg = PHYS_TO_VM_PAGE(paddr);
			vm_page_set_modified(pg);
		}
	}
#endif  /* FIXME ] */
#ifdef  lint
	dirtied++;
#endif  /* lint */

#if XNU_PLATFORM_MacOSX
	/*
	 * Preserve previous behavior on macOS for overflows due to bin
	 * compatibility i.e. return success for overflows without doing
	 * anything. Error compatibility returns VM_ERR_RETURN_NOW (on macOS)
	 * for overflow errors which gets converted to KERN_SUCCESS by
	 * vm_sanitize_get_kr.
	 */
	vm_map_offset_t start, end;
	vm_map_size_t   size;

	kret = vslock_sanitize(map,
	    addr,
	    len,
	    VM_SANITIZE_CALLER_VSUNLOCK,
	    &start,
	    &end,
	    &size);
	if (__improbable(kret != KERN_SUCCESS)) {
		switch (vm_sanitize_get_kr(kret)) {
		case KERN_SUCCESS:
			return 0;
		case KERN_INVALID_ADDRESS:
		case KERN_NO_SPACE:
			return ENOMEM;
		case KERN_PROTECTION_FAILURE:
			return EACCES;
		default:
			return EINVAL;
		}
	}
#endif /* XNU_PLATFORM_MacOSX */

	kret = vm_map_unwire(map, addr,
	    vm_sanitize_compute_unsafe_end(addr, len), false);
	switch (kret) {
	case KERN_SUCCESS:
		return 0;
	case KERN_INVALID_ADDRESS:
	case KERN_NO_SPACE:
		return ENOMEM;
	case KERN_PROTECTION_FAILURE:
		return EACCES;
	default:
		return EINVAL;
	}
}

int
subyte(
	user_addr_t addr,
	int byte)
{
	char character;

	character = (char)byte;
	return copyout((void *)&(character), addr, sizeof(char)) == 0 ? 0 : -1;
}

int
suibyte(
	user_addr_t addr,
	int byte)
{
	char character;

	character = (char)byte;
	return copyout((void *)&(character), addr, sizeof(char)) == 0 ? 0 : -1;
}

int
fubyte(user_addr_t addr)
{
	unsigned char byte;

	if (copyin(addr, (void *) &byte, sizeof(char))) {
		return -1;
	}
	return byte;
}

int
fuibyte(user_addr_t addr)
{
	unsigned char byte;

	if (copyin(addr, (void *) &(byte), sizeof(char))) {
		return -1;
	}
	return byte;
}

int
suword(
	user_addr_t addr,
	long word)
{
	return copyout((void *) &word, addr, sizeof(int)) == 0 ? 0 : -1;
}

long
fuword(user_addr_t addr)
{
	long word = 0;

	if (copyin(addr, (void *) &word, sizeof(int))) {
		return -1;
	}
	return word;
}

/* suiword and fuiword are the same as suword and fuword, respectively */

int
suiword(
	user_addr_t addr,
	long word)
{
	return copyout((void *) &word, addr, sizeof(int)) == 0 ? 0 : -1;
}

long
fuiword(user_addr_t addr)
{
	long word = 0;

	if (copyin(addr, (void *) &word, sizeof(int))) {
		return -1;
	}
	return word;
}

/*
 * With a 32-bit kernel and mixed 32/64-bit user tasks, this interface allows the
 * fetching and setting of process-sized size_t and pointer values.
 */
int
sulong(user_addr_t addr, int64_t word)
{
	if (IS_64BIT_PROCESS(current_proc())) {
		return copyout((void *)&word, addr, sizeof(word)) == 0 ? 0 : -1;
	} else {
		return suiword(addr, (long)word);
	}
}

int64_t
fulong(user_addr_t addr)
{
	int64_t longword;

	if (IS_64BIT_PROCESS(current_proc())) {
		if (copyin(addr, (void *)&longword, sizeof(longword)) != 0) {
			return -1;
		}
		return longword;
	} else {
		return (int64_t)fuiword(addr);
	}
}

int
suulong(user_addr_t addr, uint64_t uword)
{
	if (IS_64BIT_PROCESS(current_proc())) {
		return copyout((void *)&uword, addr, sizeof(uword)) == 0 ? 0 : -1;
	} else {
		return suiword(addr, (uint32_t)uword);
	}
}

uint64_t
fuulong(user_addr_t addr)
{
	uint64_t ulongword;

	if (IS_64BIT_PROCESS(current_proc())) {
		if (copyin(addr, (void *)&ulongword, sizeof(ulongword)) != 0) {
			return -1ULL;
		}
		return ulongword;
	} else {
		return (uint64_t)fuiword(addr);
	}
}

int
swapon(__unused proc_t procp, __unused struct swapon_args *uap, __unused int *retval)
{
	return ENOTSUP;
}

#if defined(SECURE_KERNEL)
static int kern_secure_kernel = 1;
#else
static int kern_secure_kernel = 0;
#endif

SYSCTL_INT(_kern, OID_AUTO, secure_kernel, CTLFLAG_RD | CTLFLAG_LOCKED, &kern_secure_kernel, 0, "");
SYSCTL_INT(_vm, OID_AUTO, shared_region_trace_level, CTLFLAG_RW | CTLFLAG_LOCKED,
    &shared_region_trace_level, 0, "");
SYSCTL_INT(_vm, OID_AUTO, shared_region_version, CTLFLAG_RD | CTLFLAG_LOCKED,
    &shared_region_version, 0, "");
SYSCTL_INT(_vm, OID_AUTO, shared_region_persistence, CTLFLAG_RW | CTLFLAG_LOCKED,
    &shared_region_persistence, 0, "");

/*
 * shared_region_check_np:
 *
 * This system call is intended for dyld.
 *
 * dyld calls this when any process starts to see if the process's shared
 * region is already set up and ready to use.
 * This call returns the base address of the first mapping in the
 * process's shared region's first mapping.
 * dyld will then check what's mapped at that address.
 *
 * If the shared region is empty, dyld will then attempt to map the shared
 * cache file in the shared region via the shared_region_map_np() system call.
 *
 * If something's already mapped in the shared region, dyld will check if it
 * matches the shared cache it would like to use for that process.
 * If it matches, evrything's ready and the process can proceed and use the
 * shared region.
 * If it doesn't match, dyld will unmap the shared region and map the shared
 * cache into the process's address space via mmap().
 *
 * A NULL pointer argument can be used by dyld to indicate it has unmapped
 * the shared region. We will remove the shared_region reference from the task.
 *
 * ERROR VALUES
 * EINVAL	no shared region
 * ENOMEM	shared region is empty
 * EFAULT	bad address for "start_address"
 */
int
shared_region_check_np(
	__unused struct proc                    *p,
	struct shared_region_check_np_args      *uap,
	__unused int                            *retvalp)
{
	vm_shared_region_t      shared_region;
	mach_vm_offset_t        start_address = 0;
	int                     error = 0;
	kern_return_t           kr;
	task_t                  task = current_task();

	SHARED_REGION_TRACE_DEBUG(
		("shared_region: %p [%d(%s)] -> check_np(0x%llx)\n",
		(void *)VM_KERNEL_ADDRPERM(current_thread()),
		proc_getpid(p), p->p_comm,
		(uint64_t)uap->start_address));

	/*
	 * Special value of start_address used to indicate that map_with_linking() should
	 * no longer be allowed in this process
	 */
	if (uap->start_address == (task_get_64bit_addr(task) ? DYLD_VM_END_MWL : (uint32_t)DYLD_VM_END_MWL)) {
		p->p_disallow_map_with_linking = TRUE;
		return 0;
	}

	/* retrieve the current tasks's shared region */
	shared_region = vm_shared_region_get(task);
	if (shared_region != NULL) {
		/*
		 * A NULL argument is used by dyld to indicate the task
		 * has unmapped its shared region.
		 */
		if (uap->start_address == 0) {
			/* unmap it first */
			vm_shared_region_remove(task, shared_region);
			vm_shared_region_set(task, NULL);
		} else {
			/* retrieve address of its first mapping... */
			kr = vm_shared_region_start_address(shared_region, &start_address, task);
			if (kr != KERN_SUCCESS) {
				SHARED_REGION_TRACE_ERROR(("shared_region: %p [%d(%s)] "
				    "check_np(0x%llx) "
				    "vm_shared_region_start_address() failed\n",
				    (void *)VM_KERNEL_ADDRPERM(current_thread()),
				    proc_getpid(p), p->p_comm,
				    (uint64_t)uap->start_address));
				error = ENOMEM;
			} else {
#if __has_feature(ptrauth_calls)
				/*
				 * Remap any section of the shared library that
				 * has authenticated pointers into private memory.
				 */
				if (vm_shared_region_auth_remap(shared_region) != KERN_SUCCESS) {
					SHARED_REGION_TRACE_ERROR(("shared_region: %p [%d(%s)] "
					    "check_np(0x%llx) "
					    "vm_shared_region_auth_remap() failed\n",
					    (void *)VM_KERNEL_ADDRPERM(current_thread()),
					    proc_getpid(p), p->p_comm,
					    (uint64_t)uap->start_address));
					error = ENOMEM;
				}
#endif /* __has_feature(ptrauth_calls) */

				/* ... and give it to the caller */
				if (error == 0) {
					error = copyout(&start_address,
					    (user_addr_t) uap->start_address,
					    sizeof(start_address));
					if (error != 0) {
						SHARED_REGION_TRACE_ERROR(
							("shared_region: %p [%d(%s)] "
							"check_np(0x%llx) "
							"copyout(0x%llx) error %d\n",
							(void *)VM_KERNEL_ADDRPERM(current_thread()),
							proc_getpid(p), p->p_comm,
							(uint64_t)uap->start_address, (uint64_t)start_address,
							error));
					}
				}
			}
		}
		vm_shared_region_deallocate(shared_region);
	} else {
		/* no shared region ! */
		error = EINVAL;
	}

	SHARED_REGION_TRACE_DEBUG(
		("shared_region: %p [%d(%s)] check_np(0x%llx) <- 0x%llx %d\n",
		(void *)VM_KERNEL_ADDRPERM(current_thread()),
		proc_getpid(p), p->p_comm,
		(uint64_t)uap->start_address, (uint64_t)start_address, error));

	return error;
}


static int
shared_region_copyin(
	struct proc  *p,
	user_addr_t  user_addr,
	unsigned int count,
	unsigned int element_size,
	void         *kernel_data)
{
	int             error = 0;
	vm_size_t       size = count * element_size;

	error = copyin(user_addr, kernel_data, size);
	if (error) {
		SHARED_REGION_TRACE_ERROR(
			("shared_region: %p [%d(%s)] map(): "
			"copyin(0x%llx, %ld) failed (error=%d)\n",
			(void *)VM_KERNEL_ADDRPERM(current_thread()),
			proc_getpid(p), p->p_comm,
			(uint64_t)user_addr, (long)size, error));
	}
	return error;
}

/*
 * A reasonable upper limit to prevent overflow of allocation/copyin.
 */
#define _SR_FILE_MAPPINGS_MAX_FILES 256

/* forward declaration */
__attribute__((noinline))
static void shared_region_map_and_slide_cleanup(
	struct proc              *p,
	uint32_t                 files_count,
	struct _sr_file_mappings *sr_file_mappings,
	struct vm_shared_region  *shared_region);

/*
 * Setup part of _shared_region_map_and_slide().
 * It had to be broken out of _shared_region_map_and_slide() to
 * prevent compiler inlining from blowing out the stack.
 */
__attribute__((noinline))
static int
shared_region_map_and_slide_setup(
	struct proc                         *p,
	uint32_t                            files_count,
	struct shared_file_np               *files,
	uint32_t                            mappings_count,
	struct shared_file_mapping_slide_np *mappings,
	struct _sr_file_mappings            **sr_file_mappings,
	struct vm_shared_region             **shared_region_ptr,
	struct vnode                        *rdir_vp)
{
	int                             error = 0;
	struct _sr_file_mappings        *srfmp;
	uint32_t                        mappings_next;
	struct vnode_attr               va;
	off_t                           fs;
#if CONFIG_MACF
	vm_prot_t                       maxprot = VM_PROT_ALL;
#endif
	uint32_t                        i;
	struct vm_shared_region         *shared_region = NULL;
	boolean_t                       is_driverkit = task_is_driver(current_task());

	SHARED_REGION_TRACE_DEBUG(
		("shared_region: %p [%d(%s)] -> map\n",
		(void *)VM_KERNEL_ADDRPERM(current_thread()),
		proc_getpid(p), p->p_comm));

	if (files_count > _SR_FILE_MAPPINGS_MAX_FILES) {
		error = E2BIG;
		goto done;
	}
	if (files_count == 0) {
		error = EINVAL;
		goto done;
	}
	*sr_file_mappings = kalloc_type(struct _sr_file_mappings, files_count,
	    Z_WAITOK | Z_ZERO);
	if (*sr_file_mappings == NULL) {
		error = ENOMEM;
		goto done;
	}
	mappings_next = 0;
	for (i = 0; i < files_count; i++) {
		srfmp = &(*sr_file_mappings)[i];
		srfmp->fd = files[i].sf_fd;
		srfmp->mappings_count = files[i].sf_mappings_count;
		srfmp->mappings = &mappings[mappings_next];
		mappings_next += srfmp->mappings_count;
		if (mappings_next > mappings_count) {
			error = EINVAL;
			goto done;
		}
		srfmp->slide = files[i].sf_slide;
	}

	/* get the process's shared region (setup in vm_map_exec()) */
	shared_region = vm_shared_region_trim_and_get(current_task());
	*shared_region_ptr = shared_region;
	if (shared_region == NULL) {
		SHARED_REGION_TRACE_ERROR(
			("shared_region: %p [%d(%s)] map(): "
			"no shared region\n",
			(void *)VM_KERNEL_ADDRPERM(current_thread()),
			proc_getpid(p), p->p_comm));
		error = EINVAL;
		goto done;
	}

	/*
	 * Check the shared region matches the current root
	 * directory of this process.  Deny the mapping to
	 * avoid tainting the shared region with something that
	 * doesn't quite belong into it.
	 */
	struct vnode *sr_vnode = vm_shared_region_root_dir(shared_region);
	if (sr_vnode != NULL ?  rdir_vp != sr_vnode : rdir_vp != rootvnode) {
		SHARED_REGION_TRACE_ERROR(
			("shared_region: map(%p) root_dir mismatch\n",
			(void *)VM_KERNEL_ADDRPERM(current_thread())));
		error = EPERM;
		goto done;
	}


	for (srfmp = &(*sr_file_mappings)[0];
	    srfmp < &(*sr_file_mappings)[files_count];
	    srfmp++) {
		if (srfmp->mappings_count == 0) {
			/* no mappings here... */
			continue;
		}

		/*
		 * A file descriptor of -1 is used to indicate that the data
		 * to be put in the shared region for this mapping comes directly
		 * from the processes address space. Ensure we have proper alignments.
		 */
		if (srfmp->fd == -1) {
			/* only allow one mapping per fd */
			if (srfmp->mappings_count > 1) {
				SHARED_REGION_TRACE_ERROR(
					("shared_region: %p [%d(%s)] map data >1 mapping\n",
					(void *)VM_KERNEL_ADDRPERM(current_thread()),
					proc_getpid(p), p->p_comm));
				error = EINVAL;
				goto done;
			}

			/*
			 * The destination address and size must be page aligned.
			 */
			struct shared_file_mapping_slide_np *mapping = &srfmp->mappings[0];
			mach_vm_address_t dest_addr = mapping->sms_address;
			mach_vm_size_t    map_size = mapping->sms_size;
			if (!vm_map_page_aligned(dest_addr, vm_map_page_mask(current_map()))) {
				SHARED_REGION_TRACE_ERROR(
					("shared_region: %p [%d(%s)] map data destination 0x%llx not aligned\n",
					(void *)VM_KERNEL_ADDRPERM(current_thread()),
					proc_getpid(p), p->p_comm, dest_addr));
				error = EINVAL;
				goto done;
			}
			if (!vm_map_page_aligned(map_size, vm_map_page_mask(current_map()))) {
				SHARED_REGION_TRACE_ERROR(
					("shared_region: %p [%d(%s)] map data size 0x%llx not aligned\n",
					(void *)VM_KERNEL_ADDRPERM(current_thread()),
					proc_getpid(p), p->p_comm, map_size));
				error = EINVAL;
				goto done;
			}
			continue;
		}

		/* get file structure from file descriptor */
		error = fp_get_ftype(p, srfmp->fd, DTYPE_VNODE, EINVAL, &srfmp->fp);
		if (error) {
			SHARED_REGION_TRACE_ERROR(
				("shared_region: %p [%d(%s)] map: "
				"fd=%d lookup failed (error=%d)\n",
				(void *)VM_KERNEL_ADDRPERM(current_thread()),
				proc_getpid(p), p->p_comm, srfmp->fd, error));
			goto done;
		}

		/* we need at least read permission on the file */
		if (!(srfmp->fp->fp_glob->fg_flag & FREAD)) {
			SHARED_REGION_TRACE_ERROR(
				("shared_region: %p [%d(%s)] map: "
				"fd=%d not readable\n",
				(void *)VM_KERNEL_ADDRPERM(current_thread()),
				proc_getpid(p), p->p_comm, srfmp->fd));
			error = EPERM;
			goto done;
		}

		/* get vnode from file structure */
		error = vnode_getwithref((vnode_t)fp_get_data(srfmp->fp));
		if (error) {
			SHARED_REGION_TRACE_ERROR(
				("shared_region: %p [%d(%s)] map: "
				"fd=%d getwithref failed (error=%d)\n",
				(void *)VM_KERNEL_ADDRPERM(current_thread()),
				proc_getpid(p), p->p_comm, srfmp->fd, error));
			goto done;
		}
		srfmp->vp = (struct vnode *)fp_get_data(srfmp->fp);

		/* make sure the vnode is a regular file */
		if (srfmp->vp->v_type != VREG) {
			SHARED_REGION_TRACE_ERROR(
				("shared_region: %p [%d(%s)] map(%p:'%s'): "
				"not a file (type=%d)\n",
				(void *)VM_KERNEL_ADDRPERM(current_thread()),
				proc_getpid(p), p->p_comm,
				(void *)VM_KERNEL_ADDRPERM(srfmp->vp),
				srfmp->vp->v_name, srfmp->vp->v_type));
			error = EINVAL;
			goto done;
		}

#if CONFIG_MACF
		/* pass in 0 for the offset argument because AMFI does not need the offset
		 *       of the shared cache */
		error = mac_file_check_mmap(vfs_context_ucred(vfs_context_current()),
		    srfmp->fp->fp_glob, VM_PROT_ALL, MAP_FILE | MAP_PRIVATE | MAP_FIXED, 0, &maxprot);
		if (error) {
			goto done;
		}
#endif /* MAC */

#if XNU_TARGET_OS_OSX && defined(__arm64__)
		/*
		 * Check if the shared cache is in the trust cache;
		 * if so, we can skip the root ownership check.
		 */
#if DEVELOPMENT || DEBUG
		/*
		 * Skip both root ownership and trust cache check if
		 * enforcement is disabled.
		 */
		if (!cs_system_enforcement()) {
			goto after_root_check;
		}
#endif /* DEVELOPMENT || DEBUG */
		struct cs_blob *blob = csvnode_get_blob(srfmp->vp, 0);
		if (blob == NULL) {
			SHARED_REGION_TRACE_ERROR(
				("shared_region: %p [%d(%s)] map(%p:'%s'): "
				"missing CS blob\n",
				(void *)VM_KERNEL_ADDRPERM(current_thread()),
				proc_getpid(p), p->p_comm,
				(void *)VM_KERNEL_ADDRPERM(srfmp->vp),
				srfmp->vp->v_name));
			goto root_check;
		}
		const uint8_t *cdhash = csblob_get_cdhash(blob);
		if (cdhash == NULL) {
			SHARED_REGION_TRACE_ERROR(
				("shared_region: %p [%d(%s)] map(%p:'%s'): "
				"missing cdhash\n",
				(void *)VM_KERNEL_ADDRPERM(current_thread()),
				proc_getpid(p), p->p_comm,
				(void *)VM_KERNEL_ADDRPERM(srfmp->vp),
				srfmp->vp->v_name));
			goto root_check;
		}

		bool in_trust_cache = false;
		TrustCacheQueryToken_t qt;
		if (query_trust_cache(kTCQueryTypeAll, cdhash, &qt) == KERN_SUCCESS) {
			TCType_t tc_type = kTCTypeInvalid;
			TCReturn_t tc_ret = amfi->TrustCache.queryGetTCType(&qt, &tc_type);
			in_trust_cache = (tc_ret.error == kTCReturnSuccess &&
			    (tc_type == kTCTypeCryptex1BootOS ||
			    tc_type == kTCTypeStatic ||
			    tc_type == kTCTypeEngineering));
		}
		if (!in_trust_cache) {
			SHARED_REGION_TRACE_ERROR(
				("shared_region: %p [%d(%s)] map(%p:'%s'): "
				"not in trust cache\n",
				(void *)VM_KERNEL_ADDRPERM(current_thread()),
				proc_getpid(p), p->p_comm,
				(void *)VM_KERNEL_ADDRPERM(srfmp->vp),
				srfmp->vp->v_name));
			goto root_check;
		}
		goto after_root_check;
root_check:
#endif /* XNU_TARGET_OS_OSX && defined(__arm64__) */

		/* The shared cache file must be owned by root */
		VATTR_INIT(&va);
		VATTR_WANTED(&va, va_uid);
		error = vnode_getattr(srfmp->vp, &va, vfs_context_current());
		if (error) {
			SHARED_REGION_TRACE_ERROR(
				("shared_region: %p [%d(%s)] map(%p:'%s'): "
				"vnode_getattr(%p) failed (error=%d)\n",
				(void *)VM_KERNEL_ADDRPERM(current_thread()),
				proc_getpid(p), p->p_comm,
				(void *)VM_KERNEL_ADDRPERM(srfmp->vp),
				srfmp->vp->v_name,
				(void *)VM_KERNEL_ADDRPERM(srfmp->vp),
				error));
			goto done;
		}
		if (va.va_uid != 0) {
			SHARED_REGION_TRACE_ERROR(
				("shared_region: %p [%d(%s)] map(%p:'%s'): "
				"owned by uid=%d instead of 0\n",
				(void *)VM_KERNEL_ADDRPERM(current_thread()),
				proc_getpid(p), p->p_comm,
				(void *)VM_KERNEL_ADDRPERM(srfmp->vp),
				srfmp->vp->v_name, va.va_uid));
			error = EPERM;
			goto done;
		}

#if XNU_TARGET_OS_OSX && defined(__arm64__)
after_root_check:
#endif /* XNU_TARGET_OS_OSX && defined(__arm64__) */

#if CONFIG_CSR
		if (csr_check(CSR_ALLOW_UNRESTRICTED_FS) != 0) {
			VATTR_INIT(&va);
			VATTR_WANTED(&va, va_flags);
			error = vnode_getattr(srfmp->vp, &va, vfs_context_current());
			if (error) {
				SHARED_REGION_TRACE_ERROR(
					("shared_region: %p [%d(%s)] map(%p:'%s'): "
					"vnode_getattr(%p) failed (error=%d)\n",
					(void *)VM_KERNEL_ADDRPERM(current_thread()),
					proc_getpid(p), p->p_comm,
					(void *)VM_KERNEL_ADDRPERM(srfmp->vp),
					srfmp->vp->v_name,
					(void *)VM_KERNEL_ADDRPERM(srfmp->vp),
					error));
				goto done;
			}

			if (!(va.va_flags & SF_RESTRICTED)) {
				/*
				 * CSR is not configured in CSR_ALLOW_UNRESTRICTED_FS mode, and
				 * the shared cache file is NOT SIP-protected, so reject the
				 * mapping request
				 */
				SHARED_REGION_TRACE_ERROR(
					("shared_region: %p [%d(%s)] map(%p:'%s'), "
					"vnode is not SIP-protected. \n",
					(void *)VM_KERNEL_ADDRPERM(current_thread()),
					proc_getpid(p), p->p_comm,
					(void *)VM_KERNEL_ADDRPERM(srfmp->vp),
					srfmp->vp->v_name));
				error = EPERM;
				goto done;
			}
		}
#else /* CONFIG_CSR */

		/*
		 * Devices without SIP/ROSP need to make sure that the shared cache
		 * is either on the root volume or in the preboot cryptex volume.
		 */
		assert(rdir_vp != NULL);
		if (srfmp->vp->v_mount != rdir_vp->v_mount) {
			vnode_t preboot_vp = NULL;
#if XNU_TARGET_OS_OSX
#define PREBOOT_CRYPTEX_PATH "/System/Volumes/Preboot/Cryptexes"
#else
#define PREBOOT_CRYPTEX_PATH "/private/preboot/Cryptexes"
#endif
			error = vnode_lookup(PREBOOT_CRYPTEX_PATH, 0, &preboot_vp, vfs_context_current());
			if (error || srfmp->vp->v_mount != preboot_vp->v_mount) {
				SHARED_REGION_TRACE_ERROR(
					("shared_region: %p [%d(%s)] map(%p:'%s'): "
					"not on process' root volume nor preboot volume\n",
					(void *)VM_KERNEL_ADDRPERM(current_thread()),
					proc_getpid(p), p->p_comm,
					(void *)VM_KERNEL_ADDRPERM(srfmp->vp),
					srfmp->vp->v_name));
				error = EPERM;
				if (preboot_vp) {
					(void)vnode_put(preboot_vp);
				}
				goto done;
			} else if (preboot_vp) {
				(void)vnode_put(preboot_vp);
			}
		}
#endif /* CONFIG_CSR */

		if (scdir_enforce) {
			char **expected_scdir_path = is_driverkit ? driverkit_scdir_path : scdir_path;
			struct vnode *scdir_vp = NULL;
			for (expected_scdir_path = is_driverkit ? driverkit_scdir_path : scdir_path;
			    *expected_scdir_path != NULL;
			    expected_scdir_path++) {
				/* get vnode for expected_scdir_path */
				error = vnode_lookup(*expected_scdir_path, 0, &scdir_vp, vfs_context_current());
				if (error) {
					SHARED_REGION_TRACE_ERROR(
						("shared_region: %p [%d(%s)]: "
						"vnode_lookup(%s) failed (error=%d)\n",
						(void *)VM_KERNEL_ADDRPERM(current_thread()),
						proc_getpid(p), p->p_comm,
						*expected_scdir_path, error));
					continue;
				}

				/* check if parent is scdir_vp */
				assert(scdir_vp != NULL);
				if (vnode_parent(srfmp->vp) == scdir_vp) {
					(void)vnode_put(scdir_vp);
					scdir_vp = NULL;
					goto scdir_ok;
				}
				(void)vnode_put(scdir_vp);
				scdir_vp = NULL;
			}
			/* nothing matches */
			SHARED_REGION_TRACE_ERROR(
				("shared_region: %p [%d(%s)] map(%p:'%s'): "
				"shared cache file not in expected directory\n",
				(void *)VM_KERNEL_ADDRPERM(current_thread()),
				proc_getpid(p), p->p_comm,
				(void *)VM_KERNEL_ADDRPERM(srfmp->vp),
				srfmp->vp->v_name));
			error = EPERM;
			goto done;
		}
scdir_ok:

		/* get vnode size */
		error = vnode_size(srfmp->vp, &fs, vfs_context_current());
		if (error) {
			SHARED_REGION_TRACE_ERROR(
				("shared_region: %p [%d(%s)] map(%p:'%s'): "
				"vnode_size(%p) failed (error=%d)\n",
				(void *)VM_KERNEL_ADDRPERM(current_thread()),
				proc_getpid(p), p->p_comm,
				(void *)VM_KERNEL_ADDRPERM(srfmp->vp),
				srfmp->vp->v_name,
				(void *)VM_KERNEL_ADDRPERM(srfmp->vp), error));
			goto done;
		}
		srfmp->file_size = fs;

		/* get the file's memory object handle */
		srfmp->file_control = ubc_getobject(srfmp->vp, UBC_HOLDOBJECT);
		if (srfmp->file_control == MEMORY_OBJECT_CONTROL_NULL) {
			SHARED_REGION_TRACE_ERROR(
				("shared_region: %p [%d(%s)] map(%p:'%s'): "
				"no memory object\n",
				(void *)VM_KERNEL_ADDRPERM(current_thread()),
				proc_getpid(p), p->p_comm,
				(void *)VM_KERNEL_ADDRPERM(srfmp->vp),
				srfmp->vp->v_name));
			error = EINVAL;
			goto done;
		}

		/* check that the mappings are properly covered by code signatures */
		if (!cs_system_enforcement()) {
			/* code signing is not enforced: no need to check */
		} else {
			for (i = 0; i < srfmp->mappings_count; i++) {
				if (srfmp->mappings[i].sms_init_prot & VM_PROT_ZF) {
					/* zero-filled mapping: not backed by the file */
					continue;
				}
				if (ubc_cs_is_range_codesigned(srfmp->vp,
				    srfmp->mappings[i].sms_file_offset,
				    srfmp->mappings[i].sms_size)) {
					/* this mapping is fully covered by code signatures */
					continue;
				}
				SHARED_REGION_TRACE_ERROR(
					("shared_region: %p [%d(%s)] map(%p:'%s'): "
					"mapping #%d/%d [0x%llx:0x%llx:0x%llx:0x%x:0x%x] "
					"is not code-signed\n",
					(void *)VM_KERNEL_ADDRPERM(current_thread()),
					proc_getpid(p), p->p_comm,
					(void *)VM_KERNEL_ADDRPERM(srfmp->vp),
					srfmp->vp->v_name,
					i, srfmp->mappings_count,
					srfmp->mappings[i].sms_address,
					srfmp->mappings[i].sms_size,
					srfmp->mappings[i].sms_file_offset,
					srfmp->mappings[i].sms_max_prot,
					srfmp->mappings[i].sms_init_prot));
				error = EINVAL;
				goto done;
			}
		}
	}
done:
	if (error != 0) {
		shared_region_map_and_slide_cleanup(p, files_count, *sr_file_mappings, shared_region);
		*sr_file_mappings = NULL;
		*shared_region_ptr = NULL;
	}
	return error;
}

/*
 * shared_region_map_np()
 *
 * This system call is intended for dyld.
 *
 * dyld uses this to map a shared cache file into a shared region.
 * This is usually done only the first time a shared cache is needed.
 * Subsequent processes will just use the populated shared region without
 * requiring any further setup.
 */
static int
_shared_region_map_and_slide(
	struct proc                         *p,
	uint32_t                            files_count,
	struct shared_file_np               *files,
	uint32_t                            mappings_count,
	struct shared_file_mapping_slide_np *mappings)
{
	int                             error = 0;
	kern_return_t                   kr = KERN_SUCCESS;
	struct _sr_file_mappings        *sr_file_mappings = NULL;
	struct vnode                    *rdir_vp = NULL;
	struct vm_shared_region         *shared_region = NULL;

	/*
	 * Get a reference to the current proc's root dir.
	 * Need this to prevent racing with chroot.
	 */
	proc_fdlock(p);
	rdir_vp = p->p_fd.fd_rdir;
	if (rdir_vp == NULL) {
		rdir_vp = rootvnode;
	}
	assert(rdir_vp != NULL);
	vnode_get(rdir_vp);
	proc_fdunlock(p);

	/*
	 * Turn files, mappings into sr_file_mappings and other setup.
	 */
	error = shared_region_map_and_slide_setup(p, files_count,
	    files, mappings_count, mappings,
	    &sr_file_mappings, &shared_region, rdir_vp);
	if (error != 0) {
		vnode_put(rdir_vp);
		return error;
	}

	/* map the file(s) into that shared region's submap */
	kr = vm_shared_region_map_file(shared_region, files_count, sr_file_mappings);
	if (kr != KERN_SUCCESS) {
		SHARED_REGION_TRACE_ERROR(("shared_region: %p [%d(%s)] map(): "
		    "vm_shared_region_map_file() failed kr=0x%x\n",
		    (void *)VM_KERNEL_ADDRPERM(current_thread()),
		    proc_getpid(p), p->p_comm, kr));
	}

	/* convert kern_return_t to errno */
	switch (kr) {
	case KERN_SUCCESS:
		error = 0;
		break;
	case KERN_INVALID_ADDRESS:
		error = EFAULT;
		break;
	case KERN_PROTECTION_FAILURE:
		error = EPERM;
		break;
	case KERN_NO_SPACE:
		error = ENOMEM;
		break;
	case KERN_FAILURE:
	case KERN_INVALID_ARGUMENT:
	default:
		error = EINVAL;
		break;
	}

	/*
	 * Mark that this process is now using split libraries.
	 */
	if (error == 0 && (p->p_flag & P_NOSHLIB)) {
		OSBitAndAtomic(~((uint32_t)P_NOSHLIB), &p->p_flag);
	}

	vnode_put(rdir_vp);
	shared_region_map_and_slide_cleanup(p, files_count, sr_file_mappings, shared_region);

	SHARED_REGION_TRACE_DEBUG(
		("shared_region: %p [%d(%s)] <- map\n",
		(void *)VM_KERNEL_ADDRPERM(current_thread()),
		proc_getpid(p), p->p_comm));

	return error;
}

/*
 * Clean up part of _shared_region_map_and_slide()
 * It had to be broken out of _shared_region_map_and_slide() to
 * prevent compiler inlining from blowing out the stack.
 */
__attribute__((noinline))
static void
shared_region_map_and_slide_cleanup(
	struct proc              *p,
	uint32_t                 files_count,
	struct _sr_file_mappings *sr_file_mappings,
	struct vm_shared_region  *shared_region)
{
	struct _sr_file_mappings *srfmp;
	struct vnode_attr        va;

	if (sr_file_mappings != NULL) {
		for (srfmp = &sr_file_mappings[0]; srfmp < &sr_file_mappings[files_count]; srfmp++) {
			if (srfmp->vp != NULL) {
				vnode_lock_spin(srfmp->vp);
				srfmp->vp->v_flag |= VSHARED_DYLD;
				vnode_unlock(srfmp->vp);

				/* update the vnode's access time */
				if (!(vnode_vfsvisflags(srfmp->vp) & MNT_NOATIME)) {
					VATTR_INIT(&va);
					nanotime(&va.va_access_time);
					VATTR_SET_ACTIVE(&va, va_access_time);
					vnode_setattr(srfmp->vp, &va, vfs_context_current());
				}

#if NAMEDSTREAMS
				/*
				 * If the shared cache is compressed, it may
				 * have a namedstream vnode instantiated for
				 * for it. That namedstream vnode will also
				 * have to be marked with VSHARED_DYLD.
				 */
				if (vnode_hasnamedstreams(srfmp->vp)) {
					vnode_t svp;
					if (vnode_getnamedstream(srfmp->vp, &svp, XATTR_RESOURCEFORK_NAME,
					    NS_OPEN, 0, vfs_context_kernel()) == 0) {
						vnode_lock_spin(svp);
						svp->v_flag |= VSHARED_DYLD;
						vnode_unlock(svp);
						vnode_put(svp);
					}
				}
#endif /* NAMEDSTREAMS */
				/*
				 * release the vnode...
				 * ubc_map() still holds it for us in the non-error case
				 */
				(void) vnode_put(srfmp->vp);
				srfmp->vp = NULL;
			}
			if (srfmp->fp != NULL) {
				/* release the file descriptor */
				fp_drop(p, srfmp->fd, srfmp->fp, 0);
				srfmp->fp = NULL;
			}
		}
		kfree_type(struct _sr_file_mappings, files_count, sr_file_mappings);
	}

	if (shared_region != NULL) {
		vm_shared_region_deallocate(shared_region);
	}
}

/*
 * For each file mapped, we may have mappings for:
 *    TEXT, EXECUTE, LINKEDIT, DATA_CONST, __AUTH, DATA
 * so let's round up to 8 mappings per file.
 */
#define SFM_MAX       (_SR_FILE_MAPPINGS_MAX_FILES * 8)     /* max mapping structs allowed to pass in */

/*
 * This is the new interface for setting up shared region mappings.
 *
 * The slide used for shared regions setup using this interface is done differently
 * from the old interface. The slide value passed in the shared_files_np represents
 * a max value. The kernel will choose a random value based on that, then use it
 * for all shared regions.
 */
#if defined (__x86_64__)
#define SLIDE_AMOUNT_MASK ~FOURK_PAGE_MASK
#else
#define SLIDE_AMOUNT_MASK ~SIXTEENK_PAGE_MASK
#endif

int
shared_region_map_and_slide_2_np(
	struct proc                                  *p,
	struct shared_region_map_and_slide_2_np_args *uap,
	__unused int                                 *retvalp)
{
	unsigned int                  files_count;
	struct shared_file_np         *shared_files = NULL;
	unsigned int                  mappings_count;
	struct shared_file_mapping_slide_np *mappings = NULL;
	kern_return_t                 kr = KERN_SUCCESS;

	files_count = uap->files_count;
	mappings_count = uap->mappings_count;

	if (files_count == 0) {
		SHARED_REGION_TRACE_INFO(
			("shared_region: %p [%d(%s)] map(): "
			"no files\n",
			(void *)VM_KERNEL_ADDRPERM(current_thread()),
			proc_getpid(p), p->p_comm));
		kr = 0; /* no files to map: we're done ! */
		goto done;
	} else if (files_count <= _SR_FILE_MAPPINGS_MAX_FILES) {
		shared_files = kalloc_data(files_count * sizeof(shared_files[0]), Z_WAITOK);
		if (shared_files == NULL) {
			kr = KERN_RESOURCE_SHORTAGE;
			goto done;
		}
	} else {
		SHARED_REGION_TRACE_ERROR(
			("shared_region: %p [%d(%s)] map(): "
			"too many files (%d) max %d\n",
			(void *)VM_KERNEL_ADDRPERM(current_thread()),
			proc_getpid(p), p->p_comm,
			files_count, _SR_FILE_MAPPINGS_MAX_FILES));
		kr = KERN_FAILURE;
		goto done;
	}

	if (mappings_count == 0) {
		SHARED_REGION_TRACE_INFO(
			("shared_region: %p [%d(%s)] map(): "
			"no mappings\n",
			(void *)VM_KERNEL_ADDRPERM(current_thread()),
			proc_getpid(p), p->p_comm));
		kr = 0; /* no mappings: we're done ! */
		goto done;
	} else if (mappings_count <= SFM_MAX) {
		mappings = kalloc_data(mappings_count * sizeof(mappings[0]), Z_WAITOK);
		if (mappings == NULL) {
			kr = KERN_RESOURCE_SHORTAGE;
			goto done;
		}
	} else {
		SHARED_REGION_TRACE_ERROR(
			("shared_region: %p [%d(%s)] map(): "
			"too many mappings (%d) max %d\n",
			(void *)VM_KERNEL_ADDRPERM(current_thread()),
			proc_getpid(p), p->p_comm,
			mappings_count, SFM_MAX));
		kr = KERN_FAILURE;
		goto done;
	}

	kr = shared_region_copyin(p, uap->files, files_count, sizeof(shared_files[0]), shared_files);
	if (kr != KERN_SUCCESS) {
		goto done;
	}

	kr = shared_region_copyin(p, uap->mappings, mappings_count, sizeof(mappings[0]), mappings);
	if (kr != KERN_SUCCESS) {
		goto done;
	}

	uint32_t max_slide = shared_files[0].sf_slide;
	uint32_t random_val;
	uint32_t slide_amount;

	if (max_slide != 0) {
		read_random(&random_val, sizeof random_val);
		slide_amount = ((random_val % max_slide) & SLIDE_AMOUNT_MASK);
	} else {
		slide_amount = 0;
	}
#if DEVELOPMENT || DEBUG
	extern bool bootarg_disable_aslr;
	if (bootarg_disable_aslr) {
		slide_amount = 0;
	}
#endif /* DEVELOPMENT || DEBUG */

	/*
	 * Fix up the mappings to reflect the desired slide.
	 */
	unsigned int f;
	unsigned int m = 0;
	unsigned int i;
	for (f = 0; f < files_count; ++f) {
		shared_files[f].sf_slide = slide_amount;
		for (i = 0; i < shared_files[f].sf_mappings_count; ++i, ++m) {
			if (m >= mappings_count) {
				SHARED_REGION_TRACE_ERROR(
					("shared_region: %p [%d(%s)] map(): "
					"mapping count argument was too small\n",
					(void *)VM_KERNEL_ADDRPERM(current_thread()),
					proc_getpid(p), p->p_comm));
				kr = KERN_FAILURE;
				goto done;
			}
			mappings[m].sms_address += slide_amount;
			if (mappings[m].sms_slide_size != 0) {
				mappings[m].sms_slide_start += slide_amount;
			}
		}
	}

	kr = _shared_region_map_and_slide(p, files_count, shared_files, mappings_count, mappings);
done:
	kfree_data(shared_files, files_count * sizeof(shared_files[0]));
	kfree_data(mappings, mappings_count * sizeof(mappings[0]));
	return kr;
}

/*
 * A syscall for dyld to use to map data pages that need load time relocation fixups.
 * The fixups are performed by a custom pager during page-in, so the pages still appear
 * "clean" and hence are easily discarded under memory pressure. They can be re-paged-in
 * on demand later, all w/o using the compressor.
 *
 * Note these page are treated as MAP_PRIVATE. So if the application dirties any pages while
 * running, they are COW'd as normal.
 */
int
map_with_linking_np(
	struct proc                     *p,
	struct map_with_linking_np_args *uap,
	__unused int                    *retvalp)
{
	uint32_t                        region_count;
	uint32_t                        r;
	struct mwl_region               *regions = NULL;
	struct mwl_region               *rp;
	uint32_t                        link_info_size;
	void                            *link_info = NULL;      /* starts with a struct mwl_info_hdr */
	struct mwl_info_hdr             *info_hdr = NULL;
	uint64_t                        binds_size;
	int                             fd;
	struct fileproc                 *fp = NULL;
	struct vnode                    *vp = NULL;
	size_t                          file_size;
	off_t                           fs;
	struct vnode_attr               va;
	memory_object_control_t         file_control = NULL;
	int                             error;
	kern_return_t                   kr = KERN_SUCCESS;

	/*
	 * Check if dyld has told us it finished with this call.
	 */
	if (p->p_disallow_map_with_linking) {
		printf("%s: [%d(%s)]: map__with_linking() was disabled\n",
		    __func__, proc_getpid(p), p->p_comm);
		kr = KERN_FAILURE;
		goto done;
	}

	/*
	 * First we do some sanity checking on what dyld has passed us.
	 */
	region_count = uap->region_count;
	link_info_size = uap->link_info_size;
	if (region_count == 0) {
		printf("%s: [%d(%s)]: region_count == 0\n",
		    __func__, proc_getpid(p), p->p_comm);
		kr = KERN_FAILURE;
		goto done;
	}
	if (region_count > MWL_MAX_REGION_COUNT) {
		printf("%s: [%d(%s)]: region_count too big %d\n",
		    __func__, proc_getpid(p), p->p_comm, region_count);
		kr = KERN_FAILURE;
		goto done;
	}

	if (link_info_size <= MWL_MIN_LINK_INFO_SIZE) {
		printf("%s: [%d(%s)]: link_info_size too small\n",
		    __func__, proc_getpid(p), p->p_comm);
		kr = KERN_FAILURE;
		goto done;
	}
	if (link_info_size >= MWL_MAX_LINK_INFO_SIZE) {
		printf("%s: [%d(%s)]: link_info_size too big %d\n",
		    __func__, proc_getpid(p), p->p_comm, link_info_size);
		kr = KERN_FAILURE;
		goto done;
	}

	/*
	 * Allocate and copyin the regions and link info
	 */
	regions = kalloc_data(region_count * sizeof(regions[0]), Z_WAITOK);
	if (regions == NULL) {
		printf("%s: [%d(%s)]: failed to allocate regions\n",
		    __func__, proc_getpid(p), p->p_comm);
		kr = KERN_RESOURCE_SHORTAGE;
		goto done;
	}
	kr = shared_region_copyin(p, uap->regions, region_count, sizeof(regions[0]), regions);
	if (kr != KERN_SUCCESS) {
		printf("%s: [%d(%s)]: failed to copyin regions kr=%d\n",
		    __func__, proc_getpid(p), p->p_comm, kr);
		goto done;
	}

	link_info = kalloc_data(link_info_size, Z_WAITOK);
	if (link_info == NULL) {
		printf("%s: [%d(%s)]: failed to allocate link_info\n",
		    __func__, proc_getpid(p), p->p_comm);
		kr = KERN_RESOURCE_SHORTAGE;
		goto done;
	}
	kr = shared_region_copyin(p, uap->link_info, 1, link_info_size, link_info);
	if (kr != KERN_SUCCESS) {
		printf("%s: [%d(%s)]: failed to copyin link_info kr=%d\n",
		    __func__, proc_getpid(p), p->p_comm, kr);
		goto done;
	}

	/*
	 * Do some verification the data structures.
	 */
	info_hdr = (struct mwl_info_hdr *)link_info;
	if (info_hdr->mwli_version != MWL_INFO_VERS) {
		printf("%s: [%d(%s)]: unrecognized mwli_version=%d\n",
		    __func__, proc_getpid(p), p->p_comm, info_hdr->mwli_version);
		kr = KERN_FAILURE;
		goto done;
	}

	if (info_hdr->mwli_binds_offset > link_info_size) {
		printf("%s: [%d(%s)]: mwli_binds_offset too large %d\n",
		    __func__, proc_getpid(p), p->p_comm, info_hdr->mwli_binds_offset);
		kr = KERN_FAILURE;
		goto done;
	}

	/* some older devs have s/w page size > h/w page size, no need to support them */
	if (info_hdr->mwli_page_size != PAGE_SIZE) {
		/* no printf, since this is expected on some devices */
		kr = KERN_INVALID_ARGUMENT;
		goto done;
	}

	binds_size = (uint64_t)info_hdr->mwli_binds_count *
	    ((info_hdr->mwli_pointer_format == DYLD_CHAINED_PTR_32) ? 4 : 8);
	if (binds_size > link_info_size - info_hdr->mwli_binds_offset) {
		printf("%s: [%d(%s)]: mwli_binds_count too large %d\n",
		    __func__, proc_getpid(p), p->p_comm, info_hdr->mwli_binds_count);
		kr = KERN_FAILURE;
		goto done;
	}

	if (info_hdr->mwli_chains_offset > link_info_size) {
		printf("%s: [%d(%s)]: mwli_chains_offset too large %d\n",
		    __func__, proc_getpid(p), p->p_comm, info_hdr->mwli_chains_offset);
		kr = KERN_FAILURE;
		goto done;
	}


	/*
	 * Ensure the chained starts in the link info and make sure the
	 * segment info offsets are within bounds.
	 */
	if (info_hdr->mwli_chains_size < sizeof(struct dyld_chained_starts_in_image)) {
		printf("%s: [%d(%s)]: mwli_chains_size too small %d\n",
		    __func__, proc_getpid(p), p->p_comm, info_hdr->mwli_chains_size);
		kr = KERN_FAILURE;
		goto done;
	}
	if (info_hdr->mwli_chains_size > link_info_size - info_hdr->mwli_chains_offset) {
		printf("%s: [%d(%s)]: mwli_chains_size too large %d\n",
		    __func__, proc_getpid(p), p->p_comm, info_hdr->mwli_chains_size);
		kr = KERN_FAILURE;
		goto done;
	}

	/* Note that more verification of offsets is done in the pager itself */

	/*
	 * Ensure we've only been given one FD and verify valid protections.
	 */
	fd = regions[0].mwlr_fd;
	for (r = 0; r < region_count; ++r) {
		if (regions[r].mwlr_fd != fd) {
			printf("%s: [%d(%s)]: mwlr_fd mismatch %d and %d\n",
			    __func__, proc_getpid(p), p->p_comm, fd, regions[r].mwlr_fd);
			kr = KERN_FAILURE;
			goto done;
		}

		/*
		 * Only allow data mappings and not zero fill. Permit TPRO
		 * mappings only when VM_PROT_READ | VM_PROT_WRITE.
		 */
		if (regions[r].mwlr_protections & VM_PROT_EXECUTE) {
			printf("%s: [%d(%s)]: mwlr_protections EXECUTE not allowed\n",
			    __func__, proc_getpid(p), p->p_comm);
			kr = KERN_FAILURE;
			goto done;
		}
		if (regions[r].mwlr_protections & VM_PROT_ZF) {
			printf("%s: [%d(%s)]: region %d, found VM_PROT_ZF not allowed\n",
			    __func__, proc_getpid(p), p->p_comm, r);
			kr = KERN_FAILURE;
			goto done;
		}
		if ((regions[r].mwlr_protections & VM_PROT_TPRO) &&
		    !(regions[r].mwlr_protections & VM_PROT_WRITE)) {
			printf("%s: [%d(%s)]: region %d, found VM_PROT_TPRO without VM_PROT_WRITE\n",
			    __func__, proc_getpid(p), p->p_comm, r);
			kr = KERN_FAILURE;
			goto done;
		}
	}


	/* get file structure from file descriptor */
	error = fp_get_ftype(p, fd, DTYPE_VNODE, EINVAL, &fp);
	if (error) {
		printf("%s: [%d(%s)]: fp_get_ftype() failed, error %d\n",
		    __func__, proc_getpid(p), p->p_comm, error);
		kr = KERN_FAILURE;
		goto done;
	}

	/* We need at least read permission on the file */
	if (!(fp->fp_glob->fg_flag & FREAD)) {
		printf("%s: [%d(%s)]: not readable\n",
		    __func__, proc_getpid(p), p->p_comm);
		kr = KERN_FAILURE;
		goto done;
	}

	/* Get the vnode from file structure */
	vp = (struct vnode *)fp_get_data(fp);
	error = vnode_getwithref(vp);
	if (error) {
		printf("%s: [%d(%s)]: failed to get vnode, error %d\n",
		    __func__, proc_getpid(p), p->p_comm, error);
		kr = KERN_FAILURE;
		vp = NULL; /* just to be sure */
		goto done;
	}

	/* Make sure the vnode is a regular file */
	if (vp->v_type != VREG) {
		printf("%s: [%d(%s)]: vnode not VREG\n",
		    __func__, proc_getpid(p), p->p_comm);
		kr = KERN_FAILURE;
		goto done;
	}

	/* get vnode size */
	error = vnode_size(vp, &fs, vfs_context_current());
	if (error) {
		goto done;
	}
	file_size = fs;

	/* get the file's memory object handle */
	file_control = ubc_getobject(vp, UBC_HOLDOBJECT);
	if (file_control == MEMORY_OBJECT_CONTROL_NULL) {
		printf("%s: [%d(%s)]: no memory object\n",
		    __func__, proc_getpid(p), p->p_comm);
		kr = KERN_FAILURE;
		goto done;
	}

	for (r = 0; r < region_count; ++r) {
		rp = &regions[r];

#if CONFIG_MACF
		vm_prot_t prot = (rp->mwlr_protections & VM_PROT_ALL);
		error = mac_file_check_mmap(vfs_context_ucred(vfs_context_current()),
		    fp->fp_glob, prot, MAP_FILE | MAP_PRIVATE | MAP_FIXED, rp->mwlr_file_offset, &prot);
		if (error) {
			printf("%s: [%d(%s)]: mac_file_check_mmap() failed, region %d, error %d\n",
			    __func__, proc_getpid(p), p->p_comm, r, error);
			kr = KERN_FAILURE;
			goto done;
		}
#endif /* MAC */

		/* check that the mappings are properly covered by code signatures */
		if (cs_system_enforcement()) {
			if (!ubc_cs_is_range_codesigned(vp, rp->mwlr_file_offset, rp->mwlr_size)) {
				printf("%s: [%d(%s)]: region %d, not code signed\n",
				    __func__, proc_getpid(p), p->p_comm, r);
				kr = KERN_FAILURE;
				goto done;
			}
		}
	}

	/* update the vnode's access time */
	if (!(vnode_vfsvisflags(vp) & MNT_NOATIME)) {
		VATTR_INIT(&va);
		nanotime(&va.va_access_time);
		VATTR_SET_ACTIVE(&va, va_access_time);
		vnode_setattr(vp, &va, vfs_context_current());
	}

	/* get the VM to do the work */
	kr = vm_map_with_linking(proc_task(p), regions, region_count, &link_info, link_info_size, file_control);

done:
	if (fp != NULL) {
		/* release the file descriptor */
		fp_drop(p, fd, fp, 0);
	}
	if (vp != NULL) {
		(void)vnode_put(vp);
	}
	if (regions != NULL) {
		kfree_data(regions, region_count * sizeof(regions[0]));
	}
	/* link info is NULL if it is used in the pager, if things worked */
	if (link_info != NULL) {
		kfree_data(link_info, link_info_size);
	}

	switch (kr) {
	case KERN_SUCCESS:
		return 0;
	case KERN_RESOURCE_SHORTAGE:
		return ENOMEM;
	default:
		return EINVAL;
	}
}

#if DEBUG || DEVELOPMENT
SYSCTL_INT(_vm, OID_AUTO, dyld_pager_count,
    CTLFLAG_RD | CTLFLAG_LOCKED, &dyld_pager_count, 0, "");
SYSCTL_INT(_vm, OID_AUTO, dyld_pager_count_max,
    CTLFLAG_RD | CTLFLAG_LOCKED, &dyld_pager_count_max, 0, "");
#endif /* DEBUG || DEVELOPMENT */

/* sysctl overflow room */

SYSCTL_INT(_vm, OID_AUTO, pagesize, CTLFLAG_RD | CTLFLAG_LOCKED,
    (int *) &page_size, 0, "vm page size");

/* vm_page_free_target is provided as a makeshift solution for applications that want to
 *       allocate buffer space, possibly purgeable memory, but not cause inactive pages to be
 *       reclaimed. It allows the app to calculate how much memory is free outside the free target. */
extern unsigned int     vm_page_free_target;
SYSCTL_INT(_vm, OID_AUTO, vm_page_free_target, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_page_free_target, 0, "Pageout daemon free target");

SYSCTL_INT(_vm, OID_AUTO, memory_pressure, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_pageout_state.vm_memory_pressure, 0, "Memory pressure indicator");

static int
vm_ctl_page_free_wanted SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg1, arg2)
	unsigned int page_free_wanted;

	page_free_wanted = mach_vm_ctl_page_free_wanted();
	return SYSCTL_OUT(req, &page_free_wanted, sizeof(page_free_wanted));
}
SYSCTL_PROC(_vm, OID_AUTO, page_free_wanted,
    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_LOCKED,
    0, 0, vm_ctl_page_free_wanted, "I", "");

extern unsigned int     vm_page_purgeable_count;
SYSCTL_INT(_vm, OID_AUTO, page_purgeable_count, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_page_purgeable_count, 0, "Purgeable page count");

extern unsigned int     vm_page_purgeable_wired_count;
SYSCTL_INT(_vm, OID_AUTO, page_purgeable_wired_count, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_page_purgeable_wired_count, 0, "Wired purgeable page count");

extern unsigned int vm_page_kern_lpage_count;
SYSCTL_INT(_vm, OID_AUTO, kern_lpage_count, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_page_kern_lpage_count, 0, "kernel used large pages");

#if DEVELOPMENT || DEBUG
#if __ARM_MIXED_PAGE_SIZE__
static int vm_mixed_pagesize_supported = 1;
#else
static int vm_mixed_pagesize_supported = 0;
#endif /*__ARM_MIXED_PAGE_SIZE__ */
SYSCTL_INT(_debug, OID_AUTO, vm_mixed_pagesize_supported, CTLFLAG_ANYBODY | CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_mixed_pagesize_supported, 0, "kernel support for mixed pagesize");

SCALABLE_COUNTER_DECLARE(vm_page_grab_count);
SYSCTL_SCALABLE_COUNTER(_vm, pages_grabbed, vm_page_grab_count, "Total pages grabbed");
SYSCTL_ULONG(_vm, OID_AUTO, pages_freed, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_pageout_vminfo.vm_page_pages_freed, "Total pages freed");

SYSCTL_INT(_vm, OID_AUTO, pageout_purged_objects, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_pageout_debug.vm_pageout_purged_objects, 0, "System purged object count");
SYSCTL_UINT(_vm, OID_AUTO, pageout_cleaned_busy, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_pageout_debug.vm_pageout_cleaned_busy, 0, "Cleaned pages busy (deactivated)");
SYSCTL_UINT(_vm, OID_AUTO, pageout_cleaned_nolock, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_pageout_debug.vm_pageout_cleaned_nolock, 0, "Cleaned pages no-lock (deactivated)");

SYSCTL_UINT(_vm, OID_AUTO, pageout_cleaned_volatile_reactivated, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_pageout_debug.vm_pageout_cleaned_volatile_reactivated, 0, "Cleaned pages volatile reactivated");
SYSCTL_UINT(_vm, OID_AUTO, pageout_cleaned_fault_reactivated, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_pageout_debug.vm_pageout_cleaned_fault_reactivated, 0, "Cleaned pages fault reactivated");
SYSCTL_UINT(_vm, OID_AUTO, pageout_cleaned_reactivated, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_pageout_debug.vm_pageout_cleaned_reactivated, 0, "Cleaned pages reactivated");         /* sum of all reactivated AND busy and nolock (even though those actually get reDEactivated */
SYSCTL_ULONG(_vm, OID_AUTO, pageout_cleaned, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_pageout_vminfo.vm_pageout_freed_cleaned, "Cleaned pages freed");
SYSCTL_UINT(_vm, OID_AUTO, pageout_cleaned_reference_reactivated, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_pageout_debug.vm_pageout_cleaned_reference_reactivated, 0, "Cleaned pages reference reactivated");
SYSCTL_UINT(_vm, OID_AUTO, pageout_enqueued_cleaned, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_pageout_debug.vm_pageout_enqueued_cleaned, 0, "");         /* sum of next two */
#endif /* DEVELOPMENT || DEBUG */

extern int madvise_free_debug;
SYSCTL_INT(_vm, OID_AUTO, madvise_free_debug, CTLFLAG_RW | CTLFLAG_LOCKED,
    &madvise_free_debug, 0, "zero-fill on madvise(MADV_FREE*)");
extern int madvise_free_debug_sometimes;
SYSCTL_INT(_vm, OID_AUTO, madvise_free_debug_sometimes, CTLFLAG_RW | CTLFLAG_LOCKED,
    &madvise_free_debug_sometimes, 0, "sometimes zero-fill on madvise(MADV_FREE*)");

SYSCTL_INT(_vm, OID_AUTO, page_reusable_count, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_page_stats_reusable.reusable_count, 0, "Reusable page count");
SYSCTL_QUAD(_vm, OID_AUTO, reusable_success, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_page_stats_reusable.reusable_pages_success, "");
SYSCTL_QUAD(_vm, OID_AUTO, reusable_failure, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_page_stats_reusable.reusable_pages_failure, "");
SYSCTL_QUAD(_vm, OID_AUTO, reusable_pages_shared, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_page_stats_reusable.reusable_pages_shared, "");
SYSCTL_QUAD(_vm, OID_AUTO, all_reusable_calls, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_page_stats_reusable.all_reusable_calls, "");
SYSCTL_QUAD(_vm, OID_AUTO, partial_reusable_calls, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_page_stats_reusable.partial_reusable_calls, "");
SYSCTL_QUAD(_vm, OID_AUTO, reuse_success, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_page_stats_reusable.reuse_pages_success, "");
SYSCTL_QUAD(_vm, OID_AUTO, reuse_failure, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_page_stats_reusable.reuse_pages_failure, "");
SYSCTL_QUAD(_vm, OID_AUTO, all_reuse_calls, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_page_stats_reusable.all_reuse_calls, "");
SYSCTL_QUAD(_vm, OID_AUTO, partial_reuse_calls, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_page_stats_reusable.partial_reuse_calls, "");
SYSCTL_QUAD(_vm, OID_AUTO, can_reuse_success, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_page_stats_reusable.can_reuse_success, "");
SYSCTL_QUAD(_vm, OID_AUTO, can_reuse_failure, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_page_stats_reusable.can_reuse_failure, "");
SYSCTL_QUAD(_vm, OID_AUTO, reusable_reclaimed, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_page_stats_reusable.reusable_reclaimed, "");
SYSCTL_QUAD(_vm, OID_AUTO, reusable_nonwritable, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_page_stats_reusable.reusable_nonwritable, "");
SYSCTL_QUAD(_vm, OID_AUTO, reusable_shared, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_page_stats_reusable.reusable_shared, "");
SYSCTL_QUAD(_vm, OID_AUTO, free_shared, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_page_stats_reusable.free_shared, "");


extern unsigned int vm_page_free_count, vm_page_speculative_count;
SYSCTL_UINT(_vm, OID_AUTO, page_free_count, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_page_free_count, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, page_speculative_count, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_page_speculative_count, 0, "");

extern unsigned int vm_page_cleaned_count;
SYSCTL_UINT(_vm, OID_AUTO, page_cleaned_count, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_page_cleaned_count, 0, "Cleaned queue size");

extern unsigned int vm_page_pageable_internal_count, vm_page_pageable_external_count;
SYSCTL_UINT(_vm, OID_AUTO, page_pageable_internal_count, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_page_pageable_internal_count, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, page_pageable_external_count, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_page_pageable_external_count, 0, "");

/* pageout counts */
SYSCTL_UINT(_vm, OID_AUTO, pageout_inactive_clean, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_pageout_state.vm_pageout_inactive_clean, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, pageout_inactive_used, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_pageout_state.vm_pageout_inactive_used, 0, "");

SYSCTL_ULONG(_vm, OID_AUTO, pageout_inactive_dirty_internal, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_pageout_vminfo.vm_pageout_inactive_dirty_internal, "");
SYSCTL_ULONG(_vm, OID_AUTO, pageout_inactive_dirty_external, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_pageout_vminfo.vm_pageout_inactive_dirty_external, "");
SYSCTL_ULONG(_vm, OID_AUTO, pageout_speculative_clean, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_pageout_vminfo.vm_pageout_freed_speculative, "");
SYSCTL_ULONG(_vm, OID_AUTO, pageout_freed_external, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_pageout_vminfo.vm_pageout_freed_external, "");
SYSCTL_ULONG(_vm, OID_AUTO, pageout_freed_speculative, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_pageout_vminfo.vm_pageout_freed_speculative, "");
SYSCTL_ULONG(_vm, OID_AUTO, pageout_freed_cleaned, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_pageout_vminfo.vm_pageout_freed_cleaned, "");

SYSCTL_ULONG(_vm, OID_AUTO, pageout_protected_sharedcache, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_pageout_vminfo.vm_pageout_protected_sharedcache, "");
SYSCTL_ULONG(_vm, OID_AUTO, pageout_forcereclaimed_sharedcache, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_pageout_vminfo.vm_pageout_forcereclaimed_sharedcache, "");
SYSCTL_ULONG(_vm, OID_AUTO, pageout_protected_realtime, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_pageout_vminfo.vm_pageout_protected_realtime, "");
SYSCTL_ULONG(_vm, OID_AUTO, pageout_forcereclaimed_realtime, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_pageout_vminfo.vm_pageout_forcereclaimed_realtime, "");
extern unsigned int vm_page_realtime_count;
SYSCTL_UINT(_vm, OID_AUTO, page_realtime_count, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_page_realtime_count, 0, "");
extern int vm_pageout_protect_realtime;
SYSCTL_INT(_vm, OID_AUTO, pageout_protect_realtime, CTLFLAG_RW | CTLFLAG_LOCKED, &vm_pageout_protect_realtime, 0, "");

/* counts of pages prefaulted when entering a memory object */
extern int64_t vm_prefault_nb_pages, vm_prefault_nb_bailout;
SYSCTL_QUAD(_vm, OID_AUTO, prefault_nb_pages, CTLFLAG_RW | CTLFLAG_LOCKED, &vm_prefault_nb_pages, "");
SYSCTL_QUAD(_vm, OID_AUTO, prefault_nb_bailout, CTLFLAG_RW | CTLFLAG_LOCKED, &vm_prefault_nb_bailout, "");

#if defined (__x86_64__)
extern unsigned int vm_clump_promote_threshold;
SYSCTL_UINT(_vm, OID_AUTO, vm_clump_promote_threshold, CTLFLAG_RW | CTLFLAG_LOCKED, &vm_clump_promote_threshold, 0, "clump size threshold for promotes");
#if DEVELOPMENT || DEBUG
extern unsigned long vm_clump_stats[];
SYSCTL_LONG(_vm, OID_AUTO, vm_clump_stats1, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_clump_stats[1], "free page allocations from clump of 1 page");
SYSCTL_LONG(_vm, OID_AUTO, vm_clump_stats2, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_clump_stats[2], "free page allocations from clump of 2 pages");
SYSCTL_LONG(_vm, OID_AUTO, vm_clump_stats3, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_clump_stats[3], "free page allocations from clump of 3 pages");
SYSCTL_LONG(_vm, OID_AUTO, vm_clump_stats4, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_clump_stats[4], "free page allocations from clump of 4 pages");
SYSCTL_LONG(_vm, OID_AUTO, vm_clump_stats5, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_clump_stats[5], "free page allocations from clump of 5 pages");
SYSCTL_LONG(_vm, OID_AUTO, vm_clump_stats6, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_clump_stats[6], "free page allocations from clump of 6 pages");
SYSCTL_LONG(_vm, OID_AUTO, vm_clump_stats7, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_clump_stats[7], "free page allocations from clump of 7 pages");
SYSCTL_LONG(_vm, OID_AUTO, vm_clump_stats8, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_clump_stats[8], "free page allocations from clump of 8 pages");
SYSCTL_LONG(_vm, OID_AUTO, vm_clump_stats9, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_clump_stats[9], "free page allocations from clump of 9 pages");
SYSCTL_LONG(_vm, OID_AUTO, vm_clump_stats10, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_clump_stats[10], "free page allocations from clump of 10 pages");
SYSCTL_LONG(_vm, OID_AUTO, vm_clump_stats11, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_clump_stats[11], "free page allocations from clump of 11 pages");
SYSCTL_LONG(_vm, OID_AUTO, vm_clump_stats12, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_clump_stats[12], "free page allocations from clump of 12 pages");
SYSCTL_LONG(_vm, OID_AUTO, vm_clump_stats13, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_clump_stats[13], "free page allocations from clump of 13 pages");
SYSCTL_LONG(_vm, OID_AUTO, vm_clump_stats14, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_clump_stats[14], "free page allocations from clump of 14 pages");
SYSCTL_LONG(_vm, OID_AUTO, vm_clump_stats15, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_clump_stats[15], "free page allocations from clump of 15 pages");
SYSCTL_LONG(_vm, OID_AUTO, vm_clump_stats16, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_clump_stats[16], "free page allocations from clump of 16 pages");
extern unsigned long vm_clump_allocs, vm_clump_inserts, vm_clump_inrange, vm_clump_promotes;
SYSCTL_LONG(_vm, OID_AUTO, vm_clump_alloc, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_clump_allocs, "free page allocations");
SYSCTL_LONG(_vm, OID_AUTO, vm_clump_inserts, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_clump_inserts, "free page insertions");
SYSCTL_LONG(_vm, OID_AUTO, vm_clump_inrange, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_clump_inrange, "free page insertions that are part of vm_pages");
SYSCTL_LONG(_vm, OID_AUTO, vm_clump_promotes, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_clump_promotes, "pages promoted to head");
#endif  /* if DEVELOPMENT || DEBUG */
#endif  /* #if defined (__x86_64__) */

#if CONFIG_SECLUDED_MEMORY

SYSCTL_UINT(_vm, OID_AUTO, num_tasks_can_use_secluded_mem, CTLFLAG_RD | CTLFLAG_LOCKED, &num_tasks_can_use_secluded_mem, 0, "");
extern unsigned int vm_page_secluded_target;
extern unsigned int vm_page_secluded_count;
extern unsigned int vm_page_secluded_count_free;
extern unsigned int vm_page_secluded_count_inuse;
extern unsigned int vm_page_secluded_count_over_target;
SYSCTL_UINT(_vm, OID_AUTO, page_secluded_target, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_page_secluded_target, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, page_secluded_count, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_page_secluded_count, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, page_secluded_count_free, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_page_secluded_count_free, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, page_secluded_count_inuse, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_page_secluded_count_inuse, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, page_secluded_count_over_target, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_page_secluded_count_over_target, 0, "");

extern struct vm_page_secluded_data vm_page_secluded;
SYSCTL_UINT(_vm, OID_AUTO, page_secluded_eligible, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_page_secluded.eligible_for_secluded, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, page_secluded_grab_success_free, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_page_secluded.grab_success_free, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, page_secluded_grab_success_other, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_page_secluded.grab_success_other, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, page_secluded_grab_failure_locked, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_page_secluded.grab_failure_locked, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, page_secluded_grab_failure_state, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_page_secluded.grab_failure_state, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, page_secluded_grab_failure_realtime, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_page_secluded.grab_failure_realtime, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, page_secluded_grab_failure_dirty, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_page_secluded.grab_failure_dirty, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, page_secluded_grab_for_iokit, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_page_secluded.grab_for_iokit, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, page_secluded_grab_for_iokit_success, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_page_secluded.grab_for_iokit_success, 0, "");

#endif /* CONFIG_SECLUDED_MEMORY */

#pragma mark Deferred Reclaim

#if CONFIG_DEFERRED_RECLAIM

#if DEVELOPMENT || DEBUG
/*
 * VM reclaim testing
 */
extern bool vm_deferred_reclamation_block_until_pid_has_been_reclaimed(pid_t pid);

static int
sysctl_vm_reclaim_drain_async_queue SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2)
	int error = EINVAL, pid = 0;
	/*
	 * Only send on write
	 */
	error = sysctl_handle_int(oidp, &pid, 0, req);
	if (error || !req->newptr) {
		return error;
	}

	bool success = vm_deferred_reclamation_block_until_pid_has_been_reclaimed(pid);
	if (success) {
		error = 0;
	}

	return error;
}

SYSCTL_PROC(_vm, OID_AUTO, reclaim_drain_async_queue,
    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_LOCKED | CTLFLAG_MASKED, 0, 0,
    &sysctl_vm_reclaim_drain_async_queue, "I", "");

static int
sysctl_vm_reclaim_from_pid SYSCTL_HANDLER_ARGS
{
	int error = EINVAL;
	pid_t pid;
	error = sysctl_handle_int(oidp, &pid, 0, req);
	/* Only reclaim on write */
	if (error || !req->newptr) {
		return error;
	}
	if (pid <= 0) {
		return EINVAL;
	}
	proc_t p = proc_find(pid);
	if (p == PROC_NULL) {
		return ESRCH;
	}
	task_t t = proc_task(p);
	if (t == TASK_NULL) {
		proc_rele(p);
		return ESRCH;
	}
	task_reference(t);
	proc_rele(p);
	vm_deferred_reclamation_reclaim_from_task_sync(t, UINT64_MAX);
	task_deallocate(t);
	return 0;
}

SYSCTL_PROC(_vm, OID_AUTO, reclaim_from_pid,
    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_LOCKED | CTLFLAG_MASKED, 0, 0,
    &sysctl_vm_reclaim_from_pid, "I",
    "Drain the deferred reclamation buffer for a pid");

static int
sysctl_vm_reclaim_drain_all_buffers SYSCTL_HANDLER_ARGS
{
	/* Only reclaim on write */
	if (!req->newptr) {
		return EINVAL;
	}
	vm_deferred_reclamation_reclaim_all_memory(RECLAIM_OPTIONS_NONE);
	return 0;
}

SYSCTL_PROC(_vm, OID_AUTO, reclaim_drain_all_buffers,
    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_LOCKED | CTLFLAG_MASKED, 0, 0,
    &sysctl_vm_reclaim_drain_all_buffers, "I",
    "Drain all system-wide deferred reclamation buffers");


extern uint64_t vm_reclaim_max_threshold;
extern uint64_t vm_reclaim_trim_divisor;

SYSCTL_ULONG(_vm, OID_AUTO, reclaim_max_threshold, CTLFLAG_RW | CTLFLAG_LOCKED, &vm_reclaim_max_threshold, "");
SYSCTL_ULONG(_vm, OID_AUTO, reclaim_trim_divisor, CTLFLAG_RW | CTLFLAG_LOCKED, &vm_reclaim_trim_divisor, "");
#endif /* DEVELOPMENT || DEBUG */

#endif /* CONFIG_DEFERRED_RECLAIM */

#include <kern/thread.h>
#include <sys/user.h>

void vm_pageout_io_throttle(void);

void
vm_pageout_io_throttle(void)
{
	struct uthread *uthread = current_uthread();

	/*
	 * thread is marked as a low priority I/O type
	 * and the I/O we issued while in this cleaning operation
	 * collided with normal I/O operations... we'll
	 * delay in order to mitigate the impact of this
	 * task on the normal operation of the system
	 */

	if (uthread->uu_lowpri_window) {
		throttle_lowpri_io(1);
	}
}

int
vm_pressure_monitor(
	__unused struct proc *p,
	struct vm_pressure_monitor_args *uap,
	int *retval)
{
	kern_return_t   kr;
	uint32_t        pages_reclaimed;
	uint32_t        pages_wanted;

	kr = mach_vm_pressure_monitor(
		(boolean_t) uap->wait_for_pressure,
		uap->nsecs_monitored,
		(uap->pages_reclaimed) ? &pages_reclaimed : NULL,
		&pages_wanted);

	switch (kr) {
	case KERN_SUCCESS:
		break;
	case KERN_ABORTED:
		return EINTR;
	default:
		return EINVAL;
	}

	if (uap->pages_reclaimed) {
		if (copyout((void *)&pages_reclaimed,
		    uap->pages_reclaimed,
		    sizeof(pages_reclaimed)) != 0) {
			return EFAULT;
		}
	}

	*retval = (int) pages_wanted;
	return 0;
}

int
kas_info(struct proc *p,
    struct kas_info_args *uap,
    int *retval __unused)
{
#ifndef CONFIG_KAS_INFO
	(void)p;
	(void)uap;
	return ENOTSUP;
#else /* CONFIG_KAS_INFO */
	int                     selector = uap->selector;
	user_addr_t     valuep = uap->value;
	user_addr_t     sizep = uap->size;
	user_size_t size, rsize;
	int                     error;

	if (!kauth_cred_issuser(kauth_cred_get())) {
		return EPERM;
	}

#if CONFIG_MACF
	error = mac_system_check_kas_info(kauth_cred_get(), selector);
	if (error) {
		return error;
	}
#endif

	if (IS_64BIT_PROCESS(p)) {
		user64_size_t size64;
		error = copyin(sizep, &size64, sizeof(size64));
		size = (user_size_t)size64;
	} else {
		user32_size_t size32;
		error = copyin(sizep, &size32, sizeof(size32));
		size = (user_size_t)size32;
	}
	if (error) {
		return error;
	}

	switch (selector) {
	case KAS_INFO_KERNEL_TEXT_SLIDE_SELECTOR:
	{
		uint64_t slide = vm_kernel_slide;

		if (sizeof(slide) != size) {
			return EINVAL;
		}

		error = copyout(&slide, valuep, sizeof(slide));
		if (error) {
			return error;
		}
		rsize = size;
	}
	break;
	case KAS_INFO_KERNEL_SEGMENT_VMADDR_SELECTOR:
	{
		uint32_t i;
		kernel_mach_header_t *mh = &_mh_execute_header;
		struct load_command *cmd;
		cmd = (struct load_command*) &mh[1];
		uint64_t *bases;
		rsize = mh->ncmds * sizeof(uint64_t);

		/*
		 * Return the size if no data was passed
		 */
		if (valuep == 0) {
			break;
		}

		if (rsize > size) {
			return EINVAL;
		}

		bases = kalloc_data(rsize, Z_WAITOK | Z_ZERO);

		for (i = 0; i < mh->ncmds; i++) {
			if (cmd->cmd == LC_SEGMENT_KERNEL) {
				__IGNORE_WCASTALIGN(kernel_segment_command_t * sg = (kernel_segment_command_t *) cmd);
				bases[i] = (uint64_t)sg->vmaddr;
			}
			cmd = (struct load_command *) ((uintptr_t) cmd + cmd->cmdsize);
		}

		error = copyout(bases, valuep, rsize);

		kfree_data(bases, rsize);

		if (error) {
			return error;
		}
	}
	break;
	case KAS_INFO_SPTM_TEXT_SLIDE_SELECTOR:
	case KAS_INFO_TXM_TEXT_SLIDE_SELECTOR:
	{
#if CONFIG_SPTM
		const uint64_t slide =
		    (selector == KAS_INFO_SPTM_TEXT_SLIDE_SELECTOR) ? vm_sptm_offsets.slide : vm_txm_offsets.slide;
#else
		const uint64_t slide = 0;
#endif

		if (sizeof(slide) != size) {
			return EINVAL;
		}

		error = copyout(&slide, valuep, sizeof(slide));
		if (error) {
			return error;
		}
		rsize = size;
	}
	break;
	default:
		return EINVAL;
	}

	if (IS_64BIT_PROCESS(p)) {
		user64_size_t size64 = (user64_size_t)rsize;
		error = copyout(&size64, sizep, sizeof(size64));
	} else {
		user32_size_t size32 = (user32_size_t)rsize;
		error = copyout(&size32, sizep, sizeof(size32));
	}

	return error;
#endif /* CONFIG_KAS_INFO */
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-qual"
#pragma clang diagnostic ignored "-Wunused-function"

static void
asserts()
{
	static_assert(sizeof(vm_min_kernel_address) == sizeof(unsigned long));
	static_assert(sizeof(vm_max_kernel_address) == sizeof(unsigned long));
}

SYSCTL_ULONG(_vm, OID_AUTO, vm_min_kernel_address, CTLFLAG_RD, (unsigned long *) &vm_min_kernel_address, "");
SYSCTL_ULONG(_vm, OID_AUTO, vm_max_kernel_address, CTLFLAG_RD, (unsigned long *) &vm_max_kernel_address, "");
#pragma clang diagnostic pop

extern uint32_t vm_page_pages;
SYSCTL_UINT(_vm, OID_AUTO, pages, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_page_pages, 0, "");

extern uint32_t vm_page_busy_absent_skipped;
SYSCTL_UINT(_vm, OID_AUTO, page_busy_absent_skipped, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_page_busy_absent_skipped, 0, "");

extern uint32_t vm_page_upl_tainted;
SYSCTL_UINT(_vm, OID_AUTO, upl_pages_tainted, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_page_upl_tainted, 0, "");

extern uint32_t vm_page_iopl_tainted;
SYSCTL_UINT(_vm, OID_AUTO, iopl_pages_tainted, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_page_iopl_tainted, 0, "");

#if __arm64__ && (DEVELOPMENT || DEBUG)
extern int vm_footprint_suspend_allowed;
SYSCTL_INT(_vm, OID_AUTO, footprint_suspend_allowed, CTLFLAG_RW | CTLFLAG_LOCKED, &vm_footprint_suspend_allowed, 0, "");

extern void pmap_footprint_suspend(vm_map_t map, boolean_t suspend);
static int
sysctl_vm_footprint_suspend SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg1, arg2)
	int error = 0;
	int new_value;

	if (req->newptr == USER_ADDR_NULL) {
		return 0;
	}
	error = SYSCTL_IN(req, &new_value, sizeof(int));
	if (error) {
		return error;
	}
	if (!vm_footprint_suspend_allowed) {
		if (new_value != 0) {
			/* suspends are not allowed... */
			return 0;
		}
		/* ... but let resumes proceed */
	}
	DTRACE_VM2(footprint_suspend,
	    vm_map_t, current_map(),
	    int, new_value);

	pmap_footprint_suspend(current_map(), new_value);

	return 0;
}
SYSCTL_PROC(_vm, OID_AUTO, footprint_suspend,
    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_ANYBODY | CTLFLAG_LOCKED | CTLFLAG_MASKED,
    0, 0, &sysctl_vm_footprint_suspend, "I", "");
#endif /* __arm64__ && (DEVELOPMENT || DEBUG) */

extern uint64_t vm_map_corpse_footprint_count;
extern uint64_t vm_map_corpse_footprint_size_avg;
extern uint64_t vm_map_corpse_footprint_size_max;
extern uint64_t vm_map_corpse_footprint_full;
extern uint64_t vm_map_corpse_footprint_no_buf;
SYSCTL_QUAD(_vm, OID_AUTO, corpse_footprint_count,
    CTLFLAG_RD | CTLFLAG_LOCKED, &vm_map_corpse_footprint_count, "");
SYSCTL_QUAD(_vm, OID_AUTO, corpse_footprint_size_avg,
    CTLFLAG_RD | CTLFLAG_LOCKED, &vm_map_corpse_footprint_size_avg, "");
SYSCTL_QUAD(_vm, OID_AUTO, corpse_footprint_size_max,
    CTLFLAG_RD | CTLFLAG_LOCKED, &vm_map_corpse_footprint_size_max, "");
SYSCTL_QUAD(_vm, OID_AUTO, corpse_footprint_full,
    CTLFLAG_RD | CTLFLAG_LOCKED, &vm_map_corpse_footprint_full, "");
SYSCTL_QUAD(_vm, OID_AUTO, corpse_footprint_no_buf,
    CTLFLAG_RD | CTLFLAG_LOCKED, &vm_map_corpse_footprint_no_buf, "");

#if CODE_SIGNING_MONITOR
extern uint64_t vm_cs_defer_to_csm;
extern uint64_t vm_cs_defer_to_csm_not;
SYSCTL_QUAD(_vm, OID_AUTO, cs_defer_to_csm,
    CTLFLAG_RD | CTLFLAG_LOCKED, &vm_cs_defer_to_csm, "");
SYSCTL_QUAD(_vm, OID_AUTO, cs_defer_to_csm_not,
    CTLFLAG_RD | CTLFLAG_LOCKED, &vm_cs_defer_to_csm_not, "");
#endif /* CODE_SIGNING_MONITOR */

extern uint64_t shared_region_pager_copied;
extern uint64_t shared_region_pager_slid;
extern uint64_t shared_region_pager_slid_error;
extern uint64_t shared_region_pager_reclaimed;
SYSCTL_QUAD(_vm, OID_AUTO, shared_region_pager_copied,
    CTLFLAG_RD | CTLFLAG_LOCKED, &shared_region_pager_copied, "");
SYSCTL_QUAD(_vm, OID_AUTO, shared_region_pager_slid,
    CTLFLAG_RD | CTLFLAG_LOCKED, &shared_region_pager_slid, "");
SYSCTL_QUAD(_vm, OID_AUTO, shared_region_pager_slid_error,
    CTLFLAG_RD | CTLFLAG_LOCKED, &shared_region_pager_slid_error, "");
SYSCTL_QUAD(_vm, OID_AUTO, shared_region_pager_reclaimed,
    CTLFLAG_RD | CTLFLAG_LOCKED, &shared_region_pager_reclaimed, "");
extern int shared_region_destroy_delay;
SYSCTL_INT(_vm, OID_AUTO, shared_region_destroy_delay,
    CTLFLAG_RW | CTLFLAG_LOCKED, &shared_region_destroy_delay, 0, "");

#if MACH_ASSERT
extern int pmap_ledgers_panic_leeway;
SYSCTL_INT(_vm, OID_AUTO, pmap_ledgers_panic_leeway, CTLFLAG_RW | CTLFLAG_LOCKED, &pmap_ledgers_panic_leeway, 0, "");
#endif /* MACH_ASSERT */


extern uint64_t vm_map_lookup_and_lock_object_copy_slowly_count;
extern uint64_t vm_map_lookup_and_lock_object_copy_slowly_size;
extern uint64_t vm_map_lookup_and_lock_object_copy_slowly_max;
extern uint64_t vm_map_lookup_and_lock_object_copy_slowly_restart;
extern uint64_t vm_map_lookup_and_lock_object_copy_slowly_error;
extern uint64_t vm_map_lookup_and_lock_object_copy_strategically_count;
extern uint64_t vm_map_lookup_and_lock_object_copy_strategically_size;
extern uint64_t vm_map_lookup_and_lock_object_copy_strategically_max;
extern uint64_t vm_map_lookup_and_lock_object_copy_strategically_restart;
extern uint64_t vm_map_lookup_and_lock_object_copy_strategically_error;
extern uint64_t vm_map_lookup_and_lock_object_copy_shadow_count;
extern uint64_t vm_map_lookup_and_lock_object_copy_shadow_size;
extern uint64_t vm_map_lookup_and_lock_object_copy_shadow_max;
SYSCTL_QUAD(_vm, OID_AUTO, map_lookup_locked_copy_slowly_count,
    CTLFLAG_RD | CTLFLAG_LOCKED, &vm_map_lookup_and_lock_object_copy_slowly_count, "");
SYSCTL_QUAD(_vm, OID_AUTO, map_lookup_locked_copy_slowly_size,
    CTLFLAG_RD | CTLFLAG_LOCKED, &vm_map_lookup_and_lock_object_copy_slowly_size, "");
SYSCTL_QUAD(_vm, OID_AUTO, map_lookup_locked_copy_slowly_max,
    CTLFLAG_RD | CTLFLAG_LOCKED, &vm_map_lookup_and_lock_object_copy_slowly_max, "");
SYSCTL_QUAD(_vm, OID_AUTO, map_lookup_locked_copy_slowly_restart,
    CTLFLAG_RD | CTLFLAG_LOCKED, &vm_map_lookup_and_lock_object_copy_slowly_restart, "");
SYSCTL_QUAD(_vm, OID_AUTO, map_lookup_locked_copy_slowly_error,
    CTLFLAG_RD | CTLFLAG_LOCKED, &vm_map_lookup_and_lock_object_copy_slowly_error, "");
SYSCTL_QUAD(_vm, OID_AUTO, map_lookup_locked_copy_strategically_count,
    CTLFLAG_RD | CTLFLAG_LOCKED, &vm_map_lookup_and_lock_object_copy_strategically_count, "");
SYSCTL_QUAD(_vm, OID_AUTO, map_lookup_locked_copy_strategically_size,
    CTLFLAG_RD | CTLFLAG_LOCKED, &vm_map_lookup_and_lock_object_copy_strategically_size, "");
SYSCTL_QUAD(_vm, OID_AUTO, map_lookup_locked_copy_strategically_max,
    CTLFLAG_RD | CTLFLAG_LOCKED, &vm_map_lookup_and_lock_object_copy_strategically_max, "");
SYSCTL_QUAD(_vm, OID_AUTO, map_lookup_locked_copy_strategically_restart,
    CTLFLAG_RD | CTLFLAG_LOCKED, &vm_map_lookup_and_lock_object_copy_strategically_restart, "");
SYSCTL_QUAD(_vm, OID_AUTO, map_lookup_locked_copy_strategically_error,
    CTLFLAG_RD | CTLFLAG_LOCKED, &vm_map_lookup_and_lock_object_copy_strategically_error, "");
SYSCTL_QUAD(_vm, OID_AUTO, map_lookup_locked_copy_shadow_count,
    CTLFLAG_RD | CTLFLAG_LOCKED, &vm_map_lookup_and_lock_object_copy_shadow_count, "");
SYSCTL_QUAD(_vm, OID_AUTO, map_lookup_locked_copy_shadow_size,
    CTLFLAG_RD | CTLFLAG_LOCKED, &vm_map_lookup_and_lock_object_copy_shadow_size, "");
SYSCTL_QUAD(_vm, OID_AUTO, map_lookup_locked_copy_shadow_max,
    CTLFLAG_RD | CTLFLAG_LOCKED, &vm_map_lookup_and_lock_object_copy_shadow_max, "");

extern int vm_protect_privileged_from_untrusted;
SYSCTL_INT(_vm, OID_AUTO, protect_privileged_from_untrusted,
    CTLFLAG_RW | CTLFLAG_LOCKED, &vm_protect_privileged_from_untrusted, 0, "");
extern uint64_t vm_copied_on_read;
SYSCTL_QUAD(_vm, OID_AUTO, copied_on_read,
    CTLFLAG_RD | CTLFLAG_LOCKED, &vm_copied_on_read, "");

extern int vm_shared_region_count;
extern int vm_shared_region_peak;
SYSCTL_INT(_vm, OID_AUTO, shared_region_count,
    CTLFLAG_RD | CTLFLAG_LOCKED, &vm_shared_region_count, 0, "");
SYSCTL_INT(_vm, OID_AUTO, shared_region_peak,
    CTLFLAG_RD | CTLFLAG_LOCKED, &vm_shared_region_peak, 0, "");
#if DEVELOPMENT || DEBUG
extern unsigned int shared_region_pagers_resident_count;
SYSCTL_INT(_vm, OID_AUTO, shared_region_pagers_resident_count,
    CTLFLAG_RD | CTLFLAG_LOCKED, &shared_region_pagers_resident_count, 0, "");
extern unsigned int shared_region_pagers_resident_peak;
SYSCTL_INT(_vm, OID_AUTO, shared_region_pagers_resident_peak,
    CTLFLAG_RD | CTLFLAG_LOCKED, &shared_region_pagers_resident_peak, 0, "");
extern int shared_region_pager_count;
SYSCTL_INT(_vm, OID_AUTO, shared_region_pager_count,
    CTLFLAG_RD | CTLFLAG_LOCKED, &shared_region_pager_count, 0, "");
#if __has_feature(ptrauth_calls)
extern int shared_region_key_count;
SYSCTL_INT(_vm, OID_AUTO, shared_region_key_count,
    CTLFLAG_RD | CTLFLAG_LOCKED, &shared_region_key_count, 0, "");
extern int vm_shared_region_reslide_count;
SYSCTL_INT(_vm, OID_AUTO, shared_region_reslide_count,
    CTLFLAG_RD | CTLFLAG_LOCKED, &vm_shared_region_reslide_count, 0, "");
#endif /* __has_feature(ptrauth_calls) */
#endif /* DEVELOPMENT || DEBUG */

#if MACH_ASSERT
extern int debug4k_filter;
SYSCTL_INT(_vm, OID_AUTO, debug4k_filter, CTLFLAG_RW | CTLFLAG_LOCKED, &debug4k_filter, 0, "");
extern int debug4k_panic_on_terminate;
SYSCTL_INT(_vm, OID_AUTO, debug4k_panic_on_terminate, CTLFLAG_RW | CTLFLAG_LOCKED, &debug4k_panic_on_terminate, 0, "");
extern int debug4k_panic_on_exception;
SYSCTL_INT(_vm, OID_AUTO, debug4k_panic_on_exception, CTLFLAG_RW | CTLFLAG_LOCKED, &debug4k_panic_on_exception, 0, "");
extern int debug4k_panic_on_misaligned_sharing;
SYSCTL_INT(_vm, OID_AUTO, debug4k_panic_on_misaligned_sharing, CTLFLAG_RW | CTLFLAG_LOCKED, &debug4k_panic_on_misaligned_sharing, 0, "");
#endif /* MACH_ASSERT */

extern uint64_t vm_map_set_size_limit_count;
extern uint64_t vm_map_set_data_limit_count;
extern uint64_t vm_map_enter_RLIMIT_AS_count;
extern uint64_t vm_map_enter_RLIMIT_DATA_count;
SYSCTL_QUAD(_vm, OID_AUTO, map_set_size_limit_count, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_map_set_size_limit_count, "");
SYSCTL_QUAD(_vm, OID_AUTO, map_set_data_limit_count, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_map_set_data_limit_count, "");
SYSCTL_QUAD(_vm, OID_AUTO, map_enter_RLIMIT_AS_count, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_map_enter_RLIMIT_AS_count, "");
SYSCTL_QUAD(_vm, OID_AUTO, map_enter_RLIMIT_DATA_count, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_map_enter_RLIMIT_DATA_count, "");

extern uint64_t vm_fault_resilient_media_initiate;
extern uint64_t vm_fault_resilient_media_retry;
extern uint64_t vm_fault_resilient_media_proceed;
extern uint64_t vm_fault_resilient_media_release;
extern uint64_t vm_fault_resilient_media_abort1;
extern uint64_t vm_fault_resilient_media_abort2;
SYSCTL_QUAD(_vm, OID_AUTO, fault_resilient_media_initiate, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_fault_resilient_media_initiate, "");
SYSCTL_QUAD(_vm, OID_AUTO, fault_resilient_media_retry, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_fault_resilient_media_retry, "");
SYSCTL_QUAD(_vm, OID_AUTO, fault_resilient_media_proceed, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_fault_resilient_media_proceed, "");
SYSCTL_QUAD(_vm, OID_AUTO, fault_resilient_media_release, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_fault_resilient_media_release, "");
SYSCTL_QUAD(_vm, OID_AUTO, fault_resilient_media_abort1, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_fault_resilient_media_abort1, "");
SYSCTL_QUAD(_vm, OID_AUTO, fault_resilient_media_abort2, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_fault_resilient_media_abort2, "");
#if MACH_ASSERT
extern int vm_fault_resilient_media_inject_error1_rate;
extern int vm_fault_resilient_media_inject_error1;
extern int vm_fault_resilient_media_inject_error2_rate;
extern int vm_fault_resilient_media_inject_error2;
extern int vm_fault_resilient_media_inject_error3_rate;
extern int vm_fault_resilient_media_inject_error3;
SYSCTL_INT(_vm, OID_AUTO, fault_resilient_media_inject_error1_rate, CTLFLAG_RW | CTLFLAG_LOCKED, &vm_fault_resilient_media_inject_error1_rate, 0, "");
SYSCTL_INT(_vm, OID_AUTO, fault_resilient_media_inject_error1, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_fault_resilient_media_inject_error1, 0, "");
SYSCTL_INT(_vm, OID_AUTO, fault_resilient_media_inject_error2_rate, CTLFLAG_RW | CTLFLAG_LOCKED, &vm_fault_resilient_media_inject_error2_rate, 0, "");
SYSCTL_INT(_vm, OID_AUTO, fault_resilient_media_inject_error2, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_fault_resilient_media_inject_error2, 0, "");
SYSCTL_INT(_vm, OID_AUTO, fault_resilient_media_inject_error3_rate, CTLFLAG_RW | CTLFLAG_LOCKED, &vm_fault_resilient_media_inject_error3_rate, 0, "");
SYSCTL_INT(_vm, OID_AUTO, fault_resilient_media_inject_error3, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_fault_resilient_media_inject_error3, 0, "");
#endif /* MACH_ASSERT */

extern uint64_t pmap_query_page_info_retries;
SYSCTL_QUAD(_vm, OID_AUTO, pmap_query_page_info_retries, CTLFLAG_RD | CTLFLAG_LOCKED, &pmap_query_page_info_retries, "");

/*
 * A sysctl which causes all existing shared regions to become stale. They
 * will no longer be used by anything new and will be torn down as soon as
 * the last existing user exits. A write of non-zero value causes that to happen.
 * This should only be used by launchd, so we check that this is initproc.
 */
static int
shared_region_pivot(__unused struct sysctl_oid *oidp, __unused void *arg1, __unused int arg2, struct sysctl_req *req)
{
	unsigned int value = 0;
	int changed = 0;
	int error = sysctl_io_number(req, 0, sizeof(value), &value, &changed);
	if (error || !changed) {
		return error;
	}
	if (current_proc() != initproc) {
		return EPERM;
	}

	vm_shared_region_pivot();

	return 0;
}

SYSCTL_PROC(_vm, OID_AUTO, shared_region_pivot,
    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_LOCKED,
    0, 0, shared_region_pivot, "I", "");

extern uint64_t vm_object_shadow_forced;
extern uint64_t vm_object_shadow_skipped;
SYSCTL_QUAD(_vm, OID_AUTO, object_shadow_forced, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_object_shadow_forced, "");
SYSCTL_QUAD(_vm, OID_AUTO, object_shadow_skipped, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vm_object_shadow_skipped, "");

SYSCTL_INT(_vm, OID_AUTO, vmtc_total, CTLFLAG_RD | CTLFLAG_LOCKED,
    &vmtc_total, 0, "total text page corruptions detected");


#if DEBUG || DEVELOPMENT
/*
 * A sysctl that can be used to corrupt a text page with an illegal instruction.
 * Used for testing text page self healing.
 */
extern kern_return_t vm_corrupt_text_addr(uintptr_t);
static int
corrupt_text_addr(__unused struct sysctl_oid *oidp, __unused void *arg1, __unused int arg2, struct sysctl_req *req)
{
	uint64_t value = 0;
	int error = sysctl_handle_quad(oidp, &value, 0, req);
	if (error || !req->newptr) {
		return error;
	}

	if (vm_corrupt_text_addr((uintptr_t)value) == KERN_SUCCESS) {
		return 0;
	} else {
		return EINVAL;
	}
}

SYSCTL_PROC(_vm, OID_AUTO, corrupt_text_addr,
    CTLTYPE_QUAD | CTLFLAG_WR | CTLFLAG_LOCKED | CTLFLAG_MASKED,
    0, 0, corrupt_text_addr, "-", "");
#endif /* DEBUG || DEVELOPMENT */

#if CONFIG_MAP_RANGES
/*
 * vm.malloc_ranges
 *
 * space-separated list of <left:right> hexadecimal addresses.
 */
static int
vm_map_malloc_ranges SYSCTL_HANDLER_ARGS
{
	vm_map_t map = current_map();
	struct mach_vm_range r1, r2;
	char str[20 * 4];
	int len;
	mach_vm_offset_t right_hole_max;

	if (vm_map_get_user_range(map, UMEM_RANGE_ID_DEFAULT, &r1)) {
		return ENOENT;
	}
	if (vm_map_get_user_range(map, UMEM_RANGE_ID_HEAP, &r2)) {
		return ENOENT;
	}

#if XNU_TARGET_OS_IOS && EXTENDED_USER_VA_SUPPORT
	right_hole_max = MACH_VM_JUMBO_ADDRESS;
#else /* !XNU_TARGET_OS_IOS || !EXTENDED_USER_VA_SUPPORT */
	right_hole_max = get_map_max(map);
#endif /* XNU_TARGET_OS_IOS && EXTENDED_USER_VA_SUPPORT */

	len = scnprintf(str, sizeof(str), "0x%llx:0x%llx 0x%llx:0x%llx",
	    r1.max_address, r2.min_address,
	    r2.max_address, right_hole_max);

	return SYSCTL_OUT(req, str, len);
}

SYSCTL_PROC(_vm, OID_AUTO, malloc_ranges,
    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_LOCKED | CTLFLAG_MASKED,
    0, 0, &vm_map_malloc_ranges, "A", "");

#if DEBUG || DEVELOPMENT
static int
vm_map_user_range_default SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2, oidp)
	struct mach_vm_range range;

	if (vm_map_get_user_range(current_map(), UMEM_RANGE_ID_DEFAULT, &range)
	    != KERN_SUCCESS) {
		return EINVAL;
	}

	return SYSCTL_OUT(req, &range, sizeof(range));
}

static int
vm_map_user_range_heap SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2, oidp)
	struct mach_vm_range range;

	if (vm_map_get_user_range(current_map(), UMEM_RANGE_ID_HEAP, &range)
	    != KERN_SUCCESS) {
		return EINVAL;
	}

	return SYSCTL_OUT(req, &range, sizeof(range));
}

static int
vm_map_user_range_large_file SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2, oidp)
	struct mach_vm_range range;

	if (vm_map_get_user_range(current_map(), UMEM_RANGE_ID_LARGE_FILE, &range)
	    != KERN_SUCCESS) {
		return EINVAL;
	}

	return SYSCTL_OUT(req, &range, sizeof(range));
}

/*
 * A sysctl that can be used to return ranges for the current VM map.
 * Used for testing VM ranges.
 */
SYSCTL_PROC(_vm, OID_AUTO, vm_map_user_range_default, CTLTYPE_STRUCT | CTLFLAG_RD | CTLFLAG_LOCKED,
    0, 0, &vm_map_user_range_default, "S,mach_vm_range", "");
SYSCTL_PROC(_vm, OID_AUTO, vm_map_user_range_heap, CTLTYPE_STRUCT | CTLFLAG_RD | CTLFLAG_LOCKED,
    0, 0, &vm_map_user_range_heap, "S,mach_vm_range", "");
SYSCTL_PROC(_vm, OID_AUTO, vm_map_user_range_large_file, CTLTYPE_STRUCT | CTLFLAG_RD | CTLFLAG_LOCKED,
    0, 0, &vm_map_user_range_large_file, "S,mach_vm_range", "");

#endif /* DEBUG || DEVELOPMENT */
#endif /* CONFIG_MAP_RANGES */

#if DEBUG || DEVELOPMENT
#endif /* DEBUG || DEVELOPMENT */

extern uint64_t vm_map_range_overflows_count;
SYSCTL_QUAD(_vm, OID_AUTO, map_range_overflows_count, CTLFLAG_RD | CTLFLAG_LOCKED, &vm_map_range_overflows_count, "");
extern boolean_t vm_map_range_overflows_log;
SYSCTL_INT(_vm, OID_AUTO, map_range_oveflows_log, CTLFLAG_RW | CTLFLAG_LOCKED, &vm_map_range_overflows_log, 0, "");

extern uint64_t c_seg_filled_no_contention;
extern uint64_t c_seg_filled_contention;
extern clock_sec_t c_seg_filled_contention_sec_max;
extern clock_nsec_t c_seg_filled_contention_nsec_max;
SYSCTL_QUAD(_vm, OID_AUTO, c_seg_filled_no_contention, CTLFLAG_RD | CTLFLAG_LOCKED, &c_seg_filled_no_contention, "");
SYSCTL_QUAD(_vm, OID_AUTO, c_seg_filled_contention, CTLFLAG_RD | CTLFLAG_LOCKED, &c_seg_filled_contention, "");
SYSCTL_ULONG(_vm, OID_AUTO, c_seg_filled_contention_sec_max, CTLFLAG_RD | CTLFLAG_LOCKED, &c_seg_filled_contention_sec_max, "");
SYSCTL_UINT(_vm, OID_AUTO, c_seg_filled_contention_nsec_max, CTLFLAG_RD | CTLFLAG_LOCKED, &c_seg_filled_contention_nsec_max, 0, "");
#if (XNU_TARGET_OS_OSX && __arm64__)
extern clock_nsec_t c_process_major_report_over_ms; /* report if over ? ms */
extern int c_process_major_yield_after; /* yield after moving ? segments */
extern uint64_t c_process_major_reports;
extern clock_sec_t c_process_major_max_sec;
extern clock_nsec_t c_process_major_max_nsec;
extern uint32_t c_process_major_peak_segcount;
SYSCTL_UINT(_vm, OID_AUTO, c_process_major_report_over_ms, CTLFLAG_RW | CTLFLAG_LOCKED, &c_process_major_report_over_ms, 0, "");
SYSCTL_INT(_vm, OID_AUTO, c_process_major_yield_after, CTLFLAG_RW | CTLFLAG_LOCKED, &c_process_major_yield_after, 0, "");
SYSCTL_QUAD(_vm, OID_AUTO, c_process_major_reports, CTLFLAG_RD | CTLFLAG_LOCKED, &c_process_major_reports, "");
SYSCTL_ULONG(_vm, OID_AUTO, c_process_major_max_sec, CTLFLAG_RD | CTLFLAG_LOCKED, &c_process_major_max_sec, "");
SYSCTL_UINT(_vm, OID_AUTO, c_process_major_max_nsec, CTLFLAG_RD | CTLFLAG_LOCKED, &c_process_major_max_nsec, 0, "");
SYSCTL_UINT(_vm, OID_AUTO, c_process_major_peak_segcount, CTLFLAG_RD | CTLFLAG_LOCKED, &c_process_major_peak_segcount, 0, "");
#endif /* (XNU_TARGET_OS_OSX && __arm64__) */

#if DEVELOPMENT || DEBUG
extern int panic_object_not_alive;
SYSCTL_INT(_vm, OID_AUTO, panic_object_not_alive, CTLFLAG_RW | CTLFLAG_LOCKED | CTLFLAG_ANYBODY, &panic_object_not_alive, 0, "");
#endif /* DEVELOPMENT || DEBUG */

#if FBDP_DEBUG_OBJECT_NO_PAGER
extern int fbdp_no_panic;
SYSCTL_INT(_vm, OID_AUTO, fbdp_no_panic, CTLFLAG_RW | CTLFLAG_LOCKED | CTLFLAG_ANYBODY, &fbdp_no_panic, 0, "");
#endif /* MACH_ASSERT */


#if DEVELOPMENT || DEBUG


/* The largest possible single segment + its slots is (sizeof(c_segment_info) + C_SLOT_MAX_INDEX * sizeof(c_slot_info)), so this should be enough  */
#define SYSCTL_SEG_BUF_SIZE (8 * 1024)

extern uint32_t c_segments_available;

struct sysctl_buf_header {
	uint32_t magic;
} __attribute__((packed));

/* This sysctl iterates over the populated c_segments and writes some info about each one and its slots.
 * instead of doing everything here, the function calls a function vm_compressor.c. */
static int
sysctl_compressor_segments(__unused struct sysctl_oid *oidp, __unused void *arg1, __unused int arg2, struct sysctl_req *req)
{
	char* buf = kalloc_data(SYSCTL_SEG_BUF_SIZE, Z_WAITOK | Z_ZERO);
	if (!buf) {
		return ENOMEM;
	}
	size_t offset = 0;
	int error = 0;
	int segno = 0;
	/* 4 byte header to identify the version of the formatting of the data.
	 * This should be incremented if c_segment_info or c_slot_info are changed */
	((struct sysctl_buf_header*)buf)->magic = VM_C_SEGMENT_INFO_MAGIC;
	offset += sizeof(uint32_t);

	while (segno < c_segments_available) {
		size_t left_sz = SYSCTL_SEG_BUF_SIZE - offset;
		kern_return_t kr = vm_compressor_serialize_segment_debug_info(segno, buf + offset, &left_sz);
		if (kr == KERN_NO_SPACE) {
			/* failed to add another segment, push the current buffer out and try again */
			if (offset == 0) {
				error = EINVAL; /* no space to write but I didn't write anything, shouldn't really happen */
				goto out;
			}
			/* write out chunk */
			error = SYSCTL_OUT(req, buf, offset);
			if (error) {
				goto out;
			}
			offset = 0;
			bzero(buf, SYSCTL_SEG_BUF_SIZE); /* zero any reserved bits that are not going to be filled */
			/* don't increment segno, need to try again saving the current one */
		} else if (kr != KERN_SUCCESS) {
			error = EINVAL;
			goto out;
		} else {
			offset += left_sz;
			++segno;
		}
	}

	if (offset > 0) { /* write last chunk */
		error = SYSCTL_OUT(req, buf, offset);
	}

out:
	kfree_data(buf, SYSCTL_SEG_BUF_SIZE)
	return error;
}

SYSCTL_PROC(_vm, OID_AUTO, compressor_segments, CTLTYPE_STRUCT | CTLFLAG_LOCKED | CTLFLAG_RD, 0, 0, sysctl_compressor_segments, "S", "");


extern uint32_t vm_compressor_fragmentation_level(void);

static int
sysctl_compressor_fragmentation_level(__unused struct sysctl_oid *oidp, __unused void *arg1, __unused int arg2, struct sysctl_req *req)
{
	uint32_t value = vm_compressor_fragmentation_level();
	return SYSCTL_OUT(req, &value, sizeof(value));
}

SYSCTL_PROC(_vm, OID_AUTO, compressor_fragmentation_level, CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_LOCKED, 0, 0, sysctl_compressor_fragmentation_level, "IU", "");


#define SYSCTL_VM_OBJECTS_SLOTMAP_BUF_SIZE (8 * 1024)


/* This sysctl iterates over all the entries of the vm_map of the a given process and write some info about the vm_object pointed by the entries.
 * This can be used for mapping where are all the pages of a process located in the compressor.
 */
static int
sysctl_task_vm_objects_slotmap(__unused struct sysctl_oid *oidp, void *arg1, int arg2, struct sysctl_req *req)
{
	int error = 0;
	char *buf = NULL;
	proc_t p = PROC_NULL;
	task_t task = TASK_NULL;
	vm_map_t map = VM_MAP_NULL;
	__block size_t offset = 0;

	/* go from pid to proc to task to vm_map. see sysctl_procargsx() for another example of this procession */
	int *name = arg1;
	int namelen = arg2;
	if (namelen < 1) {
		return EINVAL;
	}
	int pid = name[0];
	p = proc_find(pid);  /* this increments a reference to the proc */
	if (p == PROC_NULL) {
		return EINVAL;
	}
	task = proc_task(p);
	proc_rele(p);  /* decrement ref of proc */
	p = PROC_NULL;
	if (task == TASK_NULL) {
		return EINVAL;
	}
	/* convert proc reference to task reference */
	task_reference(task);
	/* task reference to map reference */
	map = get_task_map_reference(task);
	task_deallocate(task);

	if (map == VM_MAP_NULL) {
		return EINVAL;  /* nothing allocated yet */
	}

	buf = kalloc_data(SYSCTL_VM_OBJECTS_SLOTMAP_BUF_SIZE, Z_WAITOK | Z_ZERO);
	if (!buf) {
		error = ENOMEM;
		goto out;
	}

	/* 4 byte header to identify the version of the formatting of the data.
	 * This should be incremented if c_segment_info or c_slot_info are changed */
	((struct sysctl_buf_header*)buf)->magic = VM_MAP_ENTRY_INFO_MAGIC;
	offset += sizeof(uint32_t);

	kern_return_t (^write_header)(int) = ^kern_return_t (int nentries) {
		/* write the header, happens only once at the beginning so we should have enough space */
		assert(offset + sizeof(struct vm_map_info_hdr) < SYSCTL_VM_OBJECTS_SLOTMAP_BUF_SIZE);
		struct vm_map_info_hdr* out_hdr = (struct vm_map_info_hdr*)(buf + offset);
		out_hdr->vmi_nentries = nentries;
		offset += sizeof(struct vm_map_info_hdr);
		return KERN_SUCCESS;
	};

	kern_return_t (^write_entry)(void*) = ^kern_return_t (void* entry) {
		while (true) { /* try up to 2 times, first try write the the current buffer, otherwise to a new buffer */
			size_t left_sz = SYSCTL_VM_OBJECTS_SLOTMAP_BUF_SIZE - offset;
			kern_return_t kr = vm_map_dump_entry_and_compressor_pager(entry, buf + offset, &left_sz);
			if (kr == KERN_NO_SPACE) {
				/* failed to write anything, flush the current buffer and try again */
				if (offset == 0) {
					return KERN_FAILURE; /* no space to write but I didn't write anything yet, shouldn't really happen */
				}
				/* write out chunk */
				int out_error = SYSCTL_OUT(req, buf, offset);
				if (out_error) {
					return KERN_FAILURE;
				}
				offset = 0;
				bzero(buf, SYSCTL_VM_OBJECTS_SLOTMAP_BUF_SIZE); /* zero any reserved bits that are not going to be filled */
				continue; /* need to retry the entry dump again with the cleaned buffer */
			} else if (kr != KERN_SUCCESS) {
				return kr;
			}
			offset += left_sz;
			break;
		}
		return KERN_SUCCESS;
	};

	/* this foreach first calls to the first callback with the number of entries, then calls the second for every entry
	 * when the buffer is exhausted, it is flushed to the sysctl and restarted */
	kern_return_t kr = vm_map_entries_foreach(map, write_header, write_entry);

	if (kr != KERN_SUCCESS) {
		goto out;
	}

	if (offset > 0) { /* last chunk */
		error = SYSCTL_OUT(req, buf, offset);
	}

out:
	if (buf != NULL) {
		kfree_data(buf, SYSCTL_VM_OBJECTS_SLOTMAP_BUF_SIZE)
	}
	if (map != NULL) {
		vm_map_deallocate(map);
	}
	return error;
}

SYSCTL_PROC(_vm, OID_AUTO, task_vm_objects_slotmap, CTLTYPE_NODE | CTLFLAG_LOCKED | CTLFLAG_RD, 0, 0, sysctl_task_vm_objects_slotmap, "S", "");



#endif /* DEVELOPMENT || DEBUG */
