/*
 * Copyright (c) 2017-2019 Apple Inc. All rights reserved.
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
 *	File:	arm/cpu_common.c
 *
 *	cpu routines common to all supported arm variants
 */

#include <kern/machine.h>
#include <kern/cpu_number.h>
#include <kern/thread.h>
#include <kern/percpu.h>
#include <kern/timer_queue.h>
#include <kern/locks.h>
#include <kern/clock.h>
#include <arm/cpu_data.h>
#include <arm/cpuid.h>
#include <arm/caches_internal.h>
#include <arm/cpu_data_internal.h>
#include <arm/cpu_internal.h>
#include <arm/misc_protos.h>
#include <arm/machine_cpu.h>
#include <arm/rtclock.h>
#include <mach/processor_info.h>
#include <machine/atomic.h>
#include <machine/config.h>
#include <vm/vm_kern_xnu.h>
#include <vm/vm_map.h>
#include <pexpert/arm/protos.h>
#include <pexpert/device_tree.h>
#include <sys/kdebug.h>
#include <arm/machine_routines.h>
#include <arm64/proc_reg.h>
#include <libkern/OSAtomic.h>

SECURITY_READ_ONLY_LATE(struct percpu_base) percpu_base;
vm_address_t     percpu_base_cur;
cpu_data_t       PERCPU_DATA(cpu_data);
cpu_data_entry_t CpuDataEntries[MAX_CPUS];

static LCK_GRP_DECLARE(cpu_lck_grp, "cpu_lck_grp");
static LCK_RW_DECLARE(cpu_state_lock, &cpu_lck_grp);
static LCK_MTX_DECLARE(cpu_xcall_mtx, &cpu_lck_grp);

unsigned int    real_ncpus = 1;
boolean_t       idle_enable = FALSE;
uint64_t        wake_abstime = 0x0ULL;

extern uint64_t xcall_ack_timeout_abstime;

#if defined(HAS_IPI)
extern unsigned int gFastIPI;
#endif /* defined(HAS_IPI) */

cpu_data_t *
cpu_datap(int cpu)
{
	assert(cpu <= ml_get_max_cpu_number());
	return CpuDataEntries[cpu].cpu_data_vaddr;
}

kern_return_t
cpu_control(int slot_num,
    processor_info_t info,
    unsigned int count)
{
	printf("cpu_control(%d,%p,%d) not implemented\n",
	    slot_num, info, count);
	return KERN_FAILURE;
}

kern_return_t
cpu_info_count(processor_flavor_t flavor,
    unsigned int *count)
{
	switch (flavor) {
	case PROCESSOR_CPU_STAT:
		*count = PROCESSOR_CPU_STAT_COUNT;
		return KERN_SUCCESS;

	case PROCESSOR_CPU_STAT64:
		*count = PROCESSOR_CPU_STAT64_COUNT;
		return KERN_SUCCESS;

	default:
		*count = 0;
		return KERN_FAILURE;
	}
}

