/*
 * Copyright (c) 2000-2019 Apple Inc. All rights reserved.
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
 * Copyright (c) 1991,1990,1989, 1988 Carnegie Mellon University
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
 *	File:	model_dep.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Copyright (C) 1986, Avadis Tevanian, Jr., Michael Wayne Young
 *
 *	Basic initialization for I386 - ISA bus machines.
 */


#define __APPLE_API_PRIVATE 1
#define __APPLE_API_UNSTABLE 1
#include <kern/debug.h>

#include <mach/i386/vm_param.h>

#include <string.h>
#include <mach/vm_param.h>
#include <mach/vm_prot.h>
#include <mach/machine.h>
#include <mach/time_value.h>
#include <sys/kdebug.h>
#include <sys/time.h>
#include <kern/spl.h>
#include <kern/assert.h>
#include <kern/lock_group.h>
#include <kern/misc_protos.h>
#include <kern/startup.h>
#include <kern/clock.h>
#include <kern/cpu_data.h>
#include <kern/machine.h>
#include <kern/iotrace.h>
#include <kern/kern_stackshot.h>
#include <i386/postcode.h>
#include <i386/mp_desc.h>
#include <i386/misc_protos.h>
#include <i386/panic_notify.h>
#include <i386/thread.h>
#include <i386/trap_internal.h>
#include <i386/machine_routines.h>
#include <i386/mp.h>
#include <i386/cpuid.h>
#include <i386/fpu.h>
#include <i386/machine_cpu.h>
#include <i386/pmap.h>
#if CONFIG_MTRR
#include <i386/mtrr.h>
#endif
#include <i386/ucode.h>
#include <i386/pmCPU.h>
#include <i386/panic_hooks.h>
#include <i386/lbr.h>

#include <architecture/i386/pio.h> /* inb() */
#include <pexpert/i386/boot.h>

#include <kdp/kdp_dyld.h>
#include <kdp/kdp_core.h>
#include <kdp/kdp_common.h>
#include <vm/pmap.h>
#include <vm/vm_map_xnu.h>
#include <vm/vm_kern.h>

#include <IOKit/IOBSD.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOHibernatePrivate.h>

#include <pexpert/i386/efi.h>

#include <kern/thread.h>
#include <kern/sched.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#include <libkern/kernel_mach_header.h>
#include <libkern/OSKextLibPrivate.h>
#include <libkern/crc.h>

#if     DEBUG || DEVELOPMENT
#define DPRINTF(x ...)   kprintf(x)
#else
#define DPRINTF(x ...)
#endif

#ifndef ROUNDUP
#define ROUNDUP(a, b) (((a) + ((b) - 1)) & (~((b) - 1)))
#endif

#ifndef ROUNDDOWN
#define ROUNDDOWN(x, y) (((x)/(y))*(y))
#endif

static void machine_conf(void);
void panic_print_symbol_name(vm_address_t search);
void RecordPanicStackshot(void);

typedef enum paniclog_flush_type {
	kPaniclogFlushBase          = 1,/* Flush the initial log and paniclog header */
	kPaniclogFlushStackshot     = 2,/* Flush only the stackshot data, then flush the header */
	kPaniclogFlushOtherLog      = 3/* Flush the other log, then flush the header */
} paniclog_flush_type_t;

void paniclog_flush_internal(paniclog_flush_type_t variant);

extern const char       version[];
extern char             osversion[];
extern int              max_poll_quanta;
extern unsigned int     panic_is_inited;

extern uint64_t roots_installed;

/* #include <sys/proc.h> */
#define MAXCOMLEN 16
struct proc;
extern int              proc_pid(struct proc *p);
extern void             proc_name_kdp(struct proc *p, char * buf, int size);


/* Definitions for frame pointers */
#define FP_ALIGNMENT_MASK      ((uint32_t)(0x3))
#define FP_LR_OFFSET           ((uint32_t)4)
#define FP_LR_OFFSET64         ((uint32_t)8)
#define FP_MAX_NUM_TO_EVALUATE (50)

volatile int pbtcpu = -1;
hw_lock_data_t pbtlock;         /* backtrace print lock */
uint32_t pbtcnt = 0;

volatile int panic_double_fault_cpu = -1;

#define PRINT_ARGS_FROM_STACK_FRAME     0

typedef struct _cframe_t {
	struct _cframe_t    *prev;
	uintptr_t           caller;
#if PRINT_ARGS_FROM_STACK_FRAME
	unsigned            args[0];
#endif
} cframe_t;

static unsigned commit_paniclog_to_nvram;
boolean_t coprocessor_paniclog_flush = FALSE;

struct kcdata_descriptor kc_panic_data;
static boolean_t begun_panic_stackshot = FALSE;

vm_offset_t panic_stackshot_buf = 0;
size_t panic_stackshot_buf_len = 0;

size_t panic_stackshot_len = 0;

boolean_t is_clock_configured = FALSE;

static struct lbr_data lbrs[MAX_CPUS];
static uint32_t lbr_stack_size;

/*
 * Backtrace a single frame.
 */
void
print_one_backtrace(pmap_t pmap, vm_offset_t topfp, const char *cur_marker,
    boolean_t is_64_bit)
{
	unsigned int    i = 0;
	addr64_t        lr = 0;
	addr64_t        fp = topfp;
	addr64_t        fp_for_ppn = 0;
	ppnum_t         ppn = (ppnum_t)NULL;
	bool            dump_kernel_stack = (fp >= VM_MIN_KERNEL_ADDRESS);

	do {
		if ((fp == 0) || ((fp & FP_ALIGNMENT_MASK) != 0)) {
			break;
		}
		if (dump_kernel_stack && ((fp < VM_MIN_KERNEL_ADDRESS) || (fp > VM_MAX_KERNEL_ADDRESS))) {
			break;
		}
		if ((!dump_kernel_stack) && (fp >= VM_MIN_KERNEL_ADDRESS)) {
			break;
		}

		/* Check to see if current address will result in a different
		 *  ppn than previously computed (to avoid recomputation) via
		 *  (addr) ^ fp_for_ppn) >> PAGE_SHIFT) */

		if ((((fp + FP_LR_OFFSET) ^ fp_for_ppn) >> PAGE_SHIFT) != 0x0U) {
			ppn = pmap_find_phys(pmap, fp + FP_LR_OFFSET);
			fp_for_ppn = fp + (is_64_bit ? FP_LR_OFFSET64 : FP_LR_OFFSET);
		}
		if (ppn != (ppnum_t)NULL) {
			if (is_64_bit) {
				lr = ml_phys_read_double_64(((((vm_offset_t)ppn) << PAGE_SHIFT)) | ((fp + FP_LR_OFFSET64) & PAGE_MASK));
			} else {
				lr = ml_phys_read_word(((((vm_offset_t)ppn) << PAGE_SHIFT)) | ((fp + FP_LR_OFFSET) & PAGE_MASK));
			}
		} else {
			if (is_64_bit) {
				paniclog_append_noflush("%s\t  Could not read LR from frame at 0x%016llx\n", cur_marker, fp + FP_LR_OFFSET64);
			} else {
				paniclog_append_noflush("%s\t  Could not read LR from frame at 0x%08x\n", cur_marker, (uint32_t)(fp + FP_LR_OFFSET));
			}
			break;
		}
		if (((fp ^ fp_for_ppn) >> PAGE_SHIFT) != 0x0U) {
			ppn = pmap_find_phys(pmap, fp);
			fp_for_ppn = fp;
		}
		if (ppn != (ppnum_t)NULL) {
			if (is_64_bit) {
				fp = ml_phys_read_double_64(((((vm_offset_t)ppn) << PAGE_SHIFT)) | (fp & PAGE_MASK));
			} else {
				fp = ml_phys_read_word(((((vm_offset_t)ppn) << PAGE_SHIFT)) | (fp & PAGE_MASK));
			}
		} else {
			if (is_64_bit) {
				paniclog_append_noflush("%s\t  Could not read FP from frame at 0x%016llx\n", cur_marker, fp);
			} else {
				paniclog_append_noflush("%s\t  Could not read FP from frame at 0x%08x\n", cur_marker, (uint32_t)fp);
			}
			break;
		}
		/*
		 * Counter 'i' may == FP_MAX_NUM_TO_EVALUATE when running one
		 * extra round to check whether we have all frames in order to
		 * indicate (in)complete backtrace below. This happens in a case
		 * where total frame count and FP_MAX_NUM_TO_EVALUATE are equal.
		 * Do not capture anything.
		 */
		if (i < FP_MAX_NUM_TO_EVALUATE && lr) {
			if (is_64_bit) {
				paniclog_append_noflush("%s\t0x%016llx\n", cur_marker, lr);
			} else {
				paniclog_append_noflush("%s\t0x%08x\n", cur_marker, (uint32_t)lr);
			}
		}
	} while ((++i <= FP_MAX_NUM_TO_EVALUATE) && (fp != topfp));

	if (i > FP_MAX_NUM_TO_EVALUATE && fp != 0) {
		paniclog_append_noflush("Backtrace continues...\n");
	}
}
void
machine_startup(void)
{
	int     boot_arg;

#if 0
	if (PE_get_hotkey( kPEControlKey )) {
		halt_in_debugger = halt_in_debugger ? 0 : 1;
	}
#endif

	if (!PE_parse_boot_argn("nvram_paniclog", &commit_paniclog_to_nvram, sizeof(commit_paniclog_to_nvram))) {
		commit_paniclog_to_nvram = 1;
	}

	/*
	 * Entering the debugger will put the CPUs into a "safe"
	 * power mode.
	 */
	if (PE_parse_boot_argn("pmsafe_debug", &boot_arg, sizeof(boot_arg))) {
		pmsafe_debug = boot_arg;
	}

	hw_lock_init(&pbtlock);         /* initialize print backtrace lock */

	if (PE_parse_boot_argn("yield", &boot_arg, sizeof(boot_arg))) {
		sched_poll_yield_shift = boot_arg;
	}

	panic_notify_init();

	machine_conf();

	panic_hooks_init();

	/*
	 * Start the system.
	 */
	kernel_bootstrap();
	/*NOTREACHED*/
}


