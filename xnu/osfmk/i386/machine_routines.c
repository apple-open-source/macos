/*
 * Copyright (c) 2000-2012 Apple Inc. All rights reserved.
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

#include <i386/machine_routines.h>
#include <i386/cpuid.h>
#include <i386/fpu.h>
#include <mach/processor.h>
#include <kern/processor.h>
#include <kern/machine.h>

#include <kern/cpu_number.h>
#include <kern/thread.h>
#include <kern/thread_call.h>
#include <kern/policy_internal.h>

#include <prng/random.h>
#include <prng/entropy.h>
#include <i386/machine_cpu.h>
#include <i386/lapic.h>
#include <i386/bit_routines.h>
#include <i386/mp_events.h>
#include <i386/pmCPU.h>
#include <i386/trap_internal.h>
#include <i386/tsc.h>
#include <i386/cpu_threads.h>
#include <i386/proc_reg.h>
#include <mach/vm_param.h>
#include <i386/pmap.h>
#include <i386/pmap_internal.h>
#include <i386/misc_protos.h>
#include <kern/timer_queue.h>
#include <vm/vm_map_xnu.h>
#include <kern/monotonic.h>
#include <kern/kpc.h>
#include <architecture/i386/pio.h>
#include <i386/cpu_data.h>
#if DEBUG
#define DBG(x...)       kprintf("DBG: " x)
#else
#define DBG(x...)
#endif

extern void     wakeup(void *);

uint64_t        LockTimeOut;
uint64_t        TLBTimeOut;
uint64_t        LockTimeOutTSC;
uint32_t        LockTimeOutUsec;
uint64_t        MutexSpin;
uint64_t        low_MutexSpin;
int64_t         high_MutexSpin;
uint64_t        LastDebuggerEntryAllowance;
uint64_t        delay_spin_threshold;

extern uint64_t panic_restart_timeout;

boolean_t virtualized = FALSE;

static SIMPLE_LOCK_DECLARE(ml_timer_evaluation_slock, 0);
uint32_t ml_timer_eager_evaluations;
uint64_t ml_timer_eager_evaluation_max;
static boolean_t ml_timer_evaluation_in_progress = FALSE;

LCK_GRP_DECLARE(max_cpus_grp, "max_cpus");
LCK_MTX_DECLARE(max_cpus_lock, &max_cpus_grp);
static int max_cpus_initialized = 0;
#define MAX_CPUS_SET    0x1
#define MAX_CPUS_WAIT   0x2

/* IO memory map services */

/* Map memory map IO space */
vm_offset_t
ml_io_map(
	vm_offset_t phys_addr,
	vm_size_t size)
{
	return io_map(phys_addr, size, VM_WIMG_IO, VM_PROT_DEFAULT, false);
}

vm_offset_t
ml_io_map_wcomb(
	vm_offset_t phys_addr,
	vm_size_t size)
{
	return io_map(phys_addr, size, VM_WIMG_WCOMB, VM_PROT_DEFAULT, false);
}

vm_offset_t
ml_io_map_unmappable(
	vm_offset_t             phys_addr,
	vm_size_t               size,
	unsigned int            flags)
{
	return io_map(phys_addr, size, flags, VM_PROT_DEFAULT, true);
}

void
ml_get_bouncepool_info(vm_offset_t *phys_addr, vm_size_t *size)
{
	*phys_addr = 0;
	*size      = 0;
}


vm_offset_t
ml_static_ptovirt(
	vm_offset_t paddr)
{
#if defined(__x86_64__)
	return (vm_offset_t)(((unsigned long) paddr) | VM_MIN_KERNEL_ADDRESS);
#else
	return (vm_offset_t)((paddr) | LINEAR_KERNEL_ADDRESS);
#endif
}

vm_offset_t
ml_static_slide(
	vm_offset_t vaddr)
{
	return vaddr + vm_kernel_slide;
}

/*
 * base must be page-aligned, and size must be a multiple of PAGE_SIZE
 */
kern_return_t
ml_static_verify_page_protections(
	uint64_t base, uint64_t size, vm_prot_t prot)
{
	vm_prot_t pageprot;
	uint64_t offset;

	DBG("ml_static_verify_page_protections: vaddr 0x%llx sz 0x%llx prot 0x%x\n", base, size, prot);

	/*
	 * base must be within the static bounds, defined to be:
	 * (vm_kernel_stext, kc_highest_nonlinkedit_vmaddr)
	 */
#if DEVELOPMENT || DEBUG || KASAN
	assert(kc_highest_nonlinkedit_vmaddr > 0 && base > vm_kernel_stext && base < kc_highest_nonlinkedit_vmaddr);
#else   /* On release kernels, assume this is a protection mismatch failure. */
	if (kc_highest_nonlinkedit_vmaddr == 0 || base < vm_kernel_stext || base >= kc_highest_nonlinkedit_vmaddr) {
		return KERN_FAILURE;
	}
#endif

	for (offset = 0; offset < size; offset += PAGE_SIZE) {
		if (pmap_get_prot(kernel_pmap, base + offset, &pageprot) == KERN_FAILURE) {
			return KERN_FAILURE;
		}
		if ((pageprot & prot) != prot) {
			return KERN_FAILURE;
		}
	}

	return KERN_SUCCESS;
}