kern_return_t
cpu_info(processor_flavor_t flavor, int slot_num, processor_info_t info,
    unsigned int *count)
{
	cpu_data_t *cpu_data_ptr = CpuDataEntries[slot_num].cpu_data_vaddr;

	switch (flavor) {
	case PROCESSOR_CPU_STAT:
	{
		if (*count < PROCESSOR_CPU_STAT_COUNT) {
			return KERN_FAILURE;
		}

		processor_cpu_stat_t cpu_stat = (processor_cpu_stat_t)info;
		cpu_stat->irq_ex_cnt = (uint32_t)cpu_data_ptr->cpu_stat.irq_ex_cnt;
		cpu_stat->ipi_cnt = (uint32_t)cpu_data_ptr->cpu_stat.ipi_cnt;
		cpu_stat->timer_cnt = (uint32_t)cpu_data_ptr->cpu_stat.timer_cnt;
		cpu_stat->undef_ex_cnt = (uint32_t)cpu_data_ptr->cpu_stat.undef_ex_cnt;
		cpu_stat->unaligned_cnt = (uint32_t)cpu_data_ptr->cpu_stat.unaligned_cnt;
		cpu_stat->vfp_cnt = (uint32_t)cpu_data_ptr->cpu_stat.vfp_cnt;
		cpu_stat->vfp_shortv_cnt = 0;
		cpu_stat->data_ex_cnt = (uint32_t)cpu_data_ptr->cpu_stat.data_ex_cnt;
		cpu_stat->instr_ex_cnt = (uint32_t)cpu_data_ptr->cpu_stat.instr_ex_cnt;

		*count = PROCESSOR_CPU_STAT_COUNT;

		return KERN_SUCCESS;
	}

	case PROCESSOR_CPU_STAT64:
	{
		if (*count < PROCESSOR_CPU_STAT64_COUNT) {
			return KERN_FAILURE;
		}

		processor_cpu_stat64_t cpu_stat = (processor_cpu_stat64_t)info;
		cpu_stat->irq_ex_cnt = cpu_data_ptr->cpu_stat.irq_ex_cnt;
		cpu_stat->ipi_cnt = cpu_data_ptr->cpu_stat.ipi_cnt;
		cpu_stat->timer_cnt = cpu_data_ptr->cpu_stat.timer_cnt;
		cpu_stat->undef_ex_cnt = cpu_data_ptr->cpu_stat.undef_ex_cnt;
		cpu_stat->unaligned_cnt = cpu_data_ptr->cpu_stat.unaligned_cnt;
		cpu_stat->vfp_cnt = cpu_data_ptr->cpu_stat.vfp_cnt;
		cpu_stat->vfp_shortv_cnt = 0;
		cpu_stat->data_ex_cnt = cpu_data_ptr->cpu_stat.data_ex_cnt;
		cpu_stat->instr_ex_cnt = cpu_data_ptr->cpu_stat.instr_ex_cnt;
#if CONFIG_CPU_COUNTERS
		cpu_stat->pmi_cnt = cpu_data_ptr->cpu_monotonic.mtc_npmis;
#endif /* CONFIG_CPU_COUNTERS */

		*count = PROCESSOR_CPU_STAT64_COUNT;

		return KERN_SUCCESS;
	}

	default:
		return KERN_FAILURE;
	}
}

/*
 *	Routine:	cpu_doshutdown
 *	Function:
 */
void
cpu_doshutdown(void (*doshutdown)(processor_t),
    processor_t processor)
{
	doshutdown(processor);
}

/*
 *	Routine:	cpu_idle_tickle
 *
 */
void
cpu_idle_tickle(void)
{
	boolean_t       intr;
	cpu_data_t      *cpu_data_ptr;
	uint64_t        new_idle_timeout_ticks = 0x0ULL;

	intr = ml_set_interrupts_enabled(FALSE);
	cpu_data_ptr = getCpuDatap();

	if (cpu_data_ptr->idle_timer_notify != NULL) {
		cpu_data_ptr->idle_timer_notify(cpu_data_ptr->idle_timer_refcon, &new_idle_timeout_ticks);
		if (new_idle_timeout_ticks != 0x0ULL) {
			/* if a new idle timeout was requested set the new idle timer deadline */
			clock_absolutetime_interval_to_deadline(new_idle_timeout_ticks, &cpu_data_ptr->idle_timer_deadline);
		} else {
			/* turn off the idle timer */
			cpu_data_ptr->idle_timer_deadline = 0x0ULL;
		}
		timer_resync_deadlines();
	}
	(void) ml_set_interrupts_enabled(intr);
}

static void
cpu_handle_xcall(cpu_data_t *cpu_data_ptr)
{
	broadcastFunc   xfunc;
	void            *xparam;

	os_atomic_thread_fence(acquire);
	/* Come back around if cpu_signal_internal is running on another CPU and has just
	* added SIGPxcall to the pending mask, but hasn't yet assigned the call params.*/
	if (cpu_data_ptr->cpu_xcall_p0 != NULL && cpu_data_ptr->cpu_xcall_p1 != NULL) {
		xfunc = ptrauth_auth_function(cpu_data_ptr->cpu_xcall_p0, ptrauth_key_function_pointer, cpu_data_ptr);
		INTERRUPT_MASKED_DEBUG_START(xfunc, DBG_INTR_TYPE_IPI);
		xparam = cpu_data_ptr->cpu_xcall_p1;
		cpu_data_ptr->cpu_xcall_p0 = NULL;
		cpu_data_ptr->cpu_xcall_p1 = NULL;
		os_atomic_thread_fence(acq_rel);
		os_atomic_andnot(&cpu_data_ptr->cpu_signal, SIGPxcall, relaxed);
		xfunc(xparam);
		INTERRUPT_MASKED_DEBUG_END();
	}
	if (cpu_data_ptr->cpu_imm_xcall_p0 != NULL && cpu_data_ptr->cpu_imm_xcall_p1 != NULL) {
		xfunc = ptrauth_auth_function(cpu_data_ptr->cpu_imm_xcall_p0, ptrauth_key_function_pointer, cpu_data_ptr);
		INTERRUPT_MASKED_DEBUG_START(xfunc, DBG_INTR_TYPE_IPI);
		xparam = cpu_data_ptr->cpu_imm_xcall_p1;
		cpu_data_ptr->cpu_imm_xcall_p0 = NULL;
		cpu_data_ptr->cpu_imm_xcall_p1 = NULL;
		os_atomic_thread_fence(acq_rel);
		os_atomic_andnot(&cpu_data_ptr->cpu_signal, SIGPxcallImm, relaxed);
		xfunc(xparam);
		INTERRUPT_MASKED_DEBUG_END();
	}
}