static void
machine_conf(void)
{
	machine_info.memory_size = (typeof(machine_info.memory_size))mem_size;
}

extern void *gPEEFIRuntimeServices;
extern void *gPEEFISystemTable;

static void
efi_set_tables_64(EFI_SYSTEM_TABLE_64 * system_table)
{
	EFI_RUNTIME_SERVICES_64 *runtime;
	uint32_t hdr_cksum;
	uint32_t cksum;

	DPRINTF("Processing 64-bit EFI tables at %p\n", system_table);
	do {
		DPRINTF("Header:\n");
		DPRINTF("  Signature:   0x%016llx\n", system_table->Hdr.Signature);
		DPRINTF("  Revision:    0x%08x\n", system_table->Hdr.Revision);
		DPRINTF("  HeaderSize:  0x%08x\n", system_table->Hdr.HeaderSize);
		DPRINTF("  CRC32:       0x%08x\n", system_table->Hdr.CRC32);
		DPRINTF("RuntimeServices: 0x%016llx\n", system_table->RuntimeServices);
		if (system_table->Hdr.Signature != EFI_SYSTEM_TABLE_SIGNATURE) {
			kprintf("Bad EFI system table signature\n");
			break;
		}
		// Verify signature of the system table
		hdr_cksum = system_table->Hdr.CRC32;
		system_table->Hdr.CRC32 = 0;
		cksum = crc32(0L, system_table, system_table->Hdr.HeaderSize);

		DPRINTF("System table calculated CRC32 = 0x%x, header = 0x%x\n", cksum, hdr_cksum);
		system_table->Hdr.CRC32 = hdr_cksum;
		if (cksum != hdr_cksum) {
			kprintf("Bad EFI system table checksum\n");
			break;
		}

		gPEEFISystemTable     = system_table;

		if (system_table->RuntimeServices == 0) {
			kprintf("No runtime table present\n");
			break;
		}
		DPRINTF("RuntimeServices table at 0x%qx\n", system_table->RuntimeServices);
		// 64-bit virtual address is OK for 64-bit EFI and 64/32-bit kernel.
		runtime = (EFI_RUNTIME_SERVICES_64 *) (uintptr_t)system_table->RuntimeServices;
		DPRINTF("Checking runtime services table %p\n", runtime);
		if (runtime->Hdr.Signature != EFI_RUNTIME_SERVICES_SIGNATURE) {
			kprintf("Bad EFI runtime table signature\n");
			break;
		}

		// Verify signature of runtime services table
		hdr_cksum = runtime->Hdr.CRC32;
		runtime->Hdr.CRC32 = 0;
		cksum = crc32(0L, runtime, runtime->Hdr.HeaderSize);

		DPRINTF("Runtime table calculated CRC32 = 0x%x, header = 0x%x\n", cksum, hdr_cksum);
		runtime->Hdr.CRC32 = hdr_cksum;
		if (cksum != hdr_cksum) {
			kprintf("Bad EFI runtime table checksum\n");
			break;
		}

		gPEEFIRuntimeServices = runtime;
	} while (FALSE);
}

/* Map in EFI runtime areas. */
static void
efi_init(void)
{
	boot_args *args = (boot_args *)PE_state.bootArgs;

	kprintf("Initializing EFI runtime services\n");

	do {
		vm_offset_t vm_size, vm_addr;
		vm_map_offset_t phys_addr;
		EfiMemoryRange *mptr;
		unsigned int msize, mcount;
		unsigned int i;

		msize = args->MemoryMapDescriptorSize;
		mcount = args->MemoryMapSize / msize;

		DPRINTF("efi_init() kernel base: 0x%x size: 0x%x\n",
		    args->kaddr, args->ksize);
		DPRINTF("           efiSystemTable physical: 0x%x virtual: %p\n",
		    args->efiSystemTable,
		    (void *) ml_static_ptovirt(args->efiSystemTable));
		DPRINTF("           efiRuntimeServicesPageStart: 0x%x\n",
		    args->efiRuntimeServicesPageStart);
		DPRINTF("           efiRuntimeServicesPageCount: 0x%x\n",
		    args->efiRuntimeServicesPageCount);
		DPRINTF("           efiRuntimeServicesVirtualPageStart: 0x%016llx\n",
		    args->efiRuntimeServicesVirtualPageStart);
		mptr = (EfiMemoryRange *)ml_static_ptovirt(args->MemoryMap);
		for (i = 0; i < mcount; i++, mptr = (EfiMemoryRange *)(((vm_offset_t)mptr) + msize)) {
			if (((mptr->Attribute & EFI_MEMORY_RUNTIME) == EFI_MEMORY_RUNTIME)) {
				vm_size = (vm_offset_t)i386_ptob((uint32_t)mptr->NumberOfPages);
				vm_addr =   (vm_offset_t) mptr->VirtualStart;
				/* For K64 on EFI32, shadow-map into high KVA */
				if (vm_addr < VM_MIN_KERNEL_ADDRESS) {
					vm_addr |= VM_MIN_KERNEL_ADDRESS;
				}
				phys_addr = (vm_map_offset_t) mptr->PhysicalStart;
				DPRINTF(" Type: %x phys: %p EFIv: %p kv: %p size: %p\n",
				    mptr->Type,
				    (void *) (uintptr_t) phys_addr,
				    (void *) (uintptr_t) mptr->VirtualStart,
				    (void *) vm_addr,
				    (void *) vm_size);
				pmap_map_bd(vm_addr, phys_addr, phys_addr + round_page(vm_size),
				    (mptr->Type == kEfiRuntimeServicesCode) ? VM_PROT_READ | VM_PROT_EXECUTE : VM_PROT_READ | VM_PROT_WRITE,
				    (mptr->Type == EfiMemoryMappedIO)       ? VM_WIMG_IO   : VM_WIMG_USE_DEFAULT);
			}
		}

		if (args->Version != kBootArgsVersion2) {
			panic("Incompatible boot args version %d revision %d", args->Version, args->Revision);
		}

		DPRINTF("Boot args version %d revision %d mode %d\n", args->Version, args->Revision, args->efiMode);
		if (args->efiMode == kBootArgsEfiMode64) {
			efi_set_tables_64((EFI_SYSTEM_TABLE_64 *) ml_static_ptovirt(args->efiSystemTable));
		} else {
			panic("Unsupported 32-bit EFI system table!");
		}
	} while (FALSE);

	return;
}

/* Returns TRUE if a page belongs to the EFI Runtime Services (code or data) */
boolean_t
bootloader_valid_page(ppnum_t ppn)
{
	boot_args *args = (boot_args *)PE_state.bootArgs;
	ppnum_t    pstart = args->efiRuntimeServicesPageStart;
	ppnum_t    pend = pstart + args->efiRuntimeServicesPageCount;

	return pstart <= ppn && ppn < pend;
}

/* Remap EFI runtime areas. */
void
hibernate_newruntime_map(void * map, vm_size_t map_size, uint32_t system_table_offset)
{
	boot_args *args = (boot_args *)PE_state.bootArgs;

	kprintf("Reinitializing EFI runtime services\n");

	do {
		vm_offset_t vm_size, vm_addr;
		vm_map_offset_t phys_addr;
		EfiMemoryRange *mptr;
		unsigned int msize, mcount;
		unsigned int i;

		gPEEFISystemTable     = 0;
		gPEEFIRuntimeServices = 0;

		system_table_offset += ptoa_32(args->efiRuntimeServicesPageStart);

		kprintf("Old system table 0x%x, new 0x%x\n",
		    (uint32_t)args->efiSystemTable, system_table_offset);

		args->efiSystemTable    = system_table_offset;

		kprintf("Old map:\n");
		msize = args->MemoryMapDescriptorSize;
		mcount = args->MemoryMapSize / msize;
		mptr = (EfiMemoryRange *)ml_static_ptovirt(args->MemoryMap);
		for (i = 0; i < mcount; i++, mptr = (EfiMemoryRange *)(((vm_offset_t)mptr) + msize)) {
			if ((mptr->Attribute & EFI_MEMORY_RUNTIME) == EFI_MEMORY_RUNTIME) {
				vm_size = (vm_offset_t)i386_ptob((uint32_t)mptr->NumberOfPages);
				vm_addr =   (vm_offset_t) mptr->VirtualStart;
				/* K64 on EFI32 */
				if (vm_addr < VM_MIN_KERNEL_ADDRESS) {
					vm_addr |= VM_MIN_KERNEL_ADDRESS;
				}
				phys_addr = (vm_map_offset_t) mptr->PhysicalStart;

				kprintf("mapping[%u] %qx @ %lx, %llu\n", mptr->Type, phys_addr, (unsigned long)vm_addr, mptr->NumberOfPages);
			}
		}

		pmap_remove(kernel_pmap, i386_ptob(args->efiRuntimeServicesPageStart),
		    i386_ptob(args->efiRuntimeServicesPageStart + args->efiRuntimeServicesPageCount));

		kprintf("New map:\n");
		msize = args->MemoryMapDescriptorSize;
		mcount = (unsigned int)(map_size / msize);
		mptr = map;
		for (i = 0; i < mcount; i++, mptr = (EfiMemoryRange *)(((vm_offset_t)mptr) + msize)) {
			if ((mptr->Attribute & EFI_MEMORY_RUNTIME) == EFI_MEMORY_RUNTIME) {
				vm_size = (vm_offset_t)i386_ptob((uint32_t)mptr->NumberOfPages);
				vm_addr =   (vm_offset_t) mptr->VirtualStart;
				if (vm_addr < VM_MIN_KERNEL_ADDRESS) {
					vm_addr |= VM_MIN_KERNEL_ADDRESS;
				}
				phys_addr = (vm_map_offset_t) mptr->PhysicalStart;

				kprintf("mapping[%u] %qx @ %lx, %llu\n", mptr->Type, phys_addr, (unsigned long)vm_addr, mptr->NumberOfPages);

				pmap_map(vm_addr, phys_addr, phys_addr + round_page(vm_size),
				    (mptr->Type == kEfiRuntimeServicesCode) ? VM_PROT_READ | VM_PROT_EXECUTE : VM_PROT_READ | VM_PROT_WRITE,
				    (mptr->Type == EfiMemoryMappedIO)       ? VM_WIMG_IO   : VM_WIMG_USE_DEFAULT);
			}
		}

		if (args->Version != kBootArgsVersion2) {
			panic("Incompatible boot args version %d revision %d", args->Version, args->Revision);
		}

		kprintf("Boot args version %d revision %d mode %d\n", args->Version, args->Revision, args->efiMode);
		if (args->efiMode == kBootArgsEfiMode64) {
			efi_set_tables_64((EFI_SYSTEM_TABLE_64 *) ml_static_ptovirt(args->efiSystemTable));
		} else {
			panic("Unsupported 32-bit EFI system table!");
		}
	} while (FALSE);

	kprintf("Done reinitializing EFI runtime services\n");

	return;
}

