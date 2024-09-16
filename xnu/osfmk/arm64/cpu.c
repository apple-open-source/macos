/*
 * Copyright (c) 2007-2021 Apple Inc. All rights reserved.
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
 *	File:	arm64/cpu.c
 *
 *	cpu specific routines
 */

#include <pexpert/arm64/board_config.h>
#include <kern/kalloc.h>
#include <kern/machine.h>
#include <kern/cpu_number.h>
#include <kern/percpu.h>
#include <kern/thread.h>
#include <kern/timer_queue.h>
#include <arm/cpu_data.h>
#include <arm/cpuid.h>
#include <arm/caches_internal.h>
#include <arm/cpu_data_internal.h>
#include <arm/cpu_internal.h>
#include <arm/misc_protos.h>
#include <arm/machine_cpu.h>
#include <arm/rtclock.h>
#include <arm64/proc_reg.h>
#include <mach/processor_info.h>
#include <vm/pmap.h>
#include <vm/vm_kern_xnu.h>
#include <vm/vm_map.h>
#include <pexpert/arm/protos.h>
#include <pexpert/device_tree.h>
#include <sys/kdebug.h>
#include <arm/machine_routines.h>

#include <machine/atomic.h>

#include <san/kasan.h>

#include <kern/kpc.h>
#if CONFIG_CPU_COUNTERS
#include <kern/monotonic.h>
#endif /* CONFIG_CPU_COUNTERS */

#if KPERF
#include <kperf/kptimer.h>
#endif /* KPERF */

#if HIBERNATION
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOHibernatePrivate.h>
#endif /* HIBERNATION */


#include <libkern/section_keywords.h>

extern boolean_t        idle_enable;
extern uint64_t         wake_abstime;

#if WITH_CLASSIC_S2R
void sleep_token_buffer_init(void);
#endif

#if !CONFIG_SPTM
extern uintptr_t resume_idle_cpu;
extern uintptr_t start_cpu;
vm_address_t   start_cpu_paddr;
#endif

#if __ARM_KERNEL_PROTECT__
extern void exc_vectors_table;
#endif /* __ARM_KERNEL_PROTECT__ */

#if APPLEVIRTUALPLATFORM
extern vm_offset_t reset_vector_vaddr;
#endif /* APPLEVIRTUALPLATFORM */

#if APPLEVIRTUALPLATFORM
extern void __attribute__((noreturn)) arm64_prepare_for_sleep(boolean_t deep_sleep, unsigned int cpu, uint64_t entry_pa);
#else
extern void __attribute__((noreturn)) arm64_prepare_for_sleep(boolean_t deep_sleep);
#endif
extern void arm64_force_wfi_clock_gate(void);
#if defined(APPLETYPHOON)
// <rdar://problem/15827409>
extern void typhoon_prepare_for_wfi(void);
extern void typhoon_return_from_wfi(void);
#endif

#if HAS_RETENTION_STATE
extern void arm64_retention_wfi(void);
#endif

sysreg_restore_t sysreg_restore __attribute__((section("__DATA, __const"))) = {
	.tcr_el1 = TCR_EL1_BOOT,
};

// wfi - wfi mode
//  0 : disabled
//  1 : normal
//  2 : overhead simulation (delay & flags)
TUNABLE(unsigned int, wfi, "wfi", 1);
#if DEVELOPMENT || DEBUG

// wfi_flags
//  1 << 0 : flush L1s
//  1 << 1 : flush TLBs
static int wfi_flags = 0;

// wfi_delay - delay ticks after wfi exit
static uint64_t wfi_delay = 0;

#endif /* DEVELOPMENT || DEBUG */

#define CPUPM_IDLE_WFE 0x5310300
#define CPUPM_IDLE_TIMER_WFE 0x5310304

#define DEFAULT_EXPECTING_IPI_WFE_TIMEOUT_USEC (60ULL)
TUNABLE(uint32_t, expecting_ipi_wfe_timeout_usec,
    "expecting_ipi_wfe_timeout_usec", DEFAULT_EXPECTING_IPI_WFE_TIMEOUT_USEC);
uint64_t expecting_ipi_wfe_timeout_mt = 0x0ULL; /* initialized to a non-zero value in sched_init */

/* When recommended, issue WFE with [FI]IRQ unmasked in the idle
 * loop. The default.
 */
uint32_t idle_proximate_io_wfe_unmasked = 1;
#if DEVELOPMENT || DEBUG
uint32_t idle_proximate_timer_wfe = 1;
uint32_t idle_proximate_io_wfe_masked = 0;
#else
/* Issue WFE in lieu of WFI when awaiting a proximate timer. */
static uint32_t idle_proximate_timer_wfe = 1;
/* When recommended, issue WFE with [FI]IRQ masked in the idle loop.
 * Non-default, retained for experimentation.
 */
static uint32_t idle_proximate_io_wfe_masked = 0;
#endif

#if __ARM_GLOBAL_SLEEP_BIT__
volatile boolean_t arm64_stall_sleep = TRUE;
#endif

#if WITH_CLASSIC_S2R
/*
 * These must be aligned to avoid issues with calling bcopy_phys on them before
 * we are done with pmap initialization.
 */
static const uint8_t __attribute__ ((aligned(8))) suspend_signature[] = {'X', 'S', 'O', 'M', 'P', 'S', 'U', 'S'};
static const uint8_t __attribute__ ((aligned(8))) running_signature[] = {'X', 'S', 'O', 'M', 'N', 'N', 'U', 'R'};
#endif

#if WITH_CLASSIC_S2R
static vm_offset_t sleepTokenBuffer = (vm_offset_t)NULL;
#endif
static boolean_t coresight_debug_enabled = FALSE;

#if defined(CONFIG_XNUPOST)
void arm64_ipi_test_callback(void *);
void arm64_immediate_ipi_test_callback(void *);

void
arm64_ipi_test_callback(void *parm)
{
	volatile uint64_t *ipi_test_data = parm;
	cpu_data_t *cpu_data;

	cpu_data = getCpuDatap();

	*ipi_test_data = cpu_data->cpu_number;
}

void
arm64_immediate_ipi_test_callback(void *parm)
{
	volatile uint64_t *ipi_test_data = parm;
	cpu_data_t *cpu_data;

	cpu_data = getCpuDatap();

	*ipi_test_data = cpu_data->cpu_number + MAX_CPUS;
}