static unsigned int
cpu_broadcast_xcall_internal(unsigned int signal,
    uint32_t *synch,
    boolean_t self_xcall,
    broadcastFunc func,
    void *parm)
{
	boolean_t       intr;
	cpu_data_t      *cpu_data_ptr;
	cpu_data_t      *target_cpu_datap;
	unsigned int    failsig;
	int             cpu;
	int             max_cpu = ml_get_max_cpu_number() + 1;

	//yes, param ALSO cannot be NULL
	assert(synch);
	assert(func);
	assert(parm);

	lck_mtx_lock(&cpu_xcall_mtx);
	intr = ml_set_interrupts_enabled(FALSE);
	cpu_data_ptr = getCpuDatap();

	failsig = 0;

	*synch = max_cpu;
	assert_wait((event_t)synch, THREAD_UNINT);

	for (cpu = 0; cpu < max_cpu; cpu++) {
		target_cpu_datap = (cpu_data_t *)CpuDataEntries[cpu].cpu_data_vaddr;

		if (target_cpu_datap == cpu_data_ptr) {
			continue;
		}

		if ((target_cpu_datap == NULL) ||
		    KERN_SUCCESS != cpu_signal(target_cpu_datap, signal, ptrauth_nop_cast(void*, ptrauth_auth_and_resign(func, ptrauth_key_function_pointer, ptrauth_type_discriminator(broadcastFunc), ptrauth_key_function_pointer, target_cpu_datap)), parm)) {
			failsig++;
		}
	}

	if (self_xcall) {
		func(parm);
	}

	(void) ml_set_interrupts_enabled(intr);

	if (os_atomic_sub(synch, (!self_xcall) ? failsig + 1 : failsig, relaxed) == 0) {
		clear_wait(current_thread(), THREAD_AWAKENED);
	} else {
		thread_block(THREAD_CONTINUE_NULL);
	}
	lck_mtx_unlock(&cpu_xcall_mtx);

	if (!self_xcall) {
		return max_cpu - failsig - 1;
	} else {
		return max_cpu - failsig;
	}
}

unsigned int
cpu_broadcast_xcall(uint32_t *synch,
    boolean_t self_xcall,
    broadcastFunc func,
    void *parm)
{
	return cpu_broadcast_xcall_internal(SIGPxcall, synch, self_xcall, func, parm);
}

struct cpu_broadcast_xcall_simple_data {
	broadcastFunc func;
	void* parm;
	uint32_t sync;
};

static void
cpu_broadcast_xcall_simple_cbk(void *parm)
{
	struct cpu_broadcast_xcall_simple_data *data = (struct cpu_broadcast_xcall_simple_data*)parm;

	data->func(data->parm);

	if (os_atomic_dec(&data->sync, relaxed) == 0) {
		thread_wakeup((event_t)&data->sync);
	}
}

static unsigned int
cpu_xcall_simple(boolean_t self_xcall,
    broadcastFunc func,
    void *parm,
    bool immediate)
{
	struct cpu_broadcast_xcall_simple_data data = {};

	data.func = func;
	data.parm = parm;

	return cpu_broadcast_xcall_internal(immediate ? SIGPxcallImm : SIGPxcall, &data.sync, self_xcall, cpu_broadcast_xcall_simple_cbk, &data);
}