vm_offset_t
ml_static_unslide(
	vm_offset_t vaddr)
{
	return vaddr - vm_kernel_slide;
}

/*
 * Reclaim memory, by virtual address, that was used in early boot that is no longer needed
 * by the kernel.
 */
void
ml_static_mfree(
	vm_offset_t vaddr,
	vm_size_t size)
{
	addr64_t vaddr_cur;
	ppnum_t ppn;
	uint32_t freed_pages = 0;
	vm_size_t map_size;

	assert(vaddr >= VM_MIN_KERNEL_ADDRESS);

	assert((vaddr & (PAGE_SIZE - 1)) == 0); /* must be page aligned */

	for (vaddr_cur = vaddr; vaddr_cur < round_page_64(vaddr + size);) {
		map_size = pmap_query_pagesize(kernel_pmap, vaddr_cur);

		/* just skip if nothing mapped here */
		if (map_size == 0) {
			vaddr_cur += PAGE_SIZE;
			continue;
		}

		/*
		 * Can't free from the middle of a large page.
		 */
		assert((vaddr_cur & (map_size - 1)) == 0);

		ppn = pmap_find_phys(kernel_pmap, vaddr_cur);
		assert(ppn != (ppnum_t)NULL);

		pmap_remove(kernel_pmap, vaddr_cur, vaddr_cur + map_size);
		while (map_size > 0) {
			assert(pmap_valid_page(ppn));
			if (IS_MANAGED_PAGE(ppn)) {
				vm_page_create(ppn, (ppn + 1));
				freed_pages++;
			}
			map_size -= PAGE_SIZE;
			vaddr_cur += PAGE_SIZE;
			ppn++;
		}
	}
	vm_page_lockspin_queues();
	vm_page_wire_count -= freed_pages;
	vm_page_wire_count_initial -= freed_pages;
	if (vm_page_wire_count_on_boot != 0) {
		assert(vm_page_wire_count_on_boot >= freed_pages);
		vm_page_wire_count_on_boot -= freed_pages;
	}
	vm_page_unlock_queues();

#if     DEBUG
	kprintf("ml_static_mfree: Released 0x%x pages at VA %p, size:0x%llx, last ppn: 0x%x\n", freed_pages, (void *)vaddr, (uint64_t)size, ppn);
#endif
}

/* Change page protections for addresses previously loaded by efiboot */
kern_return_t
ml_static_protect(vm_offset_t vmaddr, vm_size_t size, vm_prot_t prot)
{
	boolean_t NX = !!!(prot & VM_PROT_EXECUTE), ro = !!!(prot & VM_PROT_WRITE);

	assert(prot & VM_PROT_READ);

	pmap_mark_range(kernel_pmap, vmaddr, size, NX, ro);

	return KERN_SUCCESS;
}

/* virtual to physical on wired pages */
vm_offset_t
ml_vtophys(
	vm_offset_t vaddr)
{
	return (vm_offset_t)kvtophys(vaddr);
}

/*
 *	Routine:        ml_nofault_copy
 *	Function:	Perform a physical mode copy if the source and
 *			destination have valid translations in the kernel pmap.
 *			If translations are present, they are assumed to
 *			be wired; i.e. no attempt is made to guarantee that the
 *			translations obtained remained valid for
 *			the duration of the copy process.
 */

vm_size_t
ml_nofault_copy(
	vm_offset_t virtsrc, vm_offset_t virtdst, vm_size_t size)
{
	addr64_t cur_phys_dst, cur_phys_src;
	uint32_t count, nbytes = 0;

	while (size > 0) {
		if (!(cur_phys_src = kvtophys(virtsrc))) {
			break;
		}
		if (!(cur_phys_dst = kvtophys(virtdst))) {
			break;
		}
		if (!pmap_valid_page(i386_btop(cur_phys_dst)) || !pmap_valid_page(i386_btop(cur_phys_src))) {
			break;
		}
		count = (uint32_t)(PAGE_SIZE - (cur_phys_src & PAGE_MASK));
		if (count > (PAGE_SIZE - (cur_phys_dst & PAGE_MASK))) {
			count = (uint32_t)(PAGE_SIZE - (cur_phys_dst & PAGE_MASK));
		}
		if (count > size) {
			count = (uint32_t)size;
		}

		bcopy_phys(cur_phys_src, cur_phys_dst, count);

		nbytes += count;
		virtsrc += count;
		virtdst += count;
		size -= count;
	}

	return nbytes;
}

/*
 *	Routine:        ml_validate_nofault
 *	Function: Validate that ths address range has a valid translations
 *			in the kernel pmap.  If translations are present, they are
 *			assumed to be wired; i.e. no attempt is made to guarantee
 *			that the translation persist after the check.
 *  Returns: TRUE if the range is mapped and will not cause a fault,
 *			FALSE otherwise.
 */

boolean_t
ml_validate_nofault(
	vm_offset_t virtsrc, vm_size_t size)
{
	addr64_t cur_phys_src;
	uint32_t count;

	while (size > 0) {
		if (!(cur_phys_src = kvtophys(virtsrc))) {
			return FALSE;
		}
		if (!pmap_valid_page(i386_btop(cur_phys_src))) {
			return FALSE;
		}
		count = (uint32_t)(PAGE_SIZE - (cur_phys_src & PAGE_MASK));
		if (count > size) {
			count = (uint32_t)size;
		}

		virtsrc += count;
		size -= count;
	}

	return TRUE;
}