/*
 * Find devices.  The system is alive.
 */
void
machine_init(void)
{
	/* Now with VM up, switch to dynamically allocated cpu data */
	cpu_data_realloc();

	/* Ensure panic buffer is initialized. */
	debug_log_init();

	/*
	 * Display CPU identification
	 */
	cpuid_cpu_display("CPU identification");
	cpuid_feature_display("CPU features");
	cpuid_extfeature_display("CPU extended features");

	/*
	 * Initialize EFI runtime services.
	 */
	efi_init();

	smp_init();

	/*
	 * Set up to use floating point.
	 */
	init_fpu();

	/*
	 * Configure clock devices.
	 */
	clock_config();
	is_clock_configured = TRUE;

#if CONFIG_MTRR
	/*
	 * Initialize MTRR from boot processor.
	 */
	mtrr_init();

	/*
	 * Set up PAT for boot processor.
	 */
	pat_init();
#endif

	/*
	 * Free lowmem pages and complete other setup
	 */
	pmap_lowmem_finalize();
}

/*
 * Halt a cpu.
 */
void
halt_cpu(void)
{
	halt_all_cpus(FALSE);
}

int reset_mem_on_reboot = 1;

/*
 * Halt the system or reboot.
 */
__attribute__((noreturn))
void
halt_all_cpus(boolean_t reboot)
{
	if (reboot) {
		printf("MACH Reboot\n");
		PEHaltRestart( kPERestartCPU );
	} else {
		printf("CPU halted\n");
		PEHaltRestart( kPEHaltCPU );
	}
	while (1) {
		;
	}
}

/* For use with the MP rendezvous mechanism
 */

uint64_t panic_restart_timeout = ~(0ULL);

#define PANIC_RESTART_TIMEOUT (3ULL * NSEC_PER_SEC)

/*
 * We should always return from this function with the other log offset
 * set in the panic_info structure.
 */
void
RecordPanicStackshot()
{
	int err = 0;
	size_t bytes_traced = 0, bytes_uncompressed = 0, bytes_used = 0, bytes_remaining = 0;
	char *stackshot_begin_loc = NULL;

	/* Don't re-enter this code if we panic here */
	if (begun_panic_stackshot) {
		if (panic_info->mph_other_log_offset == 0) {
			panic_info->mph_other_log_offset = PE_get_offset_into_panic_region(debug_buf_ptr);
		}
		return;
	}
	begun_panic_stackshot = TRUE;

	/* The panic log length should have been set before we came to capture a stackshot */
	if (panic_info->mph_panic_log_len == 0) {
		kdb_printf("Found zero length panic log, skipping capturing panic stackshot\n");
		if (panic_info->mph_other_log_offset == 0) {
			panic_info->mph_other_log_offset = PE_get_offset_into_panic_region(debug_buf_ptr);
		}
		return;
	}

	if (panic_stackshot_active()) {
		panic_info->mph_panic_flags |= MACOS_PANIC_HEADER_FLAG_STACKSHOT_FAILED_NESTED;
		panic_info->mph_other_log_offset = PE_get_offset_into_panic_region(debug_buf_ptr);
		kdb_printf("Panicked during stackshot, skipping panic stackshot\n");
		return;
	}

	/* Try to capture an in memory panic_stackshot */
	if (extended_debug_log_enabled) {
		/* On coprocessor systems we write this into the extended debug log */
		stackshot_begin_loc = debug_buf_ptr;
		bytes_remaining = debug_buf_size - (unsigned int)((uintptr_t)stackshot_begin_loc - (uintptr_t)debug_buf_base);
	} else if (panic_stackshot_buf != 0) {
		/* On other systems we use the panic stackshot_buf */
		stackshot_begin_loc = (char *) panic_stackshot_buf;
		bytes_remaining = panic_stackshot_buf_len;
	} else {
		panic_info->mph_other_log_offset = PE_get_offset_into_panic_region(debug_buf_ptr);
		return;
	}


	err = kcdata_memory_static_init(&kc_panic_data, (mach_vm_address_t)stackshot_begin_loc,
	    KCDATA_BUFFER_BEGIN_COMPRESSED, (unsigned int) bytes_remaining, KCFLAG_USE_MEMCOPY);
	if (err != KERN_SUCCESS) {
		panic_info->mph_panic_flags |= MACOS_PANIC_HEADER_FLAG_STACKSHOT_FAILED_ERROR;
		panic_info->mph_other_log_offset = PE_get_offset_into_panic_region(debug_buf_ptr);
		kdb_printf("Failed to initialize kcdata buffer for in-memory panic stackshot, skipping ...\n");
		return;
	}

	uint64_t stackshot_flags = (STACKSHOT_SAVE_KEXT_LOADINFO | STACKSHOT_SAVE_LOADINFO | STACKSHOT_KCDATA_FORMAT |
	    STACKSHOT_ENABLE_BT_FAULTING | STACKSHOT_ENABLE_UUID_FAULTING | STACKSHOT_FROM_PANIC | STACKSHOT_DO_COMPRESS |
	    STACKSHOT_NO_IO_STATS | STACKSHOT_THREAD_WAITINFO | STACKSHOT_DISABLE_LATENCY_INFO | STACKSHOT_GET_DQ);

	err = kcdata_init_compress(&kc_panic_data, KCDATA_BUFFER_BEGIN_STACKSHOT, kdp_memcpy, KCDCT_ZLIB);
	if (err != KERN_SUCCESS) {
		panic_info->mph_panic_flags |= MACOS_PANIC_HEADER_FLAG_STACKSHOT_FAILED_COMPRESS;
		stackshot_flags &= ~STACKSHOT_DO_COMPRESS;
	}

#if DEVELOPMENT
	/*
	 * Include the shared cache layout in panic stackshots on DEVELOPMENT kernels so that we can symbolicate
	 * panic stackshots from corefiles.
	 */
	stackshot_flags |= STACKSHOT_COLLECT_SHAREDCACHE_LAYOUT;
#endif

	kdp_snapshot_preflight(-1, (void *) stackshot_begin_loc, (uint32_t) bytes_remaining, stackshot_flags, &kc_panic_data, 0, 0);
	err = do_panic_stackshot(NULL);
	bytes_traced = (size_t) kdp_stack_snapshot_bytes_traced();
	bytes_uncompressed = (size_t) kdp_stack_snapshot_bytes_uncompressed();
	bytes_used = (size_t) kcdata_memory_get_used_bytes(&kc_panic_data);

	if ((err != KERN_SUCCESS) && (bytes_used > 0)) {
		/*
		 * We ran out of space while trying to capture a stackshot, try again without user frames.
		 * It's not safe to log from here (in case we're writing in the middle of the debug buffer on coprocessor systems)
		 * but append a flag to the panic flags.
		 */
		panic_info->mph_panic_flags |= MACOS_PANIC_HEADER_FLAG_STACKSHOT_KERNEL_ONLY;
		panic_stackshot_reset_state();

		/* Erase the stackshot data (this region is pre-populated with the NULL character) */
		memset(stackshot_begin_loc, '\0', bytes_used);

		err = kcdata_memory_static_init(&kc_panic_data, (mach_vm_address_t)stackshot_begin_loc,
		    KCDATA_BUFFER_BEGIN_STACKSHOT, (unsigned int) bytes_remaining, KCFLAG_USE_MEMCOPY);
		if (err != KERN_SUCCESS) {
			panic_info->mph_panic_flags |= MACOS_PANIC_HEADER_FLAG_STACKSHOT_FAILED_ERROR;
			panic_info->mph_other_log_offset = PE_get_offset_into_panic_region(debug_buf_ptr);
			kdb_printf("Failed to re-initialize kcdata buffer for kernel only in-memory panic stackshot, skipping ...\n");
			return;
		}

		stackshot_flags = (STACKSHOT_SAVE_KEXT_LOADINFO | STACKSHOT_KCDATA_FORMAT | STACKSHOT_FROM_PANIC | STACKSHOT_DISABLE_LATENCY_INFO |
		    STACKSHOT_NO_IO_STATS | STACKSHOT_THREAD_WAITINFO | STACKSHOT_ACTIVE_KERNEL_THREADS_ONLY | STACKSHOT_GET_DQ);
#if DEVELOPMENT
		/*
		 * Include the shared cache layout in panic stackshots on DEVELOPMENT kernels so that we can symbolicate
		 * panic stackshots from corefiles.
		 */
		stackshot_flags |= STACKSHOT_COLLECT_SHAREDCACHE_LAYOUT;
#endif

		kdp_snapshot_preflight(-1, (void *) stackshot_begin_loc, (uint32_t) bytes_remaining, stackshot_flags, &kc_panic_data, 0, 0);
		err = do_panic_stackshot(NULL);
		bytes_traced = (size_t) kdp_stack_snapshot_bytes_traced();
		bytes_uncompressed = (size_t) kdp_stack_snapshot_bytes_uncompressed();
		bytes_used = (size_t) kcdata_memory_get_used_bytes(&kc_panic_data);
	}

	if (err == KERN_SUCCESS) {
		if (extended_debug_log_enabled) {
			debug_buf_ptr += bytes_traced;
		}
		panic_info->mph_panic_flags |= MACOS_PANIC_HEADER_FLAG_STACKSHOT_SUCCEEDED;

		/* On other systems this is not in the debug buffer itself, it's in a separate buffer allocated at boot. */
		if (extended_debug_log_enabled) {
			panic_info->mph_stackshot_offset = PE_get_offset_into_panic_region(stackshot_begin_loc);
			panic_info->mph_stackshot_len = (uint32_t) bytes_traced;
		} else {
			panic_info->mph_stackshot_offset = panic_info->mph_stackshot_len = 0;
		}

		panic_info->mph_other_log_offset = PE_get_offset_into_panic_region(debug_buf_ptr);
		if (stackshot_flags & STACKSHOT_DO_COMPRESS) {
			panic_info->mph_panic_flags |= MACOS_PANIC_HEADER_FLAG_STACKSHOT_DATA_COMPRESSED;
			kdb_printf("\n** In Memory Panic Stackshot Succeeded ** Bytes Traced %zu (Uncompressed %zu) **\n", bytes_traced, bytes_uncompressed);
		} else {
			kdb_printf("\n** In Memory Panic Stackshot Succeeded ** Bytes Traced %zu **\n", bytes_traced);
		}

		/* Used by the code that writes the buffer to disk */
		panic_stackshot_buf = (vm_offset_t) stackshot_begin_loc;
		panic_stackshot_len = bytes_traced;

		if (!extended_debug_log_enabled && (gIOPolledCoreFileMode == kIOPolledCoreFileModeStackshot)) {
			/* System configured to write panic stackshot to disk */
			kern_dump(KERN_DUMP_STACKSHOT_DISK);
		}
	} else {
		if (bytes_used > 0) {
			/* Erase the stackshot data (this region is pre-populated with the NULL character) */
			memset(stackshot_begin_loc, '\0', bytes_used);
			panic_info->mph_panic_flags |= MACOS_PANIC_HEADER_FLAG_STACKSHOT_FAILED_INCOMPLETE;

			panic_info->mph_other_log_offset = PE_get_offset_into_panic_region(debug_buf_ptr);
			kdb_printf("\n** In Memory Panic Stackshot Incomplete ** Bytes Filled %zu ** Err %d\n", bytes_used, err);
		} else {
			bzero(stackshot_begin_loc, bytes_used);
			panic_info->mph_panic_flags |= MACOS_PANIC_HEADER_FLAG_STACKSHOT_FAILED_ERROR;

			panic_info->mph_other_log_offset = PE_get_offset_into_panic_region(debug_buf_ptr);
			kdb_printf("\n** In Memory Panic Stackshot Failed ** Bytes Traced %zu, err %d\n", bytes_traced, err);
		}
	}

	return;
}

