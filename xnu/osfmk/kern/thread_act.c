/*
 * Copyright (c) 2000-2016 Apple Inc. All rights reserved.
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
 * @OSF_FREE_COPYRIGHT@
 */
/*
 * Copyright (c) 1993 The University of Utah and
 * the Center for Software Science (CSS).  All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE UNIVERSITY OF UTAH AND CSS ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSS DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSS requests users of this software to return to css-dist@cs.utah.edu any
 * improvements that they make and grant CSS redistribution rights.
 *
 *	Author:	Bryan Ford, University of Utah CSS
 *
 *	Thread management routines
 */

#include <sys/kdebug.h>
#include <mach/mach_types.h>
#include <mach/kern_return.h>
#include <mach/thread_act_server.h>
#include <mach/thread_act.h>

#include <kern/kern_types.h>
#include <kern/ast.h>
#include <kern/mach_param.h>
#include <kern/zalloc.h>
#include <kern/extmod_statistics.h>
#include <kern/thread.h>
#include <kern/task.h>
#include <kern/sched_prim.h>
#include <kern/misc_protos.h>
#include <kern/assert.h>
#include <kern/exception.h>
#include <kern/ipc_mig.h>
#include <kern/ipc_tt.h>
#include <kern/machine.h>
#include <kern/spl.h>
#include <kern/syscall_subr.h>
#include <kern/processor.h>
#include <kern/restartable.h>
#include <kern/timer.h>
#include <kern/affinity.h>
#include <kern/host.h>
#include <kern/exc_guard.h>
#include <ipc/port.h>
#include <mach/arm/thread_status.h>


#include <stdatomic.h>

#include <security/mac_mach_internal.h>
#include <libkern/coreanalytics/coreanalytics.h>

static void act_abort(thread_t thread);

static void thread_suspended(void *arg, wait_result_t result);
static void thread_set_apc_ast(thread_t thread);
static void thread_set_apc_ast_locked(thread_t thread);

extern void proc_name(int pid, char * buf, int size);
extern boolean_t IOCurrentTaskHasEntitlement(const char *);

CA_EVENT(thread_set_state,
    CA_STATIC_STRING(CA_PROCNAME_LEN), current_proc);

static void
send_thread_set_state_telemetry(void)
{
	ca_event_t ca_event = CA_EVENT_ALLOCATE(thread_set_state);
	CA_EVENT_TYPE(thread_set_state) * event = ca_event->data;

	proc_name(task_pid(current_task()), (char *) &event->current_proc, CA_PROCNAME_LEN);

	CA_EVENT_SEND(ca_event);
}

/* bootarg to create lightweight corpse for thread set state lockdown */
TUNABLE(bool, tss_should_crash, "tss_should_crash", true);

static inline boolean_t
thread_set_state_allowed(thread_t thread, int flavor)
{
	task_t target_task = get_threadtask(thread);

#if DEVELOPMENT || DEBUG
	/* disable the feature if the boot-arg is disabled. */
	if (!tss_should_crash) {
		return TRUE;
	}
#endif /* DEVELOPMENT || DEBUG */

	/* hardened binaries must have entitlement - all others ok */
	if (task_is_hardened_binary(target_task)
	    && !(thread->options & TH_IN_MACH_EXCEPTION)            /* Allowed for now - rdar://103085786 */
	    && FLAVOR_MODIFIES_CORE_CPU_REGISTERS(flavor) /* only care about locking down PC/LR */
#if XNU_TARGET_OS_OSX
	    && !task_opted_out_mach_hardening(target_task)
#endif /* XNU_TARGET_OS_OSX */
#if CONFIG_ROSETTA
	    && !task_is_translated(target_task)  /* Ignore translated tasks */
#endif /* CONFIG_ROSETTA */
	    && !IOCurrentTaskHasEntitlement("com.apple.private.thread-set-state")
	    ) {
		/* fatal crash */
		mach_port_guard_exception(MACH_PORT_NULL, 0, 0, kGUARD_EXC_THREAD_SET_STATE);
		send_thread_set_state_telemetry();
		return FALSE;
	}

#if __has_feature(ptrauth_calls)
	/* Do not allow Fatal PAC exception binaries to set Debug state */
	if (task_is_pac_exception_fatal(target_task)
	    && machine_thread_state_is_debug_flavor(flavor)
#if XNU_TARGET_OS_OSX
	    && !task_opted_out_mach_hardening(target_task)
#endif /* XNU_TARGET_OS_OSX */
#if CONFIG_ROSETTA
	    && !task_is_translated(target_task)      /* Ignore translated tasks */
#endif /* CONFIG_ROSETTA */
	    && !IOCurrentTaskHasEntitlement("com.apple.private.thread-set-state")
	    ) {
		/* fatal crash */
		mach_port_guard_exception(MACH_PORT_NULL, 0, 0, kGUARD_EXC_THREAD_SET_STATE);
		send_thread_set_state_telemetry();
		return FALSE;
	}
#endif /* __has_feature(ptrauth_calls) */

	return TRUE;
}

/*
 * Internal routine to mark a thread as started.
 * Always called with the thread mutex locked.
 */
