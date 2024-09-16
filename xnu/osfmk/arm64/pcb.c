/*
 * Copyright (c) 2007-2023 Apple Inc. All rights reserved.
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

#include <debug.h>

#include <types.h>

#include <mach/mach_types.h>
#include <mach/thread_status.h>
#include <mach/vm_types.h>

#include <kern/kern_types.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <kern/misc_protos.h>
#include <kern/mach_param.h>
#include <kern/spl.h>
#include <kern/machine.h>
#include <kern/kpc.h>
#include <kern/monotonic.h>

#include <machine/atomic.h>
#include <arm64/proc_reg.h>
#include <arm64/machine_machdep.h>
#include <arm/cpu_data_internal.h>
#include <arm/machdep_call.h>
#include <arm/misc_protos.h>
#include <arm/cpuid.h>
#include <arm/cpu_capabilities_public.h>

#include <vm/vm_map_xnu.h>
#include <vm/vm_protos.h>

#include <sys/kdebug.h>


#include <san/kcov_stksz.h>

#include <IOKit/IOBSD.h>

#include <pexpert/arm64/apple_arm64_cpu.h>
#include <pexpert/pexpert.h>

// fixme: rdar://114299113 tracks resolving the supportlib issue with hwtrace features

extern int debug_task;

/* zone for debug_state area */
ZONE_DEFINE_TYPE(ads_zone, "arm debug state", arm_debug_state_t, ZC_NONE);
ZONE_DEFINE_TYPE(user_ss_zone, "user save state", arm_context_t, ZC_NONE);

#if HAS_ARM_FEAT_SME
static SECURITY_READ_ONLY_LATE(uint16_t) sme_svl_b;
/* zone for arm_sme_saved_state_t allocations */
static SECURITY_READ_ONLY_LATE(zone_t) sme_ss_zone;
#endif

#if HAVE_MACHINE_THREAD_MATRIX_STATE
struct arm_matrix_cpu_state {
#if HAS_ARM_FEAT_SME
	bool have_sme;
	bool za_is_enabled;
#endif
};

static void
machine_get_matrix_cpu_state(struct arm_matrix_cpu_state *cpu_state)
{
#if HAS_ARM_FEAT_SME
	cpu_state->have_sme = arm_sme_version() > 0;
	if (cpu_state->have_sme) {
		cpu_state->za_is_enabled = !!(__builtin_arm_rsr64("SVCR") & SVCR_ZA);
	} else {
		cpu_state->za_is_enabled = false;
	}
#endif /* HAS_ARM_FEAT_SME */
}
#endif /* HAVE_MACHINE_THREAD_MATRIX_STATE */

/*
 * Routine: consider_machine_collect
 *
 */
void
consider_machine_collect(void)
{
	pmap_gc();
}

/*
 * Routine: consider_machine_adjust
 *
 */
void
consider_machine_adjust(void)
{
}



#if HAS_ARM_FEAT_SME
static inline bool
machine_thread_has_valid_za(const arm_sme_saved_state_t *_Nullable sme_ss)
{
	return sme_ss && (sme_ss->svcr & SVCR_ZA);
}

arm_sme_saved_state_t *
machine_thread_get_sme_state(thread_t thread)
{
	arm_state_hdr_t *hdr = thread->machine.umatrix_hdr;
	if (hdr) {
		assert(hdr->flavor == ARM_SME_SAVED_STATE);
		return thread->machine.usme;
	}

	return NULL;
}

static void
machine_save_sme_context(thread_t old, arm_sme_saved_state_t *old_sme_ss, const struct arm_matrix_cpu_state *cpu_state)
{
	/*
	 * Note: we're deliberately not saving old_sme_ss->svcr, since it
	 * already happened on kernel entry.  Likewise we're not restoring the
	 * SM bit from new_sme_ss->svcr, since we don't want streaming SVE mode
	 * active while we're in kernel space; we'll put it back on kernel exit.
	 */

	old->machine.tpidr2_el0 = __builtin_arm_rsr64("TPIDR2_EL0");


	if (cpu_state->za_is_enabled) {
		arm_save_sme_za_zt0(&old_sme_ss->context, old_sme_ss->svl_b);
	}
}

static void
machine_restore_sme_context(thread_t new, const arm_sme_saved_state_t *new_sme_ss, const struct arm_matrix_cpu_state *cpu_state)
{
	__builtin_arm_wsr64("TPIDR2_EL0", new->machine.tpidr2_el0);

	if (new_sme_ss) {
		if (machine_thread_has_valid_za(new_sme_ss)) {
			if (!cpu_state->za_is_enabled) {
				asm volatile ("smstart za");
			}
			arm_load_sme_za_zt0(&new_sme_ss->context, new_sme_ss->svl_b);
		} else if (cpu_state->za_is_enabled) {
			asm volatile ("smstop za");
		}

		arm_sme_trap_at_el0(false);
	}
}

static void
machine_disable_sme_context(const struct arm_matrix_cpu_state *cpu_state)
{
	if (cpu_state->za_is_enabled) {
		asm volatile ("smstop za");
	}

	arm_sme_trap_at_el0(true);
}
#endif /* HAS_ARM_FEAT_SME */


#if HAVE_MACHINE_THREAD_MATRIX_STATE
static void
machine_switch_matrix_context(thread_t old, thread_t new)
{
	struct arm_matrix_cpu_state cpu_state;
	machine_get_matrix_cpu_state(&cpu_state);


#if HAS_ARM_FEAT_SME
	arm_sme_saved_state_t *old_sme_ss = machine_thread_get_sme_state(old);
	const arm_sme_saved_state_t *new_sme_ss = machine_thread_get_sme_state(new);

	if (cpu_state.have_sme) {
		machine_save_sme_context(old, old_sme_ss, &cpu_state);
	}
#endif /* HAS_ARM_FEAT_SME */


#if HAS_ARM_FEAT_SME
	if (cpu_state.have_sme && !new_sme_ss) {
		machine_disable_sme_context(&cpu_state);
	}
#endif /* HAS_ARM_FEAT_SME */


#if HAS_ARM_FEAT_SME
	if (cpu_state.have_sme) {
		machine_restore_sme_context(new, new_sme_ss, &cpu_state);
	}
#endif /* HAS_ARM_FEAT_SME */


}


#endif /* HAVE_MACHINE_THREAD_MATRIX_STATE */




static inline void
machine_thread_switch_cpu_data(thread_t old, thread_t new)
{
	/*
	 * We build with -fno-strict-aliasing, so the load through temporaries
	 * is required so that this generates a single load / store pair.
	 */
	cpu_data_t *datap = old->machine.CpuDatap;
	vm_offset_t base  = old->machine.pcpu_data_base_and_cpu_number;

	/* TODO: Should this be ordered? */

	old->machine.CpuDatap = NULL;
	old->machine.pcpu_data_base_and_cpu_number = 0;

	new->machine.CpuDatap = datap;
	new->machine.pcpu_data_base_and_cpu_number = base;
}