/* Interrupt handling */

/* Initialize Interrupts */
void
ml_init_interrupt(void)
{
	(void) ml_set_interrupts_enabled(TRUE);
}


/* Get Interrupts Enabled */
boolean_t
ml_get_interrupts_enabled(void)
{
	unsigned long flags;

	__asm__ volatile ("pushf; pop	%0":  "=r" (flags));
	return (flags & EFL_IF) != 0;
}

/* Set Interrupts Enabled */
boolean_t
ml_set_interrupts_enabled(boolean_t enable)
{
	unsigned long flags;
	boolean_t istate;

	__asm__ volatile ("pushf; pop	%0"  :  "=r" (flags));

	assert(get_interrupt_level() ? (enable == FALSE) : TRUE);

	istate = ((flags & EFL_IF) != 0);

	if (enable) {
		__asm__ volatile ("sti;nop");

		if ((get_preemption_level() == 0) && (*ast_pending() & AST_URGENT)) {
			__asm__ volatile ("int %0" :: "N" (T_PREEMPT));
		}
	} else {
		if (istate) {
			__asm__ volatile ("cli");
		}
	}

	return istate;
}

/* Early Set Interrupts Enabled */
boolean_t
ml_early_set_interrupts_enabled(boolean_t enable)
{
	if (enable == TRUE) {
		kprintf("Caller attempted to enable interrupts too early in "
		    "kernel startup. Halting.\n");
		hlt();
		/*NOTREACHED*/
	}

	/* On x86, do not allow interrupts to be enabled very early */
	return FALSE;
}

/* Check if running at interrupt context */
boolean_t
ml_at_interrupt_context(void)
{
	return get_interrupt_level() != 0;
}

/*
 * This answers the question
 * "after returning from this interrupt handler with the AST_URGENT bit set,
 * will I end up in ast_taken_user or ast_taken_kernel?"
 *
 * If it's called in non-interrupt context (e.g. regular syscall), it should
 * return false.
 *
 * Must be called with interrupts disabled.
 */
bool
ml_did_interrupt_userspace(void)
{
	assert(ml_get_interrupts_enabled() == false);

	x86_saved_state_t *state = current_cpu_datap()->cpu_int_state;
	if (!state) {
		return false;
	}

	uint64_t cs;

	if (is_saved_state64(state)) {
		cs = saved_state64(state)->isf.cs;
	} else {
		cs = saved_state32(state)->cs;
	}

	return (cs & SEL_PL) == SEL_PL_U;
}

void
ml_get_power_state(boolean_t *icp, boolean_t *pidlep)
{
	*icp = (get_interrupt_level() != 0);
	/* These will be technically inaccurate for interrupts that occur
	 * successively within a single "idle exit" event, but shouldn't
	 * matter statistically.
	 */
	*pidlep = (current_cpu_datap()->lcpu.package->num_idle == topoParms.nLThreadsPerPackage);
}

/* Generate a fake interrupt */
__dead2
void
ml_cause_interrupt(void)
{
	panic("ml_cause_interrupt not defined yet on Intel");
}

/*
 * TODO: transition users of this to kernel_thread_start_priority
 * ml_thread_policy is an unsupported KPI
 */
void
ml_thread_policy(
	thread_t thread,
	__unused        unsigned policy_id,
	unsigned policy_info)
{
	if (policy_info & MACHINE_NETWORK_WORKLOOP) {
		thread_precedence_policy_data_t info;
		__assert_only kern_return_t kret;

		info.importance = 1;

		kret = thread_policy_set_internal(thread, THREAD_PRECEDENCE_POLICY,
		    (thread_policy_t)&info,
		    THREAD_PRECEDENCE_POLICY_COUNT);
		assert(kret == KERN_SUCCESS);
	}
}

/* Initialize Interrupts */
void
ml_install_interrupt_handler(
	void *nub,
	int source,
	void *target,
	IOInterruptHandler handler,
	void *refCon)
{
	boolean_t current_state;

	current_state = ml_set_interrupts_enabled(FALSE);

	PE_install_interrupt_handler(nub, source, target,
	    (IOInterruptHandler) handler, refCon);

	(void) ml_set_interrupts_enabled(current_state);
}


void
machine_signal_idle(
	processor_t processor)
{
	cpu_interrupt(processor->cpu_id);
}

__dead2
void
machine_signal_idle_deferred(
	__unused processor_t processor)
{
	panic("Unimplemented");
}

__dead2
void
machine_signal_idle_cancel(
	__unused processor_t processor)
{
	panic("Unimplemented");
}