uint64_t arm64_ipi_test_data[MAX_CPUS * 2];

MACHINE_TIMEOUT(arm64_ipi_test_timeout, "arm64-ipi-test", 100, MACHINE_TIMEOUT_UNIT_MSEC, NULL);

void
arm64_ipi_test()
{
	volatile uint64_t *ipi_test_data, *immediate_ipi_test_data;
	uint64_t timeout_ms = os_atomic_load(&arm64_ipi_test_timeout, relaxed);
	uint64_t then, now, delta;
	int current_cpu_number = getCpuDatap()->cpu_number;

	/*
	 * probably the only way to have this on most systems is with the
	 * cpus=1 boot-arg, but nonetheless, if we only have 1 CPU active,
	 * IPI is not available
	 */
	if (real_ncpus == 1) {
		return;
	}

	const unsigned int max_cpu_id = ml_get_max_cpu_number();
	for (unsigned int i = 0; i <= max_cpu_id; ++i) {
		ipi_test_data = &arm64_ipi_test_data[i];
		immediate_ipi_test_data = &arm64_ipi_test_data[i + MAX_CPUS];
		*ipi_test_data = ~i;
		kern_return_t error = cpu_xcall((int)i, (void *)arm64_ipi_test_callback, (void *)(uintptr_t)ipi_test_data);
		if (error != KERN_SUCCESS) {
			panic("CPU %d was unable to IPI CPU %u: error %d", current_cpu_number, i, error);
		}

		while ((error = cpu_immediate_xcall((int)i, (void *)arm64_immediate_ipi_test_callback,
		    (void *)(uintptr_t)immediate_ipi_test_data)) == KERN_ALREADY_WAITING) {
			now = mach_absolute_time();
			absolutetime_to_nanoseconds(now - then, &delta);
			if ((delta / NSEC_PER_MSEC) > timeout_ms) {
				panic("CPU %d was unable to immediate-IPI CPU %u within %lldms", current_cpu_number, i, timeout_ms);
			}
		}

		if (error != KERN_SUCCESS) {
			panic("CPU %d was unable to immediate-IPI CPU %u: error %d", current_cpu_number, i, error);
		}

		then = mach_absolute_time();

		while ((*ipi_test_data != i) || (*immediate_ipi_test_data != (i + MAX_CPUS))) {
			now = mach_absolute_time();
			absolutetime_to_nanoseconds(now - then, &delta);
			if ((delta / NSEC_PER_MSEC) > timeout_ms) {
				panic("CPU %d tried to IPI CPU %d but didn't get correct responses within %lldms, responses: %llx, %llx",
				    current_cpu_number, i, timeout_ms, *ipi_test_data, *immediate_ipi_test_data);
			}
		}
	}
}
#endif /* defined(CONFIG_XNUPOST) */

static void
configure_coresight_registers(cpu_data_t *cdp)
{
	int             i;

	assert(cdp);
	vm_offset_t     coresight_regs = ml_get_topology_info()->cpus[cdp->cpu_number].coresight_regs;

	/*
	 * ARMv8 coresight registers are optional. If the device tree did not
	 * provide either cpu_regmap_paddr (from the legacy "reg-private" EDT property)
	 * or coresight_regs (from the new "coresight-reg" property), assume that
	 * coresight registers are not supported.
	 */
	if (cdp->cpu_regmap_paddr || coresight_regs) {
		for (i = 0; i < CORESIGHT_REGIONS; ++i) {
			if (i == CORESIGHT_CTI || i == CORESIGHT_PMU) {
				continue;
			}
			/* Skip debug-only registers on production chips */
			if (((i == CORESIGHT_ED) || (i == CORESIGHT_UTT)) && !coresight_debug_enabled) {
				continue;
			}

			if (!cdp->coresight_base[i]) {
				if (coresight_regs) {
					cdp->coresight_base[i] = coresight_regs + CORESIGHT_OFFSET(i);
				} else {
					uint64_t addr = cdp->cpu_regmap_paddr + CORESIGHT_OFFSET(i);
					cdp->coresight_base[i] = (vm_offset_t)ml_io_map(addr, CORESIGHT_SIZE);
				}
			}
			/* Unlock EDLAR (CTI and PMU are skipped above). */
			if (i != CORESIGHT_UTT) {
				*(volatile uint32_t *)(cdp->coresight_base[i] + ARM_DEBUG_OFFSET_DBGLAR) = ARM_DBG_LOCK_ACCESS_KEY;
			}
		}
	}
}


/*
 *	Routine:	cpu_bootstrap
 *	Function:
 */
void
cpu_bootstrap(void)
{
}

/*
 *	Routine:	cpu_sleep
 *	Function:
 */
void
cpu_sleep(void)
{
	cpu_data_t     *cpu_data_ptr = getCpuDatap();

	cpu_data_ptr->cpu_active_thread = current_thread();
#if CONFIG_SPTM
	cpu_data_ptr->cpu_reset_handler = (uintptr_t) VM_KERNEL_STRIP_PTR(arm_init_cpu);
#else
	cpu_data_ptr->cpu_reset_handler = (uintptr_t) start_cpu_paddr;
#endif
	os_atomic_or(&cpu_data_ptr->cpu_flags, SleepState, relaxed);

	if (cpu_data_ptr->cpu_user_debug != NULL) {
		arm_debug_set(NULL);
	}

#if CONFIG_CPU_COUNTERS
	kpc_idle();
	mt_cpu_down(cpu_data_ptr);
#endif /* CONFIG_CPU_COUNTERS */
#if KPERF
	kptimer_stop_curcpu();
#endif /* KPERF */

	CleanPoC_Dcache();

#if USE_APPLEARMSMP
	if (ml_is_quiescing()) {
		PE_cpu_machine_quiesce(cpu_data_ptr->cpu_id);
	} else {
		bool deep_sleep = PE_cpu_down(cpu_data_ptr->cpu_id);
		cpu_data_ptr->cpu_sleep_token = ARM_CPU_ON_SLEEP_PATH;
		// hang CPU on spurious wakeup
		cpu_data_ptr->cpu_reset_handler = (uintptr_t)0;
		__builtin_arm_dsb(DSB_ISH);
		CleanPoU_Dcache();
#if APPLEVIRTUALPLATFORM
		arm64_prepare_for_sleep(deep_sleep, cpu_data_ptr->cpu_number, ml_vtophys(reset_vector_vaddr));
#else /* APPLEVIRTUALPLATFORM */
		arm64_prepare_for_sleep(deep_sleep);
#endif /* APPLEVIRTUALPLATFORM */
	}
#else
	PE_cpu_machine_quiesce(cpu_data_ptr->cpu_id);
#endif
	/*NOTREACHED*/
}

