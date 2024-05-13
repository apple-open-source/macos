/*
 * Copyright (c) 2007-2017 Apple Inc. All rights reserved.
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

#include <arm64/machine_machdep.h>
#include <arm64/proc_reg.h>
#include <arm/machine_cpu.h>
#include <arm/cpu_internal.h>
#include <arm/cpuid.h>
#include <arm/cpu_data.h>
#include <arm/cpu_data_internal.h>
#include <arm/caches_internal.h>
#include <arm/misc_protos.h>
#include <arm/machdep_call.h>
#include <arm/machine_routines.h>
#include <arm/rtclock.h>
#include <arm/cpuid_internal.h>
#include <arm/cpu_capabilities.h>
#include <console/serial_protos.h>
#include <kern/machine.h>
#include <kern/misc_protos.h>
#include <prng/random.h>
#include <kern/startup.h>
#include <kern/thread.h>
#include <kern/timer_queue.h>
#include <mach/machine.h>
#include <machine/atomic.h>
#include <machine/config.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>
#include <vm/vm_shared_region.h>
#include <vm/vm_map.h>
#include <sys/codesign.h>
#include <sys/kdebug.h>
#include <kern/coalition.h>
#include <pexpert/device_tree.h>

#include <IOKit/IOPlatformExpert.h>
#if HIBERNATION
#include <IOKit/IOHibernatePrivate.h>
#endif /* HIBERNATION */

#if defined(KERNEL_INTEGRITY_KTRR) || defined(KERNEL_INTEGRITY_CTRR)
#include <arm64/amcc_rorgn.h>
#endif


#if CONFIG_SPTM
#include <arm64/sptm/sptm.h>
#endif /* CONFIG_SPTM */

#include <libkern/section_keywords.h>

/**
 * On supported hardware, debuggable builds make the HID bits read-only
 * without locking them.  This lets people manually modify HID bits while
 * debugging, since they can use a debugging tool to first reset the HID
 * bits back to read/write.  However it will still catch xnu changes that
 * accidentally write to HID bits after they've been made read-only.
 */
SECURITY_READ_ONLY_LATE(bool) skip_spr_lockdown_glb = 0;

/*
 * On some SoCs, PIO lockdown is applied in assembly in early boot by
 * secondary CPUs.
 * Since the cluster_pio_ro_ctl value is dynamic, it is stored here by the
 * primary CPU so that it doesn't have to be computed each time by the
 * startup code.
 */
SECURITY_READ_ONLY_LATE(uint64_t) cluster_pio_ro_ctl_mask_glb = 0;

#if CONFIG_CPU_COUNTERS
#include <kern/kpc.h>
#endif /* CONFIG_CPU_COUNTERS */

#define MPIDR_CPU_ID(mpidr_el1_val)             (((mpidr_el1_val) & MPIDR_AFF0_MASK) >> MPIDR_AFF0_SHIFT)
#define MPIDR_CLUSTER_ID(mpidr_el1_val)         (((mpidr_el1_val) & MPIDR_AFF1_MASK) >> MPIDR_AFF1_SHIFT)

#if HAS_CLUSTER
static uint8_t cluster_initialized = 0;
#endif

MACHINE_TIMEOUT_DEV_WRITEABLE(LockTimeOut, "lock", 6e6 /* 0.25s */, MACHINE_TIMEOUT_UNIT_TIMEBASE, NULL);
machine_timeout_t LockTimeOutUsec; // computed in ml_init_lock_timeout

MACHINE_TIMEOUT_DEV_WRITEABLE(TLockTimeOut, "ticket-lock", 3e6 /* 0.125s */, MACHINE_TIMEOUT_UNIT_TIMEBASE, NULL);

MACHINE_TIMEOUT_DEV_WRITEABLE(MutexSpin, "mutex-spin", 240 /* 10us */, MACHINE_TIMEOUT_UNIT_TIMEBASE, NULL);

uint64_t low_MutexSpin;
int64_t high_MutexSpin;



static uint64_t ml_wfe_hint_max_interval;
#define MAX_WFE_HINT_INTERVAL_US (500ULL)

/* Must be less than cpu_idle_latency to ensure ml_delay_should_spin is true */
TUNABLE(uint32_t, yield_delay_us, "yield_delay_us", 0);

extern vm_offset_t   segLOWEST;
extern vm_offset_t   segLOWESTTEXT;
extern vm_offset_t   segLASTB;
extern unsigned long segSizeLAST;

/* ARM64 specific bounds; used to test for presence in the kernelcache. */
extern vm_offset_t   vm_kernelcache_base;
extern vm_offset_t   vm_kernelcache_top;

/* Location of the physmap / physical aperture */
extern uint64_t physmap_base;

#if defined(CONFIG_SPTM)
extern const arm_physrange_t *arm_vm_kernelcache_ranges;
extern int arm_vm_kernelcache_numranges;
#else /* defined(CONFIG_SPTM) */
extern vm_offset_t arm_vm_kernelcache_phys_start;
extern vm_offset_t arm_vm_kernelcache_phys_end;
#endif /* defined(CONFIG_SPTM) */

#if defined(HAS_IPI)
unsigned int gFastIPI = 1;
#define kDeferredIPITimerDefault (64 * NSEC_PER_USEC) /* in nanoseconds */
static TUNABLE_WRITEABLE(uint64_t, deferred_ipi_timer_ns, "fastipitimeout",
    kDeferredIPITimerDefault);
#endif /* defined(HAS_IPI) */

thread_t Idle_context(void);

SECURITY_READ_ONLY_LATE(bool) cpu_config_correct = true;

SECURITY_READ_ONLY_LATE(static ml_topology_cpu_t) topology_cpu_array[MAX_CPUS];
SECURITY_READ_ONLY_LATE(static ml_topology_cluster_t) topology_cluster_array[MAX_CPU_CLUSTERS];
SECURITY_READ_ONLY_LATE(static ml_topology_info_t) topology_info = {
	.version = CPU_TOPOLOGY_VERSION,
	.cpus = topology_cpu_array,
	.clusters = topology_cluster_array,
};

_Atomic unsigned int cluster_type_num_active_cpus[MAX_CPU_TYPES];

/**
 * Represents the offset of each cluster within a hypothetical array of MAX_CPUS
 * entries of an arbitrary data type.  This is intended for use by specialized consumers
 * that must quickly access per-CPU data using only the physical CPU ID (MPIDR_EL1),
 * as follows:
 *	hypothetical_array[cluster_offsets[AFF1] + AFF0]
 * Most consumers should instead use general-purpose facilities such as PERCPU or
 * ml_get_cpu_number().
 */
SECURITY_READ_ONLY_LATE(int64_t) cluster_offsets[MAX_CPU_CLUSTER_PHY_ID + 1];

SECURITY_READ_ONLY_LATE(static uint32_t) arm64_eventi = UINT32_MAX;

extern uint32_t lockdown_done;

/**
 * Represents regions of virtual address space that should be reserved
 * (pre-mapped) in each user address space.
 */
static const struct vm_reserved_region vm_reserved_regions[] = {
	{
		.vmrr_name = "GPU Carveout",
		.vmrr_addr = MACH_VM_MIN_GPU_CARVEOUT_ADDRESS,
		.vmrr_size = (vm_map_size_t)(MACH_VM_MAX_GPU_CARVEOUT_ADDRESS - MACH_VM_MIN_GPU_CARVEOUT_ADDRESS)
	},
	/*
	 * Reserve the virtual memory space representing the commpage nesting region
	 * to prevent user processes from allocating memory within it. The actual
	 * page table entries for the commpage are inserted by vm_commpage_enter().
	 * This vm_map_enter() just prevents userspace from allocating/deallocating
	 * anything within the entire commpage nested region.
	 */
	{
		.vmrr_name = "commpage nesting",
		.vmrr_addr = _COMM_PAGE64_NESTING_START,
		.vmrr_size = _COMM_PAGE64_NESTING_SIZE
	}
};

uint32_t get_arm_cpu_version(void);

#if defined(HAS_IPI)
static inline void
ml_cpu_signal_type(unsigned int cpu_mpidr, uint32_t type)
{
#if HAS_CLUSTER
	uint64_t local_mpidr;
	/* NOTE: this logic expects that we are called in a non-preemptible
	 * context, or at least one in which the calling thread is bound
	 * to a single CPU.  Otherwise we may migrate between choosing which
	 * IPI mechanism to use and issuing the IPI. */
	MRS(local_mpidr, "MPIDR_EL1");
	if (MPIDR_CLUSTER_ID(local_mpidr) == MPIDR_CLUSTER_ID(cpu_mpidr)) {
		uint64_t x = type | MPIDR_CPU_ID(cpu_mpidr);
		MSR("S3_5_C15_C0_0", x);
	} else {
		#define IPI_RR_TARGET_CLUSTER_SHIFT 16
		uint64_t x = type | (MPIDR_CLUSTER_ID(cpu_mpidr) << IPI_RR_TARGET_CLUSTER_SHIFT) | MPIDR_CPU_ID(cpu_mpidr);
		MSR("S3_5_C15_C0_1", x);
	}
#else
	uint64_t x = type | MPIDR_CPU_ID(cpu_mpidr);
	MSR("S3_5_C15_C0_1", x);
#endif
	/* The recommended local/global IPI sequence is:
	 *   DSB <sys> (This ensures visibility of e.g. older stores to the
	 *     pending CPU signals bit vector in DRAM prior to IPI reception,
	 *     and is present in cpu_signal_internal())
	 *   MSR S3_5_C15_C0_1, Xt
	 *   ISB
	 */
	__builtin_arm_isb(ISB_SY);
}
#endif

#if !defined(HAS_IPI)
__dead2
#endif
void
ml_cpu_signal(unsigned int cpu_mpidr __unused)
{
#if defined(HAS_IPI)
	ml_cpu_signal_type(cpu_mpidr, ARM64_REG_IPI_RR_TYPE_IMMEDIATE);
#else
	panic("Platform does not support ACC Fast IPI");
#endif
}

#if !defined(HAS_IPI)
__dead2
#endif
void
ml_cpu_signal_deferred_adjust_timer(uint64_t nanosecs)
{
#if defined(HAS_IPI)
	/* adjust IPI_CR timer countdown value for deferred IPI
	 * accepts input in nanosecs, convert to absolutetime (REFCLK ticks),
	 * clamp maximum REFCLK ticks to 0xFFFF (16 bit field)
	 *
	 * global register, should only require a single write to update all
	 * CPU cores: from Skye ACC user spec section 5.7.3.3
	 *
	 * IPICR is a global register but there are two copies in ACC: one at pBLK and one at eBLK.
	 * IPICR write SPR token also traverses both pCPM and eCPM rings and updates both copies.
	 */
	uint64_t abstime;

	nanoseconds_to_absolutetime(nanosecs, &abstime);

	abstime = MIN(abstime, 0xFFFF);

	/* update deferred_ipi_timer_ns with the new clamped value */
	absolutetime_to_nanoseconds(abstime, &deferred_ipi_timer_ns);

	MSR("S3_5_C15_C3_1", abstime);
#else
	(void)nanosecs;
	panic("Platform does not support ACC Fast IPI");
#endif
}

uint64_t
ml_cpu_signal_deferred_get_timer()
{
#if defined(HAS_IPI)
	return deferred_ipi_timer_ns;
#else
	return 0;
#endif
}

#if !defined(HAS_IPI)
__dead2
#endif
void
ml_cpu_signal_deferred(unsigned int cpu_mpidr __unused)
{
#if defined(HAS_IPI)
	ml_cpu_signal_type(cpu_mpidr, ARM64_REG_IPI_RR_TYPE_DEFERRED);
#else
	panic("Platform does not support ACC Fast IPI deferral");
#endif
}

#if !defined(HAS_IPI)
__dead2
#endif
void
ml_cpu_signal_retract(unsigned int cpu_mpidr __unused)
{
#if defined(HAS_IPI)
	ml_cpu_signal_type(cpu_mpidr, ARM64_REG_IPI_RR_TYPE_RETRACT);
#else
	panic("Platform does not support ACC Fast IPI retraction");
#endif
}

extern uint32_t idle_proximate_io_wfe_unmasked;