static void
register_cpu(
	uint32_t        lapic_id,
	processor_t     *processor_out,
	boolean_t       boot_cpu )
{
	int             target_cpu;
	cpu_data_t      *this_cpu_datap;

	this_cpu_datap = cpu_data_alloc(boot_cpu);
	assert(this_cpu_datap);

	target_cpu = this_cpu_datap->cpu_number;
	assert((boot_cpu && (target_cpu == 0)) ||
	    (!boot_cpu && (target_cpu != 0)));

	lapic_cpu_map(lapic_id, target_cpu);

	/* The cpu_id is not known at registration phase. Just do
	 * lapic_id for now
	 */
	this_cpu_datap->cpu_phys_number = lapic_id;

#if CONFIG_CPU_COUNTERS
	kpc_register_cpu(this_cpu_datap);
#endif /* CONFIG_CPU_COUNTERS */

	if (!boot_cpu) {
		cpu_thread_alloc(this_cpu_datap->cpu_number);
		assert(this_cpu_datap->lcpu.core != NULL);
	}

	/*
	 * processor_init() deferred to topology start
	 * because "slot numbers" a.k.a. logical processor numbers
	 * are not yet finalized.
	 */
	*processor_out = this_cpu_datap->cpu_processor;
}

/*
 * AppleACPICPU calls this function twice for each CPU.
 * Once with start == false to register the CPU, then xnu sorts the topology,
 * then again with start == true to boot the CPU with the assigned CPU number.
 *
 * xnu or EFI can limit the number of booted CPUs.
 * xnu does it by cpu_topology_start_cpu refusing to call processor_boot.
 * EFI does it by populating the ACPI table with a flag that convinces
 * AppleACPICPU to not call ml_processor_register.
 *
 * See https://support.apple.com/en-us/101870 for when EFI does this. (nvram SMTDisable=%01)
 * When this happens the processors show up in ACPI but they do not get ml_processor_register'ed.
 */
kern_return_t
ml_processor_register(
	cpu_id_t        cpu_id,
	uint32_t        lapic_id,
	processor_t     *processor_out,
	boolean_t       boot_cpu,
	boolean_t       start )
{
	static boolean_t done_topo_sort = FALSE;
	static bool done_registering_and_starting = false;
	static uint32_t num_registered = 0;

	/* Register all CPUs first, and track max */
	if (start == FALSE) {
		num_registered++;

		DBG( "registering CPU lapic id %d\n", lapic_id );

		register_cpu( lapic_id, processor_out, boot_cpu );
		return KERN_SUCCESS;
	}

	/* Sort by topology before we start anything */
	if (!done_topo_sort) {
		DBG( "about to start CPUs. %d registered\n", num_registered );

		cpu_topology_sort( num_registered );
		done_topo_sort = TRUE;
	}

	/* Assign the cpu ID */
	uint32_t cpunum = -1;
	cpu_data_t  *this_cpu_datap = NULL;

	/* find cpu num and pointer */
	cpunum = ml_get_cpuid( lapic_id );

	if (cpunum == 0xFFFFFFFF) { /* never heard of it? */
		panic( "trying to start invalid/unregistered CPU %d", lapic_id );
	}

	this_cpu_datap = cpu_datap(cpunum);

	/* fix the CPU id */
	this_cpu_datap->cpu_id = cpu_id;

	/* allocate and initialize other per-cpu structures */
	if (!boot_cpu) {
		mp_cpus_call_cpu_init(cpunum);
		random_cpu_init(cpunum);
	}

	/* output arg */
	*processor_out = this_cpu_datap->cpu_processor;

	/* OK, try and start this CPU */
	kern_return_t ret = cpu_topology_start_cpu( cpunum );

	/*
	 * AppleACPICPU will start processors in CPU number order,
	 * so when we get the last CPU number, it's finished
	 * calling ml_processor_register.
	 *
	 * By this point max cpus has been determined.  There may be more
	 * registrations than max_cpus in the case of `cpus=` boot arg.
	 */
	if (cpunum == num_registered - 1) {
		__assert_only bool success;
		success = os_atomic_cmpxchg(&done_registering_and_starting, false, true, relaxed);
		assert(success);

		assert(max_cpus_initialized == MAX_CPUS_SET);

		ml_cpu_init_completed();
	} else {
		assert(os_atomic_load(&done_registering_and_starting, relaxed) == false);
	}

	return ret;
}


/*
 * This is called when all of the ml_processor_info_t structures have been
 * initialized and all the processors have been started through processor_start().
 *
 * Required by the scheduler subsystem.
 */
void
ml_cpu_init_completed(void)
{
	sched_cpu_init_completed();
}