/*
 *	Routine:	cpu_interrupt_is_pending
 *	Function:	Returns a bool signifying a non-zero ISR_EL1,
 *			indicating a pending IRQ, FIQ or external abort.
 */

bool
cpu_interrupt_is_pending(void)
{
	uint64_t isr_value;
	isr_value = __builtin_arm_rsr64("ISR_EL1");
	return isr_value != 0;
}

static bool
cpu_proximate_timer(void)
{
	return !SetIdlePop();
}

#ifdef ARM64_BOARD_CONFIG_T6000
int wfe_allowed = 0;
#else
int wfe_allowed = 1;
#endif /* ARM64_BOARD_CONFIG_T6000 */

#if DEVELOPMENT || DEBUG
#define WFE_STAT(x)     \
	do {            \
	        (x);    \
	} while(0)
#else
#define WFE_STAT(x)     do {} while(0)
#endif /* DEVELOPMENT || DEBUG */

bool
wfe_to_deadline_or_interrupt(uint32_t cid, uint64_t wfe_deadline, cpu_data_t *cdp, bool unmask, bool check_cluster_recommendation)
{
	bool ipending = false;
	uint64_t irqc = 0, nirqc = 0;

	/* The ARMv8 architecture permits a processor dwelling in WFE
	 * with F/IRQ masked to ignore a pending interrupt, i.e.
	 * not classify it as an 'event'. This is potentially
	 * problematic with AICv2's IRQ distribution model, as
	 * a transient interrupt masked interval can cause an SIQ
	 * query rejection, possibly routing the interrupt to
	 * another core/cluster in a powergated state.
	 * Hence, optionally unmask IRQs+FIQs across WFE.
	 */
	if (unmask) {
		/* Latch SW IRQ+FIQ counter prior to unmasking
		 * interrupts.
		 */
		irqc = nirqc = os_atomic_load(&cdp->cpu_stat.irq_ex_cnt_wake, relaxed);
		__builtin_arm_wsr("DAIFClr", DAIFSC_STANDARD_DISABLE);
	}

	while ((ipending = (cpu_interrupt_is_pending())) == false) {
		if (unmask) {
			/* If WFE was issued with IRQs unmasked, an
			 * interrupt may have been processed.
			 * Consult the SW IRQ counter to determine
			 * whether the 'idle loop' must be
			 * re-evaluated.
			 */
			nirqc = os_atomic_load(&cdp->cpu_stat.irq_ex_cnt_wake, relaxed);
			if (nirqc != irqc) {
				break;
			}
		}

		if (__probable(wfe_allowed)) {
			/*
			 * If IRQs are unmasked, there's a small window
			 * where an 'extra' WFE may be issued after
			 * the consultation of the SW interrupt counter
			 * and new interrupt arrival. Hence this WFE
			 * relies on the [FI]RQ interrupt handler
			 * epilogue issuing a 'SEVL', to post an
			 * event which causes the next WFE on the same
			 * PE to retire immediately.
			 */

			__builtin_arm_wfe();
		}

		WFE_STAT(cdp->wfe_count++);
		if (wfe_deadline != ~0ULL) {
			WFE_STAT(cdp->wfe_deadline_checks++);
			/* Check if the WFE recommendation has expired.
			 * We do not recompute the deadline here.
			 */
			if ((check_cluster_recommendation && ml_cluster_wfe_timeout(cid) == 0) ||
			    mach_absolute_time() >= wfe_deadline) {
				WFE_STAT(cdp->wfe_terminations++);
				break;
			}
		}
	}

	if (unmask) {
		__builtin_arm_wsr64("DAIFSet", DAIFSC_STANDARD_DISABLE);
		/* Refetch SW interrupt counter with IRQs masked
		 * It is important that this routine accurately flags
		 * any observed interrupts via its return value,
		 * inaccuracy may lead to an erroneous WFI fallback.
		 */
		nirqc = os_atomic_load(&cdp->cpu_stat.irq_ex_cnt_wake, relaxed);
	}

	return ipending || (nirqc != irqc);
}

/*
 *	Routine:	cpu_idle
 *	Function:
 */