#define CPUPM_IDLE_WFE 0x5310300
static bool
wfe_process_recommendation(void)
{
	bool ipending = false;
	if (__probable(idle_proximate_io_wfe_unmasked == 1)) {
		/* Check for an active perf. controller generated
		 * WFE recommendation for this cluster.
		 */
		cpu_data_t *cdp = getCpuDatap();
		uint32_t cid = cdp->cpu_cluster_id;
		uint64_t wfe_ttd = 0;
		uint64_t wfe_deadline = 0;

		if ((wfe_ttd = ml_cluster_wfe_timeout(cid)) != 0) {
			wfe_deadline = mach_absolute_time() + wfe_ttd;
		}

		if (wfe_deadline != 0) {
			/* Poll issuing event-bounded WFEs until an interrupt
			 * arrives or the WFE recommendation expires
			 */
#if DEVELOPMENT || DEBUG
			uint64_t wc = cdp->wfe_count;
			KDBG(CPUPM_IDLE_WFE | DBG_FUNC_START, ipending, wc, wfe_ttd, cdp->cpu_stat.irq_ex_cnt_wake);
#endif
			/* Issue WFE until the recommendation expires,
			 * with IRQs unmasked.
			 */
			ipending = wfe_to_deadline_or_interrupt(cid, wfe_deadline, cdp, true, true);
#if DEVELOPMENT || DEBUG
			KDBG(CPUPM_IDLE_WFE | DBG_FUNC_END, ipending, cdp->wfe_count - wc, wfe_deadline, cdp->cpu_stat.irq_ex_cnt_wake);
#endif
		}
	}
	return ipending;
}

void
machine_idle(void)
{
	/* Interrupts are expected to be masked on entry or re-entry via
	 * Idle_load_context()
	 */
	assert((__builtin_arm_rsr("DAIF") & (DAIF_IRQF | DAIF_FIQF)) == (DAIF_IRQF | DAIF_FIQF));
	/* Check for, and act on, a WFE recommendation.
	 * Bypasses context spill/fill for a minor perf. increment.
	 * May unmask and restore IRQ+FIQ mask.
	 */
	if (wfe_process_recommendation() == false) {
		/* If WFE recommendation absent, or WFE deadline
		 * arrived with no interrupt pending/processed,
		 * fall back to WFI.
		 */
		Idle_context();
	}
	__builtin_arm_wsr("DAIFClr", (DAIFSC_IRQF | DAIFSC_FIQF));
}

void
OSSynchronizeIO(void)
{
	__builtin_arm_dsb(DSB_SY);
}

uint64_t
get_aux_control(void)
{
	uint64_t        value;

	MRS(value, "ACTLR_EL1");
	return value;
}

uint64_t
get_mmu_control(void)
{
	uint64_t        value;

	MRS(value, "SCTLR_EL1");
	return value;
}

uint64_t
get_tcr(void)
{
	uint64_t        value;

	MRS(value, "TCR_EL1");
	return value;
}

boolean_t
ml_get_interrupts_enabled(void)
{
	uint64_t        value;

	MRS(value, "DAIF");
	if (value & DAIF_IRQF) {
		return FALSE;
	}
	return TRUE;
}

pmap_paddr_t
get_mmu_ttb(void)
{
	pmap_paddr_t    value;

	MRS(value, "TTBR0_EL1");
	return value;
}

uint32_t
get_arm_cpu_version(void)
{
	uint32_t value = machine_read_midr();

	/* Compose the register values into 8 bits; variant[7:4], revision[3:0]. */
	return ((value & MIDR_EL1_REV_MASK) >> MIDR_EL1_REV_SHIFT) | ((value & MIDR_EL1_VAR_MASK) >> (MIDR_EL1_VAR_SHIFT - 4));
}

bool
ml_feature_supported(uint32_t feature_bit)
{
	uint64_t aidr_el1_value = 0;

	MRS(aidr_el1_value, "AIDR_EL1");

#ifdef APPLEAVALANCHE
#endif // APPLEAVALANCHE

	return aidr_el1_value & feature_bit;
}

/*
 * user_cont_hwclock_allowed()
 *
 * Indicates whether we allow EL0 to read the virtual timebase (CNTVCT_EL0)
 * as a continuous time source (e.g. from mach_continuous_time)
 */
boolean_t
user_cont_hwclock_allowed(void)
{
#if HAS_CONTINUOUS_HWCLOCK
	return TRUE;
#else
	return FALSE;
#endif
}

/*
 * user_timebase_type()
 *
 * Indicates type of EL0 virtual timebase read (CNTVCT_EL0).
 *
 * USER_TIMEBASE_NONE: EL0 has no access to timebase register
 * USER_TIMEBASE_SPEC: EL0 has access to speculative timebase reads (CNTVCT_EL0)
 * USER_TIMEBASE_NOSPEC: EL0 has access to non speculative timebase reads (CNTVCTSS_EL0)
 *
 */

uint8_t
user_timebase_type(void)
{
#if HAS_ACNTVCT
	return USER_TIMEBASE_NOSPEC_APPLE;
#elif __ARM_ARCH_8_6__
	return USER_TIMEBASE_NOSPEC;
#else
	return USER_TIMEBASE_SPEC;
#endif
}

void
machine_startup(__unused boot_args * args)
{
#if defined(HAS_IPI) && (DEVELOPMENT || DEBUG)
	if (!PE_parse_boot_argn("fastipi", &gFastIPI, sizeof(gFastIPI))) {
		gFastIPI = 1;
	}
#endif /* defined(HAS_IPI) && (DEVELOPMENT || DEBUG)*/


	machine_conf();


	/*
	 * Kick off the kernel bootstrap.
	 */
	kernel_bootstrap();
	/* NOTREACHED */
}

typedef void (*invalidate_fn_t)(void);

static SECURITY_READ_ONLY_LATE(invalidate_fn_t) invalidate_hmac_function = NULL;

void set_invalidate_hmac_function(invalidate_fn_t fn);

void
set_invalidate_hmac_function(invalidate_fn_t fn)
{
	if (NULL != invalidate_hmac_function) {
		panic("Invalidate HMAC function already set");
	}

	invalidate_hmac_function = fn;
}

void
machine_lockdown(void)
{

#if CONFIG_SPTM
	/**
	 * On devices that make use of the SPTM, the SPTM is responsible for
	 * managing system register locks. Due to this, we skip the call to
	 * spr_lockdown() below.
	 */
#else
#endif

	arm_vm_prot_finalize(PE_state.bootArgs);

#if CONFIG_KERNEL_INTEGRITY
#if KERNEL_INTEGRITY_WT
	/* Watchtower
	 *
	 * Notify the monitor about the completion of early kernel bootstrap.
	 * From this point forward it will enforce the integrity of kernel text,
	 * rodata and page tables.
	 */

#ifdef MONITOR
	monitor_call(MONITOR_LOCKDOWN, 0, 0, 0);
#endif
#endif /* KERNEL_INTEGRITY_WT */

#if CONFIG_SPTM
	extern void pmap_prepare_commpages(void);
	pmap_prepare_commpages();

	/**
	 * sptm_lockdown_xnu() disables preemption like all SPTM calls, but may take
	 * a fair amount of time as it involves retyping a large number of pages.
	 * This preemption latency is not really a concern since we're still fairly
	 * early in the boot process, so just explicitly disable preemption before
	 * invoking the SPTM and abandon preemption latency measurements before
	 * re-enabling it.
	 */
	disable_preemption();
	/* Signal the SPTM that XNU is ready for RO memory to actually become read-only */
	sptm_lockdown_xnu();
#if SCHED_HYGIENE_DEBUG
	abandon_preemption_disable_measurement();
#endif /* SCHED_HYGIENE_DEBUG */
	enable_preemption();
#else
#if defined(KERNEL_INTEGRITY_KTRR) || defined(KERNEL_INTEGRITY_CTRR)
	/* KTRR
	 *
	 * Lock physical KTRR region. KTRR region is read-only. Memory outside
	 * the region is not executable at EL1.
	 */

	rorgn_lockdown();
#endif /* defined(KERNEL_INTEGRITY_KTRR) || defined(KERNEL_INTEGRITY_CTRR) */
#endif /* CONFIG_SPTM */

#if XNU_MONITOR
	pmap_lockdown_ppl();
#endif

#endif /* CONFIG_KERNEL_INTEGRITY */


	if (NULL != invalidate_hmac_function) {
		invalidate_hmac_function();
	}

	lockdown_done = 1;
}


char           *
machine_boot_info(
	__unused char *buf,
	__unused vm_size_t size)
{
	return PE_boot_args();
}

void
slave_machine_init(__unused void *param)
{
	cpu_machine_init();     /* Initialize the processor */
	clock_init();           /* Init the clock */
}

/*
 *	Routine:        machine_processor_shutdown
 *	Function:
 */
thread_t
machine_processor_shutdown(
	__unused thread_t thread,
	void (*doshutdown)(processor_t),
	processor_t processor)
{
	return Shutdown_context(doshutdown, processor);
}

/*
 *      Routine:        ml_init_lock_timeout
 *      Function:
 */
static void __startup_func
ml_init_lock_timeout(void)
{
	/*
	 * This function is called after STARTUP_SUB_TIMEOUTS
	 * initialization, so using the "legacy" boot-args here overrides
	 * the ml-timeout-...  configuration. (Given that these boot-args
	 * here are usually explicitly specified, this makes sense by
	 * overriding ml-timeout-..., which may come from the device tree.
	 */

	uint64_t lto_timeout_ns;
	uint64_t lto_abstime;
	uint32_t slto;

	if (PE_parse_boot_argn("slto_us", &slto, sizeof(slto))) {
		lto_timeout_ns = slto * NSEC_PER_USEC;
		nanoseconds_to_absolutetime(lto_timeout_ns, &lto_abstime);
		os_atomic_store(&LockTimeOut, lto_abstime, relaxed);
	} else {
		lto_abstime = os_atomic_load(&LockTimeOut, relaxed);
		absolutetime_to_nanoseconds(lto_abstime, &lto_timeout_ns);
	}

	os_atomic_store(&LockTimeOutUsec, lto_timeout_ns / NSEC_PER_USEC, relaxed);

	if (PE_parse_boot_argn("tlto_us", &slto, sizeof(slto))) {
		nanoseconds_to_absolutetime(slto * NSEC_PER_USEC, &lto_abstime);
		os_atomic_store(&TLockTimeOut, lto_abstime, relaxed);
	} else if (lto_abstime != 0) {
		os_atomic_store(&TLockTimeOut, lto_abstime >> 1, relaxed);
	} // else take default from MACHINE_TIMEOUT.

	uint64_t mtxspin;
	uint64_t mtx_abstime;
	if (PE_parse_boot_argn("mtxspin", &mtxspin, sizeof(mtxspin))) {
		if (mtxspin > USEC_PER_SEC >> 4) {
			mtxspin =  USEC_PER_SEC >> 4;
		}
		nanoseconds_to_absolutetime(mtxspin * NSEC_PER_USEC, &mtx_abstime);
		os_atomic_store(&MutexSpin, mtx_abstime, relaxed);
	} else {
		mtx_abstime = os_atomic_load(&MutexSpin, relaxed);
	}

	low_MutexSpin = os_atomic_load(&MutexSpin, relaxed);
	/*
	 * high_MutexSpin should be initialized as low_MutexSpin * real_ncpus, but
	 * real_ncpus is not set at this time
	 *
	 * NOTE: active spinning is disabled in arm. It can be activated
	 * by setting high_MutexSpin through the sysctl.
	 */
	high_MutexSpin = low_MutexSpin;

	uint64_t maxwfeus = MAX_WFE_HINT_INTERVAL_US;
	PE_parse_boot_argn("max_wfe_us", &maxwfeus, sizeof(maxwfeus));
	nanoseconds_to_absolutetime(maxwfeus * NSEC_PER_USEC, &ml_wfe_hint_max_interval);
}
STARTUP(TIMEOUTS, STARTUP_RANK_MIDDLE, ml_init_lock_timeout);


/*
 * This is called when all of the ml_processor_info_t structures have been
 * initialized and all the processors have been started through processor_start().
 *
 * Required by the scheduler subsystem.
 */
void
ml_cpu_init_completed(void)
{
	if (SCHED(cpu_init_completed) != NULL) {
		SCHED(cpu_init_completed)();
	}
}

/*
 * These are called from the machine-independent routine cpu_up()
 * to perform machine-dependent info updates.
 *
 * The update to CPU counts needs to be separate from other actions
 * because we don't update the counts when CLPC causes temporary
 * cluster powerdown events, as these must be transparent to the user.
 */
void
ml_cpu_up(void)
{
}

void
ml_cpu_up_update_counts(int cpu_id)
{
	ml_topology_cpu_t *cpu = &ml_get_topology_info()->cpus[cpu_id];

	os_atomic_inc(&cluster_type_num_active_cpus[cpu->cluster_type], relaxed);

	os_atomic_inc(&machine_info.physical_cpu, relaxed);
	os_atomic_inc(&machine_info.logical_cpu, relaxed);
}

/*
 * These are called from the machine-independent routine cpu_down()
 * to perform machine-dependent info updates.
 *
 * The update to CPU counts needs to be separate from other actions
 * because we don't update the counts when CLPC causes temporary
 * cluster powerdown events, as these must be transparent to the user.
 */