/**
 * routine: machine_switch_pmap_and_extended_context
 *
 * Helper function used by machine_switch_context and machine_stack_handoff to switch the
 * extended context and switch the pmap if necessary.
 *
 */

static inline void
machine_switch_pmap_and_extended_context(thread_t old, thread_t new)
{
	pmap_t new_pmap;





#if HAVE_MACHINE_THREAD_MATRIX_STATE
	machine_switch_matrix_context(old, new);
#endif




	new_pmap = new->map->pmap;
	if (old->map->pmap != new_pmap) {
		pmap_switch(new_pmap);
	} else {
		/*
		 * If the thread is preempted while performing cache or TLB maintenance,
		 * it may be migrated to a different CPU between the completion of the relevant
		 * maintenance instruction and the synchronizing DSB.   ARM requires that the
		 * synchronizing DSB must be issued *on the PE that issued the maintenance instruction*
		 * in order to guarantee completion of the instruction and visibility of its effects.
		 * Issue DSB here to enforce that guarantee.  We only do this for the case in which
		 * the pmap isn't changing, as we expect pmap_switch() to issue DSB when it updates
		 * TTBR0.  Note also that cache maintenance may be performed in userspace, so we
		 * cannot further limit this operation e.g. by setting a per-thread flag to indicate
		 * a pending kernel TLB or cache maintenance instruction.
		 */
		__builtin_arm_dsb(DSB_ISH);
	}


	machine_thread_switch_cpu_data(old, new);
}

/*
 * Routine: machine_switch_context
 *
 */
thread_t
machine_switch_context(thread_t old,
    thread_continue_t continuation,
    thread_t new)
{
	thread_t retval;

#if __ARM_PAN_AVAILABLE__
	if (__improbable(__builtin_arm_rsr("pan") == 0)) {
		panic("context switch with PAN disabled");
	}
#endif

#define machine_switch_context_kprintf(x...) \
	/* kprintf("machine_switch_context: " x) */

	if (old == new) {
		panic("machine_switch_context");
	}

#if CONFIG_CPU_COUNTERS
	kpc_off_cpu(old);
#endif /* CONFIG_CPU_COUNTERS */

	machine_switch_pmap_and_extended_context(old, new);

	machine_switch_context_kprintf("old= %x contination = %x new = %x\n", old, continuation, new);

	retval = Switch_context(old, continuation, new);
	assert(retval != NULL);

	return retval;
}

boolean_t
machine_thread_on_core(thread_t thread)
{
	return thread->machine.CpuDatap != NULL;
}

boolean_t
machine_thread_on_core_allow_invalid(thread_t thread)
{
	extern int _copyin_atomic64(const char *src, uint64_t *dst);
	uint64_t addr;

	/*
	 * Utilize that the thread zone is sequestered which means
	 * that this kernel-to-kernel copyin can't read data
	 * from anything but a thread, zeroed or freed memory.
	 */
	assert(get_preemption_level() > 0);
	thread = pgz_decode_allow_invalid(thread, ZONE_ID_THREAD);
	if (thread == THREAD_NULL) {
		return false;
	}
	thread_require(thread);
	if (_copyin_atomic64((void *)&thread->machine.CpuDatap, &addr) == 0) {
		return addr != 0;
	}
	return false;
}


/*
 * Routine: machine_thread_create
 *
 */
void
machine_thread_create(thread_t thread, task_t task, bool first_thread)
{
#define machine_thread_create_kprintf(x...) \
	/* kprintf("machine_thread_create: " x) */

	machine_thread_create_kprintf("thread = %x\n", thread);

	if (!first_thread) {
		thread->machine.CpuDatap = (cpu_data_t *)0;
		// setting this offset will cause trying to use it to panic
		thread->machine.pcpu_data_base_and_cpu_number =
		    ml_make_pcpu_base_and_cpu_number(VM_MIN_KERNEL_ADDRESS, 0);
	}
	thread->machine.arm_machine_flags = 0;
	thread->machine.preemption_count = 0;
	thread->machine.cthread_self = 0;
	thread->machine.kpcb = NULL;
	thread->machine.exception_trace_code = 0;
#if defined(HAS_APPLE_PAC)
	thread->machine.rop_pid = task->rop_pid;
	thread->machine.jop_pid = task->jop_pid;
	if (task->disable_user_jop) {
		thread->machine.arm_machine_flags |= ARM_MACHINE_THREAD_DISABLE_USER_JOP;
	}
#endif




	if (task != kernel_task) {
		/* If this isn't a kernel thread, we'll have userspace state. */
		arm_context_t *contextData = zalloc_flags(user_ss_zone,
		    Z_WAITOK | Z_NOFAIL);

#if __has_feature(ptrauth_calls)
		uint64_t intr = ml_pac_safe_interrupts_disable();
		zone_require(user_ss_zone, contextData);
#endif
		thread->machine.contextData = contextData;
		thread->machine.upcb = &contextData->ss;
		thread->machine.uNeon = &contextData->ns;
#if __has_feature(ptrauth_calls)
		ml_pac_safe_interrupts_restore(intr);
#endif

		if (task_has_64Bit_data(task)) {
			thread->machine.upcb->ash.flavor = ARM_SAVED_STATE64;
			thread->machine.upcb->ash.count = ARM_SAVED_STATE64_COUNT;
			thread->machine.uNeon->nsh.flavor = ARM_NEON_SAVED_STATE64;
			thread->machine.uNeon->nsh.count = ARM_NEON_SAVED_STATE64_COUNT;

		} else {
			thread->machine.upcb->ash.flavor = ARM_SAVED_STATE32;
			thread->machine.upcb->ash.count = ARM_SAVED_STATE32_COUNT;
			thread->machine.uNeon->nsh.flavor = ARM_NEON_SAVED_STATE32;
			thread->machine.uNeon->nsh.count = ARM_NEON_SAVED_STATE32_COUNT;
		}
	} else {
		thread->machine.upcb = NULL;
		thread->machine.uNeon = NULL;
		thread->machine.contextData = NULL;
	}

#if HAVE_MACHINE_THREAD_MATRIX_STATE
	thread->machine.umatrix_hdr = NULL;
#endif


#if HAS_ARM_FEAT_SME
	thread->machine.tpidr2_el0 = 0;
#endif

	bzero(&thread->machine.perfctrl_state, sizeof(thread->machine.perfctrl_state));
	machine_thread_state_initialize(thread);
}