void __attribute__((noreturn))
cpu_idle(void)
{
	cpu_data_t     *cpu_data_ptr = getCpuDatap();
	processor_t     processor = current_processor();
	uint64_t        new_idle_timeout_ticks = 0x0ULL, lastPop;
	bool idle_disallowed = false;
	/* Read and reset the next_idle_short flag */
	bool next_idle_short = processor->next_idle_short;
	processor->next_idle_short = false;

	if (__improbable((!idle_enable))) {
		idle_disallowed = true;
	} else if (__improbable(cpu_data_ptr->cpu_signal & SIGPdisabled)) {
		idle_disallowed = true;
	}

	if (__improbable(idle_disallowed)) {
		Idle_load_context();
	}

	bool ipending = false;
	uint32_t cid = cpu_data_ptr->cpu_cluster_id;
	bool check_cluster_recommendation = true;
	uint64_t wfe_timeout = 0;

	if (idle_proximate_io_wfe_masked == 1) {
		/* Check for an active perf. controller generated
		 * WFE recommendation for this cluster.
		 */
		wfe_timeout = ml_cluster_wfe_timeout(cid);
	}

	if (next_idle_short && expecting_ipi_wfe_timeout_mt > wfe_timeout) {
		/* In this case we should WFE because a response IPI
		 * is expected soon.
		 */
		wfe_timeout = expecting_ipi_wfe_timeout_mt;
		check_cluster_recommendation = false;
	}

	if (wfe_timeout != 0) {
		uint64_t wfe_deadline = mach_absolute_time() + wfe_timeout;
		/* Poll issuing event-bounded WFEs until an interrupt
		 * arrives or the WFE recommendation expires
		 */
		KDBG(CPUPM_IDLE_WFE | DBG_FUNC_START, ipending, cpu_data_ptr->wfe_count, wfe_timeout, !check_cluster_recommendation);
		ipending = wfe_to_deadline_or_interrupt(cid, wfe_deadline, cpu_data_ptr, false, check_cluster_recommendation);
		KDBG(CPUPM_IDLE_WFE | DBG_FUNC_END, ipending, cpu_data_ptr->wfe_count, wfe_deadline);
		if (ipending == true) {
			/* Back to machine_idle() */
			Idle_load_context();
		}
	}

	if (__improbable(cpu_proximate_timer())) {
		if (idle_proximate_timer_wfe == 1) {
			/* Poll issuing WFEs until the expected
			 * timer FIQ arrives.
			 */
			KDBG(CPUPM_IDLE_TIMER_WFE | DBG_FUNC_START, ipending, cpu_data_ptr->wfe_count, ~0ULL);
			ipending = wfe_to_deadline_or_interrupt(cid, ~0ULL, cpu_data_ptr, false, false);
			KDBG(CPUPM_IDLE_TIMER_WFE | DBG_FUNC_END, ipending, cpu_data_ptr->wfe_count, ~0ULL);
			assert(ipending == true);
		}
		Idle_load_context();
	}

	lastPop = cpu_data_ptr->rtcPop;

	cpu_data_ptr->cpu_active_thread = current_thread();

	if (wfi && (cpu_data_ptr->cpu_idle_notify != NULL)) {
		cpu_data_ptr->cpu_idle_notify(cpu_data_ptr->cpu_id, TRUE, &new_idle_timeout_ticks);
	}

	if (cpu_data_ptr->idle_timer_notify != NULL) {
		if (new_idle_timeout_ticks == 0x0ULL) {
			/* turn off the idle timer */
			cpu_data_ptr->idle_timer_deadline = 0x0ULL;
		} else {
			/* set the new idle timeout */
			clock_absolutetime_interval_to_deadline(new_idle_timeout_ticks, &cpu_data_ptr->idle_timer_deadline);
		}
		timer_resync_deadlines();
		if (cpu_data_ptr->rtcPop != lastPop) {
			SetIdlePop();
		}
	}

#if CONFIG_CPU_COUNTERS
	kpc_idle();
	mt_cpu_idle(cpu_data_ptr);
#endif /* CONFIG_CPU_COUNTERS */

	if (wfi) {
#if !defined(APPLE_ARM64_ARCH_FAMILY)
		platform_cache_idle_enter();
#endif

#if DEVELOPMENT || DEBUG
		// When simulating wfi overhead,
		// force wfi to clock gating only
		if (wfi == 2) {
			arm64_force_wfi_clock_gate();
		}
#endif /* DEVELOPMENT || DEBUG */

#if defined(APPLETYPHOON)
		// <rdar://problem/15827409> CPU1 Stuck in WFIWT Because of MMU Prefetch
		typhoon_prepare_for_wfi();
#endif
		__builtin_arm_dsb(DSB_SY);
#if HAS_RETENTION_STATE
		arm64_retention_wfi();
#else
		__builtin_arm_wfi();
#endif

#if defined(APPLETYPHOON)
		// <rdar://problem/15827409> CPU1 Stuck in WFIWT Because of MMU Prefetch
		typhoon_return_from_wfi();
#endif

#if DEVELOPMENT || DEBUG
		// Handle wfi overhead simulation
		if (wfi == 2) {
			uint64_t deadline;

			// Calculate wfi delay deadline
			clock_absolutetime_interval_to_deadline(wfi_delay, &deadline);

			// Flush L1 caches
			if ((wfi_flags & 1) != 0) {
				InvalidatePoU_Icache();
				FlushPoC_Dcache();
			}

			// Flush TLBs
			if ((wfi_flags & 2) != 0) {
				flush_core_tlb();
			}

			// Wait for the ballance of the wfi delay
			clock_delay_until(deadline);
		}
#endif /* DEVELOPMENT || DEBUG */
	}

	ClearIdlePop(TRUE);

	cpu_idle_exit(FALSE);
}

/*
 *	Routine:	cpu_idle_exit
 *	Function:
 */
void
cpu_idle_exit(boolean_t from_reset)
{
	uint64_t        new_idle_timeout_ticks = 0x0ULL;
	cpu_data_t     *cpu_data_ptr = getCpuDatap();

	assert(exception_stack_pointer() != 0);

	/* Back from WFI, unlock OSLAR and EDLAR. */
	if (from_reset) {
		configure_coresight_registers(cpu_data_ptr);
	}

#if CONFIG_CPU_COUNTERS
	kpc_idle_exit();
	mt_cpu_run(cpu_data_ptr);
#endif /* CONFIG_CPU_COUNTERS */

	if (wfi && (cpu_data_ptr->cpu_idle_notify != NULL)) {
		cpu_data_ptr->cpu_idle_notify(cpu_data_ptr->cpu_id, FALSE, &new_idle_timeout_ticks);
	}

	if (cpu_data_ptr->idle_timer_notify != NULL) {
		if (new_idle_timeout_ticks == 0x0ULL) {
			/* turn off the idle timer */
			cpu_data_ptr->idle_timer_deadline = 0x0ULL;
		} else {
			/* set the new idle timeout */
			clock_absolutetime_interval_to_deadline(new_idle_timeout_ticks, &cpu_data_ptr->idle_timer_deadline);
		}
		timer_resync_deadlines();
	}

#if KASAN_TBI
	kasan_unpoison_curstack(false);
#endif /* KASAN_TBI */

	Idle_load_context();
}