void
ml_cpu_down(void)
{
	/*
	 * If we want to deal with outstanding IPIs, we need to
	 * do relatively early in the processor_doshutdown path,
	 * as we pend decrementer interrupts using the IPI
	 * mechanism if we cannot immediately service them (if
	 * IRQ is masked).  Do so now.
	 *
	 * We aren't on the interrupt stack here; would it make
	 * more sense to disable signaling and then enable
	 * interrupts?  It might be a bit cleaner.
	 */
	cpu_data_t *cpu_data_ptr = getCpuDatap();
	cpu_data_ptr->cpu_running = FALSE;

	if (cpu_data_ptr != &BootCpuData) {
		/*
		 * Move all of this cpu's timers to the master/boot cpu,
		 * and poke it in case there's a sooner deadline for it to schedule.
		 */
		timer_queue_shutdown(&cpu_data_ptr->rtclock_timer.queue);
		kern_return_t rv = cpu_xcall(BootCpuData.cpu_number, &timer_queue_expire_local, &ml_cpu_down);
		if (rv != KERN_SUCCESS) {
			panic("ml_cpu_down: IPI failure %d", rv);
		}
	}

	cpu_signal_handler_internal(TRUE);
}
void
ml_cpu_down_update_counts(int cpu_id)
{
	ml_topology_cpu_t *cpu = &ml_get_topology_info()->cpus[cpu_id];

	os_atomic_dec(&cluster_type_num_active_cpus[cpu->cluster_type], relaxed);

	os_atomic_dec(&machine_info.physical_cpu, relaxed);
	os_atomic_dec(&machine_info.logical_cpu, relaxed);
}


unsigned int
ml_get_machine_mem(void)
{
	return machine_info.memory_size;
}

__attribute__((noreturn))
void
halt_all_cpus(boolean_t reboot)
{
	if (reboot) {
		printf("MACH Reboot\n");
		PEHaltRestart(kPERestartCPU);
	} else {
		printf("CPU halted\n");
		PEHaltRestart(kPEHaltCPU);
	}
	while (1) {
		;
	}
}

__attribute__((noreturn))
void
halt_cpu(void)
{
	halt_all_cpus(FALSE);
}

/*
 *	Routine:        machine_signal_idle
 *	Function:
 */
void
machine_signal_idle(
	processor_t processor)
{
	cpu_signal(processor_to_cpu_datap(processor), SIGPnop, (void *)NULL, (void *)NULL);
	KERNEL_DEBUG_CONSTANT_IST(KDEBUG_TRACE, MACHDBG_CODE(DBG_MACH_SCHED, MACH_REMOTE_AST), processor->cpu_id, 0 /* nop */, 0, 0, 0);
}

void
machine_signal_idle_deferred(
	processor_t processor)
{
	cpu_signal_deferred(processor_to_cpu_datap(processor));
	KERNEL_DEBUG_CONSTANT_IST(KDEBUG_TRACE, MACHDBG_CODE(DBG_MACH_SCHED, MACH_REMOTE_DEFERRED_AST), processor->cpu_id, 0 /* nop */, 0, 0, 0);
}

void
machine_signal_idle_cancel(
	processor_t processor)
{
	cpu_signal_cancel(processor_to_cpu_datap(processor));
	KERNEL_DEBUG_CONSTANT_IST(KDEBUG_TRACE, MACHDBG_CODE(DBG_MACH_SCHED, MACH_REMOTE_CANCEL_AST), processor->cpu_id, 0 /* nop */, 0, 0, 0);
}

/*
 *	Routine:        ml_install_interrupt_handler
 *	Function:	Initialize Interrupt Handler
 */
void
ml_install_interrupt_handler(
	void *nub,
	int source,
	void *target,
	IOInterruptHandler handler,
	void *refCon)
{
	cpu_data_t     *cpu_data_ptr;
	boolean_t       current_state;

	current_state = ml_set_interrupts_enabled(FALSE);
	cpu_data_ptr = getCpuDatap();

	cpu_data_ptr->interrupt_nub = nub;
	cpu_data_ptr->interrupt_source = source;
	cpu_data_ptr->interrupt_target = target;
	cpu_data_ptr->interrupt_handler = handler;
	cpu_data_ptr->interrupt_refCon = refCon;

	(void) ml_set_interrupts_enabled(current_state);
}

/*
 *	Routine:        ml_init_interrupt
 *	Function:	Initialize Interrupts
 */
void
ml_init_interrupt(void)
{
#if defined(HAS_IPI)
	/*
	 * ml_init_interrupt will get called once for each CPU, but this is redundant
	 * because there is only one global copy of the register for skye. do it only
	 * on the bootstrap cpu
	 */
	if (getCpuDatap()->cluster_master) {
		ml_cpu_signal_deferred_adjust_timer(deferred_ipi_timer_ns);
	}
#endif
}

/*
 *	Routine:        ml_init_timebase
 *	Function:	register and setup Timebase, Decremeter services
 */
void
ml_init_timebase(
	void            *args,
	tbd_ops_t       tbd_funcs,
	vm_offset_t     int_address,
	vm_offset_t     int_value __unused)
{
	cpu_data_t     *cpu_data_ptr;

	cpu_data_ptr = (cpu_data_t *)args;

	if ((cpu_data_ptr == &BootCpuData)
	    && (rtclock_timebase_func.tbd_fiq_handler == (void *)NULL)) {
		rtclock_timebase_func = *tbd_funcs;
		rtclock_timebase_addr = int_address;
	}
}

#define ML_READPROP_MANDATORY UINT64_MAX

static uint64_t
ml_readprop(const DTEntry entry, const char *propertyName, uint64_t default_value)
{
	void const *prop;
	unsigned int propSize;

	if (SecureDTGetProperty(entry, propertyName, &prop, &propSize) == kSuccess) {
		if (propSize == sizeof(uint8_t)) {
			return *((uint8_t const *)prop);
		} else if (propSize == sizeof(uint16_t)) {
			return *((uint16_t const *)prop);
		} else if (propSize == sizeof(uint32_t)) {
			return *((uint32_t const *)prop);
		} else if (propSize == sizeof(uint64_t)) {
			return *((uint64_t const *)prop);
		} else {
			panic("CPU property '%s' has bad size %u", propertyName, propSize);
		}
	} else {
		if (default_value == ML_READPROP_MANDATORY) {
			panic("Missing mandatory property '%s'", propertyName);
		}
		return default_value;
	}
}

static boolean_t
ml_read_reg_range(const DTEntry entry, const char *propertyName, uint64_t *pa_ptr, uint64_t *len_ptr)
{
	uint64_t const *prop;
	unsigned int propSize;

	if (SecureDTGetProperty(entry, propertyName, (void const **)&prop, &propSize) != kSuccess) {
		return FALSE;
	}

	if (propSize != sizeof(uint64_t) * 2) {
		panic("Wrong property size for %s", propertyName);
	}

	*pa_ptr = prop[0];
	*len_ptr = prop[1];
	return TRUE;
}

static boolean_t
ml_is_boot_cpu(const DTEntry entry)
{
	void const *prop;
	unsigned int propSize;

	if (SecureDTGetProperty(entry, "state", &prop, &propSize) != kSuccess) {
		panic("unable to retrieve state for cpu");
	}

	if (strncmp((char const *)prop, "running", propSize) == 0) {
		return TRUE;
	} else {
		return FALSE;
	}
}

static void
ml_read_chip_revision(unsigned int *rev __unused)
{
	// The CPU_VERSION_* macros are only defined on APPLE_ARM64_ARCH_FAMILY builds
#ifdef APPLE_ARM64_ARCH_FAMILY
	DTEntry         entryP;

	if ((SecureDTFindEntry("name", "arm-io", &entryP) == kSuccess)) {
		*rev = (unsigned int)ml_readprop(entryP, "chip-revision", CPU_VERSION_UNKNOWN);
	} else {
		*rev = CPU_VERSION_UNKNOWN;
	}
#endif
}

void
ml_parse_cpu_topology(void)
{
	DTEntry entry, child __unused;
	OpaqueDTEntryIterator iter;
	uint32_t cpu_boot_arg = MAX_CPUS;
	uint64_t cpumask_boot_arg = ULLONG_MAX;
	int err;

	int64_t cluster_phys_to_logical[MAX_CPU_CLUSTER_PHY_ID + 1];
	int64_t cluster_max_cpu_phys_id[MAX_CPU_CLUSTER_PHY_ID + 1];
	const boolean_t cpus_boot_arg_present = PE_parse_boot_argn("cpus", &cpu_boot_arg, sizeof(cpu_boot_arg));
	const boolean_t cpumask_boot_arg_present = PE_parse_boot_argn("cpumask", &cpumask_boot_arg, sizeof(cpumask_boot_arg));

	// The cpus=N and cpumask=N boot args cannot be used simultaneously. Flag this
	// so that we trigger a panic later in the boot process, once serial is enabled.
	if (cpus_boot_arg_present && cpumask_boot_arg_present) {
		cpu_config_correct = false;
	}

	err = SecureDTLookupEntry(NULL, "/cpus", &entry);
	assert(err == kSuccess);

	err = SecureDTInitEntryIterator(entry, &iter);
	assert(err == kSuccess);

	for (int i = 0; i <= MAX_CPU_CLUSTER_PHY_ID; i++) {
		cluster_offsets[i] = -1;
		cluster_phys_to_logical[i] = -1;
		cluster_max_cpu_phys_id[i] = 0;
	}

	while (kSuccess == SecureDTIterateEntries(&iter, &child)) {
		boolean_t is_boot_cpu = ml_is_boot_cpu(child);
		boolean_t cpu_enabled = cpumask_boot_arg & 1;
		cpumask_boot_arg >>= 1;

		// Boot CPU disabled in cpumask. Flag this so that we trigger a panic
		// later in the boot process, once serial is enabled.
		if (is_boot_cpu && !cpu_enabled) {
			cpu_config_correct = false;
		}

		// Ignore this CPU if it has been disabled by the cpumask= boot-arg.
		if (!is_boot_cpu && !cpu_enabled) {
			continue;
		}

		// If the number of CPUs is constrained by the cpus= boot-arg, and the boot CPU hasn't
		// been added to the topology struct yet, and we only have one slot left, then skip
		// every other non-boot CPU in order to leave room for the boot CPU.
		//
		// e.g. if the boot-args say "cpus=3" and CPU4 is the boot CPU, then the cpus[]
		// array will list CPU0, CPU1, and CPU4.  CPU2-CPU3 and CPU5-CPUn will be omitted.
		if (topology_info.num_cpus >= (cpu_boot_arg - 1) && topology_info.boot_cpu == NULL && !is_boot_cpu) {
			continue;
		}
		if (topology_info.num_cpus >= cpu_boot_arg) {
			break;
		}

		ml_topology_cpu_t *cpu = &topology_info.cpus[topology_info.num_cpus];

		cpu->cpu_id = topology_info.num_cpus++;
		assert(cpu->cpu_id < MAX_CPUS);
		topology_info.max_cpu_id = MAX(topology_info.max_cpu_id, cpu->cpu_id);

		cpu->die_id = 0;
		topology_info.max_die_id = 0;

		cpu->phys_id = (uint32_t)ml_readprop(child, "reg", ML_READPROP_MANDATORY);

		cpu->l2_access_penalty = (uint32_t)ml_readprop(child, "l2-access-penalty", 0);
		cpu->l2_cache_size = (uint32_t)ml_readprop(child, "l2-cache-size", 0);
		cpu->l2_cache_id = (uint32_t)ml_readprop(child, "l2-cache-id", 0);
		cpu->l3_cache_size = (uint32_t)ml_readprop(child, "l3-cache-size", 0);
		cpu->l3_cache_id = (uint32_t)ml_readprop(child, "l3-cache-id", 0);

		ml_read_reg_range(child, "cpu-uttdbg-reg", &cpu->cpu_UTTDBG_pa, &cpu->cpu_UTTDBG_len);
		ml_read_reg_range(child, "cpu-impl-reg", &cpu->cpu_IMPL_pa, &cpu->cpu_IMPL_len);
		ml_read_reg_range(child, "coresight-reg", &cpu->coresight_pa, &cpu->coresight_len);
		cpu->cluster_type = CLUSTER_TYPE_SMP;

		int cluster_type = (int)ml_readprop(child, "cluster-type", 0);
		if (cluster_type == 'E') {
			cpu->cluster_type = CLUSTER_TYPE_E;
		} else if (cluster_type == 'P') {
			cpu->cluster_type = CLUSTER_TYPE_P;
		}

		topology_info.cluster_type_num_cpus[cpu->cluster_type]++;

		/*
		 * Since we want to keep a linear cluster ID space, we cannot just rely
		 * on the value provided by EDT. Instead, use the MPIDR value to see if we have
		 * seen this exact cluster before. If so, then reuse that cluster ID for this CPU.
		 */
#if HAS_CLUSTER
		uint32_t phys_cluster_id = MPIDR_CLUSTER_ID(cpu->phys_id);
#else
		uint32_t phys_cluster_id = (cpu->cluster_type == CLUSTER_TYPE_P);
#endif
		assert(phys_cluster_id <= MAX_CPU_CLUSTER_PHY_ID);
		cpu->cluster_id = ((cluster_phys_to_logical[phys_cluster_id] == -1) ?
		    topology_info.num_clusters : cluster_phys_to_logical[phys_cluster_id]);

		assert(cpu->cluster_id < MAX_CPU_CLUSTERS);

		ml_topology_cluster_t *cluster = &topology_info.clusters[cpu->cluster_id];
		if (cluster->num_cpus == 0) {
			assert(topology_info.num_clusters < MAX_CPU_CLUSTERS);

			topology_info.num_clusters++;
			topology_info.max_cluster_id = MAX(topology_info.max_cluster_id, cpu->cluster_id);
			topology_info.cluster_types |= (1 << cpu->cluster_type);

			cluster->cluster_id = cpu->cluster_id;
			cluster->cluster_type = cpu->cluster_type;
			cluster->first_cpu_id = cpu->cpu_id;
			assert(cluster_phys_to_logical[phys_cluster_id] == -1);
			cluster_phys_to_logical[phys_cluster_id] = cpu->cluster_id;

			topology_info.cluster_type_num_clusters[cluster->cluster_type]++;

			// Since we don't have a per-cluster EDT node, this is repeated in each CPU node.
			// If we wind up with a bunch of these, we might want to create separate per-cluster
			// EDT nodes and have the CPU nodes reference them through a phandle.
			ml_read_reg_range(child, "acc-impl-reg", &cluster->acc_IMPL_pa, &cluster->acc_IMPL_len);
			ml_read_reg_range(child, "cpm-impl-reg", &cluster->cpm_IMPL_pa, &cluster->cpm_IMPL_len);
		}

#if HAS_CLUSTER
		if (MPIDR_CPU_ID(cpu->phys_id) > cluster_max_cpu_phys_id[phys_cluster_id]) {
			cluster_max_cpu_phys_id[phys_cluster_id] = MPIDR_CPU_ID(cpu->phys_id);
		}
#endif

		cpu->die_cluster_id = (int)ml_readprop(child, "die-cluster-id", MPIDR_CLUSTER_ID(cpu->phys_id));
		cpu->cluster_core_id = (int)ml_readprop(child, "cluster-core-id", MPIDR_CPU_ID(cpu->phys_id));

		cluster->num_cpus++;
		cluster->cpu_mask |= 1ULL << cpu->cpu_id;

		if (is_boot_cpu) {
			assert(topology_info.boot_cpu == NULL);
			topology_info.boot_cpu = cpu;
			topology_info.boot_cluster = cluster;
		}

#if CONFIG_SPTM
		sptm_register_cpu(cpu->phys_id);
#endif
	}

#if HAS_CLUSTER
	/*
	 * Build the cluster offset array, ensuring that the region reserved
	 * for each physical cluster contains enough entries to be indexed
	 * by the maximum physical CPU ID (AFF0) within the cluster.
	 */
	unsigned int cur_cluster_offset = 0;
	for (int i = 0; i <= MAX_CPU_CLUSTER_PHY_ID; i++) {
		if (cluster_phys_to_logical[i] != -1) {
			cluster_offsets[i] = cur_cluster_offset;
			cur_cluster_offset += (cluster_max_cpu_phys_id[i] + 1);
		}
	}
	assert(cur_cluster_offset <= MAX_CPUS);
#else
	/*
	 * For H10, there are really 2 physical clusters, but they are not separated
	 * into distinct ACCs.  AFF1 therefore always reports 0, and AFF0 numbering
	 * is linear across both clusters.   For the purpose of MPIDR_EL1-based indexing,
	 * treat H10 and earlier devices as though they contain a single cluster.
	 */
	cluster_offsets[0] = 0;
#endif
	assert(topology_info.boot_cpu != NULL);
	ml_read_chip_revision(&topology_info.chip_revision);

	/*
	 * Set TPIDR_EL0 to indicate the correct cpu number & cluster id,
	 * as we may not be booting from cpu 0. Userspace will consume
	 * the current CPU number through this register. For non-boot
	 * cores, this is done in start.s (start_cpu) using the per-cpu
	 * data object.
	 */
	ml_topology_cpu_t *boot_cpu = topology_info.boot_cpu;
	uint64_t tpidr_el0 = ((boot_cpu->cpu_id << MACHDEP_TPIDR_CPUNUM_SHIFT) & MACHDEP_TPIDR_CPUNUM_MASK) | \
	    ((boot_cpu->cluster_id << MACHDEP_TPIDR_CLUSTERID_SHIFT) & MACHDEP_TPIDR_CLUSTERID_MASK);
	assert(((tpidr_el0 & MACHDEP_TPIDR_CPUNUM_MASK) >> MACHDEP_TPIDR_CPUNUM_SHIFT) == boot_cpu->cpu_id);
	assert(((tpidr_el0 & MACHDEP_TPIDR_CLUSTERID_MASK) >> MACHDEP_TPIDR_CLUSTERID_SHIFT) == boot_cpu->cluster_id);
	__builtin_arm_wsr64("TPIDR_EL0", tpidr_el0);

	__builtin_arm_wsr64("TPIDRRO_EL0", 0);
}

