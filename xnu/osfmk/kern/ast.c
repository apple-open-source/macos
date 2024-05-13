/*
 * Copyright (c) 2000-2017 Apple Inc. All rights reserved.
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

#include <kern/ast.h>
#include <kern/counter.h>
#include <kern/misc_protos.h>
#include <kern/queue.h>
#include <kern/sched_prim.h>
#include <kern/thread.h>
#include <kern/processor.h>
#include <kern/restartable.h>
#include <kern/spl.h>
#include <kern/sfi.h>
#if CONFIG_TELEMETRY
#include <kern/telemetry.h>
#endif
#include <kern/waitq.h>
#include <kern/ledger.h>
#include <kern/machine.h>
#include <kern/kpc.h>
#include <kperf/kperf.h>
#include <mach/policy.h>
#include <security/mac_mach_internal.h> // for MACF AST hook
#include <stdatomic.h>

#if CONFIG_ARCADE
#include <kern/arcade.h>
#endif

static void __attribute__((noinline, noreturn, disable_tail_calls))
thread_preempted(__unused void* parameter, __unused wait_result_t result)
{
	/*
	 * We've been scheduled again after a userspace preemption,
	 * try again to return to userspace.
	 */
	thread_exception_return();
}

/*
 * Create a dedicated frame to clarify that this thread has been preempted
 * while running in kernel space.
 */
static void __attribute__((noinline, disable_tail_calls))
thread_preempted_in_kernel(ast_t urgent_reason)
{
	thread_block_reason(THREAD_CONTINUE_NULL, NULL, urgent_reason);

	assert(ml_get_interrupts_enabled() == FALSE);
}

/*
 * AST_URGENT was detected while in kernel mode
 * Called with interrupts disabled, returns the same way
 * Must return to caller
 */
void
ast_taken_kernel(void)
{
	assert(ml_get_interrupts_enabled() == FALSE);

	thread_t thread = current_thread();

	/* Idle threads handle preemption themselves */
	if ((thread->state & TH_IDLE)) {
		ast_off(AST_PREEMPTION);
		return;
	}

	/*
	 * It's possible for this to be called after AST_URGENT
	 * has already been handled, due to races in enable_preemption
	 */
	if (ast_peek(AST_URGENT) != AST_URGENT) {
		return;
	}

	/*
	 * Don't preempt if the thread is already preparing to block.
	 * TODO: the thread can cheese this with clear_wait()
	 */
	if (waitq_wait_possible(thread) == FALSE) {
		/* Consume AST_URGENT or the interrupt will call us again */
		ast_consume(AST_URGENT);
		return;
	}

	/* TODO: Should we csw_check again to notice if conditions have changed? */

	ast_t urgent_reason = ast_consume(AST_PREEMPTION);

	assert(urgent_reason & AST_PREEMPT);

	/* We've decided to try context switching */
	thread_preempted_in_kernel(urgent_reason);
}

/*
 * An AST flag was set while returning to user mode
 * Called with interrupts disabled, returns with interrupts enabled
 * May call continuation instead of returning
 */