void
cpu_init(void)
{
	cpu_data_t     *cdp = getCpuDatap();
	arm_cpu_info_t *cpu_info_p;

	assert(exception_stack_pointer() != 0);

	if (cdp->cpu_type != CPU_TYPE_ARM64) {
		cdp->cpu_type = CPU_TYPE_ARM64;

		timer_call_queue_init(&cdp->rtclock_timer.queue);
		cdp->rtclock_timer.deadline = EndOfAllTime;

		if (cdp == &BootCpuData) {
			do_cpuid();
			do_mvfpid();
		} else {
			/*
			 * We initialize non-boot CPUs here; the boot CPU is
			 * dealt with as part of pmap_bootstrap.
			 */
			pmap_cpu_data_init();
		}

		do_cacheid();

		/* ARM_SMP: Assuming identical cpu */
		do_debugid();

		cpu_info_p = cpuid_info();

		/* switch based on CPU's reported architecture */
		switch (cpu_info_p->arm_info.arm_arch) {
		case CPU_ARCH_ARMv8:
			cdp->cpu_subtype = CPU_SUBTYPE_ARM64_V8;
			break;
		case CPU_ARCH_ARMv8E:
			cdp->cpu_subtype = CPU_SUBTYPE_ARM64E;
			break;
		default:
			//cdp->cpu_subtype = CPU_SUBTYPE_ARM64_ALL;
			/* this panic doesn't work this early in startup */
			panic("Unknown CPU subtype...");
			break;
		}

		cdp->cpu_threadtype = CPU_THREADTYPE_NONE;
	}
	cdp->cpu_stat.irq_ex_cnt_wake = 0;
	cdp->cpu_stat.ipi_cnt_wake = 0;
#if CONFIG_CPU_COUNTERS
	cdp->cpu_stat.pmi_cnt_wake = 0;
#endif /* CONFIG_CPU_COUNTERS */
	cdp->cpu_running = TRUE;
	cdp->cpu_sleep_token_last = cdp->cpu_sleep_token;
	cdp->cpu_sleep_token = 0x0UL;
#if CONFIG_CPU_COUNTERS
	kpc_idle_exit();
	mt_cpu_up(cdp);
#endif /* CONFIG_CPU_COUNTERS */
}

void
cpu_stack_alloc(cpu_data_t *cpu_data_ptr)
{
	vm_offset_t irq_stack = 0;
	vm_offset_t exc_stack = 0;

	kmem_alloc(kernel_map, &irq_stack,
	    INTSTACK_SIZE + ptoa(2), KMA_NOFAIL | KMA_PERMANENT | KMA_ZERO |
	    KMA_GUARD_FIRST | KMA_GUARD_LAST | KMA_KSTACK | KMA_KOBJECT,
	    VM_KERN_MEMORY_STACK);

	cpu_data_ptr->intstack_top = irq_stack + PAGE_SIZE + INTSTACK_SIZE;
	cpu_data_ptr->istackptr = (void *)cpu_data_ptr->intstack_top;

	kmem_alloc(kernel_map, &exc_stack,
	    EXCEPSTACK_SIZE + ptoa(2), KMA_NOFAIL | KMA_PERMANENT | KMA_ZERO |
	    KMA_GUARD_FIRST | KMA_GUARD_LAST | KMA_KSTACK | KMA_KOBJECT,
	    VM_KERN_MEMORY_STACK);

	cpu_data_ptr->excepstack_top = exc_stack + PAGE_SIZE + EXCEPSTACK_SIZE;
	cpu_data_ptr->excepstackptr = (void *)cpu_data_ptr->excepstack_top;
}

void
cpu_data_init(cpu_data_t *cpu_data_ptr)
{
	uint32_t i;

	os_atomic_store(&cpu_data_ptr->cpu_flags, 0, relaxed);
	cpu_data_ptr->cpu_int_state = 0;
	cpu_data_ptr->cpu_pending_ast = AST_NONE;
	cpu_data_ptr->cpu_cache_dispatch = NULL;
	cpu_data_ptr->rtcPop = EndOfAllTime;
	cpu_data_ptr->rtclock_datap = &RTClockData;
	cpu_data_ptr->cpu_user_debug = NULL;


	cpu_data_ptr->cpu_base_timebase = 0;
	cpu_data_ptr->cpu_idle_notify = NULL;
	cpu_data_ptr->cpu_idle_latency = 0x0ULL;
	cpu_data_ptr->cpu_idle_pop = 0x0ULL;
	cpu_data_ptr->cpu_reset_type = 0x0UL;
	cpu_data_ptr->cpu_reset_handler = 0x0UL;
	cpu_data_ptr->cpu_reset_assist = 0x0UL;
	cpu_data_ptr->cpu_regmap_paddr = 0x0ULL;
	cpu_data_ptr->cpu_phys_id = 0x0UL;
	cpu_data_ptr->cpu_l2_access_penalty = 0;
	cpu_data_ptr->cpu_cluster_type = CLUSTER_TYPE_SMP;
	cpu_data_ptr->cpu_cluster_id = 0;
	cpu_data_ptr->cpu_l2_id = 0;
	cpu_data_ptr->cpu_l2_size = 0;
	cpu_data_ptr->cpu_l3_id = 0;
	cpu_data_ptr->cpu_l3_size = 0;

	cpu_data_ptr->cpu_signal = SIGPdisabled;

	cpu_data_ptr->cpu_get_fiq_handler = NULL;
	cpu_data_ptr->cpu_tbd_hardware_addr = NULL;
	cpu_data_ptr->cpu_tbd_hardware_val = NULL;
	cpu_data_ptr->cpu_get_decrementer_func = NULL;
	cpu_data_ptr->cpu_set_decrementer_func = NULL;
	cpu_data_ptr->cpu_sleep_token = ARM_CPU_ON_SLEEP_PATH;
	cpu_data_ptr->cpu_sleep_token_last = 0x00000000UL;
	cpu_data_ptr->cpu_xcall_p0 = NULL;
	cpu_data_ptr->cpu_xcall_p1 = NULL;
	cpu_data_ptr->cpu_imm_xcall_p0 = NULL;
	cpu_data_ptr->cpu_imm_xcall_p1 = NULL;

	for (i = 0; i < CORESIGHT_REGIONS; ++i) {
		cpu_data_ptr->coresight_base[i] = 0;
	}

#if !XNU_MONITOR
	pmap_cpu_data_t * pmap_cpu_data_ptr = &cpu_data_ptr->cpu_pmap_cpu_data;

	pmap_cpu_data_ptr->cpu_number = PMAP_INVALID_CPU_NUM;
	pmap_cpu_data_ptr->pv_free.list = NULL;
	pmap_cpu_data_ptr->pv_free.count = 0;
	pmap_cpu_data_ptr->pv_free_spill_marker = NULL;
#if !CONFIG_SPTM
	pmap_cpu_data_ptr->cpu_nested_pmap = (struct pmap *) NULL;
	bzero(&(pmap_cpu_data_ptr->cpu_sw_asids[0]), sizeof(pmap_cpu_data_ptr->cpu_sw_asids));
#endif
#endif /* !XNU_MONITOR */
	cpu_data_ptr->halt_status = CPU_NOT_HALTED;
#if __ARM_KERNEL_PROTECT__
	cpu_data_ptr->cpu_exc_vectors = (vm_offset_t)&exc_vectors_table;
#endif /* __ARM_KERNEL_PROTECT__ */

#if defined(HAS_APPLE_PAC)
	cpu_data_ptr->rop_key = 0;
	cpu_data_ptr->jop_key = ml_default_jop_pid();
#endif
}