void
SavePanicInfo(
	__unused const char *message, void *panic_data, uint64_t panic_options, __unused const char* panic_initiator)
{
	void *stackptr  = NULL;
	thread_t thread_to_trace = (thread_t) panic_data;
	cframe_t synthetic_stack_frame = { };
	char *debugger_msg = NULL;
	int cn = cpu_number();

	/*
	 * Issue an I/O port read if one has been requested - this is an event logic
	 * analyzers can use as a trigger point.
	 */
	panic_notify();

	/* Obtain frame pointer for stack to trace */
	if (panic_options & DEBUGGER_INTERNAL_OPTION_THREAD_BACKTRACE) {
		if (!mp_kdp_all_cpus_halted()) {
			debugger_msg = "Backtracing panicked thread because failed to halt all CPUs\n";
		} else if (thread_to_trace == THREAD_NULL) {
			debugger_msg = "Backtracing panicked thread because no thread pointer provided\n";
		} else if (kvtophys((vm_offset_t)thread_to_trace) == 0ULL) {
			debugger_msg = "Backtracing panicked thread because unable to access specified thread\n";
		} else if (thread_to_trace->kernel_stack == 0) {
			debugger_msg = "Backtracing panicked thread because kernel_stack is NULL for specified thread\n";
		} else if (kvtophys(STACK_IKS(thread_to_trace->kernel_stack) == 0ULL)) {
			debugger_msg = "Backtracing panicked thread because unable to access kernel_stack for specified thread\n";
		} else {
			debugger_msg = "Backtracing specified thread\n";
			/* We construct a synthetic stack frame so we can include the current instruction pointer */
			synthetic_stack_frame.prev = (cframe_t *)STACK_IKS(thread_to_trace->kernel_stack)->k_rbp;
			synthetic_stack_frame.caller = (uintptr_t) STACK_IKS(thread_to_trace->kernel_stack)->k_rip;
			stackptr = (void *) &synthetic_stack_frame;
		}
	}

	if (stackptr == NULL) {
		__asm__ volatile ("movq %%rbp, %0" : "=m" (stackptr));
	}

	/* Print backtrace - callee is internally synchronized */
	if (panic_options & DEBUGGER_OPTION_INITPROC_PANIC) {
		/* Special handling of launchd died panics */
		print_launchd_info();
	} else {
		panic_i386_backtrace(stackptr, ((panic_double_fault_cpu == cn) ? 80 : 48), debugger_msg, FALSE, NULL);
	}

	if (panic_options & DEBUGGER_OPTION_COMPANION_PROC_INITIATED_PANIC) {
		panic_info->mph_panic_flags |= MACOS_PANIC_HEADER_FLAG_COMPANION_PROC_INITIATED_PANIC;
	}

	if (panic_options & DEBUGGER_OPTION_INTEGRATED_COPROC_INITIATED_PANIC) {
		panic_info->mph_panic_flags |= MACOS_PANIC_HEADER_FLAG_INTEGRATED_COPROC_INITIATED_PANIC;
	}

	if (panic_options & DEBUGGER_OPTION_USERSPACE_INITIATED_PANIC) {
		panic_info->mph_panic_flags |= MACOS_PANIC_HEADER_FLAG_USERSPACE_INITIATED_PANIC;
	}

#if MACH_KDP
	/*
	 * If this panic is due to a PTE corruption event, use the kdp cross-cpu calling machinery to ask
	 * each CPU to dump their backtraces before proceeding.  This mechanism was preferred to adding new
	 * synchronization operations in NMIInterruptHandler and the normal panic flow; this mechanism allows
	 * each CPU to add their backtraces after all other primary panic output is complete while not adding
	 * additional complexity to the common panic path.
	 */
	if (NMI_panic_reason == PTE_CORRUPTION) {
		for (uint64_t cpu = 0; cpu < real_ncpus; cpu++) {
			if (cpu == cpu_number() || !cpu_is_running(cpu)) {
				continue;
			}
			(void) kdp_x86_xcpu_invoke(cpu, NMI_pte_corruption_callback, NULL, NULL, NSEC_PER_SEC /* 1 second timeout */);
		}
	}
#else
#error NMI PTE Corruption panic flow requires KDP
#endif

	if (PE_get_offset_into_panic_region(debug_buf_ptr) < panic_info->mph_panic_log_offset) {
		kdb_printf("Invalid panic log offset found (not properly initialized?): debug_buf_ptr : 0x%p, panic_info: 0x%p mph_panic_log_offset: 0x%x\n",
		    debug_buf_ptr, panic_info, panic_info->mph_panic_log_offset);
		panic_info->mph_panic_log_len = 0;
	} else {
		panic_info->mph_panic_log_len = PE_get_offset_into_panic_region(debug_buf_ptr) - panic_info->mph_panic_log_offset;
	}

	/* Flush the panic log */
	paniclog_flush_internal(kPaniclogFlushBase);

	/* Try to take a panic stackshot */
	RecordPanicStackshot();

	panic_info->mph_roots_installed = roots_installed;

	/*
	 * Flush the panic log again with the stackshot or any relevant logging
	 * from when we tried to capture it.
	 */
	paniclog_flush_internal(kPaniclogFlushStackshot);
}

