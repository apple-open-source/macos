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
 * @OSF_COPYRIGHT@
 */
/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
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

#ifndef _ARM_THREAD_H_
#define _ARM_THREAD_H_

#include <mach/mach_types.h>
#include <mach/boolean.h>
#include <mach/arm/vm_types.h>
#include <mach/thread_status.h>

#ifdef MACH_KERNEL_PRIVATE
#include <arm/cpu_data.h>
#include <arm64/proc_reg.h>
#include <os/base.h>
#endif

struct perfcontrol_state {
	uint64_t opaque[8] __attribute__((aligned(8)));
};

/*
 * Maps state flavor to number of words in the state:
 */
extern unsigned int _MachineStateCount[];

#ifdef MACH_KERNEL_PRIVATE
typedef arm_kernel_context_t machine_thread_kernel_state;
#include <kern/thread_kernel_state.h>

#if (!__arm64__)
#error Unknown arch
#endif

#if HAS_ARM_FEAT_SME
#define HAVE_MACHINE_THREAD_MATRIX_STATE        1
#endif


/*
 * Machine Thread Structure
 */
struct machine_thread {
#if __ARM_USER_PROTECT__
	unsigned int              uptw_ttb;
	unsigned int              kptw_ttb;
	unsigned int              asid;
#else
	unsigned int              reserved0;
	unsigned int              reserved1;
	unsigned int              reserved2;
#endif

	uint32_t                  arm_machine_flags;       /* thread flags (arm64/machine_machdep.h) */
	arm_context_t *           contextData;             /* allocated user context */
	arm_saved_state_t *       XNU_PTRAUTH_SIGNED_PTR("machine_thread.upcb") upcb;   /* pointer to user GPR state */
	arm_neon_saved_state_t *  XNU_PTRAUTH_SIGNED_PTR("machine_thread.uNeon") uNeon; /* pointer to user VFP state */
	arm_saved_state_t *       kpcb;                    /* pointer to kernel GPR state */

#if HAVE_MACHINE_THREAD_MATRIX_STATE
	union {
		arm_state_hdr_t *umatrix_hdr;
#if HAS_ARM_FEAT_SME
		arm_sme_saved_state_t *usme;               /* pointer to user SME state */
#endif
	};
#endif /* HAVE_MACHINE_THREAD_MATRIX_STATE */

	long                      reserved4;
	uint64_t                  recover_far;

	arm_debug_state_t        *DebugData;
	vm_address_t              cthread_self;               /* for use of cthread package */

	uint64_t                  recover_esr;

	void *                    XNU_PTRAUTH_SIGNED_PTR("machine_thread.kstackptr") kstackptr; /* top of kernel stack */
	struct perfcontrol_state  perfctrl_state;
	uint64_t                  reserved5;

#if SCHED_HYGIENE_DEBUG
	uint64_t                  intmask_timestamp;          /* timestamp of when interrupts were manually masked */
	uint64_t                  inthandler_timestamp;       /* timestamp of when interrupt handler started */
	uint64_t                  intmask_cycles;             /* cycle count snapshot of when interrupts were masked */
	uint64_t                  intmask_instr;              /* instruction count snapshot of when interrupts were masked */
	bool                      inthandler_abandon;         /* whether to abandon the current measurement */

	unsigned int              int_type;                   /* interrupt type of the interrupt that was processed */
	uintptr_t                 int_handler_addr;           /* slid, ptrauth-stripped virtual address of the interrupt handler */
	uintptr_t                 int_vector;                 /* IOInterruptVector */
	uint64_t                  int_time_mt;                /* total time spent in interrupt context */
#endif /* SCHED_HYGIENE_DEBUG */

#if defined(CONFIG_XNUPOST)
	volatile expected_fault_handler_t  expected_fault_handler;
	volatile uintptr_t                 expected_fault_addr;  /* Address due to which an exception is expected to be thrown (FAR_ELx) */
	volatile uintptr_t                 expected_fault_pc;    /* PC at which an exception is expected to be thrown (ELR_ELx) */
#endif

	uint64_t                  reserved6;
	union {
		long              pcpu_data_base_and_cpu_number;
		const uint16_t    cpu_number;
	};
	struct cpu_data *         CpuDatap;               /* current per cpu data */
	unsigned int              preemption_count;       /* preemption count */
	uint16_t                  exception_trace_code;
	bool                      reserved7;
#if defined(HAS_APPLE_PAC)
	uint64_t                  rop_pid;
	uint64_t                  jop_pid;
#else
	uint64_t                  reserved8;
	uint64_t                  reserved9;
#endif

	uint64_t                  reserved10;

#if HAS_ARM_FEAT_SME
	uint64_t                  tpidr2_el0;
#else
	uint64_t                  reserved12;
#endif
};
#endif

static inline long
ml_make_pcpu_base_and_cpu_number(long base, uint16_t cpu)
{
	return (base << 16) | cpu;
}

extern struct arm_saved_state *    get_user_regs(thread_t);
extern struct arm_saved_state *    find_user_regs(thread_t);
extern struct arm_saved_state *    find_kern_regs(thread_t);
extern struct arm_vfpsaved_state * find_user_vfp(thread_t);
extern arm_debug_state32_t *       find_debug_state32(thread_t);
extern arm_debug_state32_t *       find_or_allocate_debug_state32(thread_t);
extern arm_debug_state64_t *       find_debug_state64(thread_t);
extern arm_debug_state64_t *       find_or_allocate_debug_state64(thread_t);
extern arm_neon_saved_state_t *    get_user_neon_regs(thread_t);

#define FIND_PERFCONTROL_STATE(th) (&th->machine.perfctrl_state)

#ifdef MACH_KERNEL_PRIVATE
#if __ARM_VFP__
extern void vfp_state_initialize(struct arm_vfpsaved_state *vfp_state);
extern void vfp_save(struct arm_vfpsaved_state *vfp_ss);
extern void vfp_load(struct arm_vfpsaved_state *vfp_ss);
#endif /* __ARM_VFP__ */
extern void arm_debug_set(arm_debug_state_t *debug_state);
extern void arm_debug_set32(arm_debug_state_t *debug_state);
extern void arm_debug_set64(arm_debug_state_t *debug_state);
#endif /* MACH_KERNEL_PRIVATE */

extern void *act_thread_csave(void);
extern void act_thread_catt(void *ctx);
extern void act_thread_cfree(void *ctx);

#if MACH_KERNEL_PRIVATE

#if HAS_ARM_FEAT_SME
extern arm_sme_saved_state_t *machine_thread_get_sme_state(thread_t thread);
extern kern_return_t machine_thread_sme_state_alloc(thread_t thread);
extern void machine_thread_sme_state_free(thread_t thread);
#endif

#if HAVE_MACHINE_THREAD_MATRIX_STATE
extern void machine_thread_matrix_state_dup(thread_t target);
#endif
#endif /* MACH_KERNEL_PRIVATE */

#if HAS_APPLE_GENERIC_TIMER
extern void agt_thread_bootstrap(void);
#endif /* HAS_MACHINE_GENERIC_TIMER */
/*
 * Return address of the function that called current function, given
 * address of the first parameter of current function.
 */
#define GET_RETURN_PC(addr) (__builtin_return_address(0))

#endif /* _ARM_THREAD_H_ */