void
cpu_data_register(cpu_data_t *cpu_data_ptr)
{
	int     cpu = cpu_data_ptr->cpu_number;

#if KASAN
	for (int i = 0; i < CPUWINDOWS_MAX; i++) {
		kasan_notify_address_nopoison(pmap_cpu_windows_copy_addr(cpu, i), PAGE_SIZE);
	}
#endif

	__builtin_arm_dmb(DMB_ISH); // Ensure prior stores to cpu data are visible
	CpuDataEntries[cpu].cpu_data_vaddr = cpu_data_ptr;
	CpuDataEntries[cpu].cpu_data_paddr = (void *)ml_vtophys((vm_offset_t)cpu_data_ptr);
}

#if defined(KERNEL_INTEGRITY_CTRR)
/* Hibernation needs to reset this state, so data and text are in the hib segment;
 * this allows them be accessed and executed early.
 */
LCK_GRP_DECLARE(ctrr_cpu_start_lock_grp, "ctrr_cpu_start_lock");
LCK_SPIN_DECLARE(ctrr_cpu_start_lck, &ctrr_cpu_start_lock_grp);
enum ctrr_cluster_states ctrr_cluster_locked[MAX_CPU_CLUSTERS] MARK_AS_HIBERNATE_DATA;

MARK_AS_HIBERNATE_TEXT
void
init_ctrr_cluster_states(void)
{
	for (int i = 0; i < MAX_CPU_CLUSTERS; i++) {
		ctrr_cluster_locked[i] = CTRR_UNLOCKED;
	}
}
#endif

void
cpu_start(int cpu)
{
	cpu_data_t *cpu_data_ptr = CpuDataEntries[cpu].cpu_data_vaddr;
	processor_t processor = PERCPU_GET_RELATIVE(processor, cpu_data, cpu_data_ptr);

	if (processor_should_kprintf(processor, true)) {
		kprintf("cpu_start() cpu: %d\n", cpu);
	}

	if (cpu == cpu_number()) {
		/* Current CPU is already running, just needs initialization */
		cpu_machine_init();
		configure_coresight_registers(cpu_data_ptr);
	} else {
		thread_t first_thread;
#if CONFIG_SPTM
		cpu_data_ptr->cpu_reset_handler = (vm_offset_t) VM_KERNEL_STRIP_PTR(arm_init_cpu);
#else
		cpu_data_ptr->cpu_reset_handler = (vm_offset_t) start_cpu_paddr;
#if !XNU_MONITOR
		cpu_data_ptr->cpu_pmap_cpu_data.cpu_nested_pmap = NULL;
#endif
#endif /* !CONFIG_SPTM */

		if (processor->startup_thread != THREAD_NULL) {
			first_thread = processor->startup_thread;
		} else {
			first_thread = processor->idle_thread;
		}
		cpu_data_ptr->cpu_active_thread = first_thread;
		first_thread->machine.CpuDatap = cpu_data_ptr;
		first_thread->machine.pcpu_data_base_and_cpu_number =
		    ml_make_pcpu_base_and_cpu_number((char *)cpu_data_ptr - __PERCPU_ADDR(cpu_data),
		    cpu_data_ptr->cpu_number);

		configure_coresight_registers(cpu_data_ptr);

		flush_dcache((vm_offset_t)&CpuDataEntries[cpu], sizeof(cpu_data_entry_t), FALSE);
		flush_dcache((vm_offset_t)cpu_data_ptr, sizeof(cpu_data_t), FALSE);
#if CONFIG_SPTM
		/**
		 * On SPTM devices, CTRR is configured entirely by the SPTM. Due to this, this logic
		 * is no longer required in XNU.
		 */
#else
#if defined(KERNEL_INTEGRITY_CTRR)

		/* First CPU being started within a cluster goes ahead to lock CTRR for cluster;
		 * other CPUs block until cluster is locked. */
		lck_spin_lock(&ctrr_cpu_start_lck);
		switch (ctrr_cluster_locked[cpu_data_ptr->cpu_cluster_id]) {
		case CTRR_UNLOCKED:
			ctrr_cluster_locked[cpu_data_ptr->cpu_cluster_id] = CTRR_LOCKING;
			lck_spin_unlock(&ctrr_cpu_start_lck);
			break;
		case CTRR_LOCKING:

			lck_spin_sleep(&ctrr_cpu_start_lck, LCK_SLEEP_UNLOCK,
			    &ctrr_cluster_locked[cpu_data_ptr->cpu_cluster_id],
			    THREAD_UNINT | THREAD_WAIT_NOREPORT);

			assert(ctrr_cluster_locked[cpu_data_ptr->cpu_cluster_id] != CTRR_LOCKING);
			break;
		default:         // CTRR_LOCKED
			lck_spin_unlock(&ctrr_cpu_start_lck);
			break;
		}
#endif
#endif /* CONFIG_SPTM */

		PE_cpu_start_internal(cpu_data_ptr->cpu_id, (vm_offset_t)NULL, (vm_offset_t)NULL);

	}
}


void
cpu_timebase_init(boolean_t from_boot)
{
	cpu_data_t *cdp = getCpuDatap();
	uint64_t timebase_offset = 0;

	if (cdp->cpu_get_fiq_handler == NULL) {
		cdp->cpu_get_fiq_handler = rtclock_timebase_func.tbd_fiq_handler;
		cdp->cpu_get_decrementer_func = rtclock_timebase_func.tbd_get_decrementer;
		cdp->cpu_set_decrementer_func = rtclock_timebase_func.tbd_set_decrementer;
		cdp->cpu_tbd_hardware_addr = (void *)rtclock_timebase_addr;
		cdp->cpu_tbd_hardware_val = (void *)rtclock_timebase_val;
	}

	if (!from_boot && (cdp == &BootCpuData) && ml_is_quiescing()) {
		/*
		 * When we wake from sleep, we have no guarantee about the state
		 * of the hardware timebase.  It may have kept ticking across sleep, or
		 * it may have reset.
		 *
		 * To deal with this, we calculate an offset to the clock that will
		 * produce a timebase value wake_abstime at the point the boot
		 * CPU calls cpu_timebase_init on wake.
		 *
		 * This ensures that mach_absolute_time() stops ticking across sleep.
		 */
		rtclock_base_abstime = wake_abstime - ml_get_hwclock();
	} else if (from_boot) {
#if DEBUG || DEVELOPMENT
		if (PE_parse_boot_argn("timebase_offset", &timebase_offset, sizeof(timebase_offset))) {
			rtclock_base_abstime += timebase_offset;
		}
#endif
		/* On initial boot, initialize time_since_reset to CNTPCT_EL0. */
		ml_set_reset_time(ml_get_hwclock());
	}

	cdp->cpu_decrementer = 0x7FFFFFFFUL;
	cdp->cpu_timebase = timebase_offset;
	cdp->cpu_base_timebase = rtclock_base_abstime;
}