unsigned int
cpu_broadcast_xcall_simple(boolean_t self_xcall,
    broadcastFunc func,
    void *parm)
{
	return cpu_xcall_simple(self_xcall, func, parm, false);
}

static kern_return_t
cpu_xcall_internal(unsigned int signal, int cpu_number, broadcastFunc func, void *param)
{
	cpu_data_t      *target_cpu_datap;

	if ((cpu_number < 0) || (cpu_number > ml_get_max_cpu_number())) {
		panic("cpu_xcall_internal: invalid cpu_number %d", cpu_number);
	}

	if (func == NULL || param == NULL) {
		// cpu_handle_xcall uses non-NULL-ness to tell when the value is ready
		panic("cpu_xcall_internal: cannot have null func/param: %p %p", func, param);
	}

	target_cpu_datap = (cpu_data_t*)CpuDataEntries[cpu_number].cpu_data_vaddr;
	if (target_cpu_datap == NULL) {
		panic("cpu_xcall_internal: cpu %d not initialized", cpu_number);
	}

	return cpu_signal(target_cpu_datap, signal, ptrauth_nop_cast(void*, ptrauth_auth_and_resign(func, ptrauth_key_function_pointer, ptrauth_type_discriminator(broadcastFunc), ptrauth_key_function_pointer, target_cpu_datap)), param);
}

kern_return_t
cpu_xcall(int cpu_number, broadcastFunc func, void *param)
{
	return cpu_xcall_internal(SIGPxcall, cpu_number, func, param);
}

kern_return_t
cpu_immediate_xcall(int cpu_number, broadcastFunc func, void *param)
{
	return cpu_xcall_internal(SIGPxcallImm, cpu_number, func, param);
}

static kern_return_t
cpu_signal_internal(cpu_data_t *target_proc,
    cpu_signal_t signal,
    void *p0,
    void *p1,
    boolean_t defer)
{
	cpu_signal_t    current_signals;
	bool            swap_success;
	boolean_t       interruptible = ml_set_interrupts_enabled(FALSE);
	cpu_data_t      *current_proc = getCpuDatap();

	/* We'll mandate that only IPIs meant to kick a core out of idle may ever be deferred. */
	if (defer) {
		assert(signal == SIGPnop);
	}

	if ((signal == SIGPxcall) || (signal == SIGPxcallImm)) {
		uint64_t start_mabs_time, max_mabs_time, current_mabs_time;
		current_mabs_time = start_mabs_time = mach_absolute_time();
		max_mabs_time = xcall_ack_timeout_abstime + current_mabs_time;
		assert(max_mabs_time > current_mabs_time);

		do {
			current_signals = target_proc->cpu_signal;
			if ((current_signals & SIGPdisabled) == SIGPdisabled) {
				ml_set_interrupts_enabled(interruptible);
				return KERN_FAILURE;
			}
			swap_success = os_atomic_cmpxchg(&target_proc->cpu_signal, current_signals & (~signal),
			    current_signals | signal, release);

			if (!swap_success && (signal == SIGPxcallImm) && (target_proc->cpu_signal & SIGPxcallImm)) {
				ml_set_interrupts_enabled(interruptible);
				return KERN_ALREADY_WAITING;
			}

			/* Drain pending xcalls on this cpu; the CPU we're trying to xcall may in turn
			 * be trying to xcall us.  Since we have interrupts disabled that can deadlock,
			 * so break the deadlock by draining pending xcalls. */
			if (!swap_success && (current_proc->cpu_signal & signal)) {
				cpu_handle_xcall(current_proc);
			}
		} while (!swap_success && ((current_mabs_time = mach_absolute_time()) < max_mabs_time));

		/*
		 * If we time out while waiting for the target CPU to respond, it's possible that no
		 * other CPU is available to handle the watchdog interrupt that would eventually trigger
		 * a panic. To prevent this from happening, we just panic here to flag this condition.
		 */
		if (__improbable(current_mabs_time >= max_mabs_time)) {
			uint64_t end_time_ns, xcall_ack_timeout_ns;
			absolutetime_to_nanoseconds(current_mabs_time - start_mabs_time, &end_time_ns);
			absolutetime_to_nanoseconds(xcall_ack_timeout_abstime, &xcall_ack_timeout_ns);
			panic("CPU%u has failed to respond to cross-call after %llu nanoseconds (timeout = %llu ns)",
			    target_proc->cpu_number, end_time_ns, xcall_ack_timeout_ns);
		}

		if (signal == SIGPxcallImm) {
			target_proc->cpu_imm_xcall_p0 = p0;
			target_proc->cpu_imm_xcall_p1 = p1;
		} else {
			target_proc->cpu_xcall_p0 = p0;
			target_proc->cpu_xcall_p1 = p1;
		}
	} else {
		do {
			current_signals = target_proc->cpu_signal;
			if ((current_signals & SIGPdisabled) == SIGPdisabled) {
				if (current_proc == target_proc) {
					panic("cpu_signal of self while signals are disabled");
				}
				ml_set_interrupts_enabled(interruptible);
				return KERN_FAILURE;
			}

			swap_success = os_atomic_cmpxchg(&target_proc->cpu_signal, current_signals,
			    current_signals | signal, release);
		} while (!swap_success);
	}

	/*
	 * DSB is needed here to ensure prior stores to the pending signal mask and xcall params
	 * will be visible by the time the other cores are signaled.  The IPI mechanism on any
	 * given platform will very likely use either an MSR or a non-coherent store that would
	 * not be ordered by a simple DMB.
	 */
	__builtin_arm_dsb(DSB_ISHST);

	if (!(target_proc->cpu_signal & SIGPdisabled)) {
		/* Make sure cpu_phys_id is actually initialized */
		assert3u(os_atomic_load(&target_proc->cpu_flags, relaxed) & (InitState | StartedState),
		    ==, InitState | StartedState);

		if (defer) {
#if defined(HAS_IPI)
			if (gFastIPI) {
				ml_cpu_signal_deferred(target_proc->cpu_phys_id);
			} else {
				PE_cpu_signal_deferred(getCpuDatap()->cpu_id, target_proc->cpu_id);
			}
#else
			PE_cpu_signal_deferred(getCpuDatap()->cpu_id, target_proc->cpu_id);
#endif /* defined(HAS_IPI) */
		} else {
#if defined(HAS_IPI)
			if (gFastIPI) {
				ml_cpu_signal(target_proc->cpu_phys_id);
			} else {
				PE_cpu_signal(getCpuDatap()->cpu_id, target_proc->cpu_id);
			}
#else
			PE_cpu_signal(getCpuDatap()->cpu_id, target_proc->cpu_id);
#endif /* defined(HAS_IPI) */
		}
	}

	ml_set_interrupts_enabled(interruptible);
	return KERN_SUCCESS;
}