void
ml_cpu_get_info_type(ml_cpu_info_t *cpu_infop, cluster_type_t cluster_type __unused)
{
	boolean_t       os_supports_sse;
	i386_cpu_info_t *cpuid_infop;

	if (cpu_infop == NULL) {
		return;
	}

	/*
	 * Are we supporting MMX/SSE/SSE2/SSE3?
	 * As distinct from whether the cpu has these capabilities.
	 */
	os_supports_sse = !!(get_cr4() & CR4_OSXMM);

	if (ml_fpu_avx_enabled()) {
		cpu_infop->vector_unit = 9;
	} else if ((cpuid_features() & CPUID_FEATURE_SSE4_2) && os_supports_sse) {
		cpu_infop->vector_unit = 8;
	} else if ((cpuid_features() & CPUID_FEATURE_SSE4_1) && os_supports_sse) {
		cpu_infop->vector_unit = 7;
	} else if ((cpuid_features() & CPUID_FEATURE_SSSE3) && os_supports_sse) {
		cpu_infop->vector_unit = 6;
	} else if ((cpuid_features() & CPUID_FEATURE_SSE3) && os_supports_sse) {
		cpu_infop->vector_unit = 5;
	} else if ((cpuid_features() & CPUID_FEATURE_SSE2) && os_supports_sse) {
		cpu_infop->vector_unit = 4;
	} else if ((cpuid_features() & CPUID_FEATURE_SSE) && os_supports_sse) {
		cpu_infop->vector_unit = 3;
	} else if (cpuid_features() & CPUID_FEATURE_MMX) {
		cpu_infop->vector_unit = 2;
	} else {
		cpu_infop->vector_unit = 0;
	}

	cpuid_infop  = cpuid_info();

	cpu_infop->cache_line_size = cpuid_infop->cache_linesize;

	cpu_infop->l1_icache_size = cpuid_infop->cache_size[L1I];
	cpu_infop->l1_dcache_size = cpuid_infop->cache_size[L1D];

	if (cpuid_infop->cache_size[L2U] > 0) {
		cpu_infop->l2_settings = 1;
		cpu_infop->l2_cache_size = cpuid_infop->cache_size[L2U];
	} else {
		cpu_infop->l2_settings = 0;
		cpu_infop->l2_cache_size = 0xFFFFFFFF;
	}

	if (cpuid_infop->cache_size[L3U] > 0) {
		cpu_infop->l3_settings = 1;
		cpu_infop->l3_cache_size = cpuid_infop->cache_size[L3U];
	} else {
		cpu_infop->l3_settings = 0;
		cpu_infop->l3_cache_size = 0xFFFFFFFF;
	}
}

/*
 *	Routine:        ml_cpu_get_info
 *	Function: Fill out the ml_cpu_info_t structure with parameters associated
 *	with the boot cluster.
 */
void
ml_cpu_get_info(ml_cpu_info_t * ml_cpu_info)
{
	ml_cpu_get_info_type(ml_cpu_info, CLUSTER_TYPE_SMP);
}

unsigned int
ml_get_cpu_number_type(cluster_type_t cluster_type __unused, bool logical, bool available)
{
	/*
	 * At present no supported x86 system features more than 1 CPU type. Because
	 * of this, the cluster_type parameter is ignored.
	 */
	if (logical && available) {
		return machine_info.logical_cpu;
	} else if (logical && !available) {
		return machine_info.logical_cpu_max;
	} else if (!logical && available) {
		return machine_info.physical_cpu;
	} else {
		return machine_info.physical_cpu_max;
	}
}

void
ml_get_cluster_type_name(cluster_type_t cluster_type __unused, char *name, size_t name_size)
{
	strlcpy(name, "Standard", name_size);
}

unsigned int
ml_get_cluster_number_type(cluster_type_t cluster_type __unused)
{
	/*
	 * At present no supported x86 system has more than 1 CPU type and multiple
	 * clusters.
	 */
	return 1;
}

unsigned int
ml_get_cpu_types(void)
{
	return 1 << CLUSTER_TYPE_SMP;
}

unsigned int
ml_get_cluster_count(void)
{
	/*
	 * At present no supported x86 system has more than 1 CPU type and multiple
	 * clusters.
	 */
	return 1;
}

static_assert(MAX_CPUS <= 256, "MAX_CPUS must fit in _COMM_PAGE_CPU_TO_CLUSTER; Increase table size if needed");

void
ml_map_cpus_to_clusters(uint8_t *table)
{
	for (uint16_t cpu_id = 0; cpu_id < machine_info.logical_cpu_max; cpu_id++) {
		// Supported x86 systems have 1 cluster
		*(table + cpu_id) = (uint8_t)0;
	}
}

int
ml_early_cpu_max_number(void)
{
	int n = max_ncpus;

	assert(startup_phase >= STARTUP_SUB_TUNABLES);
	if (max_cpus_from_firmware) {
		n = MIN(n, max_cpus_from_firmware);
	}
	return n - 1;
}

void
ml_set_max_cpus(unsigned int max_cpus)
{
	lck_mtx_lock(&max_cpus_lock);
	if (max_cpus_initialized != MAX_CPUS_SET) {
		if (max_cpus > 0 && max_cpus <= MAX_CPUS) {
			/*
			 * Note: max_cpus is the number of enabled processors
			 * that ACPI found; max_ncpus is the maximum number
			 * that the kernel supports or that the "cpus="
			 * boot-arg has set. Here we take int minimum.
			 */
			machine_info.max_cpus = (integer_t)MIN(max_cpus, max_ncpus);
		}
		if (max_cpus_initialized == MAX_CPUS_WAIT) {
			thread_wakeup((event_t) &max_cpus_initialized);
		}
		max_cpus_initialized = MAX_CPUS_SET;
	}
	lck_mtx_unlock(&max_cpus_lock);
}

unsigned int
ml_wait_max_cpus(void)
{
	lck_mtx_lock(&max_cpus_lock);
	while (max_cpus_initialized != MAX_CPUS_SET) {
		max_cpus_initialized = MAX_CPUS_WAIT;
		lck_mtx_sleep(&max_cpus_lock, LCK_SLEEP_DEFAULT, &max_cpus_initialized, THREAD_UNINT);
	}
	lck_mtx_unlock(&max_cpus_lock);
	return machine_info.max_cpus;
}