const ml_topology_info_t *
ml_get_topology_info(void)
{
	return &topology_info;
}

void
ml_map_cpu_pio(void)
{
	unsigned int i;

	for (i = 0; i < topology_info.num_cpus; i++) {
		ml_topology_cpu_t *cpu = &topology_info.cpus[i];
		if (cpu->cpu_IMPL_pa) {
			cpu->cpu_IMPL_regs = (vm_offset_t)ml_io_map(cpu->cpu_IMPL_pa, cpu->cpu_IMPL_len);
			cpu->coresight_regs = (vm_offset_t)ml_io_map(cpu->coresight_pa, cpu->coresight_len);
		}
		if (cpu->cpu_UTTDBG_pa) {
			cpu->cpu_UTTDBG_regs = (vm_offset_t)ml_io_map(cpu->cpu_UTTDBG_pa, cpu->cpu_UTTDBG_len);
		}
	}

	for (i = 0; i < topology_info.num_clusters; i++) {
		ml_topology_cluster_t *cluster = &topology_info.clusters[i];
		if (cluster->acc_IMPL_pa) {
			cluster->acc_IMPL_regs = (vm_offset_t)ml_io_map(cluster->acc_IMPL_pa, cluster->acc_IMPL_len);
		}
		if (cluster->cpm_IMPL_pa) {
			cluster->cpm_IMPL_regs = (vm_offset_t)ml_io_map(cluster->cpm_IMPL_pa, cluster->cpm_IMPL_len);
		}
	}
}

unsigned int
ml_get_cpu_count(void)
{
	return topology_info.num_cpus;
}

unsigned int
ml_get_cluster_count(void)
{
	return topology_info.num_clusters;
}

int
ml_get_boot_cpu_number(void)
{
	return topology_info.boot_cpu->cpu_id;
}

cluster_type_t
ml_get_boot_cluster_type(void)
{
	return topology_info.boot_cluster->cluster_type;
}

int
ml_get_cpu_number(uint32_t phys_id)
{
	phys_id &= MPIDR_AFF1_MASK | MPIDR_AFF0_MASK;

	for (unsigned i = 0; i < topology_info.num_cpus; i++) {
		if (topology_info.cpus[i].phys_id == phys_id) {
			return i;
		}
	}

	return -1;
}

int
ml_get_cluster_number(uint32_t phys_id)
{
	int cpu_id = ml_get_cpu_number(phys_id);
	if (cpu_id < 0) {
		return -1;
	}

	ml_topology_cpu_t *cpu = &topology_info.cpus[cpu_id];

	return cpu->cluster_id;
}

unsigned int
ml_get_cpu_number_local(void)
{
	uint64_t mpidr_el1_value = 0;
	unsigned cpu_id;

	/* We identify the CPU based on the constant bits of MPIDR_EL1. */
	MRS(mpidr_el1_value, "MPIDR_EL1");
	cpu_id = ml_get_cpu_number((uint32_t)mpidr_el1_value);

	assert(cpu_id <= (unsigned int)ml_get_max_cpu_number());

	return cpu_id;
}

int
ml_get_cluster_number_local()
{
	uint64_t mpidr_el1_value = 0;
	unsigned cluster_id;

	/* We identify the cluster based on the constant bits of MPIDR_EL1. */
	MRS(mpidr_el1_value, "MPIDR_EL1");
	cluster_id = ml_get_cluster_number((uint32_t)mpidr_el1_value);

	assert(cluster_id <= (unsigned int)ml_get_max_cluster_number());

	return cluster_id;
}

int
ml_get_max_cpu_number(void)
{
	return topology_info.max_cpu_id;
}

int
ml_get_max_cluster_number(void)
{
	return topology_info.max_cluster_id;
}

unsigned int
ml_get_first_cpu_id(unsigned int cluster_id)
{
	return topology_info.clusters[cluster_id].first_cpu_id;
}

static_assert(MAX_CPUS <= 256, "MAX_CPUS must fit in _COMM_PAGE_CPU_TO_CLUSTER; Increase table size if needed");

void
ml_map_cpus_to_clusters(uint8_t *table)
{
	for (uint16_t cpu_id = 0; cpu_id < topology_info.num_cpus; cpu_id++) {
		*(table + cpu_id) = (uint8_t)(topology_info.cpus[cpu_id].cluster_id);
	}
}

/*
 * Return the die id of a cluster.
 */
unsigned int
ml_get_die_id(unsigned int cluster_id)
{
	/*
	 * The current implementation gets the die_id from the
	 * first CPU of the cluster.
	 * rdar://80917654 (Add the die_id field to the cluster topology info)
	 */
	unsigned int first_cpu = ml_get_first_cpu_id(cluster_id);
	return topology_info.cpus[first_cpu].die_id;
}

/*
 * Return the index of a cluster in its die.
 */
unsigned int
ml_get_die_cluster_id(unsigned int cluster_id)
{
	/*
	 * The current implementation gets the die_id from the
	 * first CPU of the cluster.
	 * rdar://80917654 (Add the die_id field to the cluster topology info)
	 */
	unsigned int first_cpu = ml_get_first_cpu_id(cluster_id);
	return topology_info.cpus[first_cpu].die_cluster_id;
}

/*
 * Return the highest die id of the system.
 */
unsigned int
ml_get_max_die_id(void)
{
	return topology_info.max_die_id;
}

void
ml_lockdown_init()
{
#if defined(KERNEL_INTEGRITY_KTRR) || defined(KERNEL_INTEGRITY_CTRR)
	rorgn_stash_range();
#endif
}

kern_return_t
ml_lockdown_handler_register(lockdown_handler_t f, void *this)
{
	if (!f) {
		return KERN_FAILURE;
	}

	assert(lockdown_done);
	f(this); // XXX: f this whole function

	return KERN_SUCCESS;
}

static mcache_flush_function mcache_flush_func;
static void* mcache_flush_service;
kern_return_t
ml_mcache_flush_callback_register(mcache_flush_function func, void *service)
{
	mcache_flush_service = service;
	mcache_flush_func = func;

	return KERN_SUCCESS;
}

kern_return_t
ml_mcache_flush(void)
{
	if (!mcache_flush_func) {
		panic("Cannot flush M$ with no flush callback registered");

		return KERN_FAILURE;
	} else {
		return mcache_flush_func(mcache_flush_service);
	}
}


extern lck_mtx_t pset_create_lock;

