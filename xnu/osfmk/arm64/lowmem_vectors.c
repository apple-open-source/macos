/*
 * Copyright (c) 2012-2013 Apple Inc. All rights reserved.
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

#include <mach_kdp.h>
#include <mach/vm_param.h>
#include <arm64/lowglobals.h>
#include <vm/vm_object_xnu.h>
#include <vm/vm_page.h>

/*
 * On arm64, the low globals get mapped low via machine_init() during kernel
 * bootstrap.
 */

extern vm_offset_t vm_kernel_stext;
extern void     *version;
extern void     *kmod;
extern void     *kdp_trans_off;
extern void     *osversion;
extern void     *flag_kdp_trigger_reboot;
extern void     *manual_pkt;
extern struct vm_object pmap_object_store;      /* store pt pages */
extern vm_offset_t  c_buffers;
extern vm_size_t    c_buffers_size;

lowglo lowGlo __attribute__ ((aligned(PAGE_MAX_SIZE))) = {
	// Increment the major version for changes that break the current Astris
	// usage of lowGlo values
	// Increment the minor version for changes that provide additonal info/function
	// but does not break current usage
	.lgLayoutMajorVersion = 3,
	.lgLayoutMinorVersion = 3,
	.lgLayoutMagic = LOWGLO_LAYOUT_MAGIC,
	.lgVerCode = { 'K', 'r', 'a', 'k', 'e', 'n', ' ', ' ' },
	.lgZero = 0,
	.lgStext = 0, // To be filled in below
	.lgVersion = (uint64_t) &version,
	.lgOSVersion = (uint64_t) &osversion,
	.lgKmodptr = (uint64_t) &kmod,
#if MACH_KDP && CONFIG_KDP_INTERACTIVE_DEBUGGING
	.lgTransOff = (uint64_t) &kdp_trans_off,
	.lgRebootFlag = (uint64_t) &flag_kdp_trigger_reboot,
	.lgManualPktAddr = (uint64_t) &manual_pkt,
#endif
	.lgPmapMemQ = (uint64_t)&(pmap_object_store.memq),
	.lgPmapMemPageOffset = offsetof(struct vm_page_with_ppnum, vmp_phys_page),
	.lgPmapMemChainOffset = offsetof(struct vm_page, vmp_listq),
	.lgPmapMemPagesize = (uint64_t)sizeof(struct vm_page),
	.lgPmapMemFromArrayMask = VM_PAGE_PACKED_FROM_ARRAY,
	.lgPmapMemPackedShift = VM_PAGE_PACKED_PTR_SHIFT,
	.lgPmapMemPackedBaseAddr = VM_PAGE_PACKED_PTR_BASE,
	.lgPmapMemStartAddr = -1,
	.lgPmapMemEndAddr = -1,
	.lgPmapMemFirstppnum = -1,
	.lgPageShift = ARM_PGSHIFT,
	.lgVmFirstPhys = -1,
	.lgVmLastPhys = -1,
	.lgPhysMapBase = -1,
	.lgPhysMapEnd = -1,
	.lgPmapIoRangePtr = -1,
	.lgNumPmapIoRanges = -1,
	.lgCompressorBufferAddr = (uint64_t) &c_buffers,  // added in 3.3
	.lgCompressorSizeAddr   = (uint64_t) &c_buffers_size // added in 3.3
};

void
patch_low_glo(void)
{
	lowGlo.lgStext = (uint64_t)vm_kernel_stext;
}

void
patch_low_glo_static_region(uint64_t address, uint64_t size)
{
	lowGlo.lgStaticAddr = address;
	lowGlo.lgStaticSize = size;

	/**
	 * These values are set in pmap_bootstrap() and represent the range of
	 * kernel managed memory.
	 */
	extern const pmap_paddr_t vm_first_phys;
	extern const pmap_paddr_t vm_last_phys;
	assertf((vm_first_phys != 0) && (vm_last_phys != 0),
	    "Tried setting the Low Globals before pmap_bootstrap()");
	lowGlo.lgVmFirstPhys = vm_first_phys;
	lowGlo.lgVmLastPhys = vm_last_phys;

	/**
	 * These values are set in pmap_bootstrap() and represent an array of all
	 * kernel-managed I/O regions (pmap-io-ranges in the device tree). Some of
	 * these regions may include DRAM carved out for usage by other agents on
	 * the system.
	 *
	 * Need to forward-declare pmap_io_range_t since that only exists in the
	 * PMAP code.
	 */
	typedef struct pmap_io_range pmap_io_range_t;
	extern const pmap_io_range_t* io_attr_table;
	extern const unsigned int num_io_rgns;
	lowGlo.lgPmapIoRangePtr = (uint64_t)io_attr_table;
	lowGlo.lgNumPmapIoRanges = (uint64_t)num_io_rgns;

	/**
	 * These values are set in arm_vm_init() and represent the virtual address
	 * space used by the physical aperture.
	 */
	extern const vm_map_address_t physmap_base;
	extern const vm_map_address_t physmap_end;
	assertf((physmap_base != 0) && (physmap_end != 0),
	    "Tried setting the Low Globals before arm_vm_init()");
	lowGlo.lgPhysMapBase = physmap_base;
	lowGlo.lgPhysMapEnd = physmap_end;
}

void
patch_low_glo_vm_page_info(void * start_addr, void * end_addr, uint32_t first_ppnum)
{
	lowGlo.lgPmapMemStartAddr = (uint64_t)start_addr;
	lowGlo.lgPmapMemEndAddr = (uint64_t)end_addr;
	lowGlo.lgPmapMemFirstppnum = first_ppnum;
	lowGlo.lgPageShift = PAGE_SHIFT;
}