void
ast_taken_user(void)
{
	assert(ml_get_interrupts_enabled() == FALSE);

	thread_t thread = current_thread();
	task_t   task   = get_threadtask(thread);

	/* We are about to return to userspace, there must not be a pending wait */
	assert(waitq_wait_possible(thread));
	assert((thread->state & TH_IDLE) == 0);

	/* TODO: Add more 'return to userspace' assertions here */

	/*
	 * If this thread was urgently preempted in userspace,
	 * take the preemption before processing the ASTs.
	 * The trap handler will call us again if we have more ASTs, so it's
	 * safe to block in a continuation here.
	 */
	if (ast_peek(AST_URGENT) == AST_URGENT) {
		ast_t urgent_reason = ast_consume(AST_PREEMPTION);

		assert(urgent_reason & AST_PREEMPT);

		/* TODO: Should we csw_check again to notice if conditions have changed? */

		thread_block_reason(thread_preempted, NULL, urgent_reason);
		/* NOTREACHED */
	}

	/*
	 * AST_KEVENT does not send an IPI when setting the ast for a thread running in parallel
	 * on a different processor. Only the ast bit on the thread will be set.
	 *
	 * Force a propagate for concurrent updates without an IPI.
	 */
	ast_propagate(thread);

	/*
	 * Consume all non-preemption processor ASTs matching reasons
	 * because we're handling them here.
	 *
	 * If one of the AST handlers blocks in a continuation,
	 * we'll reinstate the unserviced thread-level AST flags
	 * from the thread to the processor on context switch.
	 * If one of the AST handlers sets another AST,
	 * the trap handler will call ast_taken_user again.
	 *
	 * We expect the AST handlers not to thread_exception_return
	 * without an ast_propagate or context switch to reinstate
	 * the per-processor ASTs.
	 *
	 * TODO: Why are AST_DTRACE and AST_KPERF not per-thread ASTs?
	 */
	ast_t reasons = ast_consume(AST_PER_THREAD | AST_KPERF | AST_DTRACE);

	ml_set_interrupts_enabled(TRUE);

#if CONFIG_DTRACE
	if (reasons & AST_DTRACE) {
		dtrace_ast();
	}
#endif

#ifdef MACH_BSD
	if (reasons & AST_BSD) {
		thread_ast_clear(thread, AST_BSD);
		bsd_ast(thread);
	}
#endif

#if CONFIG_MACF
	if (reasons & AST_MACF) {
		thread_ast_clear(thread, AST_MACF);
		mac_thread_userret(thread);
	}
#endif

#if CONFIG_ARCADE
	if (reasons & AST_ARCADE) {
		thread_ast_clear(thread, AST_ARCADE);
		arcade_ast(thread);
	}
#endif

	if (reasons & AST_APC) {
		thread_ast_clear(thread, AST_APC);
		thread_apc_ast(thread);
	}

	if (reasons & AST_GUARD) {
		thread_ast_clear(thread, AST_GUARD);
		guard_ast(thread);
	}

	if (reasons & AST_LEDGER) {
		thread_ast_clear(thread, AST_LEDGER);
		ledger_ast(thread);
	}

	if (reasons & AST_KPERF) {
		thread_ast_clear(thread, AST_KPERF);
#if CONFIG_CPU_COUNTERS
		kpc_thread_ast_handler(thread);
#endif /* CONFIG_CPU_COUNTERS */
		kperf_thread_ast_handler(thread);
		thread->kperf_ast = 0;
	}

	if (reasons & AST_RESET_PCS) {
		thread_ast_clear(thread, AST_RESET_PCS);
		thread_reset_pcs_ast(task, thread);
	}

	if (reasons & AST_KEVENT) {
		thread_ast_clear(thread, AST_KEVENT);
		uint16_t bits = atomic_exchange(&thread->kevent_ast_bits, 0);
		if (bits) {
			kevent_ast(thread, bits);
		}
	}

	if (reasons & AST_PROC_RESOURCE) {
		thread_ast_clear(thread, AST_PROC_RESOURCE);
		task_port_space_ast(task);
#if MACH_BSD
		proc_filedesc_ast(task);
#endif /* MACH_BSD */
	}

#if CONFIG_TELEMETRY
	if (reasons & AST_TELEMETRY_ALL) {
		ast_t telemetry_reasons = reasons & AST_TELEMETRY_ALL;
		thread_ast_clear(thread, AST_TELEMETRY_ALL);
		telemetry_ast(thread, telemetry_reasons);
	}
#endif

#if MACH_ASSERT
	if (reasons & AST_DEBUG_ASSERT) {
		thread_ast_clear(thread, AST_DEBUG_ASSERT);
		thread_debug_return_to_user_ast(thread);
	}
#endif

	spl_t s = splsched();

#if CONFIG_SCHED_SFI
	/*
	 * SFI is currently a per-processor AST, not a per-thread AST
	 *      TODO: SFI should be a per-thread AST
	 */
	if (ast_consume(AST_SFI) == AST_SFI) {
		sfi_ast(thread);
	}
#endif

	/* We are about to return to userspace, there must not be a pending wait */
	assert(waitq_wait_possible(thread));

	/*
	 * We've handled all per-thread ASTs, time to handle non-urgent preemption.
	 *
	 * We delay reading the preemption bits until now in case the thread
	 * blocks while handling per-thread ASTs.
	 *
	 * If one of the AST handlers had managed to set a new AST bit,
	 * thread_exception_return will call ast_taken again.
	 */
	ast_t preemption_reasons = ast_consume(AST_PREEMPTION);

	if (preemption_reasons & AST_PREEMPT) {
		/* Conditions may have changed from when the AST_PREEMPT was originally set, so re-check. */

		thread_lock(thread);
		preemption_reasons = csw_check(thread, current_processor(), (preemption_reasons & AST_QUANTUM));
		thread_unlock(thread);

#if CONFIG_SCHED_SFI
		/* csw_check might tell us that SFI is needed */
		if (preemption_reasons & AST_SFI) {
			sfi_ast(thread);
		}
#endif

		if (preemption_reasons & AST_PREEMPT) {
			/* switching to a continuation implicitly re-enables interrupts */
			thread_block_reason(thread_preempted, NULL, preemption_reasons);
			/* NOTREACHED */
		}

		/*
		 * We previously had a pending AST_PREEMPT, but csw_check
		 * decided that it should no longer be set, and to keep
		 * executing the current thread instead.
		 * Clear the pending preemption timer as we no longer
		 * have a pending AST_PREEMPT to time out.
		 *
		 * TODO: just do the thread block if we see AST_PREEMPT
		 * to avoid taking the pset lock twice.
		 * To do that thread block needs to be smarter
		 * about not context switching when it's not necessary
		 * e.g. the first-timeslice check for queue has priority
		 */
		clear_pending_nonurgent_preemption(current_processor());
	}

	splx(s);

	/*
	 * Here's a good place to put assertions of things which must be true
	 * upon return to userspace.
	 */
	assert(thread->kern_promotion_schedpri == 0);
	if (thread->rwlock_count > 0) {
		panic("rwlock_count is %d for thread %p, possibly it still holds a rwlock", thread->rwlock_count, thread);
	}
	assert(thread->priority_floor_count == 0);

	assert3u(0, ==, thread->sched_flags &
	    (TH_SFLAG_WAITQ_PROMOTED |
	    TH_SFLAG_RW_PROMOTED |
	    TH_SFLAG_EXEC_PROMOTED |
	    TH_SFLAG_FLOOR_PROMOTED |
	    TH_SFLAG_PROMOTED |
	    TH_SFLAG_DEPRESS));
}