kern_return_t
ml_processor_register(ml_processor_info_t *in_processor_info,
    processor_t *processor_out, ipi_handler_t *ipi_handler_out,
    perfmon_interrupt_handler_func *pmi_handler_out)
{
	cpu_data_t *this_cpu_datap;
	processor_set_t pset;
	boolean_t  is_boot_cpu;
	static unsigned int reg_cpu_count = 0;

	if (in_processor_info->log_id > (uint32_t)ml_get_max_cpu_number()) {
		return KERN_FAILURE;
	}

	if ((unsigned)OSIncrementAtomic((SInt32*)&reg_cpu_count) >= topology_info.num_cpus) {
		return KERN_FAILURE;
	}

	if (in_processor_info->log_id != (uint32_t)ml_get_boot_cpu_number()) {
		is_boot_cpu = FALSE;
		this_cpu_datap = cpu_data_alloc(FALSE);
		cpu_data_init(this_cpu_datap);
	} else {
		this_cpu_datap = &BootCpuData;
		is_boot_cpu = TRUE;
	}

	assert(in_processor_info->log_id <= (uint32_t)ml_get_max_cpu_number());

	this_cpu_datap->cpu_id = in_processor_info->cpu_id;

	if (!is_boot_cpu) {
		this_cpu_datap->cpu_number = (unsigned short)(in_processor_info->log_id);

		if (cpu_data_register(this_cpu_datap) != KERN_SUCCESS) {
			goto processor_register_error;
		}
		assert((this_cpu_datap->cpu_number & MACHDEP_TPIDR_CPUNUM_MASK) == this_cpu_datap->cpu_number);
	}

	this_cpu_datap->cpu_idle_notify = in_processor_info->processor_idle;
	this_cpu_datap->cpu_cache_dispatch = (cache_dispatch_t)in_processor_info->platform_cache_dispatch;
	nanoseconds_to_absolutetime((uint64_t) in_processor_info->powergate_latency, &this_cpu_datap->cpu_idle_latency);
	this_cpu_datap->cpu_reset_assist = kvtophys(in_processor_info->powergate_stub_addr);

	this_cpu_datap->idle_timer_notify = in_processor_info->idle_timer;
	this_cpu_datap->idle_timer_refcon = in_processor_info->idle_timer_refcon;

	this_cpu_datap->platform_error_handler = in_processor_info->platform_error_handler;
	this_cpu_datap->cpu_regmap_paddr = in_processor_info->regmap_paddr;
	this_cpu_datap->cpu_phys_id = in_processor_info->phys_id;
	this_cpu_datap->cpu_l2_access_penalty = in_processor_info->l2_access_penalty;

	this_cpu_datap->cpu_cluster_type = in_processor_info->cluster_type;
	this_cpu_datap->cpu_cluster_id = in_processor_info->cluster_id;
	this_cpu_datap->cpu_l2_id = in_processor_info->l2_cache_id;
	this_cpu_datap->cpu_l2_size = in_processor_info->l2_cache_size;
	this_cpu_datap->cpu_l3_id = in_processor_info->l3_cache_id;
	this_cpu_datap->cpu_l3_size = in_processor_info->l3_cache_size;

	/*
	 * Encode cpu_id, cluster_id to be stored in TPIDR_EL0 (see
	 * cswitch.s:set_thread_registers, start.s:start_cpu) for consumption
	 * by userspace.
	 */
	this_cpu_datap->cpu_tpidr_el0 = ((this_cpu_datap->cpu_number << MACHDEP_TPIDR_CPUNUM_SHIFT) & MACHDEP_TPIDR_CPUNUM_MASK) | \
	    ((this_cpu_datap->cpu_cluster_id << MACHDEP_TPIDR_CLUSTERID_SHIFT) & MACHDEP_TPIDR_CLUSTERID_MASK);
	assert(((this_cpu_datap->cpu_tpidr_el0 & MACHDEP_TPIDR_CPUNUM_MASK) >> MACHDEP_TPIDR_CPUNUM_SHIFT) == this_cpu_datap->cpu_number);
	assert(((this_cpu_datap->cpu_tpidr_el0 & MACHDEP_TPIDR_CLUSTERID_MASK) >> MACHDEP_TPIDR_CLUSTERID_SHIFT) == this_cpu_datap->cpu_cluster_id);

#if HAS_CLUSTER
	this_cpu_datap->cluster_master = !OSTestAndSet(this_cpu_datap->cpu_cluster_id, &cluster_initialized);
#else /* HAS_CLUSTER */
	this_cpu_datap->cluster_master = is_boot_cpu;
#endif /* HAS_CLUSTER */
	lck_mtx_lock(&pset_create_lock);
	pset = pset_find(in_processor_info->cluster_id, NULL);
	kprintf("[%d]%s>pset_find(cluster_id=%d) returned pset %d\n", current_processor()->cpu_id, __FUNCTION__, in_processor_info->cluster_id, pset ? pset->pset_id : -1);
	if (pset == NULL) {
#if __AMP__
		pset_cluster_type_t pset_cluster_type = this_cpu_datap->cpu_cluster_type == CLUSTER_TYPE_E ? PSET_AMP_E : PSET_AMP_P;
		pset = pset_create(ml_get_boot_cluster_type() == this_cpu_datap->cpu_cluster_type ? &pset_node0 : &pset_node1, pset_cluster_type, this_cpu_datap->cpu_cluster_id, this_cpu_datap->cpu_cluster_id);
		assert(pset != PROCESSOR_SET_NULL);
		kprintf("[%d]%s>pset_create(cluster_id=%d) returned pset %d\n", current_processor()->cpu_id, __FUNCTION__, this_cpu_datap->cpu_cluster_id, pset->pset_id);
#else /* __AMP__ */
		pset_cluster_type_t pset_cluster_type = PSET_SMP;
		pset = pset_create(&pset_node0, pset_cluster_type, this_cpu_datap->cpu_cluster_id, this_cpu_datap->cpu_cluster_id);
		assert(pset != PROCESSOR_SET_NULL);
#endif /* __AMP__ */
	}
	kprintf("[%d]%s>cpu_id %p cluster_id %d cpu_number %d is type %d\n", current_processor()->cpu_id, __FUNCTION__, in_processor_info->cpu_id, in_processor_info->cluster_id, this_cpu_datap->cpu_number, in_processor_info->cluster_type);
	lck_mtx_unlock(&pset_create_lock);

	processor_t processor = PERCPU_GET_RELATIVE(processor, cpu_data, this_cpu_datap);
	if (!is_boot_cpu) {
		processor_init(processor, this_cpu_datap->cpu_number, pset);

		if (this_cpu_datap->cpu_l2_access_penalty) {
			/*
			 * Cores that have a non-zero L2 access penalty compared
			 * to the boot processor should be de-prioritized by the
			 * scheduler, so that threads use the cores with better L2
			 * preferentially.
			 */
			processor_set_primary(processor, master_processor);
		}
	}

	*processor_out = processor;
	*ipi_handler_out = cpu_signal_handler;
#if CPMU_AIC_PMI && CONFIG_CPU_COUNTERS
	*pmi_handler_out = mt_cpmu_aic_pmi;
#else
	*pmi_handler_out = NULL;
#endif /* CPMU_AIC_PMI && CONFIG_CPU_COUNTERS */
	if (in_processor_info->idle_tickle != (idle_tickle_t *) NULL) {
		*in_processor_info->idle_tickle = (idle_tickle_t) cpu_idle_tickle;
	}

#if CONFIG_CPU_COUNTERS
	if (kpc_register_cpu(this_cpu_datap) != TRUE) {
		goto processor_register_error;
	}
#endif /* CONFIG_CPU_COUNTERS */


	if (!is_boot_cpu) {
		random_cpu_init(this_cpu_datap->cpu_number);
		// now let next CPU register itself
		OSIncrementAtomic((SInt32*)&real_ncpus);
	}

	return KERN_SUCCESS;

processor_register_error:
#if CONFIG_CPU_COUNTERS
	kpc_unregister_cpu(this_cpu_datap);
#endif /* CONFIG_CPU_COUNTERS */
	if (!is_boot_cpu) {
		cpu_data_free(this_cpu_datap);
	}

	return KERN_FAILURE;
}

void
ml_init_arm_debug_interface(
	void * in_cpu_datap,
	vm_offset_t virt_address)
{
	((cpu_data_t *)in_cpu_datap)->cpu_debug_interface_map = virt_address;
	do_debugid();
}

/*
 *	Routine:        init_ast_check
 *	Function:
 */
void
init_ast_check(
	__unused processor_t processor)
{
}

/*
 *	Routine:        cause_ast_check
 *	Function:
 */
void
cause_ast_check(
	processor_t processor)
{
	if (current_processor() != processor) {
		cpu_signal(processor_to_cpu_datap(processor), SIGPast, (void *)NULL, (void *)NULL);
		KERNEL_DEBUG_CONSTANT_IST(KDEBUG_TRACE, MACHDBG_CODE(DBG_MACH_SCHED, MACH_REMOTE_AST), processor->cpu_id, 1 /* ast */, 0, 0, 0);
	}
}

extern uint32_t cpu_idle_count;

void
ml_get_power_state(boolean_t *icp, boolean_t *pidlep)
{
	*icp = ml_at_interrupt_context();
	*pidlep = (cpu_idle_count == real_ncpus);
}

/*
 *	Routine:        ml_cause_interrupt
 *	Function:	Generate a fake interrupt
 */
void
ml_cause_interrupt(void)
{
	return;                 /* BS_XXX */
}

/* Map memory map IO space */
vm_offset_t
ml_io_map(
	vm_offset_t phys_addr,
	vm_size_t size)
{
	return io_map(phys_addr, size, VM_WIMG_IO, VM_PROT_DEFAULT, false);
}

/* Map memory map IO space (with protections specified) */
vm_offset_t
ml_io_map_with_prot(
	vm_offset_t phys_addr,
	vm_size_t size,
	vm_prot_t prot)
{
	return io_map(phys_addr, size, VM_WIMG_IO, prot, false);
}

vm_offset_t
ml_io_map_unmappable(
	vm_offset_t             phys_addr,
	vm_size_t               size,
	unsigned int            flags)
{
	return io_map(phys_addr, size, flags, VM_PROT_DEFAULT, true);
}

vm_offset_t
ml_io_map_wcomb(
	vm_offset_t phys_addr,
	vm_size_t size)
{
	return io_map(phys_addr, size, VM_WIMG_WCOMB, VM_PROT_DEFAULT, false);
}

void
ml_io_unmap(vm_offset_t addr, vm_size_t sz)
{
	pmap_remove(kernel_pmap, addr, addr + sz);
	kmem_free(kernel_map, addr, sz);
}

vm_map_address_t
ml_map_high_window(
	vm_offset_t     phys_addr,
	vm_size_t       len)
{
	return pmap_map_high_window_bd(phys_addr, len, VM_PROT_READ | VM_PROT_WRITE);
}

vm_offset_t
ml_static_ptovirt(
	vm_offset_t paddr)
{
	return phystokv(paddr);
}

vm_offset_t
ml_static_slide(
	vm_offset_t vaddr)
{
	vm_offset_t slid_vaddr = 0;

#if CONFIG_SPTM
	if ((vaddr >= vm_sptm_offsets.unslid_base) && (vaddr < vm_sptm_offsets.unslid_top)) {
		slid_vaddr = vaddr + vm_sptm_offsets.slide;
	} else if ((vaddr >= vm_txm_offsets.unslid_base) && (vaddr < vm_txm_offsets.unslid_top)) {
		slid_vaddr = vaddr + vm_txm_offsets.slide;
	} else
#endif /* CONFIG_SPTM */
	{
		slid_vaddr = vaddr + vm_kernel_slide;
	}

	if (!VM_KERNEL_IS_SLID(slid_vaddr)) {
		/* This is only intended for use on static kernel addresses. */
		return 0;
	}

	return slid_vaddr;
}

vm_offset_t
ml_static_unslide(
	vm_offset_t vaddr)
{
	if (!VM_KERNEL_IS_SLID(vaddr)) {
		/* This is only intended for use on static kernel addresses. */
		return 0;
	}

#if CONFIG_SPTM
	/**
	 * Addresses coming from the SPTM and TXM have a different slide than the
	 * rest of the kernel.
	 */
	if ((vaddr >= vm_sptm_offsets.slid_base) && (vaddr < vm_sptm_offsets.slid_top)) {
		return vaddr - vm_sptm_offsets.slide;
	}

	if ((vaddr >= vm_txm_offsets.slid_base) && (vaddr < vm_txm_offsets.slid_top)) {
		return vaddr - vm_txm_offsets.slide;
	}
#endif /* CONFIG_SPTM */

	return vaddr - vm_kernel_slide;
}

extern tt_entry_t *arm_kva_to_tte(vm_offset_t va);