void
ml_panic_trap_to_debugger(__unused const char *panic_format_str,
    __unused va_list *panic_args,
    __unused unsigned int reason,
    __unused void *ctx,
    __unused uint64_t panic_options_mask,
    __unused unsigned long panic_caller,
    __unused const char *panic_initiator)
{
	return;
}

static uint64_t
virtual_timeout_inflate64(unsigned int vti, uint64_t timeout, uint64_t max_timeout)
{
	if (vti >= 64) {
		return max_timeout;
	}

	if ((timeout << vti) >> vti != timeout) {
		return max_timeout;
	}

	if ((timeout << vti) > max_timeout) {
		return max_timeout;
	}

	return timeout << vti;
}

static uint32_t
virtual_timeout_inflate32(unsigned int vti, uint32_t timeout, uint32_t max_timeout)
{
	if (vti >= 32) {
		return max_timeout;
	}

	if ((timeout << vti) >> vti != timeout) {
		return max_timeout;
	}

	return timeout << vti;
}

/*
 * Some timeouts are later adjusted or used in calculations setting
 * other values. In order to avoid overflow, cap the max timeout as
 * 2^47ns (~39 hours).
 */
static const uint64_t max_timeout_ns = 1ULL << 47;

/*
 * Inflate a timeout in absolutetime.
 */
static uint64_t
virtual_timeout_inflate_abs(unsigned int vti, uint64_t timeout)
{
	uint64_t max_timeout;
	nanoseconds_to_absolutetime(max_timeout_ns, &max_timeout);
	return virtual_timeout_inflate64(vti, timeout, max_timeout);
}

/*
 * Inflate a value in TSC ticks.
 */
static uint64_t
virtual_timeout_inflate_tsc(unsigned int vti, uint64_t timeout)
{
	const uint64_t max_timeout = tmrCvt(max_timeout_ns, tscFCvtn2t);
	return virtual_timeout_inflate64(vti, timeout, max_timeout);
}

/*
 * Inflate a timeout in microseconds.
 */
static uint32_t
virtual_timeout_inflate_us(unsigned int vti, uint64_t timeout)
{
	const uint32_t max_timeout = ~0;
	return virtual_timeout_inflate32(vti, timeout, max_timeout);
}

uint64_t
ml_get_timebase_entropy(void)
{
	return __builtin_ia32_rdtsc();
}

/*
 *	Routine:        ml_init_lock_timeout
 *	Function:
 */
static void __startup_func
ml_init_lock_timeout(void)
{
	uint64_t        abstime;
	uint32_t        mtxspin;
#if DEVELOPMENT || DEBUG
	uint64_t        default_timeout_ns = NSEC_PER_SEC >> 2;
#else
	uint64_t        default_timeout_ns = NSEC_PER_SEC >> 1;
#endif
	uint32_t        slto;
	uint32_t        prt;

	if (PE_parse_boot_argn("slto_us", &slto, sizeof(slto))) {
		default_timeout_ns = slto * NSEC_PER_USEC;
	}

	/*
	 * LockTimeOut is absolutetime, LockTimeOutTSC is in TSC ticks,
	 * and LockTimeOutUsec is in microseconds and it's 32-bits.
	 */
	LockTimeOutUsec = (uint32_t) (default_timeout_ns / NSEC_PER_USEC);
	nanoseconds_to_absolutetime(default_timeout_ns, &abstime);
	LockTimeOut = abstime;
	LockTimeOutTSC = tmrCvt(abstime, tscFCvtn2t);

	/*
	 * TLBTimeOut dictates the TLB flush timeout period. It defaults to
	 * LockTimeOut but can be overriden separately. In particular, a
	 * zero value inhibits the timeout-panic and cuts a trace evnt instead
	 * - see pmap_flush_tlbs().
	 */
	if (PE_parse_boot_argn("tlbto_us", &slto, sizeof(slto))) {
		default_timeout_ns = slto * NSEC_PER_USEC;
		nanoseconds_to_absolutetime(default_timeout_ns, &abstime);
		TLBTimeOut = (uint32_t) abstime;
	} else {
		TLBTimeOut = LockTimeOut;
	}

#if DEVELOPMENT || DEBUG
	report_phy_read_delay = LockTimeOut >> 1;
#endif
	if (PE_parse_boot_argn("phyreadmaxus", &slto, sizeof(slto))) {
		default_timeout_ns = slto * NSEC_PER_USEC;
		nanoseconds_to_absolutetime(default_timeout_ns, &abstime);
		report_phy_read_delay = abstime;
	}

	if (PE_parse_boot_argn("phywritemaxus", &slto, sizeof(slto))) {
		nanoseconds_to_absolutetime((uint64_t)slto * NSEC_PER_USEC, &abstime);
		report_phy_write_delay = abstime;
	}

	if (PE_parse_boot_argn("tracephyreadus", &slto, sizeof(slto))) {
		nanoseconds_to_absolutetime((uint64_t)slto * NSEC_PER_USEC, &abstime);
		trace_phy_read_delay = abstime;
	}

	if (PE_parse_boot_argn("tracephywriteus", &slto, sizeof(slto))) {
		nanoseconds_to_absolutetime((uint64_t)slto * NSEC_PER_USEC, &abstime);
		trace_phy_write_delay = abstime;
	}

	if (PE_parse_boot_argn("mtxspin", &mtxspin, sizeof(mtxspin))) {
		if (mtxspin > USEC_PER_SEC >> 4) {
			mtxspin =  USEC_PER_SEC >> 4;
		}
		nanoseconds_to_absolutetime(mtxspin * NSEC_PER_USEC, &abstime);
	} else {
		nanoseconds_to_absolutetime(10 * NSEC_PER_USEC, &abstime);
	}
	MutexSpin = (unsigned int)abstime;
	low_MutexSpin = MutexSpin;
	/*
	 * high_MutexSpin should be initialized as low_MutexSpin * real_ncpus, but
	 * real_ncpus is not set at this time
	 */
	high_MutexSpin = -1;

	nanoseconds_to_absolutetime(4ULL * NSEC_PER_SEC, &LastDebuggerEntryAllowance);
	if (PE_parse_boot_argn("panic_restart_timeout", &prt, sizeof(prt))) {
		nanoseconds_to_absolutetime(prt * NSEC_PER_SEC, &panic_restart_timeout);
	}

	virtualized = ((cpuid_features() & CPUID_FEATURE_VMM) != 0);
	if (virtualized) {
		unsigned int vti;

		if (!PE_parse_boot_argn("vti", &vti, sizeof(vti))) {
			vti = 6;
		}

#define VIRTUAL_TIMEOUT_INFLATE_ABS(_timeout)              \
MACRO_BEGIN                                                \
	_timeout = virtual_timeout_inflate_abs(vti, _timeout); \
MACRO_END

#define VIRTUAL_TIMEOUT_INFLATE_TSC(_timeout)              \
MACRO_BEGIN                                                \
	_timeout = virtual_timeout_inflate_tsc(vti, _timeout); \
MACRO_END
#define VIRTUAL_TIMEOUT_INFLATE_US(_timeout)               \
MACRO_BEGIN                                                \
	_timeout = virtual_timeout_inflate_us(vti, _timeout);  \
MACRO_END
		/*
		 * These timeout values are inflated because they cause
		 * the kernel to panic when they expire.
		 * (Needed when running as a guest VM as the host OS
		 * may not always schedule vcpu threads in time to
		 * meet the deadline implied by the narrower time
		 * window used on hardware.)
		 */
		VIRTUAL_TIMEOUT_INFLATE_US(LockTimeOutUsec);
		VIRTUAL_TIMEOUT_INFLATE_ABS(LockTimeOut);
		VIRTUAL_TIMEOUT_INFLATE_TSC(LockTimeOutTSC);
		VIRTUAL_TIMEOUT_INFLATE_ABS(TLBTimeOut);
		VIRTUAL_TIMEOUT_INFLATE_ABS(report_phy_read_delay);
		VIRTUAL_TIMEOUT_INFLATE_TSC(lock_panic_timeout);
	}

	interrupt_latency_tracker_setup();
}
STARTUP(TIMEOUTS, STARTUP_RANK_MIDDLE, ml_init_lock_timeout);