int
cpu_cluster_id(void)
{
	return getCpuDatap()->cpu_cluster_id;
}

__attribute__((noreturn))
void
ml_arm_sleep(void)
{
	cpu_data_t              *cpu_data_ptr = getCpuDatap();

	if (cpu_data_ptr == &BootCpuData) {
		cpu_data_t      *target_cdp;
		int             cpu;
		int             max_cpu;

		max_cpu = ml_get_max_cpu_number();
		for (cpu = 0; cpu <= max_cpu; cpu++) {
			target_cdp = (cpu_data_t *)CpuDataEntries[cpu].cpu_data_vaddr;

			if ((target_cdp == NULL) || (target_cdp == cpu_data_ptr)) {
				continue;
			}

			while (target_cdp->cpu_sleep_token != ARM_CPU_ON_SLEEP_PATH) {
				;
			}
		}

		/*
		 * Now that the other cores have entered the sleep path, set
		 * the abstime value we'll use when we resume.
		 */
		wake_abstime = ml_get_timebase();
		ml_set_reset_time(UINT64_MAX);
	} else {
		CleanPoU_Dcache();
	}

	cpu_data_ptr->cpu_sleep_token = ARM_CPU_ON_SLEEP_PATH;

	if (cpu_data_ptr == &BootCpuData) {
#if WITH_CLASSIC_S2R
		// Classic suspend to RAM writes the suspend signature into the
		// sleep token buffer so that iBoot knows that it's on the warm
		// boot (wake) path (as opposed to the cold boot path). Newer SoC
		// do not go through SecureROM/iBoot on the warm boot path. The
		// reconfig engine script brings the CPU out of reset at the kernel's
		// reset vector which points to the warm boot initialization code.
		if (sleepTokenBuffer != (vm_offset_t) NULL) {
			platform_cache_shutdown();
			bcopy((const void *)suspend_signature, (void *)sleepTokenBuffer, sizeof(SleepToken));
		} else {
			panic("No sleep token buffer");
		}
#endif

#if __ARM_GLOBAL_SLEEP_BIT__
		/* Allow other CPUs to go to sleep. */
		arm64_stall_sleep = FALSE;
		__builtin_arm_dmb(DMB_ISH);
#endif

		/* Architectural debug state: <rdar://problem/12390433>:
		 *      Grab debug lock EDLAR and clear bit 0 in EDPRCR,
		 *      tell debugger to not prevent power gating .
		 */
		if (cpu_data_ptr->coresight_base[CORESIGHT_ED]) {
			*(volatile uint32_t *)(cpu_data_ptr->coresight_base[CORESIGHT_ED] + ARM_DEBUG_OFFSET_DBGLAR) = ARM_DBG_LOCK_ACCESS_KEY;
			*(volatile uint32_t *)(cpu_data_ptr->coresight_base[CORESIGHT_ED] + ARM_DEBUG_OFFSET_DBGPRCR) = 0;
		}

#if HIBERNATION
		uint32_t mode = hibernate_write_image();
		if (mode == kIOHibernatePostWriteHalt) {
			HIBLOG("powering off after writing hibernation image\n");
			int halt_result = -1;
			if (PE_halt_restart) {
				/**
				 * Drain serial FIFOs now as the normal call further down won't
				 * be hit when the CPU halts here for hibernation. Here, it'll
				 * make sure the preceding HIBLOG is flushed as well.
				 */
				serial_go_to_sleep();
				halt_result = (*PE_halt_restart)(kPEHaltCPU);
			}
			panic("can't shutdown: PE_halt_restart returned %d", halt_result);
		}
#endif /* HIBERNATION */

		serial_go_to_sleep();

#if CONFIG_CPU_COUNTERS
		mt_sleep();
#endif /* CONFIG_CPU_COUNTERS */
		/* ARM64-specific preparation */
#if APPLEVIRTUALPLATFORM
		extern bool test_sleep_in_vm;
		if (test_sleep_in_vm) {
			/*
			 * Until sleep is supported on APPLEVIRTUALPLATFORM, use this
			 * trick for testing sleep - just jump straight to the CPU resume point.
			 */
			arm_init_cpu(cpu_data_ptr, 0);
		}
		arm64_prepare_for_sleep(true, cpu_data_ptr->cpu_number, ml_vtophys(reset_vector_vaddr));
#else /* APPLEVIRTUALPLATFORM */
		arm64_prepare_for_sleep(true);
#endif /* APPLEVIRTUALPLATFORM */
	} else {
#if __ARM_GLOBAL_SLEEP_BIT__
		/*
		 * With the exception of the CPU revisions listed above, our ARM64 CPUs have a
		 * global register to manage entering deep sleep, as opposed to a per-CPU
		 * register.  We cannot update this register until all CPUs are ready to enter
		 * deep sleep, because if a CPU executes WFI outside of the deep sleep context
		 * (by idling), it will hang (due to the side effects of enabling deep sleep),
		 * which can hang the sleep process or cause memory corruption on wake.
		 *
		 * To avoid these issues, we'll stall on this global value, which CPU0 will
		 * manage.
		 */
		while (arm64_stall_sleep) {
			__builtin_arm_wfe();
		}
#endif
		CleanPoU_DcacheRegion((vm_offset_t) cpu_data_ptr, sizeof(cpu_data_t));

		/* Architectural debug state: <rdar://problem/12390433>:
		 *      Grab debug lock EDLAR and clear bit 0 in EDPRCR,
		 *      tell debugger to not prevent power gating .
		 */
		if (cpu_data_ptr->coresight_base[CORESIGHT_ED]) {
			*(volatile uint32_t *)(cpu_data_ptr->coresight_base[CORESIGHT_ED] + ARM_DEBUG_OFFSET_DBGLAR) = ARM_DBG_LOCK_ACCESS_KEY;
			*(volatile uint32_t *)(cpu_data_ptr->coresight_base[CORESIGHT_ED] + ARM_DEBUG_OFFSET_DBGPRCR) = 0;
		}

		/* ARM64-specific preparation */
#if APPLEVIRTUALPLATFORM
		arm64_prepare_for_sleep(true, cpu_data_ptr->cpu_number, ml_vtophys(reset_vector_vaddr));
#else /* APPLEVIRTUALPLATFORM */
		arm64_prepare_for_sleep(true);
#endif /* APPLEVIRTUALPLATFORM */
	}
}