/*
 * Routine: machine_thread_process_signature
 *
 * Called to allow code signature dependent adjustments to the thread
 * state. Note that this is usually called twice for the main thread:
 * Once at thread creation by thread_create, when the signature is
 * potentially not attached yet (which is usually the case for the
 * first/main thread of a task), and once after the task's signature
 * has actually been attached.
 *
 */
kern_return_t
machine_thread_process_signature(thread_t __unused thread, task_t __unused task)
{
	kern_return_t result = KERN_SUCCESS;

	/*
	 * Reset to default state.
	 *
	 * In general, this function must not assume anything about the
	 * previous signature dependent thread state.
	 *
	 * At least at the time of writing this, threads don't transition
	 * to different code signatures, so each thread this function
	 * operates on is "fresh" in the sense that
	 * machine_thread_process_signature() has either not even been
	 * called on it yet, or only been called as part of thread
	 * creation when there was no signature yet.
	 *
	 * But for easier reasoning, and to prevent future bugs, this
	 * function should always recalculate all signature-dependent
	 * thread state, as if the signature could actually change from an
	 * actual signature to another.
	 */
#if !__ARM_KERNEL_PROTECT__
	thread->machine.arm_machine_flags &= ~(ARM_MACHINE_THREAD_PRESERVE_X18);
#endif /* !__ARM_KERNEL_PROTECT__ */
	thread->machine.arm_machine_flags &= ~(ARM_MACHINE_THREAD_USES_1GHZ_TIMBASE);

	/*
	 * Set signature dependent state.
	 */
	if (task != kernel_task && task_has_64Bit_data(task)) {
#if !__ARM_KERNEL_PROTECT__
#if CONFIG_ROSETTA
		if (task_is_translated(task)) {
			/* Note that for x86_64 translation specifically, the
			 * context switch path implicitly switches x18 regardless
			 * of this flag. */
			thread->machine.arm_machine_flags |= ARM_MACHINE_THREAD_PRESERVE_X18;
		}
#endif /* CONFIG_ROSETTA */

		if (task->preserve_x18) {
			thread->machine.arm_machine_flags |= ARM_MACHINE_THREAD_PRESERVE_X18;
		}
#endif /* !__ARM_KERNEL_PROTECT__ */

		if (task->uses_1ghz_timebase) {
			thread->machine.arm_machine_flags |= ARM_MACHINE_THREAD_USES_1GHZ_TIMBASE;
		}
	} else {
#if !__ARM_KERNEL_PROTECT__
		/*
		 * For informational value only, context switch only trashes
		 * x18 for user threads.  (Except for devices with
		 * __ARM_KERNEL_PROTECT__, which make real destructive use of
		 * x18.)
		 */
		thread->machine.arm_machine_flags |= ARM_MACHINE_THREAD_PRESERVE_X18;
#endif /* !__ARM_KERNEL_PROTECT__ */
		thread->machine.arm_machine_flags |= ARM_MACHINE_THREAD_USES_1GHZ_TIMBASE;
	}

	/**
	 * Make sure the machine flags are observed before the thread becomes available
	 * to run in user mode, especially in the posix_spawn() path.
	 */
	os_atomic_thread_fence(release);
	return result;
}

/*
 * Routine: machine_thread_destroy
 *
 */
void
machine_thread_destroy(thread_t thread)
{
	arm_context_t *thread_user_ss;

	if (thread->machine.contextData) {
		/* Disassociate the user save state from the thread before we free it. */
		thread_user_ss = thread->machine.contextData;
		thread->machine.upcb = NULL;
		thread->machine.uNeon = NULL;
		thread->machine.contextData = NULL;

#if HAS_ARM_FEAT_SME
		machine_thread_sme_state_free(thread);
#endif

		zfree(user_ss_zone, thread_user_ss);
	}

	if (thread->machine.DebugData != NULL) {
		if (thread->machine.DebugData == getCpuDatap()->cpu_user_debug) {
			arm_debug_set(NULL);
		}

		if (os_ref_release(&thread->machine.DebugData->ref) == 0) {
			zfree(ads_zone, thread->machine.DebugData);
		}
	}
}


#if HAS_ARM_FEAT_SME
static arm_sme_saved_state_t *
zalloc_sme_saved_state(void)
{
	arm_sme_saved_state_t *sme_ss = zalloc_flags(sme_ss_zone, Z_WAITOK | Z_ZERO | Z_NOFAIL);
	sme_ss->hdr.flavor = ARM_SME_SAVED_STATE;
	sme_ss->hdr.count = arm_sme_saved_state_count(sme_svl_b);
	sme_ss->svl_b = sme_svl_b;
	return sme_ss;
}

kern_return_t
machine_thread_sme_state_alloc(thread_t thread)
{
	assert(arm_sme_version());


	if (thread->machine.usme) {
		panic("thread %p already has SME saved state %p",
		    thread, thread->machine.usme);
	}

	arm_sme_saved_state_t *sme_ss = zalloc_sme_saved_state();
	disable_preemption();

	arm_sme_trap_at_el0(false);
	__builtin_arm_isb(ISB_SY);
	thread->machine.usme = sme_ss;

	enable_preemption();

	return KERN_SUCCESS;
}

void
machine_thread_sme_state_free(thread_t thread)
{
	arm_sme_saved_state_t *sme_ss = machine_thread_get_sme_state(thread);

	if (sme_ss) {
		thread->machine.usme = NULL;
		zfree(sme_ss_zone, sme_ss);
	}
}

static void
machine_thread_sme_state_dup(const arm_sme_saved_state_t *src_sme_ss, thread_t target)
{
	arm_sme_saved_state_t *sme_ss = zalloc_sme_saved_state();
	assert(sme_ss->svl_b == src_sme_ss->svl_b);

	arm_sme_context_t *context = &sme_ss->context;
	uint16_t svl_b = sme_ss->svl_b;

	sme_ss->svcr = src_sme_ss->svcr;
	/* Z and P are saved on kernel entry.  ZA and ZT0 may be stale. */
	if (sme_ss->svcr & SVCR_SM) {
		const arm_sme_context_t *src_context = &src_sme_ss->context;
		memcpy(arm_sme_z(context), const_arm_sme_z(src_context), arm_sme_z_size(svl_b));
		memcpy(arm_sme_p(context, svl_b), const_arm_sme_p(src_context, svl_b), arm_sme_p_size(svl_b));
	}
	if (sme_ss->svcr & SVCR_ZA) {
		arm_save_sme_za_zt0(context, svl_b);
	}

	target->machine.usme = sme_ss;
}
#endif /* HAS_ARM_FEAT_SME */