void
paniclog_flush_internal(paniclog_flush_type_t variant)
{
	/* Update the other log offset if we've opened the other log */
	if (panic_info->mph_other_log_offset != 0) {
		panic_info->mph_other_log_len = PE_get_offset_into_panic_region(debug_buf_ptr) - panic_info->mph_other_log_offset;
	}

	/*
	 * If we've detected that we're on a co-processor system, we flush the panic log via the kPEPanicSync
	 * panic callbacks, otherwise we flush via nvram (unless that has been disabled).
	 */
	if (coprocessor_paniclog_flush) {
		uint32_t overall_buffer_size = debug_buf_size;
		uint32_t size_to_flush = 0, offset_to_flush = 0;
		if (extended_debug_log_enabled) {
			/*
			 * debug_buf_size for the extended log does not include the length of the header.
			 * There may be some extra data at the end of the 'basic' log that wouldn't get flushed
			 * for the non-extended case (this is a concession we make to not shrink the paniclog data
			 * for non-coprocessor systems that only use the basic log).
			 */
			overall_buffer_size = debug_buf_size + sizeof(struct macos_panic_header);
		}

		/* Update the CRC */
		panic_info->mph_crc = crc32(0L, &panic_info->mph_version, (overall_buffer_size - offsetof(struct macos_panic_header, mph_version)));

		if (variant == kPaniclogFlushBase) {
			/* Flush the header and base panic log. */
			kprintf("Flushing base panic log\n");
			size_to_flush = ROUNDUP((panic_info->mph_panic_log_offset + panic_info->mph_panic_log_len), PANIC_FLUSH_BOUNDARY);
			offset_to_flush = 0;
			PESavePanicInfoAction(panic_info, offset_to_flush, size_to_flush);
		} else if ((variant == kPaniclogFlushStackshot) || (variant == kPaniclogFlushOtherLog)) {
			if (variant == kPaniclogFlushStackshot) {
				/*
				 * We flush the stackshot before flushing the updated header because the stackshot
				 * can take a while to flush. We want the paniclog header to be as consistent as possible even
				 * if the stackshot isn't flushed completely. Flush starting from the end of the panic log.
				 */
				kprintf("Flushing panic log stackshot\n");
				offset_to_flush = ROUNDDOWN((panic_info->mph_panic_log_offset + panic_info->mph_panic_log_len), PANIC_FLUSH_BOUNDARY);
				size_to_flush = ROUNDUP((panic_info->mph_stackshot_len + (panic_info->mph_stackshot_offset - offset_to_flush)), PANIC_FLUSH_BOUNDARY);
				PESavePanicInfoAction(panic_info, offset_to_flush, size_to_flush);
			}

			/* Flush the other log -- everything after the stackshot */
			kprintf("Flushing panic 'other' log\n");
			offset_to_flush = ROUNDDOWN((panic_info->mph_stackshot_offset + panic_info->mph_stackshot_len), PANIC_FLUSH_BOUNDARY);
			size_to_flush = ROUNDUP((panic_info->mph_other_log_len + (panic_info->mph_other_log_offset - offset_to_flush)), PANIC_FLUSH_BOUNDARY);
			PESavePanicInfoAction(panic_info, offset_to_flush, size_to_flush);

			/* Flush the header -- everything before the paniclog */
			kprintf("Flushing panic log header\n");
			size_to_flush = ROUNDUP(panic_info->mph_panic_log_offset, PANIC_FLUSH_BOUNDARY);
			offset_to_flush = 0;
			PESavePanicInfoAction(panic_info, offset_to_flush, size_to_flush);
		}
	} else if (commit_paniclog_to_nvram) {
		assert(debug_buf_size != 0);
		unsigned int bufpos;
		unsigned long pi_size = 0;
		uintptr_t cr0;

		debug_putc(0);

		/*
		 * Now call the compressor
		 * XXX Consider using the WKdm compressor in the
		 * future, rather than just packing - would need to
		 * be co-ordinated with crashreporter, which decodes
		 * this post-restart. The compressor should be
		 * capable of in-place compression.
		 *
		 * Don't include the macOS panic header (for co-processor systems only)
		 */
		bufpos = packA(debug_buf_base, (unsigned int) (debug_buf_ptr - debug_buf_base),
		    debug_buf_size);
		/*
		 * If compression was successful, use the compressed length
		 */
		pi_size = bufpos ? bufpos : (unsigned) (debug_buf_ptr - debug_buf_base);

		/*
		 * The following sequence is a workaround for:
		 * <rdar://problem/5915669> SnowLeopard10A67: AppleEFINVRAM should not invoke
		 * any routines that use floating point (MMX in this case) when saving panic
		 * logs to nvram/flash.
		 */
		cr0 = get_cr0();
		clear_ts();

		/*
		 * Save panic log to non-volatile store
		 * Panic info handler must truncate data that is
		 * too long for this platform.
		 * This call must save data synchronously,
		 * since we can subsequently halt the system.
		 */
		kprintf("Attempting to commit panic log to NVRAM\n");
		pi_size = PESavePanicInfo((unsigned char *)debug_buf_base,
		    (uint32_t)pi_size );
		set_cr0(cr0);

		/*
		 * Uncompress in-place, to permit examination of
		 * the panic log by debuggers.
		 */
		if (bufpos) {
			unpackA(debug_buf_base, bufpos);
		}
	}
}

void
paniclog_flush()
{
	/* Called outside of this file to update logging appended to the "other" log */
	paniclog_flush_internal(kPaniclogFlushOtherLog);
	return;
}

char *
machine_boot_info(char *buf, __unused vm_size_t size)
{
	*buf = '\0';
	return buf;
}

/* Routines for address - symbol translation. Not called unless the "keepsyms"
 * boot-arg is supplied.
 */

static int
panic_print_macho_symbol_name(kernel_mach_header_t *mh, vm_address_t search, const char *module_name)
{
	kernel_nlist_t      *sym = NULL;
	struct load_command         *cmd;
	kernel_segment_command_t    *orig_ts = NULL, *orig_le = NULL;
	struct symtab_command       *orig_st = NULL;
	unsigned int                        i;
	char                                        *strings, *bestsym = NULL;
	vm_address_t                        bestaddr = 0, diff, curdiff;

	/* Assume that if it's loaded and linked into the kernel, it's a valid Mach-O */

	cmd = (struct load_command *) &mh[1];
	for (i = 0; i < mh->ncmds; i++) {
		if (cmd->cmd == LC_SEGMENT_KERNEL) {
			kernel_segment_command_t *orig_sg = (kernel_segment_command_t *) cmd;

			if (strncmp(SEG_TEXT, orig_sg->segname,
			    sizeof(orig_sg->segname)) == 0) {
				orig_ts = orig_sg;
			} else if (strncmp(SEG_LINKEDIT, orig_sg->segname,
			    sizeof(orig_sg->segname)) == 0) {
				orig_le = orig_sg;
			} else if (strncmp("", orig_sg->segname,
			    sizeof(orig_sg->segname)) == 0) {
				orig_ts = orig_sg; /* pre-Lion i386 kexts have a single unnamed segment */
			}
		} else if (cmd->cmd == LC_SYMTAB) {
			orig_st = (struct symtab_command *) cmd;
		}

		cmd = (struct load_command *) ((uintptr_t) cmd + cmd->cmdsize);
	}

	if ((orig_ts == NULL) || (orig_st == NULL) || (orig_le == NULL)) {
		return 0;
	}

	if ((search < orig_ts->vmaddr) ||
	    (search >= orig_ts->vmaddr + orig_ts->vmsize)) {
		/* search out of range for this mach header */
		return 0;
	}

	sym = (kernel_nlist_t *)(uintptr_t)(orig_le->vmaddr + orig_st->symoff - orig_le->fileoff);
	strings = (char *)(uintptr_t)(orig_le->vmaddr + orig_st->stroff - orig_le->fileoff);
	diff = search;

	for (i = 0; i < orig_st->nsyms; i++) {
		if (sym[i].n_type & N_STAB) {
			continue;
		}

		if (sym[i].n_value <= search) {
			curdiff = search - (vm_address_t)sym[i].n_value;
			if (curdiff < diff) {
				diff = curdiff;
				bestaddr = sym[i].n_value;
				bestsym = strings + sym[i].n_un.n_strx;
			}
		}
	}

	if (bestsym != NULL) {
		if (diff != 0) {
			paniclog_append_noflush("%s : %s + 0x%lx", module_name, bestsym, (unsigned long)diff);
		} else {
			paniclog_append_noflush("%s : %s", module_name, bestsym);
		}
		return 1;
	}
	return 0;
}

static void
panic_display_uptime(void)
{
	uint64_t        uptime;
	absolutetime_to_nanoseconds(mach_absolute_time(), &uptime);

	paniclog_append_noflush("\nSystem uptime in nanoseconds: %llu\n", uptime);
}

extern uint32_t         gIOHibernateCount;

static void
panic_display_hib_count(void)
{
	paniclog_append_noflush("Hibernation exit count: %u\n", gIOHibernateCount);
}

extern AbsoluteTime      gIOLastSleepAbsTime;
extern AbsoluteTime      gIOLastWakeAbsTime;
extern uint64_t          gAcpiLastSleepTscBase;
extern uint64_t          gAcpiLastSleepNanoBase;
extern uint64_t          gAcpiLastWakeTscBase;
extern uint64_t          gAcpiLastWakeNanoBase;
extern boolean_t         is_clock_configured;