kern_return_t
cpu_signal(cpu_data_t *target_proc,
    cpu_signal_t signal,
    void *p0,
    void *p1)
{
	return cpu_signal_internal(target_proc, signal, p0, p1, FALSE);
}

kern_return_t
cpu_signal_deferred(cpu_data_t *target_proc)
{
	return cpu_signal_internal(target_proc, SIGPnop, NULL, NULL, TRUE);
}

void
cpu_signal_cancel(cpu_data_t *target_proc)
{
	/* TODO: Should we care about the state of a core as far as squashing deferred IPIs goes? */
	if (!(target_proc->cpu_signal & SIGPdisabled)) {
#if defined(HAS_IPI)
		if (gFastIPI) {
			ml_cpu_signal_retract(target_proc->cpu_phys_id);
		} else {
			PE_cpu_signal_cancel(getCpuDatap()->cpu_id, target_proc->cpu_id);
		}
#else
		PE_cpu_signal_cancel(getCpuDatap()->cpu_id, target_proc->cpu_id);
#endif /* defined(HAS_IPI) */
	}
}

void
cpu_signal_handler(void)
{
	cpu_signal_handler_internal(FALSE);
}

bool
cpu_has_SIGPdebug_pending(void)
{
	cpu_data_t *cpu_data_ptr = getCpuDatap();

	return cpu_data_ptr->cpu_signal & SIGPdebug;
}