/*
 * Threshold above which we should attempt to block
 * instead of spinning for clock_delay_until().
 */

void
ml_init_delay_spin_threshold(int threshold_us)
{
	nanoseconds_to_absolutetime(threshold_us * NSEC_PER_USEC, &delay_spin_threshold);
}

boolean_t
ml_delay_should_spin(uint64_t interval)
{
	return (interval < delay_spin_threshold) ? TRUE : FALSE;
}

TUNABLE(uint32_t, yield_delay_us, "yield_delay_us", 0);

void
ml_delay_on_yield(void)
{
#if DEVELOPMENT || DEBUG
	if (yield_delay_us) {
		delay(yield_delay_us);
	}
#endif
}

/*
 * This is called from the machine-independent layer
 * to perform machine-dependent info updates. Defer to cpu_thread_init().
 */
void
ml_cpu_up(void)
{
	return;
}

void
ml_cpu_up_update_counts(__unused int cpu_id)
{
	return;
}

/*
 * This is called from the machine-independent layer
 * to perform machine-dependent info updates.
 */
void
ml_cpu_down(void)
{
	i386_deactivate_cpu();

	return;
}

void
ml_cpu_down_update_counts(__unused int cpu_id)
{
	return;
}

thread_t
current_thread(void)
{
	return current_thread_fast();
}


boolean_t
ml_is64bit(void)
{
	return cpu_mode_is64bit();
}


boolean_t
ml_thread_is64bit(thread_t thread)
{
	return thread_is_64bit_addr(thread);
}


boolean_t
ml_state_is64bit(void *saved_state)
{
	return is_saved_state64(saved_state);
}

void
ml_cpu_set_ldt(int selector)
{
	/*
	 * Avoid loading the LDT
	 * if we're setting the KERNEL LDT and it's already set.
	 */
	if (selector == KERNEL_LDT &&
	    current_cpu_datap()->cpu_ldt == KERNEL_LDT) {
		return;
	}

	lldt(selector);
	current_cpu_datap()->cpu_ldt = selector;
}

void
ml_fp_setvalid(boolean_t value)
{
	fp_setvalid(value);
}

uint64_t
ml_cpu_int_event_time(void)
{
	return current_cpu_datap()->cpu_int_event_time;
}