static void
panic_display_times(void)
{
	if (!is_clock_configured) {
		paniclog_append_noflush("Warning: clock is not configured. Can't get time\n");
		return;
	}

	paniclog_append_noflush("Last Sleep:           absolute           base_tsc          base_nano\n");
	paniclog_append_noflush("  Uptime  : 0x%016llx\n", mach_absolute_time());
	paniclog_append_noflush("  Sleep   : 0x%016llx 0x%016llx 0x%016llx\n", gIOLastSleepAbsTime, gAcpiLastSleepTscBase, gAcpiLastSleepNanoBase);
	paniclog_append_noflush("  Wake    : 0x%016llx 0x%016llx 0x%016llx\n", gIOLastWakeAbsTime, gAcpiLastWakeTscBase, gAcpiLastWakeNanoBase);
}

static void
panic_display_disk_errors(void)
{
	if (panic_disk_error_description[0]) {
		panic_disk_error_description[panic_disk_error_description_size - 1] = '\0';
		paniclog_append_noflush("Root disk errors: \"%s\"\n", panic_disk_error_description);
	}
}

static void
panic_display_shutdown_status(void)
{
#if defined(__i386__) || defined(__x86_64__)
	paniclog_append_noflush("System shutdown begun: %s\n", IOPMRootDomainGetWillShutdown() ? "YES" : "NO");
	if (gIOPolledCoreFileMode == kIOPolledCoreFileModeNotInitialized) {
		paniclog_append_noflush("Panic diags file unavailable, panic occurred prior to initialization\n");
	} else if (gIOPolledCoreFileMode != kIOPolledCoreFileModeDisabled) {
		/*
		 * If we haven't marked the corefile as explicitly disabled, and we've made it past initialization, then we know the current
		 * system was configured to use disk based diagnostics at some point.
		 */
		paniclog_append_noflush("Panic diags file available: %s (0x%x)\n", (gIOPolledCoreFileMode != kIOPolledCoreFileModeClosed && gIOPolledCoreFileMode != kIOPolledCoreFileModeUnlinked) ? "YES" : "NO", kdp_polled_corefile_error());
	}
#endif
}

extern const char version[];
extern char osversion[];

static volatile uint32_t config_displayed = 0;

static void
panic_display_system_configuration(boolean_t launchd_exit)
{
	if (!launchd_exit) {
		panic_display_process_name();
	}
	if (OSCompareAndSwap(0, 1, &config_displayed)) {
		char buf[256];
		if (!launchd_exit && strlcpy(buf, PE_boot_args(), sizeof(buf))) {
			paniclog_append_noflush("Boot args: %s\n", buf);
		}
		paniclog_append_noflush("\nMac OS version:\n%s\n",
		    (osversion[0] != 0) ? osversion : "Not yet set");
		paniclog_append_noflush("\nKernel version:\n%s\n", version);
		panic_display_kernel_uuid();
		paniclog_append_noflush("roots installed: %lld\n", roots_installed);
		if (!launchd_exit) {
			panic_display_kernel_aslr();
			panic_display_hibb();
			panic_display_pal_info();
		}
		panic_display_model_name();
		panic_display_disk_errors();
		panic_display_shutdown_status();
		if (!launchd_exit) {
			panic_display_hib_count();
			panic_display_uptime();
			panic_display_times();
			panic_display_compressor_stats();
			panic_display_zalloc();
			kext_dump_panic_lists(&paniclog_append_noflush);
		}
	}
}

extern kmod_info_t * kmod; /* the list of modules */

static void
panic_print_kmod_symbol_name(vm_address_t search)
{
	u_int i;

	if (gLoadedKextSummaries == NULL) {
		return;
	}
	for (i = 0; i < gLoadedKextSummaries->numSummaries; ++i) {
		OSKextLoadedKextSummary *summary = gLoadedKextSummaries->summaries + i;

		if ((search >= summary->address) &&
		    (search < (summary->address + summary->size))) {
			kernel_mach_header_t *header = (kernel_mach_header_t *)(uintptr_t) summary->address;
			if (panic_print_macho_symbol_name(header, search, summary->name) == 0) {
				paniclog_append_noflush("%s + %llu", summary->name, (unsigned long)search - summary->address);
			}
			break;
		}
	}
}

static void
read_lbr_empty(void)
{
}

void (*read_lbr)(void) = read_lbr_empty;

static void
capture_lbr_state(void)
{
	thread_t thr_act = current_thread();
	int i;
	last_branch_state_t thread_lbr_data;
	struct lbr_data *lbr = &lbrs[cpu_number()];

	if (lbr_stack_size > 0) {
		i386_lbr_disable();

		if (i386_filtered_lbr_state_to_mach_thread_state(thr_act, &thread_lbr_data, false) == 0) {
			for (i = 0; i < thread_lbr_data.lbr_count; i++) {
				lbr->from[i] = thread_lbr_data.lbrs[i].from_ip;
				lbr->to[i] = thread_lbr_data.lbrs[i].to_ip;
			}
		}
	}

	i386_lbr_enable();
}

struct panic_lbr_header_s {
	uint32_t id;
	uint8_t ncpus;
	uint8_t lbr_count;
	uint64_t pcarveout_va;
};
struct panic_lbr_header_s panic_lbr_header = {0};

static void
copy_lbr_data_for_core(void)
{
	unsigned int cpu;

	if (phys_carveout) {
		// The minimum size of phys_carveout is 1MiB but just in case
		if (phys_carveout_size >= sizeof(last_branch_state_t) * max_ncpus) {
			for (cpu = 0; cpu < real_ncpus; cpu++) {
				void *buf = (void *)(phys_carveout + lbr_stack_size * sizeof(uint64_t) * cpu);
				memcpy(buf, lbrs[cpu].from, sizeof(uint64_t) * lbr_stack_size);
				memcpy((uint64_t *)buf + lbr_stack_size * sizeof(uint64_t), lbrs[cpu].to,
				    sizeof(uint64_t) * lbr_stack_size);
			}
			/* Write 'LBRS' identifier, the number of CPUs and the LBR stack size */
			panic_lbr_header.id = LBR_MAGIC; /* 'LBRS' */
			panic_lbr_header.ncpus = real_ncpus;
			panic_lbr_header.lbr_count = lbr_stack_size;
			panic_lbr_header.pcarveout_va = phys_carveout;
		}
	}
}

void
lbr_for_kmode_init(uint32_t lbr_count)
{
	uint32_t size;
	int i;

	lbr_stack_size = lbr_count;

	/* Cannot use real_ncpus here as only one CPU is registered yet*/

	size = sizeof(uint64_t) * lbr_stack_size;
	for (i = 0; i < max_ncpus; i++) {
		lbrs[i].from = kalloc_data(size, Z_WAITOK | Z_ZERO);
		lbrs[i].to = kalloc_data(size, Z_WAITOK | Z_ZERO);
		if (!lbrs[i].from || !lbrs[i].to) {
			kprintf("LBR: Kalloc failed for lbrs.from/to\n");
			if (lbrs[i].from) {
				kfree_data(lbrs[i].from, size);
			}
			if (lbrs[i].to) {
				kfree_data(lbrs[i].to, size);
			}
			while (--i >= 0) {
				kfree_data(lbrs[i].from, size);
				kfree_data(lbrs[i].to, size);
			}
			goto err;
		}
	}

	read_lbr = capture_lbr_state;

	return;

err:
	last_branch_enabled_modes = LBR_ENABLED_NONE;
	return;
}

static void
write_lbr_to_panic_log(void)
{
	unsigned int cpu;
	int i;

	for (cpu = 0; cpu < real_ncpus; cpu++) {
		paniclog_append_noflush("LBR Stack (CPU %d):\n", cpu);
		for (i = 0; i < lbr_stack_size; i++) {
			if (lbrs[cpu].from[i] == 0x0 && lbrs[cpu].to[i] == 0x0) {
				continue;
			}
			paniclog_append_noflush("0x%llx : 0x%llx\n", lbrs[cpu].from[i], lbrs[cpu].to[i]);
		}
	}
	if (debug_can_coredump_phys_carveout()) {
		copy_lbr_data_for_core();
	}
}

void
panic_print_symbol_name(vm_address_t search)
{
	/* try searching in the kernel */
	if (panic_print_macho_symbol_name(&_mh_execute_header, search, "mach_kernel") == 0) {
		/* that failed, now try to search for the right kext */
		panic_print_kmod_symbol_name(search);
	}
}

/* Generate a backtrace, given a frame pointer - this routine
 * should walk the stack safely. The trace is appended to the panic log
 * and conditionally, to the console. If the trace contains kernel module
 * addresses, display the module name, load address and dependencies.
 */