void
cpu_signal_handler_internal(boolean_t disable_signal)
{
	cpu_data_t     *cpu_data_ptr = getCpuDatap();

	cpu_data_ptr->cpu_stat.ipi_cnt++;
	cpu_data_ptr->cpu_stat.ipi_cnt_wake++;
	SCHED_STATS_INC(ipi_count);

	/*
	 * Employ an acquire barrier when loading cpu_signal to ensure that
	 * loads within individual signal handlers won't be speculated ahead
	 * of the load of cpu_signal.  This pairs with the release barrier
	 * in cpu_signal_internal() to ensure that once a flag has been set in
	 * the cpu_signal mask, any prerequisite setup is also visible to signal
	 * handlers.
	 */
	cpu_signal_t cpu_signal = os_atomic_or(&cpu_data_ptr->cpu_signal, 0, acquire);

	if ((!(cpu_signal & SIGPdisabled)) && (disable_signal == TRUE)) {
		cpu_signal = os_atomic_or(&cpu_data_ptr->cpu_signal, SIGPdisabled, acq_rel);
	} else if ((cpu_signal & SIGPdisabled) && (disable_signal == FALSE)) {
		/* We must not clear SIGPdisabled unless the CPU is properly started */
		assert3u(os_atomic_load(&cpu_data_ptr->cpu_flags, relaxed) & (InitState | StartedState),
		    ==, InitState | StartedState);
		cpu_signal = os_atomic_andnot(&cpu_data_ptr->cpu_signal, SIGPdisabled, acq_rel);
	}

	while (cpu_signal & ~SIGPdisabled) {
		if (cpu_signal & SIGPdebug) {
			os_atomic_andnot(&cpu_data_ptr->cpu_signal, SIGPdebug, acquire);
			INTERRUPT_MASKED_DEBUG_START(DebuggerXCall, DBG_INTR_TYPE_IPI);
			DebuggerXCall(cpu_data_ptr->cpu_int_state);
			INTERRUPT_MASKED_DEBUG_END();
		}
#if KPERF
		if (cpu_signal & SIGPkppet) {
			os_atomic_andnot(&cpu_data_ptr->cpu_signal, SIGPkppet, acquire);
			extern void kperf_signal_handler(void);
			INTERRUPT_MASKED_DEBUG_START(kperf_signal_handler, DBG_INTR_TYPE_IPI);
			kperf_signal_handler();
			INTERRUPT_MASKED_DEBUG_END();
		}
#endif /* KPERF */
		if (cpu_signal & (SIGPxcall | SIGPxcallImm)) {
			cpu_handle_xcall(cpu_data_ptr);
		}
		if (cpu_signal & SIGPast) {
			os_atomic_andnot(&cpu_data_ptr->cpu_signal, SIGPast, acquire);
			INTERRUPT_MASKED_DEBUG_START(ast_check, DBG_INTR_TYPE_IPI);
			ast_check(current_processor());
			INTERRUPT_MASKED_DEBUG_END();
		}
		if (cpu_signal & SIGPTimerLocal) {
			os_atomic_andnot(&cpu_data_ptr->cpu_signal, SIGPTimerLocal, acquire);
			INTERRUPT_MASKED_DEBUG_START(timer_queue_expire_local, DBG_INTR_TYPE_IPI);
			timer_queue_expire_local(current_processor());
			INTERRUPT_MASKED_DEBUG_END();
		}

		cpu_signal = os_atomic_or(&cpu_data_ptr->cpu_signal, 0, acquire);
	}
}