/*
 * Set AST flags on current processor
 * Called at splsched
 */
void
ast_on(ast_t reasons)
{
	ast_t *pending_ast = ast_pending();

	*pending_ast |= reasons;
}

/*
 * Clear AST flags on current processor
 * Called at splsched
 */
void
ast_off(ast_t reasons)
{
	ast_t *pending_ast = ast_pending();

	*pending_ast &= ~reasons;
}

/*
 * Consume the requested subset of the AST flags set on the processor
 * Return the bits that were set
 * Called at splsched
 */
ast_t
ast_consume(ast_t reasons)
{
	ast_t *pending_ast = ast_pending();

	reasons &= *pending_ast;
	*pending_ast &= ~reasons;

	return reasons;
}

/*
 * Read the requested subset of the AST flags set on the processor
 * Return the bits that were set, don't modify the processor
 * Called at splsched
 */
ast_t
ast_peek(ast_t reasons)
{
	ast_t *pending_ast = ast_pending();

	reasons &= *pending_ast;

	return reasons;
}

/*
 * Re-set current processor's per-thread AST flags to those set on thread
 * Called at splsched
 */
void
ast_context(thread_t thread)
{
	ast_t *pending_ast = ast_pending();

	*pending_ast = (*pending_ast & ~AST_PER_THREAD) | thread_ast_get(thread);
}

/*
 * Propagate ASTs set on a thread to the current processor
 * Called at splsched
 */
void
ast_propagate(thread_t thread)
{
	ast_on(thread_ast_get(thread));
}

void
ast_dtrace_on(void)
{
	ast_on(AST_DTRACE);
}