#define DUMPFRAMES 32
#define PBT_TIMEOUT_CYCLES (5 * 1000 * 1000 * 1000ULL)
void
panic_i386_backtrace(void *_frame, int nframes, const char *msg, boolean_t regdump, x86_saved_state_t *regs)
{
	cframe_t        *frame = (cframe_t *)_frame;
	vm_offset_t raddrs[DUMPFRAMES];
	vm_offset_t PC = 0;
	int frame_index;
	volatile uint32_t *ppbtcnt = &pbtcnt;
	uint64_t bt_tsc_timeout;
	boolean_t keepsyms = FALSE;
	int cn = cpu_number();
	boolean_t old_doprnt_hide_pointers = doprnt_hide_pointers;
	thread_t cur_thread = current_thread();
	task_t task;
	struct proc *proc;

	/* Turn off I/O tracing now that we're panicking */
	iotrace_disable();

	if (pbtcpu != cn) {
		os_atomic_inc(&pbtcnt, relaxed);
		/* Spin on print backtrace lock, which serializes output
		 * Continue anyway if a timeout occurs.
		 */
		(void)hw_lock_to(&pbtlock, &hw_lock_spin_panic_policy, LCK_GRP_NULL);
		pbtcpu = cn;
	}

	if (__improbable(doprnt_hide_pointers == TRUE)) {
		/* If we're called directly, the Debugger() function will not be called,
		 * so we need to reset the value in here. */
		doprnt_hide_pointers = FALSE;
	}

	panic_check_hook();

	PE_parse_boot_argn("keepsyms", &keepsyms, sizeof(keepsyms));

	if (msg != NULL) {
		paniclog_append_noflush("%s", msg);
	}

	if ((regdump == TRUE) && (regs != NULL)) {
		x86_saved_state64_t     *ss64p = saved_state64(regs);
		paniclog_append_noflush(
			"RAX: 0x%016llx, RBX: 0x%016llx, RCX: 0x%016llx, RDX: 0x%016llx\n"
			"RSP: 0x%016llx, RBP: 0x%016llx, RSI: 0x%016llx, RDI: 0x%016llx\n"
			"R8:  0x%016llx, R9:  0x%016llx, R10: 0x%016llx, R11: 0x%016llx\n"
			"R12: 0x%016llx, R13: 0x%016llx, R14: 0x%016llx, R15: 0x%016llx\n"
			"RFL: 0x%016llx, RIP: 0x%016llx, CS:  0x%016llx, SS:  0x%016llx\n",
			ss64p->rax, ss64p->rbx, ss64p->rcx, ss64p->rdx,
			ss64p->isf.rsp, ss64p->rbp, ss64p->rsi, ss64p->rdi,
			ss64p->r8, ss64p->r9, ss64p->r10, ss64p->r11,
			ss64p->r12, ss64p->r13, ss64p->r14, ss64p->r15,
			ss64p->isf.rflags, ss64p->isf.rip, ss64p->isf.cs,
			ss64p->isf.ss);
		PC = ss64p->isf.rip;
	}

	// print current task info
	if (panic_get_thread_proc_task(cur_thread, &task, &proc)) {
		paniclog_append_noflush("Panicked task %p: %d threads: ",
		    task, task->thread_count);
		if (proc) {
			char name[MAXCOMLEN + 1];
			proc_name_kdp(proc, name, sizeof(name));
			paniclog_append_noflush("pid %d: %s", proc_pid(proc), name);
		} else {
			paniclog_append_noflush("unknown task");
		}

		paniclog_append_noflush("\n");
	}

	paniclog_append_noflush("Backtrace (CPU %d), panicked thread: %p, "
#if PRINT_ARGS_FROM_STACK_FRAME
	    "Frame : Return Address (4 potential args on stack)\n",
#else
	    "Frame : Return Address\n",
#endif
	    cn, cur_thread);

	for (frame_index = 0; frame_index < nframes; frame_index++) {
		vm_offset_t curframep = (vm_offset_t) frame;

		if (!curframep) {
			break;
		}

		if (curframep & 0x3) {
			paniclog_append_noflush("Unaligned frame\n");
			goto invalid;
		}

		if (!kvtophys(curframep) ||
		    !kvtophys(curframep + sizeof(cframe_t) - 1)) {
			paniclog_append_noflush("No mapping exists for frame pointer\n");
			goto invalid;
		}

		paniclog_append_noflush("%p : 0x%lx ", frame, frame->caller);
		if (frame_index < DUMPFRAMES) {
			raddrs[frame_index] = frame->caller;
		}

#if PRINT_ARGS_FROM_STACK_FRAME
		if (kvtophys((vm_offset_t)&(frame->args[3]))) {
			paniclog_append_noflush("(0x%x 0x%x 0x%x 0x%x) ",
			    frame->args[0], frame->args[1],
			    frame->args[2], frame->args[3]);
		}
#endif

		/* Display address-symbol translation only if the "keepsyms"
		 * boot-arg is suppplied, since we unload LINKEDIT otherwise.
		 * This routine is potentially unsafe; also, function
		 * boundary identification is unreliable after a strip -x.
		 */
		if (keepsyms) {
			panic_print_symbol_name((vm_address_t)frame->caller);
		}

		paniclog_append_noflush("\n");

		frame = frame->prev;
	}

	if (frame_index >= nframes && (vm_offset_t)frame != 0) {
		paniclog_append_noflush("\tBacktrace continues...\n");
	}

	goto out;

invalid:
	paniclog_append_noflush("Backtrace terminated-invalid frame pointer %p\n", frame);
out:

	/* Identify kernel modules in the backtrace and display their
	 * load addresses and dependencies. This routine should walk
	 * the kmod list safely.
	 */
	if (frame_index) {
		kmod_panic_dump((vm_offset_t *)&raddrs[0], frame_index);
	}

	if (PC != 0) {
		kmod_panic_dump(&PC, 1);
	}

	if (last_branch_enabled_modes == LBR_ENABLED_KERNELMODE) {
		write_lbr_to_panic_log();
	}

	panic_display_system_configuration(FALSE);

	doprnt_hide_pointers = old_doprnt_hide_pointers;

	/* Release print backtrace lock, to permit other callers in the
	 * event of panics on multiple processors.
	 */
	hw_lock_unlock(&pbtlock);
	os_atomic_dec(&pbtcnt, relaxed);
	/* Wait for other processors to complete output
	 * Timeout and continue after PBT_TIMEOUT_CYCLES.
	 */
	bt_tsc_timeout = rdtsc64() + PBT_TIMEOUT_CYCLES;
	while (*ppbtcnt && (rdtsc64() < bt_tsc_timeout)) {
		;
	}
}

static boolean_t
debug_copyin(pmap_t p, uint64_t uaddr, void *dest, size_t size)
{
	size_t rem = size;
	char *kvaddr = dest;

	while (rem) {
		ppnum_t upn = pmap_find_phys(p, uaddr);
		uint64_t phys_src = ptoa_64(upn) | (uaddr & PAGE_MASK);
		uint64_t phys_dest = kvtophys((vm_offset_t)kvaddr);
		uint64_t src_rem = PAGE_SIZE - (phys_src & PAGE_MASK);
		uint64_t dst_rem = PAGE_SIZE - (phys_dest & PAGE_MASK);
		size_t cur_size = (uint32_t) MIN(src_rem, dst_rem);
		cur_size = MIN(cur_size, rem);

		if (upn && pmap_valid_page(upn) && phys_dest) {
			bcopy_phys(phys_src, phys_dest, cur_size);
		} else {
			break;
		}
		uaddr += cur_size;
		kvaddr += cur_size;
		rem -= cur_size;
	}
	return rem == 0;
}

void
print_threads_registers(thread_t thread)
{
	x86_saved_state_t *savestate;

	savestate = get_user_regs(thread);
	paniclog_append_noflush(
		"\nRAX: 0x%016llx, RBX: 0x%016llx, RCX: 0x%016llx, RDX: 0x%016llx\n"
		"RSP: 0x%016llx, RBP: 0x%016llx, RSI: 0x%016llx, RDI: 0x%016llx\n"
		"R8:  0x%016llx, R9:  0x%016llx, R10: 0x%016llx, R11: 0x%016llx\n"
		"R12: 0x%016llx, R13: 0x%016llx, R14: 0x%016llx, R15: 0x%016llx\n"
		"RFL: 0x%016llx, RIP: 0x%016llx, CS:  0x%016llx, SS:  0x%016llx\n\n",
		savestate->ss_64.rax, savestate->ss_64.rbx, savestate->ss_64.rcx, savestate->ss_64.rdx,
		savestate->ss_64.isf.rsp, savestate->ss_64.rbp, savestate->ss_64.rsi, savestate->ss_64.rdi,
		savestate->ss_64.r8, savestate->ss_64.r9, savestate->ss_64.r10, savestate->ss_64.r11,
		savestate->ss_64.r12, savestate->ss_64.r13, savestate->ss_64.r14, savestate->ss_64.r15,
		savestate->ss_64.isf.rflags, savestate->ss_64.isf.rip, savestate->ss_64.isf.cs,
		savestate->ss_64.isf.ss);
}

void
print_tasks_user_threads(task_t task)
{
	thread_t                thread = current_thread();
	x86_saved_state_t *savestate;
	pmap_t                  pmap = 0;
	uint64_t                rbp;
	const char              *cur_marker = 0;
	int             j;

	for (j = 0, thread = (thread_t) queue_first(&task->threads); j < task->thread_count;
	    ++j, thread = (thread_t) queue_next(&thread->task_threads)) {
		paniclog_append_noflush("Thread %d: %p\n", j, thread);
		pmap = get_task_pmap(task);
		savestate = get_user_regs(thread);
		rbp = savestate->ss_64.rbp;
		paniclog_append_noflush("\t0x%016llx\n", savestate->ss_64.isf.rip);
		print_one_backtrace(pmap, (vm_offset_t)rbp, cur_marker, TRUE);
		paniclog_append_noflush("\n");
	}
}

void
print_thread_num_that_crashed(task_t task)
{
	thread_t                c_thread = current_thread();
	thread_t                thread;
	int             j;

	for (j = 0, thread = (thread_t) queue_first(&task->threads); j < task->thread_count;
	    ++j, thread = (thread_t) queue_next(&thread->task_threads)) {
		if (c_thread == thread) {
			paniclog_append_noflush("\nThread %d crashed\n", j);
			break;
		}
	}
}