void
cpu_exit_wait(int cpu_id)
{
#if !APPLEVIRTUALPLATFORM /* AVP doesn't provide an equivalent poll-for-powerdown operation */
#if USE_APPLEARMSMP
	if (!ml_is_quiescing()) {
		// For runtime disable (non S2R) the CPU will shut down immediately.
		ml_topology_cpu_t *cpu = &ml_get_topology_info()->cpus[cpu_id];
		assert(cpu && cpu->cpu_IMPL_regs);
		volatile uint64_t *cpu_sts = (void *)(cpu->cpu_IMPL_regs + CPU_PIO_CPU_STS_OFFSET);

		// Poll the "CPU running state" field until it is 0 (off)
		// This loop typically finishes in about 600ns.  Sometimes it takes as long as 10us.
		// If it takes longer than 10s, assume something went horribly wrong and panic.
		uint64_t start = mach_absolute_time(), interval;
		nanoseconds_to_absolutetime(10 * NSEC_PER_SEC, &interval);

		while ((*cpu_sts & CPU_PIO_CPU_STS_cpuRunSt_mask) != 0x00) {
			__builtin_arm_dsb(DSB_ISH);
			if (mach_absolute_time() > start + interval) {
#if NO_CPU_OVRD
				// On platforms where CPU_OVRD is unavailable, a core can get stuck
				// in a loop where it tries to enter WFI but is constantly woken up
				// by an IRQ or FIQ.  This condition persists until the cluster-wide
				// deep sleep bits are set.
				//
				// Making this a fatal condition would be a poor UX, but it's good to
				// print a warning so we know how often it happens.
				kprintf("CPU%d failed to shut down\n", cpu_id);
#else
				panic("CPU%d failed to shut down", cpu_id);
#endif
			}
		}
	}
#endif /* USE_APPLEARMSMP */
#endif /* !APPLEVIRTUALPLATFORM */

	if (cpu_id != master_cpu || (support_bootcpu_shutdown && !ml_is_quiescing())) {
		// For S2R, ml_arm_sleep() will do some extra polling after setting ARM_CPU_ON_SLEEP_PATH.
		cpu_data_t      *cpu_data_ptr;

		uint64_t start = mach_absolute_time(), interval;
		nanoseconds_to_absolutetime(10 * NSEC_PER_SEC, &interval);

		cpu_data_ptr = CpuDataEntries[cpu_id].cpu_data_vaddr;
		while (!((*(volatile unsigned int*)&cpu_data_ptr->cpu_sleep_token) == ARM_CPU_ON_SLEEP_PATH)) {
			if (mach_absolute_time() > start + interval) {
				panic("CPU %d failed to reach ARM_CPU_ON_SLEEP_PATH: %d", cpu_id, cpu_data_ptr->cpu_sleep_token);
			}
		}
	}
}

void
cpu_machine_init(void)
{
	cpu_data_t *cpu_data_ptr = getCpuDatap();

	if (cpu_data_ptr->cpu_cache_dispatch != NULL) {
		platform_cache_init();
	}

	bool started = os_atomic_or_orig(&cpu_data_ptr->cpu_flags, StartedState, relaxed) & StartedState;

	/* Note: this calls IOCPURunPlatformActiveActions when resuming on boot cpu */
	PE_cpu_machine_init(cpu_data_ptr->cpu_id, !started);

	ml_init_interrupt();
}

processor_t
current_processor(void)
{
	return PERCPU_GET(processor);
}

processor_t
cpu_to_processor(int cpu)
{
	cpu_data_t *cpu_data = cpu_datap(cpu);
	if (cpu_data != NULL) {
		return PERCPU_GET_RELATIVE(processor, cpu_data, cpu_data);
	} else {
		return NULL;
	}
}

cpu_data_t *
processor_to_cpu_datap(processor_t processor)
{
	assert(processor->cpu_id <= ml_get_max_cpu_number());
	assert(CpuDataEntries[processor->cpu_id].cpu_data_vaddr != NULL);

	return PERCPU_GET_RELATIVE(cpu_data, processor, processor);
}

__startup_func
static void
cpu_data_startup_init(void)
{
	vm_size_t size = percpu_section_size() * (ml_get_cpu_count() - 1);

	percpu_base.size = percpu_section_size();
	if (ml_get_cpu_count() == 1) {
		percpu_base.start = VM_MAX_KERNEL_ADDRESS;
		return;
	}

	/*
	 * The memory needs to be physically contiguous because it contains
	 * cpu_data_t structures sometimes accessed during reset
	 * with the MMU off.
	 *
	 * kmem_alloc_contig() can't be used early, at the time STARTUP_SUB_PERCPU
	 * normally runs, so we instead steal the memory for the PERCPU subsystem
	 * even earlier.
	 */
	percpu_base.start  = (vm_offset_t)pmap_steal_memory(size, PAGE_SIZE);
	bzero((void *)percpu_base.start, size);

	percpu_base.start -= percpu_section_start();
	percpu_base.end    = percpu_base.start + size - 1;
	percpu_base_cur    = percpu_base.start;
}
STARTUP(PMAP_STEAL, STARTUP_RANK_FIRST, cpu_data_startup_init);

cpu_data_t *
cpu_data_alloc(boolean_t is_boot_cpu)
{
	cpu_data_t   *cpu_data_ptr = NULL;
	vm_address_t  base;

	if (is_boot_cpu) {
		cpu_data_ptr = PERCPU_GET_MASTER(cpu_data);
	} else {
		base = os_atomic_add_orig(&percpu_base_cur,
		    percpu_section_size(), relaxed);

		cpu_data_ptr = PERCPU_GET_WITH_BASE(base, cpu_data);
		cpu_stack_alloc(cpu_data_ptr);
	}

	return cpu_data_ptr;
}