kern_return_t
ml_static_protect(
	vm_offset_t vaddr, /* kernel virtual address */
	vm_size_t size,
	vm_prot_t new_prot __unused)
{
#if CONFIG_SPTM
	/**
	 * Retype any frames that may be passed to the VM to XNU_DEFAULT.
	 */
	for (vm_offset_t sptm_vaddr_cur = vaddr; sptm_vaddr_cur < trunc_page_64(vaddr + size); sptm_vaddr_cur += PAGE_SIZE) {
		/* Check if this frame is XNU_DEFAULT and only retype it if is not */
		sptm_paddr_t sptm_paddr_cur = kvtophys_nofail(sptm_vaddr_cur);
		sptm_frame_type_t current_type = sptm_get_frame_type(sptm_paddr_cur);
		if (current_type != XNU_DEFAULT) {
			sptm_retype_params_t retype_params = {.raw = SPTM_RETYPE_PARAMS_NULL};
			sptm_retype(sptm_paddr_cur, current_type, XNU_DEFAULT, retype_params);
		}
	}

	return KERN_SUCCESS;
#else /* CONFIG_SPTM */
	pt_entry_t    arm_prot = 0;
	pt_entry_t    arm_block_prot = 0;
	vm_offset_t   vaddr_cur;
	ppnum_t       ppn;
	kern_return_t result = KERN_SUCCESS;

	if (vaddr < physmap_base) {
		panic("ml_static_protect(): %p < %p", (void *) vaddr, (void *) physmap_base);
		return KERN_FAILURE;
	}

	assert((vaddr & (PAGE_SIZE - 1)) == 0); /* must be page aligned */

	if ((new_prot & VM_PROT_WRITE) && (new_prot & VM_PROT_EXECUTE)) {
		panic("ml_static_protect(): WX request on %p", (void *) vaddr);
	}
	if (lockdown_done && (new_prot & VM_PROT_EXECUTE)) {
		panic("ml_static_protect(): attempt to inject executable mapping on %p", (void *) vaddr);
	}

	/* Set up the protection bits, and block bits so we can validate block mappings. */
	if (new_prot & VM_PROT_WRITE) {
		arm_prot |= ARM_PTE_AP(AP_RWNA);
		arm_block_prot |= ARM_TTE_BLOCK_AP(AP_RWNA);
	} else {
		arm_prot |= ARM_PTE_AP(AP_RONA);
		arm_block_prot |= ARM_TTE_BLOCK_AP(AP_RONA);
	}

	arm_prot |= ARM_PTE_NX;
	arm_block_prot |= ARM_TTE_BLOCK_NX;

	if (!(new_prot & VM_PROT_EXECUTE)) {
		arm_prot |= ARM_PTE_PNX;
		arm_block_prot |= ARM_TTE_BLOCK_PNX;
	}

	for (vaddr_cur = vaddr;
	    vaddr_cur < trunc_page_64(vaddr + size);
	    vaddr_cur += PAGE_SIZE) {
		ppn = pmap_find_phys(kernel_pmap, vaddr_cur);
		if (ppn != (vm_offset_t) NULL) {
			tt_entry_t      *tte2;
			pt_entry_t      *pte_p;
			pt_entry_t      ptmp;

#if XNU_MONITOR
			assert(!pmap_is_monitor(ppn));
			assert(!TEST_PAGE_RATIO_4);
#endif

			tte2 = arm_kva_to_tte(vaddr_cur);

			if (((*tte2) & ARM_TTE_TYPE_MASK) != ARM_TTE_TYPE_TABLE) {
				if ((((*tte2) & ARM_TTE_TYPE_MASK) == ARM_TTE_TYPE_BLOCK) &&
				    ((*tte2 & (ARM_TTE_BLOCK_NXMASK | ARM_TTE_BLOCK_PNXMASK | ARM_TTE_BLOCK_APMASK)) == arm_block_prot)) {
					/*
					 * We can support ml_static_protect on a block mapping if the mapping already has
					 * the desired protections.  We still want to run checks on a per-page basis.
					 */
					continue;
				}

				result = KERN_FAILURE;
				break;
			}

			pte_p = (pt_entry_t *)&((tt_entry_t*)(phystokv((*tte2) & ARM_TTE_TABLE_MASK)))[(((vaddr_cur) & ARM_TT_L3_INDEX_MASK) >> ARM_TT_L3_SHIFT)];
			ptmp = *pte_p;

			if ((ptmp & ARM_PTE_HINT_MASK) && ((ptmp & (ARM_PTE_APMASK | ARM_PTE_PNXMASK | ARM_PTE_NXMASK)) != arm_prot)) {
				/*
				 * The contiguous hint is similar to a block mapping for ml_static_protect; if the existing
				 * protections do not match the desired protections, then we will fail (as we cannot update
				 * this mapping without updating other mappings as well).
				 */
				result = KERN_FAILURE;
				break;
			}

			__unreachable_ok_push
			if (TEST_PAGE_RATIO_4) {
				{
					unsigned int    i;
					pt_entry_t      *ptep_iter;

					ptep_iter = pte_p;
					for (i = 0; i < 4; i++, ptep_iter++) {
						/* Note that there is a hole in the HINT sanity checking here. */
						ptmp = *ptep_iter;

						/* We only need to update the page tables if the protections do not match. */
						if ((ptmp & (ARM_PTE_APMASK | ARM_PTE_PNXMASK | ARM_PTE_NXMASK)) != arm_prot) {
							ptmp = (ptmp & ~(ARM_PTE_APMASK | ARM_PTE_PNXMASK | ARM_PTE_NXMASK)) | arm_prot;
							*ptep_iter = ptmp;
						}
					}
				}
			} else {
				ptmp = *pte_p;
				/* We only need to update the page tables if the protections do not match. */
				if ((ptmp & (ARM_PTE_APMASK | ARM_PTE_PNXMASK | ARM_PTE_NXMASK)) != arm_prot) {
					ptmp = (ptmp & ~(ARM_PTE_APMASK | ARM_PTE_PNXMASK | ARM_PTE_NXMASK)) | arm_prot;
					*pte_p = ptmp;
				}
			}
			__unreachable_ok_pop
		}
	}

	if (vaddr_cur > vaddr) {
		assert(((vaddr_cur - vaddr) & 0xFFFFFFFF00000000ULL) == 0);
		flush_mmu_tlb_region(vaddr, (uint32_t)(vaddr_cur - vaddr));
	}


	return result;
#endif /* CONFIG_SPTM */
}

#if defined(CONFIG_SPTM)
/*
 * Returns true if the given physical address is in one of the boot kernelcache ranges.
 */
static bool
ml_physaddr_in_bootkc_range(vm_offset_t physaddr)
{
	for (int i = 0; i < arm_vm_kernelcache_numranges; i++) {
		if (physaddr >= arm_vm_kernelcache_ranges[i].start_phys && physaddr < arm_vm_kernelcache_ranges[i].end_phys) {
			return true;
		}
	}
	return false;
}
#endif /* defined(CONFIG_SPTM) */

/*
 *	Routine:        ml_static_mfree
 *	Function:
 */