#if HAVE_MACHINE_THREAD_MATRIX_STATE
void
machine_thread_matrix_state_dup(thread_t target)
{
	assert(!target->machine.umatrix_hdr);
	thread_t thread = current_thread();

#if HAS_ARM_FEAT_SME
	const arm_sme_saved_state_t *sme_ss = machine_thread_get_sme_state(thread);
	if (sme_ss) {
		machine_thread_sme_state_dup(sme_ss, target);
		return;
	}
#endif

}
#endif /* HAVE_MACHINE_THREAD_MATRIX_STATE */

/*
 * Routine: machine_thread_init
 *
 */
void
machine_thread_init(void)
{

#if HAS_ARM_FEAT_SME
	if (arm_sme_version()) {
		sme_svl_b = arm_sme_svl_b();
		vm_size_t size = arm_sme_saved_state_count(sme_svl_b) * sizeof(unsigned int);
		sme_ss_zone = zone_create_ext("SME saved state", size, ZC_NONE, ZONE_ID_ANY, NULL);
	}
#endif
}

/*
 * Routine:	machine_thread_template_init
 *
 */
void
machine_thread_template_init(thread_t __unused thr_template)
{
	/* Nothing to do on this platform. */
}

/*
 * Routine: get_useraddr
 *
 */
user_addr_t
get_useraddr()
{
	return get_saved_state_pc(current_thread()->machine.upcb);
}

/*
 * Routine: machine_stack_detach
 *
 */
vm_offset_t
machine_stack_detach(thread_t thread)
{
	vm_offset_t stack;

	KERNEL_DEBUG(MACHDBG_CODE(DBG_MACH_SCHED, MACH_STACK_DETACH),
	    (uintptr_t)thread_tid(thread), thread->priority, thread->sched_pri, 0, 0);

	stack = thread->kernel_stack;
#if CONFIG_STKSZ
	kcov_stksz_set_thread_stack(thread, stack);
#endif
	thread->kernel_stack = 0;
	thread->machine.kstackptr = NULL;

	return stack;
}


/*
 * Routine: machine_stack_attach
 *
 */
void
machine_stack_attach(thread_t thread,
    vm_offset_t stack)
{
	struct arm_kernel_context *context;
	struct arm_kernel_saved_state *savestate;
	struct arm_kernel_neon_saved_state *neon_savestate;
	uint32_t current_el;

#define machine_stack_attach_kprintf(x...) \
	/* kprintf("machine_stack_attach: " x) */

	KERNEL_DEBUG(MACHDBG_CODE(DBG_MACH_SCHED, MACH_STACK_ATTACH),
	    (uintptr_t)thread_tid(thread), thread->priority, thread->sched_pri, 0, 0);

	thread->kernel_stack = stack;
#if CONFIG_STKSZ
	kcov_stksz_set_thread_stack(thread, 0);
#endif
	void *kstackptr = (void *)(stack + kernel_stack_size - sizeof(struct thread_kernel_state));
	thread->machine.kstackptr = kstackptr;
	thread_initialize_kernel_state(thread);

	machine_stack_attach_kprintf("kstackptr: %lx\n", (vm_address_t)kstackptr);

	current_el = (uint32_t) __builtin_arm_rsr64("CurrentEL");
	context = &((thread_kernel_state_t) thread->machine.kstackptr)->machine;
	savestate = &context->ss;
	savestate->fp = 0;
	savestate->sp = (uint64_t)kstackptr;
	savestate->pc_was_in_userspace = false;
#if defined(HAS_APPLE_PAC)
	/* Sign the initial kernel stack saved state */
	uint64_t intr = ml_pac_safe_interrupts_disable();
	asm volatile (
                "adrp	x17, _thread_continue@page"             "\n"
                "add	x17, x17, _thread_continue@pageoff"     "\n"
                "ldr	x16, [%[ss], %[SS64_SP]]"               "\n"
                "pacia1716"                                     "\n"
                "str	x17, [%[ss], %[SS64_LR]]"               "\n"
                :
                : [ss]                  "r"(&context->ss),
                  [SS64_SP]             "i"(offsetof(struct arm_kernel_saved_state, sp)),
                  [SS64_LR]             "i"(offsetof(struct arm_kernel_saved_state, lr))
                : "x16", "x17"
        );
	ml_pac_safe_interrupts_restore(intr);
#else
	savestate->lr = (uintptr_t)thread_continue;
#endif /* defined(HAS_APPLE_PAC) */
	neon_savestate = &context->ns;
	neon_savestate->fpcr = FPCR_DEFAULT;
	machine_stack_attach_kprintf("thread = %p pc = %llx, sp = %llx\n", thread, savestate->lr, savestate->sp);
}


/*
 * Routine: machine_stack_handoff
 *
 */
void
machine_stack_handoff(thread_t old,
    thread_t new)
{
	vm_offset_t  stack;

#if __ARM_PAN_AVAILABLE__
	if (__improbable(__builtin_arm_rsr("pan") == 0)) {
		panic("stack handoff with PAN disabled");
	}
#endif

#if CONFIG_CPU_COUNTERS
	kpc_off_cpu(old);
#endif /* CONFIG_CPU_COUNTERS */

	stack = machine_stack_detach(old);
#if CONFIG_STKSZ
	kcov_stksz_set_thread_stack(new, 0);
#endif
	new->kernel_stack = stack;
	void *kstackptr = (void *)(stack + kernel_stack_size - sizeof(struct thread_kernel_state));
	new->machine.kstackptr = kstackptr;
	if (stack == old->reserved_stack) {
		assert(new->reserved_stack);
		old->reserved_stack = new->reserved_stack;
#if KASAN_TBI
		kasan_unpoison_stack(old->reserved_stack, kernel_stack_size);
#endif /* KASAN_TBI */
		new->reserved_stack = stack;
	}

	machine_switch_pmap_and_extended_context(old, new);

	machine_set_current_thread(new);
	thread_initialize_kernel_state(new);
}


/*
 * Routine: call_continuation
 *
 */
void
call_continuation(thread_continue_t continuation,
    void *parameter,
    wait_result_t wresult,
    boolean_t enable_interrupts)
{
#define call_continuation_kprintf(x...) \
	/* kprintf("call_continuation_kprintf:" x) */

	call_continuation_kprintf("thread = %p continuation = %p, stack = %lx\n",
	    current_thread(), continuation, current_thread()->machine.kstackptr);
	Call_continuation(continuation, parameter, wresult, enable_interrupts);
}

#define SET_DBGBCRn(n, value, accum) \
	__asm__ volatile( \
	        "msr DBGBCR" #n "_EL1, %[val]\n" \
	        "orr %[result], %[result], %[val]\n" \
	        : [result] "+r"(accum) : [val] "r"((value)))