ast_t *
ast_pending(void)
{
	return &getCpuDatap()->cpu_pending_ast;
}

cpu_type_t
slot_type(int slot_num)
{
	return cpu_datap(slot_num)->cpu_type;
}

cpu_subtype_t
slot_subtype(int slot_num)
{
	return cpu_datap(slot_num)->cpu_subtype;
}

cpu_threadtype_t
slot_threadtype(int slot_num)
{
	return cpu_datap(slot_num)->cpu_threadtype;
}

cpu_type_t
cpu_type(void)
{
	return getCpuDatap()->cpu_type;
}

cpu_subtype_t
cpu_subtype(void)
{
	return getCpuDatap()->cpu_subtype;
}

cpu_threadtype_t
cpu_threadtype(void)
{
	return getCpuDatap()->cpu_threadtype;
}

int
cpu_number(void)
{
	return current_thread()->machine.cpu_number;
}

vm_offset_t
current_percpu_base(void)
{
	long base = current_thread()->machine.pcpu_data_base_and_cpu_number;

	return (vm_offset_t)(base >> 16);
}

vm_offset_t
other_percpu_base(int cpu)
{
	return (char *)cpu_datap(cpu) - __PERCPU_ADDR(cpu_data);
}

uint64_t
ml_get_wake_timebase(void)
{
	return wake_abstime;
}

/*
 * Called while running on a specific CPU to wait for it to handle the
 * self-IPI that clears SIGPdisabled.
 */
void
ml_wait_for_cpu_signal_to_enable(void)
{
	assert(ml_get_interrupts_enabled());
	cpu_data_t *cpu_data_ptr = getCpuDatap();

	hw_wait_while_equals32(__DEVOLATILE(uint32_t*, &cpu_data_ptr->cpu_signal), SIGPdisabled);
}

void
assert_ml_cpu_signal_is_enabled(bool enabled)
{
	if (enabled) {
		assert((getCpuDatap()->cpu_signal & SIGPdisabled) == 0);
	} else {
		assert3u(getCpuDatap()->cpu_signal, ==, SIGPdisabled);
	}
}

bool
ml_cpu_can_exit(__unused int cpu_id)
{
#if USE_APPLEARMSMP
	/*
	 * Cyprus and newer chips can disable individual non-boot CPUs. The
	 * implementation polls cpuX_IMPL_CPU_STS, which differs on older chips.
	 * Until the feature is known to be stable, guard it with a boot-arg.
	 */

	bool cpu_supported = true;

	if (cpu_id == master_cpu && !support_bootcpu_shutdown) {
		cpu_supported = false;
	}

	if (enable_processor_exit) {
		return cpu_supported;
	}

#if HAS_CLUSTER
	if (ml_get_topology_info()->cluster_power_down) {
		return cpu_supported;
	}
#endif /* HAS_CLUSTER */

#endif /* USE_APPLEARMSMP */

	return false;
}

#ifdef USE_APPLEARMSMP

void
ml_cpu_begin_state_transition(int cpu_id)
{
	lck_rw_lock_exclusive(&cpu_state_lock);
	CpuDataEntries[cpu_id].cpu_data_vaddr->in_state_transition = true;
	lck_rw_unlock_exclusive(&cpu_state_lock);
}

void
ml_cpu_end_state_transition(int cpu_id)
{
	lck_rw_lock_exclusive(&cpu_state_lock);
	CpuDataEntries[cpu_id].cpu_data_vaddr->in_state_transition = false;
	lck_rw_unlock_exclusive(&cpu_state_lock);
}

void
ml_cpu_begin_loop(void)
{
	lck_rw_lock_shared(&cpu_state_lock);
}

void
ml_cpu_end_loop(void)
{
	lck_rw_unlock_shared(&cpu_state_lock);
}

void
ml_cpu_power_enable(int cpu_id)
{
	PE_cpu_power_enable(cpu_id);
}

void
ml_cpu_power_disable(int cpu_id)
{
	PE_cpu_power_disable(cpu_id);
}

#else /* USE_APPLEARMSMP */

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

void
ml_cpu_power_enable(__unused int cpu_id)
{
}

void
ml_cpu_power_disable(__unused int cpu_id)
{
}

#endif /* USE_APPLEARMSMP */