void
cpu_machine_idle_init(boolean_t from_boot)
{
#if !CONFIG_SPTM
	static vm_address_t     resume_idle_cpu_paddr = (vm_address_t)NULL;
#endif
	cpu_data_t              *cpu_data_ptr   = getCpuDatap();

	if (from_boot) {
		uint32_t        production = 1;
		DTEntry         entry;

		unsigned long   jtag = 0;

		if (PE_parse_boot_argn("jtag", &jtag, sizeof(jtag))) {
			if (jtag != 0) {
				idle_enable = FALSE;
			} else {
				idle_enable = TRUE;
			}
		} else {
			idle_enable = TRUE;
		}

#if DEVELOPMENT || DEBUG
		uint32_t wfe_mode = 0;
		if (PE_parse_boot_argn("wfe_mode", &wfe_mode, sizeof(wfe_mode))) {
			idle_proximate_timer_wfe = ((wfe_mode & 1) == 1);
			idle_proximate_io_wfe_masked = ((wfe_mode & 2) == 2);
			extern uint32_t idle_proximate_io_wfe_unmasked;
			idle_proximate_io_wfe_unmasked = ((wfe_mode & 4) == 4);
		}
#endif

		// bits 7..0 give the wfi type
		switch (wfi & 0xff) {
		case 0:
			// disable wfi
			wfi = 0;
			break;

#if DEVELOPMENT || DEBUG
		case 2:
			// wfi overhead simulation
			// 31..16 - wfi delay is us
			// 15..8  - flags
			// 7..0   - 2
			wfi = 2;
			wfi_flags = (wfi >> 8) & 0xFF;
			nanoseconds_to_absolutetime(((wfi >> 16) & 0xFFFF) * NSEC_PER_MSEC, &wfi_delay);
			break;
#endif /* DEVELOPMENT || DEBUG */

		case 1:
		default:
			// do nothing
			break;
		}
#if !CONFIG_SPTM
		ResetHandlerData.assist_reset_handler = 0;
		ResetHandlerData.cpu_data_entries = ml_static_vtop((vm_offset_t)CpuDataEntries);
#endif

#ifdef MONITOR
		monitor_call(MONITOR_SET_ENTRY, (uintptr_t)ml_static_vtop((vm_offset_t)&LowResetVectorBase), 0, 0);
#elif !defined(NO_MONITOR)
#error MONITOR undefined, WFI power gating may not operate correctly
#endif /* MONITOR */

		// Determine if we are on production or debug chip
		if (kSuccess == SecureDTLookupEntry(NULL, "/chosen", &entry)) {
			unsigned int    size;
			void const      *prop;

			if (kSuccess == SecureDTGetProperty(entry, "effective-production-status-ap", &prop, &size)) {
				if (size == 4) {
					bcopy(prop, &production, size);
				}
			}
		}
		if (!production) {
#if defined(APPLE_ARM64_ARCH_FAMILY)
			// Enable coresight debug registers on debug-fused chips
			coresight_debug_enabled = TRUE;
#endif
		}
#if !CONFIG_SPTM
		start_cpu_paddr = ml_static_vtop((vm_offset_t)&start_cpu);
		resume_idle_cpu_paddr = ml_static_vtop((vm_offset_t)&resume_idle_cpu);
#endif
	}

#if WITH_CLASSIC_S2R
	if (cpu_data_ptr == &BootCpuData) {
		static addr64_t SleepToken_low_paddr = (addr64_t)NULL;
		if (sleepTokenBuffer != (vm_offset_t) NULL) {
			SleepToken_low_paddr = ml_vtophys(sleepTokenBuffer);
		} else {
			panic("No sleep token buffer");
		}

		bcopy_phys((addr64_t)ml_static_vtop((vm_offset_t)running_signature),
		    SleepToken_low_paddr, sizeof(SleepToken));
		flush_dcache((vm_offset_t)SleepToken, sizeof(SleepToken), TRUE);
	}
	;
#endif
#if CONFIG_SPTM
	cpu_data_ptr->cpu_reset_handler = (uintptr_t) VM_KERNEL_STRIP_PTR(arm_init_idle_cpu);
#else
	cpu_data_ptr->cpu_reset_handler = resume_idle_cpu_paddr;
#endif
	clean_dcache((vm_offset_t)cpu_data_ptr, sizeof(cpu_data_t), FALSE);
}

_Atomic uint32_t cpu_idle_count = 0;

void
machine_track_platform_idle(boolean_t entry)
{
	if (entry) {
		os_atomic_inc(&cpu_idle_count, relaxed);
	} else {
		os_atomic_dec(&cpu_idle_count, relaxed);
	}
}

#if WITH_CLASSIC_S2R
void
sleep_token_buffer_init(void)
{
	cpu_data_t      *cpu_data_ptr = getCpuDatap();
	DTEntry         entry;
	size_t          size;
	void const * const *prop;

	if ((cpu_data_ptr == &BootCpuData) && (sleepTokenBuffer == (vm_offset_t) NULL)) {
		/* Find the stpage node in the device tree */
		if (kSuccess != SecureDTLookupEntry(0, "stram", &entry)) {
			return;
		}

		if (kSuccess != SecureDTGetProperty(entry, "reg", (const void **)&prop, (unsigned int *)&size)) {
			return;
		}

		/* Map the page into the kernel space */
		sleepTokenBuffer = ml_io_map(((vm_offset_t const *)prop)[0], ((vm_size_t const *)prop)[1]);
	}
}
#endif