#define PANICLOG_UUID_BUF_SIZE 256

void
print_uuid_info(task_t task)
{
	uint32_t                uuid_info_count = 0;
	mach_vm_address_t       uuid_info_addr = 0;
	boolean_t               have_map = (task->map != NULL) && (ml_validate_nofault((vm_offset_t)(task->map), sizeof(struct _vm_map)));
	boolean_t               have_pmap = have_map && (task->map->pmap != NULL) && (ml_validate_nofault((vm_offset_t)(task->map->pmap), sizeof(struct pmap)));
	int                             task_pid = pid_from_task(task);
	char                    uuidbuf[PANICLOG_UUID_BUF_SIZE] = {0};
	char                    *uuidbufptr = uuidbuf;
	uint32_t                k;

	if (have_pmap && task->active && task_pid > 0) {
		/* Read dyld_all_image_infos struct from task memory to get UUID array count & location */
		struct user64_dyld_all_image_infos task_image_infos;
		if (debug_copyin(task->map->pmap, task->all_image_info_addr,
		    &task_image_infos, sizeof(struct user64_dyld_all_image_infos))) {
			uuid_info_count = (uint32_t)task_image_infos.uuidArrayCount;
			uuid_info_addr = task_image_infos.uuidArray;
		}

		/* If we get a NULL uuid_info_addr (which can happen when we catch dyld
		 * in the middle of updating this data structure), we zero the
		 * uuid_info_count so that we won't even try to save load info for this task
		 */
		if (!uuid_info_addr) {
			uuid_info_count = 0;
		}
	}

	if (task_pid > 0 && uuid_info_count > 0) {
		uint32_t uuid_info_size = sizeof(struct user64_dyld_uuid_info);
		uint32_t uuid_array_size = uuid_info_count * uuid_info_size;
		uint32_t uuid_copy_size = 0;
		uint32_t uuid_image_count = 0;
		char *current_uuid_buffer = NULL;
		/* Copy in the UUID info array. It may be nonresident, in which case just fix up nloadinfos to 0 */

		paniclog_append_noflush("\nuuid info:\n");
		while (uuid_array_size) {
			if (uuid_array_size <= PANICLOG_UUID_BUF_SIZE) {
				uuid_copy_size = uuid_array_size;
				uuid_image_count = uuid_array_size / uuid_info_size;
			} else {
				uuid_image_count = PANICLOG_UUID_BUF_SIZE / uuid_info_size;
				uuid_copy_size = uuid_image_count * uuid_info_size;
			}
			if (have_pmap && !debug_copyin(task->map->pmap, uuid_info_addr, uuidbufptr,
			    uuid_copy_size)) {
				paniclog_append_noflush("Error!! Failed to copy UUID info for task %p pid %d\n", task, task_pid);
				uuid_image_count = 0;
				break;
			}

			if (uuid_image_count > 0) {
				current_uuid_buffer = uuidbufptr;
				for (k = 0; k < uuid_image_count; k++) {
					paniclog_append_noflush(" %#llx", *(uint64_t *)current_uuid_buffer);
					current_uuid_buffer += sizeof(uint64_t);
					uint8_t *uuid = (uint8_t *)current_uuid_buffer;
					paniclog_append_noflush("\tuuid = <%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x>\n",
					    uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7], uuid[8],
					    uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
					current_uuid_buffer += 16;
				}
				bzero(&uuidbuf, sizeof(uuidbuf));
			}
			uuid_info_addr += uuid_copy_size;
			uuid_array_size -= uuid_copy_size;
		}
	}
}

void
print_launchd_info(void)
{
	task_t          task = current_task();
	thread_t        thread = current_thread();
	volatile        uint32_t *ppbtcnt = &pbtcnt;
	uint64_t        bt_tsc_timeout;
	int             cn = cpu_number();

	if (pbtcpu != cn) {
		os_atomic_inc(&pbtcnt, relaxed);
		/* Spin on print backtrace lock, which serializes output
		 * Continue anyway if a timeout occurs.
		 */
		(void)hw_lock_to(&pbtlock, &hw_lock_spin_panic_policy, LCK_GRP_NULL);
		pbtcpu = cn;
	}

	print_uuid_info(task);
	print_thread_num_that_crashed(task);
	print_threads_registers(thread);
	print_tasks_user_threads(task);

	panic_display_system_configuration(TRUE);

	/* Release print backtrace lock, to permit other callers in the
	 * event of panics on multiple processors.
	 */
	hw_lock_unlock(&pbtlock);
	os_atomic_dec(&pbtcnt, relaxed);
	/* Wait for other processors to complete output
	 * Timeout and continue after PBT_TIMEOUT_CYCLES.
	 */
	bt_tsc_timeout = rdtsc64() + PBT_TIMEOUT_CYCLES;
	while (*ppbtcnt && (rdtsc64() < bt_tsc_timeout)) {
		;
	}
}

/*
 * Compares 2 EFI GUIDs. Returns true if they match.
 */
static bool
efi_compare_guids(EFI_GUID *guid1, EFI_GUID *guid2)
{
	return (bcmp(guid1, guid2, sizeof(EFI_GUID)) == 0) ? true : false;
}

/*
 * Converts from an efiboot-originated virtual address to a physical
 * address.
 */
static inline uint64_t
efi_efiboot_virtual_to_physical(uint64_t addr)
{
	if (addr >= VM_MIN_KERNEL_ADDRESS) {
		return addr & (0x40000000ULL - 1);
	} else {
		return addr;
	}
}

/*
 * Convers from a efiboot-originated virtual address to an accessible
 * pointer to that physical address by translating it to a physmap-relative
 * address.
 */
static void *
efi_efiboot_virtual_to_physmap_virtual(uint64_t addr)
{
	return PHYSMAP_PTOV(efi_efiboot_virtual_to_physical(addr));
}

/*
 * Returns the physical address of the firmware table identified
 * by the passed-in GUID, or 0 if the table could not be located.
 */
static uint64_t
efi_get_cfgtbl_by_guid(EFI_GUID *guidp)
{
	EFI_CONFIGURATION_TABLE_64 *cfg_table_entp, *cfgTable;
	boot_args *args = (boot_args *)PE_state.bootArgs;
	EFI_SYSTEM_TABLE_64 *estp;
	uint32_t i, hdr_cksum, cksum;

	estp = (EFI_SYSTEM_TABLE_64 *)efi_efiboot_virtual_to_physmap_virtual(args->efiSystemTable);

	assert(estp != 0);

	// Verify signature of the system table
	hdr_cksum = estp->Hdr.CRC32;
	estp->Hdr.CRC32 = 0;
	cksum = crc32(0L, estp, estp->Hdr.HeaderSize);
	estp->Hdr.CRC32 = hdr_cksum;

	if (cksum != hdr_cksum) {
		DPRINTF("efi_get_cfgtbl_by_guid: EST CRC32 = 0x%x, header = 0x%x\n", cksum, hdr_cksum);
		DPRINTF("Bad EFI system table checksum\n");
		return 0;
	}

	/*
	 * efiboot can (and will) change the address of ConfigurationTable (and each table's VendorTable address)
	 * to a kernel-virtual address.  Reverse that to get the physical address, which we then use to get a
	 * physmap-based virtual address.
	 */
	cfgTable = (EFI_CONFIGURATION_TABLE_64 *)efi_efiboot_virtual_to_physmap_virtual(estp->ConfigurationTable);

	for (i = 0; i < estp->NumberOfTableEntries; i++) {
		cfg_table_entp = (EFI_CONFIGURATION_TABLE_64 *)&cfgTable[i];

		DPRINTF("EST: Comparing GUIDs for entry %d\n", i);
		if (cfg_table_entp == 0) {
			continue;
		}

		if (efi_compare_guids(&cfg_table_entp->VendorGuid, guidp) == true) {
			DPRINTF("GUID match: returning %p\n", (void *)(uintptr_t)cfg_table_entp->VendorTable);
			return efi_efiboot_virtual_to_physical(cfg_table_entp->VendorTable);
		}
	}

	/* Not found */
	return 0;
}

/*
 * Returns the physical address of the RSDP (either v1 or >=v2) or 0
 * if the RSDP could not be located.
 */
uint64_t
efi_get_rsdp_physaddr(void)
{
	uint64_t rsdp_addr;
#define ACPI_RSDP_GUID \
    { 0xeb9d2d30, 0x2d88, 0x11d3, {0x9a, 0x16, 0x0, 0x90, 0x27, 0x3f, 0xc1, 0x4d} }
#define ACPI_20_RSDP_GUID \
    { 0x8868e871, 0xe4f1, 0x11d3, {0xbc, 0x22, 0x0, 0x80, 0xc7, 0x3c, 0x88, 0x81} }

	static EFI_GUID EFI_RSDP_GUID_ACPI20 = ACPI_20_RSDP_GUID;
	static EFI_GUID EFI_RSDP_GUID_ACPI10 = ACPI_RSDP_GUID;

	if ((rsdp_addr = efi_get_cfgtbl_by_guid(&EFI_RSDP_GUID_ACPI20)) == 0) {
		DPRINTF("RSDP ACPI 2.0 lookup failed.  Trying RSDP ACPI 1.0...\n");
		rsdp_addr = efi_get_cfgtbl_by_guid(&EFI_RSDP_GUID_ACPI10);
		if (rsdp_addr == 0) {
			DPRINTF("RSDP ACPI 1.0 lookup failed also.\n");
		}
	}

	return rsdp_addr;
}