void
ml_static_mfree(
	vm_offset_t vaddr,
	vm_size_t   size)
{
	vm_offset_t vaddr_cur;
	vm_offset_t paddr_cur;
	ppnum_t     ppn;
	uint32_t    freed_pages = 0;
	uint32_t    freed_kernelcache_pages = 0;


	/* It is acceptable (if bad) to fail to free. */
	if (vaddr < physmap_base) {
		return;
	}

	assert((vaddr & (PAGE_SIZE - 1)) == 0); /* must be page aligned */

	for (vaddr_cur = vaddr;
	    vaddr_cur < trunc_page_64(vaddr + size);
	    vaddr_cur += PAGE_SIZE) {
		ppn = pmap_find_phys(kernel_pmap, vaddr_cur);
		if (ppn != (vm_offset_t) NULL) {
			/*
			 * It is not acceptable to fail to update the protections on a page
			 * we will release to the VM.  We need to either panic or continue.
			 * For now, we'll panic (to help flag if there is memory we can
			 * reclaim).
			 */
			if (ml_static_protect(vaddr_cur, PAGE_SIZE, VM_PROT_WRITE | VM_PROT_READ) != KERN_SUCCESS) {
				panic("Failed ml_static_mfree on %p", (void *) vaddr_cur);
			}

			paddr_cur = ptoa(ppn);


			vm_page_create(ppn, (ppn + 1));
			freed_pages++;
#if defined(CONFIG_SPTM)
			if (ml_physaddr_in_bootkc_range(paddr_cur)) {
#else
			if (paddr_cur >= arm_vm_kernelcache_phys_start && paddr_cur < arm_vm_kernelcache_phys_end) {
#endif
				freed_kernelcache_pages++;
			}
		}
	}
	vm_page_lockspin_queues();
	vm_page_wire_count -= freed_pages;
	vm_page_wire_count_initial -= freed_pages;
	vm_page_kernelcache_count -= freed_kernelcache_pages;
	vm_page_unlock_queues();
#if     DEBUG
	kprintf("ml_static_mfree: Released 0x%x pages at VA %p, size:0x%llx, last ppn: 0x%x, +%d bad\n", freed_pages, (void *)vaddr, (uint64_t)size, ppn, bad_page_cnt);
#endif
}

/*
 * Routine: ml_page_protection_type
 * Function: Returns the type of page protection that the system supports.
 */
ml_page_protection_t
ml_page_protection_type(void)
{
#if CONFIG_SPTM
	return 2;
#elif XNU_MONITOR
	return 1;
#else
	return 0;
#endif
}

/* virtual to physical on wired pages */
vm_offset_t
ml_vtophys(vm_offset_t vaddr)
{
	return kvtophys(vaddr);
}

/*
 * Routine: ml_nofault_copy
 * Function: Perform a physical mode copy if the source and destination have
 * valid translations in the kernel pmap. If translations are present, they are
 * assumed to be wired; e.g., no attempt is made to guarantee that the
 * translations obtained remain valid for the duration of the copy process.
 */
vm_size_t
ml_nofault_copy(vm_offset_t virtsrc, vm_offset_t virtdst, vm_size_t size)
{
	addr64_t        cur_phys_dst, cur_phys_src;
	vm_size_t       count, nbytes = 0;

	while (size > 0) {
		if (!(cur_phys_src = kvtophys(virtsrc))) {
			break;
		}
		if (!(cur_phys_dst = kvtophys(virtdst))) {
			break;
		}
		if (!pmap_valid_address(trunc_page_64(cur_phys_dst)) ||
		    !pmap_valid_address(trunc_page_64(cur_phys_src))) {
			break;
		}
		count = PAGE_SIZE - (cur_phys_src & PAGE_MASK);
		if (count > (PAGE_SIZE - (cur_phys_dst & PAGE_MASK))) {
			count = PAGE_SIZE - (cur_phys_dst & PAGE_MASK);
		}
		if (count > size) {
			count = size;
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
		if (!pmap_valid_address(trunc_page_64(cur_phys_src))) {
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

void
ml_get_bouncepool_info(vm_offset_t * phys_addr, vm_size_t * size)
{
	*phys_addr = 0;
	*size = 0;
}

void
active_rt_threads(__unused boolean_t active)
{
}

static void
cpu_qos_cb_default(__unused int urgency, __unused uint64_t qos_param1, __unused uint64_t qos_param2)
{
	return;
}

cpu_qos_update_t cpu_qos_update = cpu_qos_cb_default;

void
cpu_qos_update_register(cpu_qos_update_t cpu_qos_cb)
{
	if (cpu_qos_cb != NULL) {
		cpu_qos_update = cpu_qos_cb;
	} else {
		cpu_qos_update = cpu_qos_cb_default;
	}
}

void
thread_tell_urgency(thread_urgency_t urgency, uint64_t rt_period, uint64_t rt_deadline, uint64_t sched_latency __unused, __unused thread_t nthread)
{
	SCHED_DEBUG_PLATFORM_KERNEL_DEBUG_CONSTANT(MACHDBG_CODE(DBG_MACH_SCHED, MACH_URGENCY) | DBG_FUNC_START, urgency, rt_period, rt_deadline, sched_latency, 0);

	cpu_qos_update((int)urgency, rt_period, rt_deadline);

	SCHED_DEBUG_PLATFORM_KERNEL_DEBUG_CONSTANT(MACHDBG_CODE(DBG_MACH_SCHED, MACH_URGENCY) | DBG_FUNC_END, urgency, rt_period, rt_deadline, 0, 0);
}

void
machine_run_count(__unused uint32_t count)
{
}

processor_t
machine_choose_processor(__unused processor_set_t pset, processor_t processor)
{
	return processor;
}

#if KASAN
vm_offset_t ml_stack_base(void);
vm_size_t ml_stack_size(void);

vm_offset_t
ml_stack_base(void)
{
	uintptr_t local = (uintptr_t) &local;
	vm_offset_t     intstack_top_ptr;

	intstack_top_ptr = getCpuDatap()->intstack_top;
	if ((local < intstack_top_ptr) && (local > intstack_top_ptr - INTSTACK_SIZE)) {
		return intstack_top_ptr - INTSTACK_SIZE;
	} else {
		return current_thread()->kernel_stack;
	}
}
vm_size_t
ml_stack_size(void)
{
	uintptr_t local = (uintptr_t) &local;
	vm_offset_t     intstack_top_ptr;

	intstack_top_ptr = getCpuDatap()->intstack_top;
	if ((local < intstack_top_ptr) && (local > intstack_top_ptr - INTSTACK_SIZE)) {
		return INTSTACK_SIZE;
	} else {
		return kernel_stack_size;
	}
}
#endif

#ifdef CONFIG_KCOV

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

boolean_t
machine_timeout_suspended(void)
{
	return FALSE;
}

kern_return_t
ml_interrupt_prewarm(__unused uint64_t deadline)
{
	return KERN_FAILURE;
}

/*
 * Assumes fiq, irq disabled.
 */
void
ml_set_decrementer(uint32_t dec_value)
{
	cpu_data_t      *cdp = getCpuDatap();

	assert(ml_get_interrupts_enabled() == FALSE);
	cdp->cpu_decrementer = dec_value;

	if (cdp->cpu_set_decrementer_func) {
		cdp->cpu_set_decrementer_func(dec_value);
	} else {
		__builtin_arm_wsr64("CNTV_TVAL_EL0", (uint64_t)dec_value);
	}
}

/**
 * Perform a read of the timebase which is permitted to be executed
 * speculatively and/or out of program order.
 */
static inline uint64_t
speculative_timebase(void)
{
	return __builtin_arm_rsr64("CNTVCT_EL0");
}

/**
 * Read a non-speculative view of the timebase if one is available,
 * otherwise fallback on an ISB to prevent prevent speculation and
 * enforce ordering.
 */
static inline uint64_t
nonspeculative_timebase(void)
{
#if defined(HAS_ACNTVCT)
	return __builtin_arm_rsr64("ACNTVCT_EL0");
#elif __ARM_ARCH_8_6__
	return __builtin_arm_rsr64("CNTVCTSS_EL0");
#else
	// ISB required by ARMV7C.b section B8.1.2 & ARMv8 section D6.1.2
	// "Reads of CNT[PV]CT[_EL0] can occur speculatively and out of order relative
	// to other instructions executed on the same processor."
	__builtin_arm_isb(ISB_SY);
	return speculative_timebase();
#endif
}


uint64_t
ml_get_hwclock()
{
	uint64_t timebase = nonspeculative_timebase();
	return timebase;
}

uint64_t
ml_get_timebase()
{
	uint64_t clock, timebase;

	//the retry is for the case where S2R catches us in the middle of this. see rdar://77019633
	do {
		timebase = getCpuDatap()->cpu_base_timebase;
		os_compiler_barrier();
		clock = ml_get_hwclock();
		os_compiler_barrier();
	} while (getCpuDatap()->cpu_base_timebase != timebase);

	return clock + timebase;
}

/**
 * Issue a barrier that guarantees all prior memory accesses will complete
 * before any subsequent timebase reads.
 */
void
ml_memory_to_timebase_fence(void)
{
	__builtin_arm_dmb(DMB_SY);
	const uint64_t take_backwards_branch = 0;
	asm volatile (
        "1:"
                "ldr	x0, [%[take_backwards_branch]]" "\n"
                "cbnz	x0, 1b"                         "\n"
                :
                : [take_backwards_branch] "r"(&take_backwards_branch)
                : "x0"
        );

	/* throwaway read to prevent ml_get_speculative_timebase() reordering */
	(void)ml_get_hwclock();
}

/**
 * Issue a barrier that guarantees all prior timebase reads will
 * be ordered before any subsequent memory accesses.
 */
void
ml_timebase_to_memory_fence(void)
{
	__builtin_arm_isb(ISB_SY);
}

/*
 * Get the speculative timebase without an ISB.
 */
uint64_t
ml_get_speculative_timebase(void)
{
	uint64_t clock, timebase;

	//the retry is for the case where S2R catches us in the middle of this. see rdar://77019633&77697482
	do {
		timebase = getCpuDatap()->cpu_base_timebase;
		os_compiler_barrier();
		clock = speculative_timebase();

		os_compiler_barrier();
	} while (getCpuDatap()->cpu_base_timebase != timebase);

	return clock + timebase;
}

uint64_t
ml_get_timebase_entropy(void)
{
	return ml_get_speculative_timebase();
}

uint32_t
ml_get_decrementer(void)
{
	cpu_data_t *cdp = getCpuDatap();
	uint32_t dec;

	assert(ml_get_interrupts_enabled() == FALSE);

	if (cdp->cpu_get_decrementer_func) {
		dec = cdp->cpu_get_decrementer_func();
	} else {
		uint64_t wide_val;

		wide_val = __builtin_arm_rsr64("CNTV_TVAL_EL0");
		dec = (uint32_t)wide_val;
		assert(wide_val == (uint64_t)dec);
	}

	return dec;
}

boolean_t
ml_get_timer_pending(void)
{
	uint64_t cntv_ctl = __builtin_arm_rsr64("CNTV_CTL_EL0");
	return ((cntv_ctl & CNTV_CTL_EL0_ISTATUS) != 0) ? TRUE : FALSE;
}

__attribute__((noreturn))
void
platform_syscall(arm_saved_state_t *state)
{
	uint32_t code;

#define platform_syscall_kprintf(x...) /* kprintf("platform_syscall: " x) */

	code = (uint32_t)get_saved_state_reg(state, 3);

	KDBG(MACHDBG_CODE(DBG_MACH_MACHDEP_EXCP_SC_ARM, code) | DBG_FUNC_START,
	    get_saved_state_reg(state, 0),
	    get_saved_state_reg(state, 1),
	    get_saved_state_reg(state, 2));

	switch (code) {
	case 2:
		/* set cthread */
		platform_syscall_kprintf("set cthread self.\n");
		thread_set_cthread_self(get_saved_state_reg(state, 0));
		break;
	case 3:
		/* get cthread */
		platform_syscall_kprintf("get cthread self.\n");
		set_user_saved_state_reg(state, 0, thread_get_cthread_self());
		break;
	case 0: /* I-Cache flush (removed) */
	case 1: /* D-Cache flush (removed) */
	default:
		platform_syscall_kprintf("unknown: %d\n", code);
		break;
	}

	KDBG(MACHDBG_CODE(DBG_MACH_MACHDEP_EXCP_SC_ARM, code) | DBG_FUNC_END,
	    get_saved_state_reg(state, 0));

	thread_exception_return();
}

static void
_enable_timebase_event_stream(uint32_t bit_index)
{
	uint64_t cntkctl; /* One wants to use 32 bits, but "mrs" prefers it this way */

	if (bit_index >= 64) {
		panic("%s: invalid bit index (%u)", __FUNCTION__, bit_index);
	}

	__asm__ volatile ("mrs	%0, CNTKCTL_EL1" : "=r"(cntkctl));

	cntkctl |= (bit_index << CNTKCTL_EL1_EVENTI_SHIFT);
	cntkctl |= CNTKCTL_EL1_EVNTEN;
	cntkctl |= CNTKCTL_EL1_EVENTDIR; /* 1->0; why not? */

	/*
	 * If the SOC supports it (and it isn't broken), enable
	 * EL0 access to the timebase registers.
	 */
	if (user_timebase_type() != USER_TIMEBASE_NONE) {
		cntkctl |= (CNTKCTL_EL1_PL0PCTEN | CNTKCTL_EL1_PL0VCTEN);
	}

	__builtin_arm_wsr64("CNTKCTL_EL1", cntkctl);
}

/*
 * Turn timer on, unmask that interrupt.
 */
static void
_enable_virtual_timer(void)
{
	uint64_t cntvctl = CNTV_CTL_EL0_ENABLE; /* One wants to use 32 bits, but "mrs" prefers it this way */

	__builtin_arm_wsr64("CNTV_CTL_EL0", cntvctl);
	/* disable the physical timer as a precaution, as its registers reset to architecturally unknown values */
	__builtin_arm_wsr64("CNTP_CTL_EL0", CNTP_CTL_EL0_IMASKED);
}

void
fiq_context_init(boolean_t enable_fiq __unused)
{
	/* Interrupts still disabled. */
	assert(ml_get_interrupts_enabled() == FALSE);
	_enable_virtual_timer();
}

void
wfe_timeout_init(void)
{
	_enable_timebase_event_stream(arm64_eventi);
}

/**
 * Configures, but does not enable, the WFE event stream. The event stream
 * generates an event at a set interval to act as a timeout for WFEs.
 *
 * This function sets the static global variable arm64_eventi to be the proper
 * bit index for the CNTKCTL_EL1.EVENTI field to generate events at the correct
 * period (1us unless specified by the "wfe_events_sec" boot-arg). arm64_eventi
 * is used by wfe_timeout_init to actually poke the registers and enable the
 * event stream.
 *
 * The CNTKCTL_EL1.EVENTI field contains the index of the bit of CNTVCT_EL0 that
 * is the trigger for the system to generate an event. The trigger can occur on
 * either the rising or falling edge of the bit depending on the value of
 * CNTKCTL_EL1.EVNTDIR. This is arbitrary for our purposes, so we use the
 * falling edge (1->0) transition to generate events.
 */
void
wfe_timeout_configure(void)
{
	/* Could fill in our own ops here, if we needed them */
	uint64_t        ticks_per_sec, ticks_per_event, events_per_sec = 0;
	uint32_t        bit_index;

	if (PE_parse_boot_argn("wfe_events_sec", &events_per_sec, sizeof(events_per_sec))) {
		if (events_per_sec <= 0) {
			events_per_sec = 1;
		} else if (events_per_sec > USEC_PER_SEC) {
			events_per_sec = USEC_PER_SEC;
		}
	} else {
		events_per_sec = USEC_PER_SEC;
	}
	ticks_per_sec = gPEClockFrequencyInfo.timebase_frequency_hz;
	ticks_per_event = ticks_per_sec / events_per_sec;

	/* Bit index of next power of two greater than ticks_per_event */
	bit_index = flsll(ticks_per_event) - 1;
	/* Round up to next power of two if ticks_per_event is initially power of two */
	if ((ticks_per_event & ((1 << bit_index) - 1)) != 0) {
		bit_index++;
	}

	/*
	 * The timer can only trigger on rising or falling edge, not both; we don't
	 * care which we trigger on, but we do need to adjust which bit we are
	 * interested in to account for this.
	 *
	 * In particular, we set CNTKCTL_EL1.EVENTDIR to trigger events on the
	 * falling edge of the given bit. Therefore, we must decrement the bit index
	 * by one as when the bit before the one we care about makes a 1 -> 0
	 * transition, the bit we care about makes a 0 -> 1 transition.
	 *
	 * For example if we want an event generated every 8 ticks (if we calculated
	 * a bit_index of 3), we would want the event to be generated whenever the
	 * lower four bits of the counter transition from 0b0111 -> 0b1000. We can
	 * see that the bit at index 2 makes a falling transition in this scenario,
	 * so we would want EVENTI to be 2 instead of 3.
	 */
	if (bit_index != 0) {
		bit_index--;
	}

	arm64_eventi = bit_index;
}

boolean_t
ml_delay_should_spin(uint64_t interval)
{
	cpu_data_t     *cdp = getCpuDatap();

	if (cdp->cpu_idle_latency) {
		return (interval < cdp->cpu_idle_latency) ? TRUE : FALSE;
	} else {
		/*
		 * Early boot, latency is unknown. Err on the side of blocking,
		 * which should always be safe, even if slow
		 */
		return FALSE;
	}
}

boolean_t
ml_thread_is64bit(thread_t thread)
{
	return thread_is_64bit_addr(thread);
}

void
ml_delay_on_yield(void)
{
#if DEVELOPMENT || DEBUG
	if (yield_delay_us) {
		delay(yield_delay_us);
	}
#endif
}

void
ml_timer_evaluate(void)
{
}

boolean_t
ml_timer_forced_evaluation(void)
{
	return FALSE;
}

void
ml_gpu_stat_update(__unused uint64_t gpu_ns_delta)
{
	/*
	 * For now: update the resource coalition stats of the
	 * current thread's coalition
	 */
	task_coalition_update_gpu_stats(current_task(), gpu_ns_delta);
}

uint64_t
ml_gpu_stat(__unused thread_t t)
{
	return 0;
}

thread_t
current_thread(void)
{
	return current_thread_fast();
}

#if defined(HAS_APPLE_PAC)
uint8_t
ml_task_get_disable_user_jop(task_t task)
{
	assert(task);
	return task->disable_user_jop;
}

void
ml_task_set_disable_user_jop(task_t task, uint8_t disable_user_jop)
{
	assert(task);
	task->disable_user_jop = disable_user_jop;
}

void
ml_thread_set_disable_user_jop(thread_t thread, uint8_t disable_user_jop)
{
	assert(thread);
	if (disable_user_jop) {
		thread->machine.arm_machine_flags |= ARM_MACHINE_THREAD_DISABLE_USER_JOP;
	} else {
		thread->machine.arm_machine_flags &= ~ARM_MACHINE_THREAD_DISABLE_USER_JOP;
	}
}

void
ml_task_set_rop_pid(task_t task, task_t parent_task, boolean_t inherit)
{
	if (inherit) {
		task->rop_pid = parent_task->rop_pid;
	} else {
		task->rop_pid = early_random();
	}
}

/**
 * jop_pid may be inherited from the parent task or generated inside the shared
 * region.  Unfortunately these two parameters are available at very different
 * times during task creation, so we need to split this into two steps.
 */
void
ml_task_set_jop_pid(task_t task, task_t parent_task, boolean_t inherit, boolean_t disable_user_jop)
{
	if (inherit) {
		task->jop_pid = parent_task->jop_pid;
	} else if (disable_user_jop) {
		task->jop_pid = ml_non_arm64e_user_jop_pid();
	} else {
		task->jop_pid = ml_default_jop_pid();
	}
}

void
ml_task_set_jop_pid_from_shared_region(task_t task, boolean_t disable_user_jop)
{
	if (disable_user_jop) {
		task->jop_pid = ml_non_arm64e_user_jop_pid();
		return;
	}

	vm_shared_region_t sr = vm_shared_region_get(task);
	/*
	 * If there's no shared region, we can assign the key arbitrarily.  This
	 * typically happens when Mach-O image activation failed part of the way
	 * through, and this task is in the middle of dying with SIGKILL anyway.
	 */
	if (__improbable(!sr)) {
		task->jop_pid = early_random();
		return;
	}
	vm_shared_region_deallocate(sr);

	/*
	 * Similarly we have to worry about jetsam having killed the task and
	 * already cleared the shared_region_id.
	 */
	task_lock(task);
	if (task->shared_region_id != NULL) {
		task->jop_pid = shared_region_find_key(task->shared_region_id);
	} else {
		task->jop_pid = early_random();
	}
	task_unlock(task);
}

void
ml_thread_set_jop_pid(thread_t thread, task_t task)
{
	thread->machine.jop_pid = task->jop_pid;
}
#endif /* defined(HAS_APPLE_PAC) */

#if DEVELOPMENT || DEBUG
static uint64_t minor_badness_suffered = 0;
#endif
void
ml_report_minor_badness(uint32_t __unused badness_id)
{
	#if DEVELOPMENT || DEBUG
	(void)os_atomic_or(&minor_badness_suffered, 1ULL << badness_id, relaxed);
	#endif
}

#if defined(HAS_APPLE_PAC)
#if __ARM_ARCH_8_6__ || APPLEVIRTUALPLATFORM
/**
 * The ARMv8.6 implementation is also safe for non-FPAC CPUs, but less efficient;
 * guest kernels need to use it because it does not know at compile time whether
 * the host CPU supports FPAC.
 */

/**
 * Emulates the poisoning done by ARMv8.3-PAuth instructions on auth failure.
 */
static void *
ml_poison_ptr(void *ptr, ptrauth_key key)
{
	bool b_key = key & (1ULL << 0);
	uint64_t error_code;
	if (b_key) {
		error_code = 2;
	} else {
		error_code = 1;
	}

	bool kernel_pointer = (uintptr_t)ptr & (1ULL << 55);
	bool data_key = key & (1ULL << 1);
	/* When PAC is enabled, only userspace data pointers use TBI, regardless of boot parameters */
	bool tbi = data_key && !kernel_pointer;
	unsigned int poison_shift;
	if (tbi) {
		poison_shift = 53;
	} else {
		poison_shift = 61;
	}

	uintptr_t poisoned = (uintptr_t)ptr;
	poisoned &= ~(3ULL << poison_shift);
	poisoned |= error_code << poison_shift;
	return (void *)poisoned;
}

/*
 * ptrauth_sign_unauthenticated() reimplemented using asm volatile, forcing the
 * compiler to assume this operation has side-effects and cannot be reordered
 */
#define ptrauth_sign_volatile(__value, __suffix, __data)                \
	({                                                              \
	        void *__ret = __value;                                  \
	        asm volatile (                                          \
	                "pac" #__suffix "	%[value], %[data]"          \
	                : [value] "+r"(__ret)                           \
	                : [data] "r"(__data)                            \
	        );                                                      \
	        __ret;                                                  \
	})

#define ml_auth_ptr_unchecked_for_key(_ptr, _suffix, _key, _modifier)                           \
	do {                                                                                    \
	        void *stripped = ptrauth_strip(_ptr, _key);                                     \
	        void *reauthed = ptrauth_sign_volatile(stripped, _suffix, _modifier);           \
	        if (__probable(_ptr == reauthed)) {                                             \
	                _ptr = stripped;                                                        \
	        } else {                                                                        \
	                _ptr = ml_poison_ptr(stripped, _key);                                   \
	        }                                                                               \
	} while (0)

#define _ml_auth_ptr_unchecked(_ptr, _suffix, _modifier) \
	ml_auth_ptr_unchecked_for_key(_ptr, _suffix, ptrauth_key_as ## _suffix, _modifier)
#else
#define _ml_auth_ptr_unchecked(_ptr, _suffix, _modifier) \
	asm volatile ("aut" #_suffix " %[ptr], %[modifier]" : [ptr] "+r"(_ptr) : [modifier] "r"(_modifier));
#endif /* __ARM_ARCH_8_6__ || APPLEVIRTUALPLATFORM */

/**
 * Authenticates a signed pointer without trapping on failure.
 *
 * @warning This function must be called with interrupts disabled.
 *
 * @warning Pointer authentication failure should normally be treated as a fatal
 * error.  This function is intended for a handful of callers that cannot panic
 * on failure, and that understand the risks in handling a poisoned return
 * value.  Other code should generally use the trapping variant
 * ptrauth_auth_data() instead.
 *
 * @param ptr the pointer to authenticate
 * @param key which key to use for authentication
 * @param modifier a modifier to mix into the key
 * @return an authenticated version of ptr, possibly with poison bits set
 */
void *
ml_auth_ptr_unchecked(void *ptr, ptrauth_key key, uint64_t modifier)
{
	switch (key & 0x3) {
	case ptrauth_key_asia:
		_ml_auth_ptr_unchecked(ptr, ia, modifier);
		break;
	case ptrauth_key_asib:
		_ml_auth_ptr_unchecked(ptr, ib, modifier);
		break;
	case ptrauth_key_asda:
		_ml_auth_ptr_unchecked(ptr, da, modifier);
		break;
	case ptrauth_key_asdb:
		_ml_auth_ptr_unchecked(ptr, db, modifier);
		break;
	}

	return ptr;
}
#endif /* defined(HAS_APPLE_PAC) */

#ifdef CONFIG_XNUPOST
void
ml_expect_fault_begin(expected_fault_handler_t expected_fault_handler, uintptr_t expected_fault_addr)
{
	thread_t thread = current_thread();
	thread->machine.expected_fault_handler = expected_fault_handler;
	thread->machine.expected_fault_addr = expected_fault_addr;
	thread->machine.expected_fault_pc = 0;
}

/** Expect an exception to be thrown at EXPECTED_FAULT_PC */
void
ml_expect_fault_pc_begin(expected_fault_handler_t expected_fault_handler, uintptr_t expected_fault_pc)
{
	thread_t thread = current_thread();
	thread->machine.expected_fault_handler = expected_fault_handler;
	thread->machine.expected_fault_addr = 0;
	uintptr_t raw_func = (uintptr_t)ptrauth_strip(
		(void *)expected_fault_pc,
		ptrauth_key_function_pointer);
	thread->machine.expected_fault_pc = raw_func;
}

void
ml_expect_fault_end(void)
{
	thread_t thread = current_thread();
	thread->machine.expected_fault_handler = NULL;
	thread->machine.expected_fault_addr = 0;
	thread->machine.expected_fault_pc = 0;
}
#endif /* CONFIG_XNUPOST */

void
ml_hibernate_active_pre(void)
{
#if HIBERNATION
	if (kIOHibernateStateWakingFromHibernate == gIOHibernateState) {

		hibernate_rebuild_vm_structs();
	}
#endif /* HIBERNATION */
}

void
ml_hibernate_active_post(void)
{
#if HIBERNATION
	if (kIOHibernateStateWakingFromHibernate == gIOHibernateState) {
		hibernate_machine_init();
		hibernate_vm_lock_end();
		current_cpu_datap()->cpu_hibernate = 0;
	}
#endif /* HIBERNATION */
}

/**
 * Return back a machine-dependent array of address space regions that should be
 * reserved by the VM (pre-mapped in the address space). This will prevent user
 * processes from allocating or deallocating from within these regions.
 *
 * @param vm_is64bit True if the process has a 64-bit address space.
 * @param regions An out parameter representing an array of regions to reserve.
 *
 * @return The number of reserved regions returned through `regions`.
 */
size_t
ml_get_vm_reserved_regions(bool vm_is64bit, const struct vm_reserved_region **regions)
{
	assert(regions != NULL);

	/**
	 * Reserved regions only apply to 64-bit address spaces. This is because
	 * we only expect to grow the maximum user VA address on 64-bit address spaces
	 * (we've essentially already reached the max for 32-bit spaces). The reserved
	 * regions should safely fall outside of the max user VA for 32-bit processes.
	 */
	if (vm_is64bit) {
		*regions = vm_reserved_regions;
		return ARRAY_COUNT(vm_reserved_regions);
	} else {
		/* Don't reserve any VA regions on arm64_32 processes. */
		*regions = NULL;
		return 0;
	}
}

/* These WFE recommendations are expected to be updated on a relatively
 * infrequent cadence, possibly from a different cluster, hence
 * false cacheline sharing isn't expected to be material
 */
static uint64_t arm64_cluster_wfe_recs[MAX_CPU_CLUSTERS];

uint32_t
ml_update_cluster_wfe_recommendation(uint32_t wfe_cluster_id, uint64_t wfe_timeout_abstime_interval, __unused uint64_t wfe_hint_flags)
{
	assert(wfe_cluster_id < MAX_CPU_CLUSTERS);
	assert(wfe_timeout_abstime_interval <= ml_wfe_hint_max_interval);
	os_atomic_store(&arm64_cluster_wfe_recs[wfe_cluster_id], wfe_timeout_abstime_interval, relaxed);
	return 0; /* Success */
}

#if DEVELOPMENT || DEBUG
int wfe_rec_max = 0;
int wfe_rec_none = 0;
uint64_t wfe_rec_override_mat = 0;
uint64_t wfe_rec_clamp = 0;
#endif

uint64_t
ml_cluster_wfe_timeout(uint32_t wfe_cluster_id)
{
	/* This and its consumer does not synchronize vis-a-vis updates
	 * of the recommendation; races are acceptable.
	 */
	uint64_t wfet = os_atomic_load(&arm64_cluster_wfe_recs[wfe_cluster_id], relaxed);
#if DEVELOPMENT || DEBUG
	if (wfe_rec_clamp) {
		wfet = MIN(wfe_rec_clamp, wfet);
	}

	if (wfe_rec_max) {
		for (int i = 0; i < MAX_CPU_CLUSTERS; i++) {
			if (arm64_cluster_wfe_recs[i] > wfet) {
				wfet = arm64_cluster_wfe_recs[i];
			}
		}
	}

	if (wfe_rec_none) {
		wfet = 0;
	}

	if (wfe_rec_override_mat) {
		wfet = wfe_rec_override_mat;
	}
#endif
	return wfet;
}

__pure2 bool
ml_addr_in_non_xnu_stack(__unused uintptr_t addr)
{
#if CONFIG_SPTM
	/**
	 * If the address is within one of the SPTM-allocated per-cpu stacks, then
	 * return true.
	 */
	if ((addr >= SPTMArgs->cpu_stack_papt_start) &&
	    (addr < SPTMArgs->cpu_stack_papt_end)) {
		return true;
	}

	/**
	 * If the address is within one of the TXM thread stacks, then return true.
	 * The SPTM guarantees that these stacks are virtually contiguous.
	 */
	if ((addr >= SPTMArgs->txm_thread_stacks[0]) &&
	    (addr < SPTMArgs->txm_thread_stacks[MAX_CPUS - 1])) {
		return true;
	}

	return false;
#elif XNU_MONITOR
	return (addr >= (uintptr_t)pmap_stacks_start) && (addr < (uintptr_t)pmap_stacks_end);
#else
	return false;
#endif /* CONFIG_SPTM || XNU_MONITOR */
}

uint64_t
ml_get_backtrace_pc(struct arm_saved_state *state)
{
	assert((state != NULL) && is_saved_state64(state));

#if CONFIG_SPTM
	/**
	 * On SPTM-based systems, when a non-XNU domain (e.g., SPTM) is interrupted,
	 * the PC value saved into the state is not the actual PC at the interrupted
	 * point, but a fixed value to a handler that knows how to re-enter the
	 * interrupted domain. The interrupted domain's actual PC value is saved
	 * into x14, so let's return that instead.
	 */
	if (ml_addr_in_non_xnu_stack(get_saved_state_fp(state))) {
		return saved_state64(state)->x[14];
	}
#endif /* CONFIG_SPTM */

	return get_saved_state_pc(state);
}


bool
ml_paddr_is_exclaves_owned(vm_offset_t paddr)
{
#if CONFIG_SPTM
	const sptm_frame_type_t type = sptm_get_frame_type(paddr);
	return type == SK_DEFAULT || type == SK_IO;   // SK_SHARED_R[OW] are not exclusively exclaves frames
#else
	#pragma unused(paddr)
	return false;
#endif /* CONFIG_SPTM */
}

/**
 * Panic because an ARM saved-state accessor expected user saved-state but was
 * passed non-user saved-state.
 *
 * @param ss invalid saved-state (CPSR.M != EL0)
 */
void
ml_panic_on_invalid_old_cpsr(const arm_saved_state_t *ss)
{
	panic("invalid CPSR in user saved-state %p", ss);
}

/**
 * Panic because an ARM saved-state accessor was passed user saved-state and
 * asked to assign a non-user CPSR.
 *
 * @param ss original EL0 saved-state
 * @param cpsr invalid new CPSR value (CPSR.M != EL0)
 */
void
ml_panic_on_invalid_new_cpsr(const arm_saved_state_t *ss, uint32_t cpsr)
{
	panic("attempt to set non-user CPSR %#010x on user saved-state %p", cpsr, ss);
}

/**
 * Explicitly preallocates a floating point save area.
 * This is a noop on ARM because preallocation isn't required at this time.
 */
void
ml_fp_save_area_prealloc(void)
{
}