vm_offset_t
ml_stack_remaining(void)
{
	uintptr_t local = (uintptr_t) &local;

	if (ml_at_interrupt_context() != 0) {
		return local - (current_cpu_datap()->cpu_int_stack_top - INTSTACK_SIZE);
	} else {
		return local - current_thread()->kernel_stack;
	}
}

#if KASAN
vm_offset_t ml_stack_base(void);
vm_size_t ml_stack_size(void);

vm_offset_t
ml_stack_base(void)
{
	if (ml_at_interrupt_context()) {
		return current_cpu_datap()->cpu_int_stack_top - INTSTACK_SIZE;
	} else {
		return current_thread()->kernel_stack;
	}
}

vm_size_t
ml_stack_size(void)
{
	if (ml_at_interrupt_context()) {
		return INTSTACK_SIZE;
	} else {
		return kernel_stack_size;
	}
}
#endif

#if CONFIG_KCOV
kcov_cpu_data_t *
current_kcov_data(void)
{
	return &current_cpu_datap()->cpu_kcov_data;
}

kcov_cpu_data_t *
cpu_kcov_data(int cpuid)
{
	return &cpu_datap(cpuid)->cpu_kcov_data;
}
#endif /* CONFIG_KCOV */

void
kernel_preempt_check(void)
{
	boolean_t       intr;
	unsigned long flags;

	assert(get_preemption_level() == 0);

	if (__improbable(*ast_pending() & AST_URGENT)) {
		/*
		 * can handle interrupts and preemptions
		 * at this point
		 */
		__asm__ volatile ("pushf; pop	%0"  :  "=r" (flags));

		intr = ((flags & EFL_IF) != 0);

		/*
		 * now cause the PRE-EMPTION trap
		 */
		if (intr == TRUE) {
			__asm__ volatile ("int %0" :: "N" (T_PREEMPT));
		}
	}
}

boolean_t
machine_timeout_suspended(void)
{
	return pmap_tlb_flush_timeout || lck_spinlock_timeout_in_progress ||
	       panic_active() || mp_recent_debugger_activity() ||
	       ml_recent_wake();
}

/* Eagerly evaluate all pending timer and thread callouts
 */
void
ml_timer_evaluate(void)
{
	KERNEL_DEBUG_CONSTANT(DECR_TIMER_RESCAN | DBG_FUNC_START, 0, 0, 0, 0, 0);

	uint64_t te_end, te_start = mach_absolute_time();
	simple_lock(&ml_timer_evaluation_slock, LCK_GRP_NULL);
	ml_timer_evaluation_in_progress = TRUE;
	thread_call_delayed_timer_rescan_all();
	mp_cpus_call(CPUMASK_ALL, ASYNC, timer_queue_expire_rescan, NULL);
	ml_timer_evaluation_in_progress = FALSE;
	ml_timer_eager_evaluations++;
	te_end = mach_absolute_time();
	ml_timer_eager_evaluation_max = MAX(ml_timer_eager_evaluation_max, (te_end - te_start));
	simple_unlock(&ml_timer_evaluation_slock);

	KERNEL_DEBUG_CONSTANT(DECR_TIMER_RESCAN | DBG_FUNC_END, 0, 0, 0, 0, 0);
}

boolean_t
ml_timer_forced_evaluation(void)
{
	return ml_timer_evaluation_in_progress;
}

void
ml_gpu_stat_update(uint64_t gpu_ns_delta)
{
	current_thread()->machine.thread_gpu_ns += gpu_ns_delta;
}

uint64_t
ml_gpu_stat(thread_t t)
{
	return t->machine.thread_gpu_ns;
}

int plctrace_enabled = 0;

void
_disable_preemption(void)
{
	disable_preemption_internal();
}

void
_enable_preemption(void)
{
	enable_preemption_internal();
}

void
plctrace_disable(void)
{
	plctrace_enabled = 0;
}

static boolean_t ml_quiescing;

void
ml_set_is_quiescing(boolean_t quiescing)
{
	ml_quiescing = quiescing;
}

boolean_t
ml_is_quiescing(void)
{
	return ml_quiescing;
}

uint64_t
ml_get_booter_memory_size(void)
{
	return 0;
}

void
machine_lockdown(void)
{
	x86_64_protect_data_const();
}

void
ml_cpu_begin_state_transition(__unused int cpu_id)
{
}

void
ml_cpu_end_state_transition(__unused int cpu_id)
{
}

void
ml_cpu_begin_loop(void)
{
}

void
ml_cpu_end_loop(void)
{
}

size_t
ml_get_vm_reserved_regions(bool vm_is64bit, const struct vm_reserved_region **regions)
{
#pragma unused(vm_is64bit)
	assert(regions != NULL);

	*regions = NULL;
	return 0;
}

void
ml_cpu_power_enable(__unused int cpu_id)
{
}

void
ml_cpu_power_disable(__unused int cpu_id)
{
}

int
ml_page_protection_type(void)
{
	return 0; // not supported on x86
}

bool
ml_addr_in_non_xnu_stack(__unused uintptr_t addr)
{
	/* There are no non-XNU stacks on x86 systems. */
	return false;
}

/**
 * Explicitly preallocates a floating point save area.
 */
void
ml_fp_save_area_prealloc(void)
{
	fpnoextflt();
}

void
ml_task_post_signature_processing_hook(__unused task_t task)
{
}