void
thread_start(
	thread_t                        thread)
{
	clear_wait(thread, THREAD_AWAKENED);
	thread->started = TRUE;
}

/*
 * Internal routine to mark a thread as waiting
 * right after it has been created.  The caller
 * is responsible to call wakeup()/thread_wakeup()
 * or thread_terminate() to get it going.
 *
 * Always called with the thread mutex locked.
 *
 * Task and task_threads mutexes also held
 * (so nobody can set the thread running before
 * this point)
 *
 * Converts TH_UNINT wait to THREAD_INTERRUPTIBLE
 * to allow termination from this point forward.
 */
void
thread_start_in_assert_wait(
	thread_t            thread,
	struct waitq       *waitq,
	event64_t           event,
	wait_interrupt_t    interruptible)
{
	wait_result_t wait_result;
	spl_t spl;

	spl = splsched();
	waitq_lock(waitq);

	/* clear out startup condition (safe because thread not started yet) */
	thread_lock(thread);
	assert(!thread->started);
	assert((thread->state & (TH_WAIT | TH_UNINT)) == (TH_WAIT | TH_UNINT));
	thread->state &= ~(TH_WAIT | TH_UNINT);
	thread_unlock(thread);

	/* assert wait interruptibly forever */
	wait_result = waitq_assert_wait64_locked(waitq, event,
	    interruptible,
	    TIMEOUT_URGENCY_SYS_NORMAL,
	    TIMEOUT_WAIT_FOREVER,
	    TIMEOUT_NO_LEEWAY,
	    thread);
	assert(wait_result == THREAD_WAITING);

	/* mark thread started while we still hold the waitq lock */
	thread_lock(thread);
	thread->started = TRUE;
	thread_unlock(thread);

	waitq_unlock(waitq);
	splx(spl);
}

/*
 * Internal routine to terminate a thread.
 * Sometimes called with task already locked.
 *
 * If thread is on core, cause AST check immediately;
 * Otherwise, let the thread continue running in kernel
 * until it hits AST.
 */
kern_return_t
thread_terminate_internal(
	thread_t                        thread)
{
	kern_return_t           result = KERN_SUCCESS;

	thread_mtx_lock(thread);

	if (thread->active) {
		thread->active = FALSE;

		act_abort(thread);

		if (thread->started) {
			clear_wait(thread, THREAD_INTERRUPTED);
		} else {
			thread_start(thread);
		}
	} else {
		result = KERN_TERMINATED;
	}

	if (thread->affinity_set != NULL) {
		thread_affinity_terminate(thread);
	}

	/* unconditionally unpin the thread in internal termination */
	ipc_thread_port_unpin(get_thread_ro(thread)->tro_self_port);

	thread_mtx_unlock(thread);

	if (thread != current_thread() && result == KERN_SUCCESS) {
		thread_wait(thread, FALSE);
	}

	return result;
}