#define SET_DBGBVRn(n, value) \
	__asm__ volatile("msr DBGBVR" #n "_EL1, %0" : : "r"(value))

#define SET_DBGWCRn(n, value, accum) \
	__asm__ volatile( \
	        "msr DBGWCR" #n "_EL1, %[val]\n" \
	        "orr %[result], %[result], %[val]\n" \
	        : [result] "+r"(accum) : [val] "r"((value)))

#define SET_DBGWVRn(n, value) \
	__asm__ volatile("msr DBGWVR" #n "_EL1, %0" : : "r"(value))

void
arm_debug_set32(arm_debug_state_t *debug_state)
{
	struct cpu_data *  cpu_data_ptr;
	arm_debug_info_t * debug_info    = arm_debug_info();
	boolean_t          intr;
	arm_debug_state_t  off_state;
	arm_debug_state_t  *cpu_debug;
	uint64_t           all_ctrls = 0;

	intr = ml_set_interrupts_enabled(FALSE);
	cpu_data_ptr = getCpuDatap();
	cpu_debug = cpu_data_ptr->cpu_user_debug;

	/*
	 * Retain and set new per-cpu state.
	 * Reference count does not matter when turning off debug state.
	 */
	if (debug_state == NULL) {
		bzero(&off_state, sizeof(off_state));
		cpu_data_ptr->cpu_user_debug = NULL;
		debug_state = &off_state;
	} else {
		os_ref_retain(&debug_state->ref);
		cpu_data_ptr->cpu_user_debug = debug_state;
	}

	/* Release previous debug state. */
	if (cpu_debug != NULL) {
		if (os_ref_release(&cpu_debug->ref) == 0) {
			zfree(ads_zone, cpu_debug);
		}
	}

	switch (debug_info->num_breakpoint_pairs) {
	case 16:
		SET_DBGBVRn(15, (uint64_t)debug_state->uds.ds32.bvr[15]);
		SET_DBGBCRn(15, (uint64_t)debug_state->uds.ds32.bcr[15], all_ctrls);
		OS_FALLTHROUGH;
	case 15:
		SET_DBGBVRn(14, (uint64_t)debug_state->uds.ds32.bvr[14]);
		SET_DBGBCRn(14, (uint64_t)debug_state->uds.ds32.bcr[14], all_ctrls);
		OS_FALLTHROUGH;
	case 14:
		SET_DBGBVRn(13, (uint64_t)debug_state->uds.ds32.bvr[13]);
		SET_DBGBCRn(13, (uint64_t)debug_state->uds.ds32.bcr[13], all_ctrls);
		OS_FALLTHROUGH;
	case 13:
		SET_DBGBVRn(12, (uint64_t)debug_state->uds.ds32.bvr[12]);
		SET_DBGBCRn(12, (uint64_t)debug_state->uds.ds32.bcr[12], all_ctrls);
		OS_FALLTHROUGH;
	case 12:
		SET_DBGBVRn(11, (uint64_t)debug_state->uds.ds32.bvr[11]);
		SET_DBGBCRn(11, (uint64_t)debug_state->uds.ds32.bcr[11], all_ctrls);
		OS_FALLTHROUGH;
	case 11:
		SET_DBGBVRn(10, (uint64_t)debug_state->uds.ds32.bvr[10]);
		SET_DBGBCRn(10, (uint64_t)debug_state->uds.ds32.bcr[10], all_ctrls);
		OS_FALLTHROUGH;
	case 10:
		SET_DBGBVRn(9, (uint64_t)debug_state->uds.ds32.bvr[9]);
		SET_DBGBCRn(9, (uint64_t)debug_state->uds.ds32.bcr[9], all_ctrls);
		OS_FALLTHROUGH;
	case 9:
		SET_DBGBVRn(8, (uint64_t)debug_state->uds.ds32.bvr[8]);
		SET_DBGBCRn(8, (uint64_t)debug_state->uds.ds32.bcr[8], all_ctrls);
		OS_FALLTHROUGH;
	case 8:
		SET_DBGBVRn(7, (uint64_t)debug_state->uds.ds32.bvr[7]);
		SET_DBGBCRn(7, (uint64_t)debug_state->uds.ds32.bcr[7], all_ctrls);
		OS_FALLTHROUGH;
	case 7:
		SET_DBGBVRn(6, (uint64_t)debug_state->uds.ds32.bvr[6]);
		SET_DBGBCRn(6, (uint64_t)debug_state->uds.ds32.bcr[6], all_ctrls);
		OS_FALLTHROUGH;
	case 6:
		SET_DBGBVRn(5, (uint64_t)debug_state->uds.ds32.bvr[5]);
		SET_DBGBCRn(5, (uint64_t)debug_state->uds.ds32.bcr[5], all_ctrls);
		OS_FALLTHROUGH;
	case 5:
		SET_DBGBVRn(4, (uint64_t)debug_state->uds.ds32.bvr[4]);
		SET_DBGBCRn(4, (uint64_t)debug_state->uds.ds32.bcr[4], all_ctrls);
		OS_FALLTHROUGH;
	case 4:
		SET_DBGBVRn(3, (uint64_t)debug_state->uds.ds32.bvr[3]);
		SET_DBGBCRn(3, (uint64_t)debug_state->uds.ds32.bcr[3], all_ctrls);
		OS_FALLTHROUGH;
	case 3:
		SET_DBGBVRn(2, (uint64_t)debug_state->uds.ds32.bvr[2]);
		SET_DBGBCRn(2, (uint64_t)debug_state->uds.ds32.bcr[2], all_ctrls);
		OS_FALLTHROUGH;
	case 2:
		SET_DBGBVRn(1, (uint64_t)debug_state->uds.ds32.bvr[1]);
		SET_DBGBCRn(1, (uint64_t)debug_state->uds.ds32.bcr[1], all_ctrls);
		OS_FALLTHROUGH;
	case 1:
		SET_DBGBVRn(0, (uint64_t)debug_state->uds.ds32.bvr[0]);
		SET_DBGBCRn(0, (uint64_t)debug_state->uds.ds32.bcr[0], all_ctrls);
		OS_FALLTHROUGH;
	default:
		break;
	}

	switch (debug_info->num_watchpoint_pairs) {
	case 16:
		SET_DBGWVRn(15, (uint64_t)debug_state->uds.ds32.wvr[15]);
		SET_DBGWCRn(15, (uint64_t)debug_state->uds.ds32.wcr[15], all_ctrls);
		OS_FALLTHROUGH;
	case 15:
		SET_DBGWVRn(14, (uint64_t)debug_state->uds.ds32.wvr[14]);
		SET_DBGWCRn(14, (uint64_t)debug_state->uds.ds32.wcr[14], all_ctrls);
		OS_FALLTHROUGH;
	case 14:
		SET_DBGWVRn(13, (uint64_t)debug_state->uds.ds32.wvr[13]);
		SET_DBGWCRn(13, (uint64_t)debug_state->uds.ds32.wcr[13], all_ctrls);
		OS_FALLTHROUGH;
	case 13:
		SET_DBGWVRn(12, (uint64_t)debug_state->uds.ds32.wvr[12]);
		SET_DBGWCRn(12, (uint64_t)debug_state->uds.ds32.wcr[12], all_ctrls);
		OS_FALLTHROUGH;
	case 12:
		SET_DBGWVRn(11, (uint64_t)debug_state->uds.ds32.wvr[11]);
		SET_DBGWCRn(11, (uint64_t)debug_state->uds.ds32.wcr[11], all_ctrls);
		OS_FALLTHROUGH;
	case 11:
		SET_DBGWVRn(10, (uint64_t)debug_state->uds.ds32.wvr[10]);
		SET_DBGWCRn(10, (uint64_t)debug_state->uds.ds32.wcr[10], all_ctrls);
		OS_FALLTHROUGH;
	case 10:
		SET_DBGWVRn(9, (uint64_t)debug_state->uds.ds32.wvr[9]);
		SET_DBGWCRn(9, (uint64_t)debug_state->uds.ds32.wcr[9], all_ctrls);
		OS_FALLTHROUGH;
	case 9:
		SET_DBGWVRn(8, (uint64_t)debug_state->uds.ds32.wvr[8]);
		SET_DBGWCRn(8, (uint64_t)debug_state->uds.ds32.wcr[8], all_ctrls);
		OS_FALLTHROUGH;
	case 8:
		SET_DBGWVRn(7, (uint64_t)debug_state->uds.ds32.wvr[7]);
		SET_DBGWCRn(7, (uint64_t)debug_state->uds.ds32.wcr[7], all_ctrls);
		OS_FALLTHROUGH;
	case 7:
		SET_DBGWVRn(6, (uint64_t)debug_state->uds.ds32.wvr[6]);
		SET_DBGWCRn(6, (uint64_t)debug_state->uds.ds32.wcr[6], all_ctrls);
		OS_FALLTHROUGH;
	case 6:
		SET_DBGWVRn(5, (uint64_t)debug_state->uds.ds32.wvr[5]);
		SET_DBGWCRn(5, (uint64_t)debug_state->uds.ds32.wcr[5], all_ctrls);
		OS_FALLTHROUGH;
	case 5:
		SET_DBGWVRn(4, (uint64_t)debug_state->uds.ds32.wvr[4]);
		SET_DBGWCRn(4, (uint64_t)debug_state->uds.ds32.wcr[4], all_ctrls);
		OS_FALLTHROUGH;
	case 4:
		SET_DBGWVRn(3, (uint64_t)debug_state->uds.ds32.wvr[3]);
		SET_DBGWCRn(3, (uint64_t)debug_state->uds.ds32.wcr[3], all_ctrls);
		OS_FALLTHROUGH;
	case 3:
		SET_DBGWVRn(2, (uint64_t)debug_state->uds.ds32.wvr[2]);
		SET_DBGWCRn(2, (uint64_t)debug_state->uds.ds32.wcr[2], all_ctrls);
		OS_FALLTHROUGH;
	case 2:
		SET_DBGWVRn(1, (uint64_t)debug_state->uds.ds32.wvr[1]);
		SET_DBGWCRn(1, (uint64_t)debug_state->uds.ds32.wcr[1], all_ctrls);
		OS_FALLTHROUGH;
	case 1:
		SET_DBGWVRn(0, (uint64_t)debug_state->uds.ds32.wvr[0]);
		SET_DBGWCRn(0, (uint64_t)debug_state->uds.ds32.wcr[0], all_ctrls);
		OS_FALLTHROUGH;
	default:
		break;
	}

#if defined(CONFIG_KERNEL_INTEGRITY)
	if ((all_ctrls & (ARM_DBG_CR_MODE_CONTROL_PRIVILEGED | ARM_DBG_CR_HIGHER_MODE_ENABLE)) != 0) {
		panic("sorry, self-hosted debug is not supported: 0x%llx", all_ctrls);
	}
#endif

	/*
	 * Breakpoint/Watchpoint Enable
	 */
	if (all_ctrls != 0) {
		update_mdscr(0, 0x8000); // MDSCR_EL1[MDE]
	} else {
		update_mdscr(0x8000, 0);
	}

	/*
	 * Software debug single step enable
	 */
	if (debug_state->uds.ds32.mdscr_el1 & 0x1) {
		update_mdscr(0, 1); // MDSCR_EL1[SS]

		mask_user_saved_state_cpsr(current_thread()->machine.upcb, PSR64_SS, 0);
	} else {
		update_mdscr(0x1, 0);
	}

	__builtin_arm_isb(ISB_SY);
	(void) ml_set_interrupts_enabled(intr);
}

void
arm_debug_set64(arm_debug_state_t *debug_state)
{
	struct cpu_data *  cpu_data_ptr;
	arm_debug_info_t * debug_info    = arm_debug_info();
	boolean_t          intr;
	arm_debug_state_t  off_state;
	arm_debug_state_t  *cpu_debug;
	uint64_t           all_ctrls = 0;

	intr = ml_set_interrupts_enabled(FALSE);
	cpu_data_ptr = getCpuDatap();
	cpu_debug = cpu_data_ptr->cpu_user_debug;

	/*
	 * Retain and set new per-cpu state.
	 * Reference count does not matter when turning off debug state.
	 */
	if (debug_state == NULL) {
		bzero(&off_state, sizeof(off_state));
		cpu_data_ptr->cpu_user_debug = NULL;
		debug_state = &off_state;
	} else {
		os_ref_retain(&debug_state->ref);
		cpu_data_ptr->cpu_user_debug = debug_state;
	}

	/* Release previous debug state. */
	if (cpu_debug != NULL) {
		if (os_ref_release(&cpu_debug->ref) == 0) {
			zfree(ads_zone, cpu_debug);
		}
	}

	switch (debug_info->num_breakpoint_pairs) {
	case 16:
		SET_DBGBVRn(15, debug_state->uds.ds64.bvr[15]);
		SET_DBGBCRn(15, (uint64_t)debug_state->uds.ds64.bcr[15], all_ctrls);
		OS_FALLTHROUGH;
	case 15:
		SET_DBGBVRn(14, debug_state->uds.ds64.bvr[14]);
		SET_DBGBCRn(14, (uint64_t)debug_state->uds.ds64.bcr[14], all_ctrls);
		OS_FALLTHROUGH;
	case 14:
		SET_DBGBVRn(13, debug_state->uds.ds64.bvr[13]);
		SET_DBGBCRn(13, (uint64_t)debug_state->uds.ds64.bcr[13], all_ctrls);
		OS_FALLTHROUGH;
	case 13:
		SET_DBGBVRn(12, debug_state->uds.ds64.bvr[12]);
		SET_DBGBCRn(12, (uint64_t)debug_state->uds.ds64.bcr[12], all_ctrls);
		OS_FALLTHROUGH;
	case 12:
		SET_DBGBVRn(11, debug_state->uds.ds64.bvr[11]);
		SET_DBGBCRn(11, (uint64_t)debug_state->uds.ds64.bcr[11], all_ctrls);
		OS_FALLTHROUGH;
	case 11:
		SET_DBGBVRn(10, debug_state->uds.ds64.bvr[10]);
		SET_DBGBCRn(10, (uint64_t)debug_state->uds.ds64.bcr[10], all_ctrls);
		OS_FALLTHROUGH;
	case 10:
		SET_DBGBVRn(9, debug_state->uds.ds64.bvr[9]);
		SET_DBGBCRn(9, (uint64_t)debug_state->uds.ds64.bcr[9], all_ctrls);
		OS_FALLTHROUGH;
	case 9:
		SET_DBGBVRn(8, debug_state->uds.ds64.bvr[8]);
		SET_DBGBCRn(8, (uint64_t)debug_state->uds.ds64.bcr[8], all_ctrls);
		OS_FALLTHROUGH;
	case 8:
		SET_DBGBVRn(7, debug_state->uds.ds64.bvr[7]);
		SET_DBGBCRn(7, (uint64_t)debug_state->uds.ds64.bcr[7], all_ctrls);
		OS_FALLTHROUGH;
	case 7:
		SET_DBGBVRn(6, debug_state->uds.ds64.bvr[6]);
		SET_DBGBCRn(6, (uint64_t)debug_state->uds.ds64.bcr[6], all_ctrls);
		OS_FALLTHROUGH;
	case 6:
		SET_DBGBVRn(5, debug_state->uds.ds64.bvr[5]);
		SET_DBGBCRn(5, (uint64_t)debug_state->uds.ds64.bcr[5], all_ctrls);
		OS_FALLTHROUGH;
	case 5:
		SET_DBGBVRn(4, debug_state->uds.ds64.bvr[4]);
		SET_DBGBCRn(4, (uint64_t)debug_state->uds.ds64.bcr[4], all_ctrls);
		OS_FALLTHROUGH;
	case 4:
		SET_DBGBVRn(3, debug_state->uds.ds64.bvr[3]);
		SET_DBGBCRn(3, (uint64_t)debug_state->uds.ds64.bcr[3], all_ctrls);
		OS_FALLTHROUGH;
	case 3:
		SET_DBGBVRn(2, debug_state->uds.ds64.bvr[2]);
		SET_DBGBCRn(2, (uint64_t)debug_state->uds.ds64.bcr[2], all_ctrls);
		OS_FALLTHROUGH;
	case 2:
		SET_DBGBVRn(1, debug_state->uds.ds64.bvr[1]);
		SET_DBGBCRn(1, (uint64_t)debug_state->uds.ds64.bcr[1], all_ctrls);
		OS_FALLTHROUGH;
	case 1:
		SET_DBGBVRn(0, debug_state->uds.ds64.bvr[0]);
		SET_DBGBCRn(0, (uint64_t)debug_state->uds.ds64.bcr[0], all_ctrls);
		OS_FALLTHROUGH;
	default:
		break;
	}

	switch (debug_info->num_watchpoint_pairs) {
	case 16:
		SET_DBGWVRn(15, debug_state->uds.ds64.wvr[15]);
		SET_DBGWCRn(15, (uint64_t)debug_state->uds.ds64.wcr[15], all_ctrls);
		OS_FALLTHROUGH;
	case 15:
		SET_DBGWVRn(14, debug_state->uds.ds64.wvr[14]);
		SET_DBGWCRn(14, (uint64_t)debug_state->uds.ds64.wcr[14], all_ctrls);
		OS_FALLTHROUGH;
	case 14:
		SET_DBGWVRn(13, debug_state->uds.ds64.wvr[13]);
		SET_DBGWCRn(13, (uint64_t)debug_state->uds.ds64.wcr[13], all_ctrls);
		OS_FALLTHROUGH;
	case 13:
		SET_DBGWVRn(12, debug_state->uds.ds64.wvr[12]);
		SET_DBGWCRn(12, (uint64_t)debug_state->uds.ds64.wcr[12], all_ctrls);
		OS_FALLTHROUGH;
	case 12:
		SET_DBGWVRn(11, debug_state->uds.ds64.wvr[11]);
		SET_DBGWCRn(11, (uint64_t)debug_state->uds.ds64.wcr[11], all_ctrls);
		OS_FALLTHROUGH;
	case 11:
		SET_DBGWVRn(10, debug_state->uds.ds64.wvr[10]);
		SET_DBGWCRn(10, (uint64_t)debug_state->uds.ds64.wcr[10], all_ctrls);
		OS_FALLTHROUGH;
	case 10:
		SET_DBGWVRn(9, debug_state->uds.ds64.wvr[9]);
		SET_DBGWCRn(9, (uint64_t)debug_state->uds.ds64.wcr[9], all_ctrls);
		OS_FALLTHROUGH;
	case 9:
		SET_DBGWVRn(8, debug_state->uds.ds64.wvr[8]);
		SET_DBGWCRn(8, (uint64_t)debug_state->uds.ds64.wcr[8], all_ctrls);
		OS_FALLTHROUGH;
	case 8:
		SET_DBGWVRn(7, debug_state->uds.ds64.wvr[7]);
		SET_DBGWCRn(7, (uint64_t)debug_state->uds.ds64.wcr[7], all_ctrls);
		OS_FALLTHROUGH;
	case 7:
		SET_DBGWVRn(6, debug_state->uds.ds64.wvr[6]);
		SET_DBGWCRn(6, (uint64_t)debug_state->uds.ds64.wcr[6], all_ctrls);
		OS_FALLTHROUGH;
	case 6:
		SET_DBGWVRn(5, debug_state->uds.ds64.wvr[5]);
		SET_DBGWCRn(5, (uint64_t)debug_state->uds.ds64.wcr[5], all_ctrls);
		OS_FALLTHROUGH;
	case 5:
		SET_DBGWVRn(4, debug_state->uds.ds64.wvr[4]);
		SET_DBGWCRn(4, (uint64_t)debug_state->uds.ds64.wcr[4], all_ctrls);
		OS_FALLTHROUGH;
	case 4:
		SET_DBGWVRn(3, debug_state->uds.ds64.wvr[3]);
		SET_DBGWCRn(3, (uint64_t)debug_state->uds.ds64.wcr[3], all_ctrls);
		OS_FALLTHROUGH;
	case 3:
		SET_DBGWVRn(2, debug_state->uds.ds64.wvr[2]);
		SET_DBGWCRn(2, (uint64_t)debug_state->uds.ds64.wcr[2], all_ctrls);
		OS_FALLTHROUGH;
	case 2:
		SET_DBGWVRn(1, debug_state->uds.ds64.wvr[1]);
		SET_DBGWCRn(1, (uint64_t)debug_state->uds.ds64.wcr[1], all_ctrls);
		OS_FALLTHROUGH;
	case 1:
		SET_DBGWVRn(0, debug_state->uds.ds64.wvr[0]);
		SET_DBGWCRn(0, (uint64_t)debug_state->uds.ds64.wcr[0], all_ctrls);
		OS_FALLTHROUGH;
	default:
		break;
	}

#if defined(CONFIG_KERNEL_INTEGRITY)
	if ((all_ctrls & (ARM_DBG_CR_MODE_CONTROL_PRIVILEGED | ARM_DBG_CR_HIGHER_MODE_ENABLE)) != 0) {
		panic("sorry, self-hosted debug is not supported: 0x%llx", all_ctrls);
	}
#endif

	/*
	 * Breakpoint/Watchpoint Enable
	 */
	if (all_ctrls != 0) {
		update_mdscr(0, 0x8000); // MDSCR_EL1[MDE]
	} else {
		update_mdscr(0x8000, 0);
	}

	/*
	 * Software debug single step enable
	 */
	if (debug_state->uds.ds64.mdscr_el1 & 0x1) {

		update_mdscr(0, 1); // MDSCR_EL1[SS]

		mask_user_saved_state_cpsr(current_thread()->machine.upcb, PSR64_SS, 0);
	} else {
		update_mdscr(0x1, 0);
	}

	__builtin_arm_isb(ISB_SY);
	(void) ml_set_interrupts_enabled(intr);
}

void
arm_debug_set(arm_debug_state_t *debug_state)
{
	if (debug_state) {
		switch (debug_state->dsh.flavor) {
		case ARM_DEBUG_STATE32:
			arm_debug_set32(debug_state);
			break;
		case ARM_DEBUG_STATE64:
			arm_debug_set64(debug_state);
			break;
		default:
			panic("arm_debug_set");
			break;
		}
	} else {
		if (thread_is_64bit_data(current_thread())) {
			arm_debug_set64(debug_state);
		} else {
			arm_debug_set32(debug_state);
		}
	}
}

#define VM_MAX_ADDRESS32          ((vm_address_t) 0x80000000)
boolean_t
debug_legacy_state_is_valid(arm_legacy_debug_state_t *debug_state)
{
	arm_debug_info_t *debug_info = arm_debug_info();
	uint32_t i;
	for (i = 0; i < debug_info->num_breakpoint_pairs; i++) {
		if (0 != debug_state->bcr[i] && VM_MAX_ADDRESS32 <= debug_state->bvr[i]) {
			return FALSE;
		}
	}

	for (i = 0; i < debug_info->num_watchpoint_pairs; i++) {
		if (0 != debug_state->wcr[i] && VM_MAX_ADDRESS32 <= debug_state->wvr[i]) {
			return FALSE;
		}
	}
	return TRUE;
}

boolean_t
debug_state_is_valid32(arm_debug_state32_t *debug_state)
{
	arm_debug_info_t *debug_info = arm_debug_info();
	uint32_t i;
	for (i = 0; i < debug_info->num_breakpoint_pairs; i++) {
		if (0 != debug_state->bcr[i] && VM_MAX_ADDRESS32 <= debug_state->bvr[i]) {
			return FALSE;
		}
	}

	for (i = 0; i < debug_info->num_watchpoint_pairs; i++) {
		if (0 != debug_state->wcr[i] && VM_MAX_ADDRESS32 <= debug_state->wvr[i]) {
			return FALSE;
		}
	}
	return TRUE;
}

boolean_t
debug_state_is_valid64(arm_debug_state64_t *debug_state)
{
	arm_debug_info_t *debug_info = arm_debug_info();
	uint32_t i;
	for (i = 0; i < debug_info->num_breakpoint_pairs; i++) {
		if (0 != debug_state->bcr[i] && MACH_VM_MAX_ADDRESS <= debug_state->bvr[i]) {
			return FALSE;
		}
	}

	for (i = 0; i < debug_info->num_watchpoint_pairs; i++) {
		if (0 != debug_state->wcr[i] && MACH_VM_MAX_ADDRESS <= debug_state->wvr[i]) {
			return FALSE;
		}
	}
	return TRUE;
}

/*
 * Duplicate one arm_debug_state_t to another.  "all" parameter
 * is ignored in the case of ARM -- Is this the right assumption?
 */
void
copy_legacy_debug_state(arm_legacy_debug_state_t * src,
    arm_legacy_debug_state_t * target,
    __unused boolean_t         all)
{
	bcopy(src, target, sizeof(arm_legacy_debug_state_t));
}

void
copy_debug_state32(arm_debug_state32_t * src,
    arm_debug_state32_t * target,
    __unused boolean_t    all)
{
	bcopy(src, target, sizeof(arm_debug_state32_t));
}

void
copy_debug_state64(arm_debug_state64_t * src,
    arm_debug_state64_t * target,
    __unused boolean_t    all)
{
	bcopy(src, target, sizeof(arm_debug_state64_t));
}

kern_return_t
machine_thread_set_tsd_base(thread_t         thread,
    mach_vm_offset_t tsd_base)
{
	if (get_threadtask(thread) == kernel_task) {
		return KERN_INVALID_ARGUMENT;
	}

	if (thread_is_64bit_addr(thread)) {
		if (tsd_base > vm_map_max(thread->map)) {
			tsd_base = 0ULL;
		}
	} else {
		if (tsd_base > UINT32_MAX) {
			tsd_base = 0ULL;
		}
	}

	thread->machine.cthread_self = tsd_base;

	/* For current thread, make the TSD base active immediately */
	if (thread == current_thread()) {
		mp_disable_preemption();
		set_tpidrro(tsd_base);
		mp_enable_preemption();
	}

	return KERN_SUCCESS;
}

void
machine_tecs(__unused thread_t thr)
{
}

int
machine_csv(__unused cpuvn_e cve)
{
	return 0;
}

#if __ARM_ARCH_8_5__
void
arm_context_switch_requires_sync()
{
	current_cpu_datap()->sync_on_cswitch = 1;
}
#endif

#if __has_feature(ptrauth_calls)
boolean_t
arm_user_jop_disabled(void)
{
	return FALSE;
}
#endif /* __has_feature(ptrauth_calls) */