kern_return_t
thread_terminate(
	thread_t                thread)
{
	task_t task;

	if (thread == THREAD_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	if (thread->state & TH_IDLE) {
		panic("idle thread calling thread_terminate!");
	}

	task = get_threadtask(thread);

	/* Kernel threads can't be terminated without their own cooperation */
	if (task == kernel_task && thread != current_thread()) {
		return KERN_FAILURE;
	}

	kern_return_t result = thread_terminate_internal(thread);

	/*
	 * If a kernel thread is terminating itself, force handle the APC_AST here.
	 * Kernel threads don't pass through the return-to-user AST checking code,
	 * but all threads must finish their own termination in thread_apc_ast.
	 */
	if (task == kernel_task) {
		assert(thread->active == FALSE);
		thread_ast_clear(thread, AST_APC);
		thread_apc_ast(thread);

		panic("thread_terminate");
		/* NOTREACHED */
	}

	return result;
}

/*
 * [MIG Call] Terminate a thread.
 *
 * Cannot be used on threads managed by pthread.
 */
kern_return_t
thread_terminate_from_user(
	thread_t                thread)
{
	if (thread == THREAD_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	if (thread_get_tag(thread) & THREAD_TAG_PTHREAD) {
		return KERN_DENIED;
	}

	return thread_terminate(thread);
}

/*
 * Terminate a thread with pinned control port.
 *
 * Can only be used on threads managed by pthread. Exported in pthread_kern.
 */
kern_return_t
thread_terminate_pinned(
	thread_t                thread)
{
	task_t task;

	if (thread == THREAD_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	task = get_threadtask(thread);


	assert(task != kernel_task);
	assert(thread_get_tag(thread) & (THREAD_TAG_PTHREAD | THREAD_TAG_MAINTHREAD));

	thread_mtx_lock(thread);
	if (task_is_pinned(task) && thread->active) {
		assert(get_thread_ro(thread)->tro_self_port->ip_pinned == 1);
	}
	thread_mtx_unlock(thread);

	kern_return_t result = thread_terminate_internal(thread);
	return result;
}

/*
 * Suspend execution of the specified thread.
 * This is a recursive-style suspension of the thread, a count of
 * suspends is maintained.
 *
 * Called with thread mutex held.
 */
void
thread_hold(thread_t thread)
{
	if (thread->suspend_count++ == 0) {
		thread_set_apc_ast(thread);
		assert(thread->suspend_parked == FALSE);
	}
}

/*
 * Decrement internal suspension count, setting thread
 * runnable when count falls to zero.
 *
 * Because the wait is abortsafe, we can't be guaranteed that the thread
 * is currently actually waiting even if suspend_parked is set.
 *
 * Called with thread mutex held.
 */
void
thread_release(thread_t thread)
{
	assertf(thread->suspend_count > 0, "thread %p over-resumed", thread);

	/* fail-safe on non-assert builds */
	if (thread->suspend_count == 0) {
		return;
	}

	if (--thread->suspend_count == 0) {
		if (!thread->started) {
			thread_start(thread);
		} else if (thread->suspend_parked) {
			thread->suspend_parked = FALSE;
			thread_wakeup_thread(&thread->suspend_count, thread);
		}
	}
}

kern_return_t
thread_suspend(thread_t thread)
{
	kern_return_t result = KERN_SUCCESS;
	int32_t thread_user_stop_count;

	if (thread == THREAD_NULL || get_threadtask(thread) == kernel_task) {
		return KERN_INVALID_ARGUMENT;
	}

	thread_mtx_lock(thread);

	if (thread->active) {
		if (thread->user_stop_count++ == 0) {
			thread_hold(thread);
		}
	} else {
		result = KERN_TERMINATED;
	}
	thread_user_stop_count = thread->user_stop_count;

	thread_mtx_unlock(thread);

	if (result == KERN_SUCCESS) {
		KDBG_RELEASE(MACHDBG_CODE(DBG_MACH_IPC, MACH_THREAD_SUSPEND) | DBG_FUNC_NONE,
		    thread->thread_id, thread_user_stop_count);
	}

	if (thread != current_thread() && result == KERN_SUCCESS) {
		thread_wait(thread, FALSE);
	}

	return result;
}

kern_return_t
thread_resume(thread_t thread)
{
	kern_return_t result = KERN_SUCCESS;
	int32_t thread_user_stop_count;

	if (thread == THREAD_NULL || get_threadtask(thread) == kernel_task) {
		return KERN_INVALID_ARGUMENT;
	}

	thread_mtx_lock(thread);

	if (thread->active) {
		if (thread->user_stop_count > 0) {
			if (--thread->user_stop_count == 0) {
				thread_release(thread);
			}
		} else {
			result = KERN_FAILURE;
		}
	} else {
		result = KERN_TERMINATED;
	}
	thread_user_stop_count = thread->user_stop_count;

	thread_mtx_unlock(thread);

	KDBG_RELEASE(MACHDBG_CODE(DBG_MACH_IPC, MACH_THREAD_RESUME) | DBG_FUNC_NONE,
	    thread->thread_id, thread_user_stop_count, result);

	return result;
}

/*
 *	thread_depress_abort_from_user:
 *
 *	Prematurely abort priority depression if there is one.
 */
kern_return_t
thread_depress_abort_from_user(thread_t thread)
{
	kern_return_t result;

	if (thread == THREAD_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	thread_mtx_lock(thread);

	if (thread->active) {
		result = thread_depress_abort(thread);
	} else {
		result = KERN_TERMINATED;
	}

	thread_mtx_unlock(thread);

	return result;
}


/*
 * Indicate that the thread should run the AST_APC callback
 * to detect an abort condition.
 *
 * Called with thread mutex held.
 */
static void
act_abort(
	thread_t        thread)
{
	spl_t           s = splsched();

	thread_lock(thread);

	if (!(thread->sched_flags & TH_SFLAG_ABORT)) {
		thread->sched_flags |= TH_SFLAG_ABORT;
		thread_set_apc_ast_locked(thread);
		thread_depress_abort_locked(thread);
	} else {
		thread->sched_flags &= ~TH_SFLAG_ABORTSAFELY;
	}

	thread_unlock(thread);
	splx(s);
}

kern_return_t
thread_abort(
	thread_t        thread)
{
	kern_return_t   result = KERN_SUCCESS;

	if (thread == THREAD_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	thread_mtx_lock(thread);

	if (thread->active) {
		act_abort(thread);
		clear_wait(thread, THREAD_INTERRUPTED);
	} else {
		result = KERN_TERMINATED;
	}

	thread_mtx_unlock(thread);

	return result;
}

kern_return_t
thread_abort_safely(
	thread_t                thread)
{
	kern_return_t   result = KERN_SUCCESS;

	if (thread == THREAD_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	thread_mtx_lock(thread);

	if (thread->active) {
		spl_t           s = splsched();

		thread_lock(thread);
		if (!thread->at_safe_point ||
		    clear_wait_internal(thread, THREAD_INTERRUPTED) != KERN_SUCCESS) {
			if (!(thread->sched_flags & TH_SFLAG_ABORT)) {
				thread->sched_flags |= TH_SFLAG_ABORTED_MASK;
				thread_set_apc_ast_locked(thread);
				thread_depress_abort_locked(thread);
			}
		}
		thread_unlock(thread);
		splx(s);
	} else {
		result = KERN_TERMINATED;
	}

	thread_mtx_unlock(thread);

	return result;
}

/*** backward compatibility hacks ***/
#include <mach/thread_info.h>
#include <mach/thread_special_ports.h>
#include <ipc/ipc_port.h>

kern_return_t
thread_info(
	thread_t                        thread,
	thread_flavor_t                 flavor,
	thread_info_t                   thread_info_out,
	mach_msg_type_number_t  *thread_info_count)
{
	kern_return_t                   result;

	if (thread == THREAD_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	thread_mtx_lock(thread);

	if (thread->active || thread->inspection) {
		result = thread_info_internal(
			thread, flavor, thread_info_out, thread_info_count);
	} else {
		result = KERN_TERMINATED;
	}

	thread_mtx_unlock(thread);

	return result;
}

static inline kern_return_t
thread_get_state_internal(
	thread_t                thread,
	int                                             flavor,
	thread_state_t                  state,                  /* pointer to OUT array */
	mach_msg_type_number_t  *state_count,   /*IN/OUT*/
	thread_set_status_flags_t  flags)
{
	kern_return_t           result = KERN_SUCCESS;
	boolean_t               to_user = !!(flags & TSSF_TRANSLATE_TO_USER);

	if (thread == THREAD_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	thread_mtx_lock(thread);

	if (thread->active) {
		if (thread != current_thread()) {
			thread_hold(thread);

			thread_mtx_unlock(thread);

			if (thread_stop(thread, FALSE)) {
				thread_mtx_lock(thread);
				result = machine_thread_get_state(
					thread, flavor, state, state_count);
				thread_unstop(thread);
			} else {
				thread_mtx_lock(thread);
				result = KERN_ABORTED;
			}

			thread_release(thread);
		} else {
			result = machine_thread_get_state(
				thread, flavor, state, state_count);
		}
	} else if (thread->inspection) {
		result = machine_thread_get_state(
			thread, flavor, state, state_count);
	} else {
		result = KERN_TERMINATED;
	}

	if (to_user && result == KERN_SUCCESS) {
		result = machine_thread_state_convert_to_user(thread, flavor, state,
		    state_count, flags);
	}

	thread_mtx_unlock(thread);

	return result;
}

/* No prototype, since thread_act_server.h has the _to_user version if KERNEL_SERVER */

kern_return_t
thread_get_state(
	thread_t                thread,
	int                                             flavor,
	thread_state_t                  state,
	mach_msg_type_number_t  *state_count);

kern_return_t
thread_get_state(
	thread_t                thread,
	int                                             flavor,
	thread_state_t                  state,                  /* pointer to OUT array */
	mach_msg_type_number_t  *state_count)   /*IN/OUT*/
{
	return thread_get_state_internal(thread, flavor, state, state_count, TSSF_FLAGS_NONE);
}

kern_return_t
thread_get_state_to_user(
	thread_t                thread,
	int                                             flavor,
	thread_state_t                  state,                  /* pointer to OUT array */
	mach_msg_type_number_t  *state_count)   /*IN/OUT*/
{
	return thread_get_state_internal(thread, flavor, state, state_count, TSSF_TRANSLATE_TO_USER);
}

/*
 *	Change thread's machine-dependent state.  Called with nothing
 *	locked.  Returns same way.
 */
static inline kern_return_t
thread_set_state_internal(
	thread_t                        thread,
	int                             flavor,
	thread_state_t                  state,
	mach_msg_type_number_t          state_count,
	thread_state_t                  old_state,
	mach_msg_type_number_t          old_state_count,
	thread_set_status_flags_t       flags)
{
	kern_return_t           result = KERN_SUCCESS;
	boolean_t               from_user = !!(flags & TSSF_TRANSLATE_TO_USER);

	if (thread == THREAD_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	if ((flags & TSSF_CHECK_ENTITLEMENT) &&
	    !thread_set_state_allowed(thread, flavor)) {
		return KERN_NO_ACCESS;
	}

	thread_mtx_lock(thread);

	if (thread->active) {
		if (from_user) {
			result = machine_thread_state_convert_from_user(thread, flavor,
			    state, state_count, old_state, old_state_count, flags);
			if (result != KERN_SUCCESS) {
				goto out;
			}
		}
		if (thread != current_thread()) {
			thread_hold(thread);

			thread_mtx_unlock(thread);

			if (thread_stop(thread, TRUE)) {
				thread_mtx_lock(thread);
				result = machine_thread_set_state(
					thread, flavor, state, state_count);
				thread_unstop(thread);
			} else {
				thread_mtx_lock(thread);
				result = KERN_ABORTED;
			}

			thread_release(thread);
		} else {
			result = machine_thread_set_state(
				thread, flavor, state, state_count);
		}
	} else {
		result = KERN_TERMINATED;
	}

	if ((result == KERN_SUCCESS) && from_user) {
		extmod_statistics_incr_thread_set_state(thread);
	}

out:
	thread_mtx_unlock(thread);

	return result;
}

/* No prototype, since thread_act_server.h has the _from_user version if KERNEL_SERVER */
kern_return_t
thread_set_state(
	thread_t                thread,
	int                                             flavor,
	thread_state_t                  state,
	mach_msg_type_number_t  state_count);

kern_return_t
thread_set_state(
	thread_t                thread,
	int                                             flavor,
	thread_state_t                  state,
	mach_msg_type_number_t  state_count)
{
	return thread_set_state_internal(thread, flavor, state, state_count, NULL, 0, TSSF_FLAGS_NONE);
}

kern_return_t
thread_set_state_from_user(
	thread_t                thread,
	int                                             flavor,
	thread_state_t                  state,
	mach_msg_type_number_t  state_count)
{
	return thread_set_state_internal(thread, flavor, state, state_count, NULL,
	           0, TSSF_TRANSLATE_TO_USER | TSSF_CHECK_ENTITLEMENT);
}

kern_return_t
thread_convert_thread_state(
	thread_t                thread,
	int                     direction,
	thread_state_flavor_t   flavor,
	thread_state_t          in_state,          /* pointer to IN array */
	mach_msg_type_number_t  in_state_count,
	thread_state_t          out_state,         /* pointer to OUT array */
	mach_msg_type_number_t  *out_state_count)   /*IN/OUT*/
{
	kern_return_t kr;
	thread_t to_thread = THREAD_NULL;
	thread_t from_thread = THREAD_NULL;
	mach_msg_type_number_t state_count = in_state_count;

	if (direction != THREAD_CONVERT_THREAD_STATE_TO_SELF &&
	    direction != THREAD_CONVERT_THREAD_STATE_FROM_SELF) {
		return KERN_INVALID_ARGUMENT;
	}

	if (thread == THREAD_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	if (state_count > *out_state_count) {
		return KERN_INSUFFICIENT_BUFFER_SIZE;
	}

	if (direction == THREAD_CONVERT_THREAD_STATE_FROM_SELF) {
		to_thread = thread;
		from_thread = current_thread();
	} else {
		to_thread = current_thread();
		from_thread = thread;
	}

	/* Authenticate and convert thread state to kernel representation */
	kr = machine_thread_state_convert_from_user(from_thread, flavor,
	    in_state, state_count, NULL, 0, TSSF_FLAGS_NONE);

	/* Return early if one of the thread was jop disabled while other wasn't */
	if (kr != KERN_SUCCESS) {
		return kr;
	}

	/* Convert thread state to target thread user representation */
	kr = machine_thread_state_convert_to_user(to_thread, flavor,
	    in_state, &state_count, TSSF_PRESERVE_FLAGS);

	if (kr == KERN_SUCCESS) {
		if (state_count <= *out_state_count) {
			memcpy(out_state, in_state, state_count * sizeof(uint32_t));
			*out_state_count = state_count;
		} else {
			kr = KERN_INSUFFICIENT_BUFFER_SIZE;
		}
	}

	return kr;
}

/*
 * Kernel-internal "thread" interfaces used outside this file:
 */

/* Initialize (or re-initialize) a thread state.  Called from execve
 * with nothing locked, returns same way.
 */
kern_return_t
thread_state_initialize(
	thread_t                thread)
{
	kern_return_t           result = KERN_SUCCESS;

	if (thread == THREAD_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	thread_mtx_lock(thread);

	if (thread->active) {
		if (thread != current_thread()) {
			/* Thread created in exec should be blocked in UNINT wait */
			assert(!(thread->state & TH_RUN));
		}
		machine_thread_state_initialize( thread );
	} else {
		result = KERN_TERMINATED;
	}

	thread_mtx_unlock(thread);

	return result;
}

kern_return_t
thread_dup(
	thread_t        target)
{
	thread_t                        self = current_thread();
	kern_return_t           result = KERN_SUCCESS;

	if (target == THREAD_NULL || target == self) {
		return KERN_INVALID_ARGUMENT;
	}

	thread_mtx_lock(target);

	if (target->active) {
		thread_hold(target);

		thread_mtx_unlock(target);

		if (thread_stop(target, TRUE)) {
			thread_mtx_lock(target);
			result = machine_thread_dup(self, target, FALSE);

			if (self->affinity_set != AFFINITY_SET_NULL) {
				thread_affinity_dup(self, target);
			}
			thread_unstop(target);
		} else {
			thread_mtx_lock(target);
			result = KERN_ABORTED;
		}

		thread_release(target);
	} else {
		result = KERN_TERMINATED;
	}

	thread_mtx_unlock(target);

	return result;
}


kern_return_t
thread_dup2(
	thread_t        source,
	thread_t        target)
{
	kern_return_t           result = KERN_SUCCESS;
	uint32_t                active = 0;

	if (source == THREAD_NULL || target == THREAD_NULL || target == source) {
		return KERN_INVALID_ARGUMENT;
	}

	thread_mtx_lock(source);
	active = source->active;
	thread_mtx_unlock(source);

	if (!active) {
		return KERN_TERMINATED;
	}

	thread_mtx_lock(target);

	if (target->active || target->inspection) {
		thread_hold(target);

		thread_mtx_unlock(target);

		if (thread_stop(target, TRUE)) {
			thread_mtx_lock(target);
			result = machine_thread_dup(source, target, TRUE);
			if (source->affinity_set != AFFINITY_SET_NULL) {
				thread_affinity_dup(source, target);
			}
			thread_unstop(target);
		} else {
			thread_mtx_lock(target);
			result = KERN_ABORTED;
		}

		thread_release(target);
	} else {
		result = KERN_TERMINATED;
	}

	thread_mtx_unlock(target);

	return result;
}

/*
 *	thread_setstatus:
 *
 *	Set the status of the specified thread.
 *	Called with (and returns with) no locks held.
 */
kern_return_t
thread_setstatus(
	thread_t                thread,
	int                                             flavor,
	thread_state_t                  tstate,
	mach_msg_type_number_t  count)
{
	return thread_set_state(thread, flavor, tstate, count);
}

kern_return_t
thread_setstatus_from_user(
	thread_t                thread,
	int                                             flavor,
	thread_state_t                  tstate,
	mach_msg_type_number_t  count,
	thread_state_t                  old_tstate,
	mach_msg_type_number_t  old_count,
	thread_set_status_flags_t flags)
{
	return thread_set_state_internal(thread, flavor, tstate, count, old_tstate,
	           old_count, flags | TSSF_TRANSLATE_TO_USER);
}

/*
 *	thread_getstatus:
 *
 *	Get the status of the specified thread.
 */
kern_return_t
thread_getstatus(
	thread_t                thread,
	int                                             flavor,
	thread_state_t                  tstate,
	mach_msg_type_number_t  *count)
{
	return thread_get_state(thread, flavor, tstate, count);
}

kern_return_t
thread_getstatus_to_user(
	thread_t                thread,
	int                                             flavor,
	thread_state_t                  tstate,
	mach_msg_type_number_t  *count,
	thread_set_status_flags_t flags)
{
	return thread_get_state_internal(thread, flavor, tstate, count, flags | TSSF_TRANSLATE_TO_USER);
}

/*
 *	Change thread's machine-dependent userspace TSD base.
 *  Called with nothing locked.  Returns same way.
 */
kern_return_t
thread_set_tsd_base(
	thread_t                        thread,
	mach_vm_offset_t        tsd_base)
{
	kern_return_t           result = KERN_SUCCESS;

	if (thread == THREAD_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	thread_mtx_lock(thread);

	if (thread->active) {
		if (thread != current_thread()) {
			thread_hold(thread);

			thread_mtx_unlock(thread);

			if (thread_stop(thread, TRUE)) {
				thread_mtx_lock(thread);
				result = machine_thread_set_tsd_base(thread, tsd_base);
				thread_unstop(thread);
			} else {
				thread_mtx_lock(thread);
				result = KERN_ABORTED;
			}

			thread_release(thread);
		} else {
			result = machine_thread_set_tsd_base(thread, tsd_base);
		}
	} else {
		result = KERN_TERMINATED;
	}

	thread_mtx_unlock(thread);

	return result;
}

/*
 * thread_set_apc_ast:
 *
 * Register the AST_APC callback that handles suspension and
 * termination, if it hasn't been installed already.
 *
 * Called with the thread mutex held.
 */
static void
thread_set_apc_ast(thread_t thread)
{
	spl_t s = splsched();

	thread_lock(thread);
	thread_set_apc_ast_locked(thread);
	thread_unlock(thread);

	splx(s);
}

/*
 * thread_set_apc_ast_locked:
 *
 * Do the work of registering for the AST_APC callback.
 *
 * Called with the thread mutex and scheduling lock held.
 */
static void
thread_set_apc_ast_locked(thread_t thread)
{
	thread_ast_set(thread, AST_APC);

	if (thread == current_thread()) {
		ast_propagate(thread);
	} else {
		processor_t processor = thread->last_processor;

		if (processor != PROCESSOR_NULL &&
		    processor->state == PROCESSOR_RUNNING &&
		    processor->active_thread == thread) {
			cause_ast_check(processor);
		}
	}
}

/*
 * Activation control support routines internal to this file:
 *
 */

/*
 * thread_suspended
 *
 * Continuation routine for thread suspension.  It checks
 * to see whether there has been any new suspensions.  If so, it
 * installs the AST_APC handler again.
 */
__attribute__((noreturn))
static void
thread_suspended(__unused void *parameter, wait_result_t result)
{
	thread_t thread = current_thread();

	thread_mtx_lock(thread);

	if (result == THREAD_INTERRUPTED) {
		thread->suspend_parked = FALSE;
	} else {
		assert(thread->suspend_parked == FALSE);
	}

	if (thread->suspend_count > 0) {
		thread_set_apc_ast(thread);
	}

	thread_mtx_unlock(thread);

	thread_exception_return();
	/*NOTREACHED*/
}

/*
 * thread_apc_ast - handles AST_APC and drives thread suspension and termination.
 * Called with nothing locked.  Returns (if it returns) the same way.
 */
void
thread_apc_ast(thread_t thread)
{
	thread_mtx_lock(thread);

	assert(thread->suspend_parked == FALSE);

	spl_t s = splsched();
	thread_lock(thread);

	/* TH_SFLAG_POLLDEPRESS is OK to have here */
	assert((thread->sched_flags & TH_SFLAG_DEPRESS) == 0);

	thread->sched_flags &= ~TH_SFLAG_ABORTED_MASK;
	thread_unlock(thread);
	splx(s);

	if (!thread->active) {
		/* Thread is ready to terminate, time to tear it down */
		thread_mtx_unlock(thread);

		thread_terminate_self();
		/*NOTREACHED*/
	}

	/* If we're suspended, go to sleep and wait for someone to wake us up. */
	if (thread->suspend_count > 0) {
		thread->suspend_parked = TRUE;
		assert_wait(&thread->suspend_count,
		    THREAD_ABORTSAFE | THREAD_WAIT_NOREPORT_USER);
		thread_mtx_unlock(thread);

		thread_block(thread_suspended);
		/*NOTREACHED*/
	}

	thread_mtx_unlock(thread);
}

#if CONFIG_ROSETTA
extern kern_return_t
exception_deliver(
	thread_t                thread,
	exception_type_t        exception,
	mach_exception_data_t   code,
	mach_msg_type_number_t  codeCnt,
	struct exception_action *excp,
	lck_mtx_t               *mutex);

kern_return_t
thread_raise_exception(
	thread_t thread,
	exception_type_t exception,
	natural_t code_count,
	int64_t code,
	int64_t sub_code)
{
	task_t task;

	if (thread == THREAD_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	task = get_threadtask(thread);

	if (task != current_task()) {
		return KERN_FAILURE;
	}

	if (!task_is_translated(task)) {
		return KERN_FAILURE;
	}

	if (exception == EXC_CRASH) {
		return KERN_INVALID_ARGUMENT;
	}

	int64_t codes[] = { code, sub_code };
	host_priv_t host_priv = host_priv_self();
	kern_return_t kr = exception_deliver(thread, exception, codes, code_count, host_priv->exc_actions, &host_priv->lock);
	if (kr != KERN_SUCCESS) {
		return kr;
	}

	return thread_resume(thread);
}
#endif

void
thread_debug_return_to_user_ast(
	thread_t thread)
{
#pragma unused(thread)
#if MACH_ASSERT
	if ((thread->sched_flags & TH_SFLAG_RW_PROMOTED) ||
	    thread->rwlock_count > 0) {
		panic("Returning to userspace with rw lock held, thread %p sched_flag %u rwlock_count %d", thread, thread->sched_flags, thread->rwlock_count);
	}

	if ((thread->sched_flags & TH_SFLAG_FLOOR_PROMOTED) ||
	    thread->priority_floor_count > 0) {
		panic("Returning to userspace with floor boost set, thread %p sched_flag %u priority_floor_count %d", thread, thread->sched_flags, thread->priority_floor_count);
	}

	if (thread->th_vm_faults_disabled) {
		panic("Returning to userspace with vm faults disabled, thread %p", thread);
	}

#if CONFIG_EXCLAVES
	assert3u(thread->th_exclaves_state & TH_EXCLAVES_STATE_ANY, ==, 0);
#endif /* CONFIG_EXCLAVES */

#endif /* MACH_ASSERT */
}


/* Prototype, see justification above */
kern_return_t
act_set_state(
	thread_t                                thread,
	int                                             flavor,
	thread_state_t                  state,
	mach_msg_type_number_t  count);

kern_return_t
act_set_state(
	thread_t                                thread,
	int                                             flavor,
	thread_state_t                  state,
	mach_msg_type_number_t  count)
{
	if (thread == current_thread()) {
		return KERN_INVALID_ARGUMENT;
	}

	return thread_set_state(thread, flavor, state, count);
}

kern_return_t
act_set_state_from_user(
	thread_t                                thread,
	int                                             flavor,
	thread_state_t                  state,
	mach_msg_type_number_t  count)
{
	if (thread == current_thread()) {
		return KERN_INVALID_ARGUMENT;
	}

	return thread_set_state_from_user(thread, flavor, state, count);
}

/* Prototype, see justification above */
kern_return_t
act_get_state(
	thread_t                                thread,
	int                                             flavor,
	thread_state_t                  state,
	mach_msg_type_number_t  *count);

kern_return_t
act_get_state(
	thread_t                                thread,
	int                                             flavor,
	thread_state_t                  state,
	mach_msg_type_number_t  *count)
{
	if (thread == current_thread()) {
		return KERN_INVALID_ARGUMENT;
	}

	return thread_get_state(thread, flavor, state, count);
}

kern_return_t
act_get_state_to_user(
	thread_t                                thread,
	int                                             flavor,
	thread_state_t                  state,
	mach_msg_type_number_t  *count)
{
	if (thread == current_thread()) {
		return KERN_INVALID_ARGUMENT;
	}

	return thread_get_state_to_user(thread, flavor, state, count);
}

static void
act_set_ast(
	thread_t   thread,
	ast_t      ast)
{
	spl_t s = splsched();

	if (thread == current_thread()) {
		thread_ast_set(thread, ast);
		ast_propagate(thread);
	} else {
		processor_t processor;

		thread_lock(thread);
		thread_ast_set(thread, ast);
		processor = thread->last_processor;
		if (processor != PROCESSOR_NULL &&
		    processor->state == PROCESSOR_RUNNING &&
		    processor->active_thread == thread) {
			cause_ast_check(processor);
		}
		thread_unlock(thread);
	}

	splx(s);
}

/*
 * set AST on thread without causing an AST check
 * and without taking the thread lock
 *
 * If thread is not the current thread, then it may take
 * up until the next context switch or quantum expiration
 * on that thread for it to notice the AST.
 */
static void
act_set_ast_async(thread_t  thread,
    ast_t     ast)
{
	thread_ast_set(thread, ast);

	if (thread == current_thread()) {
		spl_t s = splsched();
		ast_propagate(thread);
		splx(s);
	}
}

void
act_set_debug_assert(void)
{
	thread_t thread = current_thread();
	if (thread_ast_peek(thread, AST_DEBUG_ASSERT) != AST_DEBUG_ASSERT) {
		thread_ast_set(thread, AST_DEBUG_ASSERT);
	}
	if (ast_peek(AST_DEBUG_ASSERT) != AST_DEBUG_ASSERT) {
		spl_t s = splsched();
		ast_propagate(thread);
		splx(s);
	}
}

void
act_set_astbsd(thread_t thread)
{
	act_set_ast(thread, AST_BSD);
}

void
act_set_astkevent(thread_t thread, uint16_t bits)
{
	os_atomic_or(&thread->kevent_ast_bits, bits, relaxed);

	/* kevent AST shouldn't send immediate IPIs */
	act_set_ast_async(thread, AST_KEVENT);
}

uint16_t
act_clear_astkevent(thread_t thread, uint16_t bits)
{
	/*
	 * avoid the atomic operation if none of the bits is set,
	 * which will be the common case.
	 */
	uint16_t cur = os_atomic_load(&thread->kevent_ast_bits, relaxed);
	if (cur & bits) {
		cur = os_atomic_andnot_orig(&thread->kevent_ast_bits, bits, relaxed);
	}
	return cur & bits;
}

bool
act_set_ast_reset_pcs(task_t task, thread_t thread)
{
	processor_t processor;
	bool needs_wait = false;
	spl_t s;

	s = splsched();

	if (thread == current_thread()) {
		/*
		 * this is called from the signal code,
		 * just set the AST and move on
		 */
		thread_ast_set(thread, AST_RESET_PCS);
		ast_propagate(thread);
	} else {
		thread_lock(thread);

		assert(thread->t_rr_state.trr_ipi_ack_pending == 0);
		assert(thread->t_rr_state.trr_sync_waiting == 0);

		processor = thread->last_processor;
		if (!thread->active) {
			/*
			 * ->active is being set before the thread is added
			 * to the thread list (under the task lock which
			 * the caller holds), and is reset before the thread
			 * lock is being taken by thread_terminate_self().
			 *
			 * The result is that this will never fail to
			 * set the AST on an thread that is active,
			 * but will not set it past thread_terminate_self().
			 */
		} else if (processor != PROCESSOR_NULL &&
		    processor->state == PROCESSOR_RUNNING &&
		    processor->active_thread == thread) {
			thread->t_rr_state.trr_ipi_ack_pending = true;
			needs_wait = true;
			thread_ast_set(thread, AST_RESET_PCS);
			cause_ast_check(processor);
		} else if (thread_reset_pcs_in_range(task, thread)) {
			if (thread->t_rr_state.trr_fault_state) {
				thread->t_rr_state.trr_fault_state =
				    TRR_FAULT_OBSERVED;
				needs_wait = true;
			}
			thread_ast_set(thread, AST_RESET_PCS);
		}
		thread_unlock(thread);
	}

	splx(s);

	return needs_wait;
}

void
act_set_kperf(thread_t thread)
{
	/* safety check */
	if (thread != current_thread()) {
		if (!ml_get_interrupts_enabled()) {
			panic("unsafe act_set_kperf operation");
		}
	}

	act_set_ast(thread, AST_KPERF);
}

#if CONFIG_MACF
void
act_set_astmacf(
	thread_t        thread)
{
	act_set_ast( thread, AST_MACF);
}
#endif

void
act_set_astledger(thread_t thread)
{
	act_set_ast(thread, AST_LEDGER);
}

/*
 * The ledger AST may need to be set while already holding
 * the thread lock.  This routine skips sending the IPI,
 * allowing us to avoid the lock hold.
 *
 * However, it means the targeted thread must context switch
 * to recognize the ledger AST.
 */
void
act_set_astledger_async(thread_t thread)
{
	act_set_ast_async(thread, AST_LEDGER);
}

void
act_set_io_telemetry_ast(thread_t thread)
{
	act_set_ast(thread, AST_TELEMETRY_IO);
}

void
act_set_macf_telemetry_ast(thread_t thread)
{
	act_set_ast(thread, AST_TELEMETRY_MACF);
}

void
act_set_astproc_resource(thread_t thread)
{
	act_set_ast(thread, AST_PROC_RESOURCE);
}
