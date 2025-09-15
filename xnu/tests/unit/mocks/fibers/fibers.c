/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
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

#define _XOPEN_SOURCE // To use *context deprecated API on OSX
#define BSD_KERNEL_PRIVATE

#include "fibers.h"
#include "random.h"

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/signal.h>

#ifdef __BUILDING_WITH_TSAN__
#include <sanitizer/tsan_interface.h>
#endif
#ifdef __BUILDING_WITH_ASAN__
#include <sanitizer/asan_interface.h>
#endif

// from ucontext.h
#include <sys/_types/_ucontext.h>
extern void makecontext(ucontext_t *ucp, void (*func)(), int argc, ...);
extern int swapcontext(ucontext_t *oucp, const ucontext_t *ucp);
extern int getcontext(ucontext_t *ucp);
extern int setcontext(const ucontext_t *ucp);

int fibers_log_level;
bool fibers_debug;
int fibers_abort_on_error = 0;

uint64_t fibers_may_yield_probability = FIBERS_DEFAULT_YIELD_PROB;

struct fiber_context fibers_main = {
	.id = 0,
	.state = FIBER_RUN,
};
static int fibers_last_forged_id = 0;

fiber_t fibers_current = &fibers_main; /* currently running */
struct fibers_queue fibers_run_queue; /* ready to be scheduled */
struct fibers_queue fibers_existing_queue = { .top = &fibers_main, .count = 1 }; /* existing fibers */

static void
fibers_default_choose_next(__unused void *arg, int state)
{
	fibers_switch_random(state);
}

static bool
fibers_default_should_yield(__unused void *arg, uint64_t probability, __unused fiber_yield_reason_t reason)
{
	return probability && random_below(probability) == 0;
}

struct fibers_scheduler_t fibers_default_scheduler = {
	.fibers_choose_next = &fibers_default_choose_next,
	.fibers_should_yield = &fibers_default_should_yield
};

struct fibers_scheduler_t *fibers_scheduler = &fibers_default_scheduler;
void *fibers_scheduler_context = 0;

void
fibers_scheduler_get(struct fibers_scheduler_t **scheduler, void **context)
{
	*scheduler = fibers_scheduler;
	*context = fibers_scheduler_context;
}

void
fibers_scheduler_set(struct fibers_scheduler_t *scheduler, void *context)
{
	fibers_scheduler = scheduler;
	fibers_scheduler_context = context;
}

struct fibers_create_trampoline_args {
	fiber_t fiber;
	void *start_routine_arg;
	jmp_buf parent_env;
};

static void
fibers_create_trampoline(int arg1, int arg2)
{
	struct fibers_create_trampoline_args *args = (struct fibers_create_trampoline_args *)(((uintptr_t)arg1 << 32) | (uintptr_t)arg2);
	// Copy fiber and arg to the local scope as by the time start_routine is called the parent fibers_create stack may have been deallocated
	fiber_t fiber = args->fiber;
	void *start_routine_arg = args->start_routine_arg;

    #ifdef __BUILDING_WITH_ASAN__
	__sanitizer_finish_switch_fiber(&fiber->sanitizer_fake_stack, &fiber->stack_bottom, &fiber->stack_size);
    #endif

	// setjmp/longjmp are faster context switch primitives compared to swapcontext
	if (setjmp(fiber->env) == 0) {
		// The first time the setjmp is called to save the current context in fiber->env
		// we end un in this branch in which we switch back to fibers_create
		// When the fiber will be scheduled for the first time, setjmp(fiber->env) != 0
		// and thus the execution will continue in the other branch that calls args.start_routine
#ifdef __BUILDING_WITH_ASAN__
		__sanitizer_start_switch_fiber(&fibers_current->sanitizer_fake_stack, fibers_current->stack_bottom, fibers_current->stack_size);
#endif
#ifdef __BUILDING_WITH_TSAN__
		__tsan_switch_to_fiber(fibers_current->tsan_fiber, 0);
#endif
		longjmp(args->parent_env, 1337);
	}

    #ifdef __BUILDING_WITH_ASAN__
	__sanitizer_finish_switch_fiber(&fiber->sanitizer_fake_stack, &fiber->stack_bottom, &fiber->stack_size);
    #endif

	fibers_current = fiber;
	FIBERS_LOG(FIBERS_LOG_INFO, "starting to execute the routine");

	void *ret_value = fiber->start_routine(start_routine_arg);
	fibers_exit(ret_value);
}

fiber_t
fibers_create(size_t stack_size, void* (*start_routine)(void*), void* arg)
{
	if (fibers_current == &fibers_main && fibers_main.stack_bottom == NULL) {
		// fibers_main has no stack_bottom or stack_size, get them here the first time
		void* stackaddr = pthread_get_stackaddr_np(pthread_self());
		size_t stacksize = pthread_get_stacksize_np(pthread_self());
		fibers_main.stack_bottom = stackaddr - stacksize;
		fibers_main.stack_size = stacksize;

#ifdef __BUILDING_WITH_TSAN__
		fibers_main.tsan_fiber = __tsan_get_current_fiber();
		__tsan_set_fiber_name(fibers_main.tsan_fiber, "fiber0");
#endif
	}

	void *stack_addr = malloc(stack_size);

	fiber_t fiber = calloc(1, sizeof(struct fiber_context));
	fiber->id = ++fibers_last_forged_id;
	FIBERS_ASSERT(fibers_last_forged_id != 0, "fibers_create: new fiber id integer overflow");
	fiber->state = FIBER_STOP;
	fiber->start_routine = start_routine;
	fiber->stack_size = stack_size;
	fiber->stack_bottom = stack_addr;
	FIBERS_ASSERT(fiber->stack_bottom, "fibers_create: stack malloc failed");

#ifdef __BUILDING_WITH_TSAN__
	fiber->tsan_fiber = __tsan_create_fiber(0);
	char tsan_fiber_name[32];
	snprintf(tsan_fiber_name, 32, "fiber%d", fiber->id);
	__tsan_set_fiber_name(fiber->tsan_fiber, tsan_fiber_name);
#endif

	ucontext_t tmp_uc;
	ucontext_t child_uc = {0};
	FIBERS_ASSERT(getcontext(&child_uc) == 0, "fibers_create: getcontext");
	child_uc.uc_stack.ss_sp = stack_addr;
	child_uc.uc_stack.ss_size = stack_size;
	child_uc.uc_link = 0;

	struct fibers_create_trampoline_args trampoline_args = {0};
	trampoline_args.fiber = fiber;
	trampoline_args.start_routine_arg = arg;

	int trampoline_args1 = (int)((uintptr_t)&trampoline_args >> 32);
	int trampoline_args2 = (int)((uintptr_t)&trampoline_args);

	makecontext(&child_uc, (void (*)())fibers_create_trampoline, 2, trampoline_args1, trampoline_args2);

	// switch to the trampoline to setup the setjmp env of the fiber on the newly created stack, then switch back
	// setjmp/longjmp are faster context switch primitives, swapcontext will never be used again for this fiber
	// ref. the ThreadSanitizer fibers example in LLVM at compiler-rt/test/tsan/fiber_longjmp.cpp
	if (setjmp(trampoline_args.parent_env) == 0) {
#ifdef __BUILDING_WITH_ASAN__
		__sanitizer_start_switch_fiber(&fiber->sanitizer_fake_stack, fiber->stack_bottom, fiber->stack_size);
#endif
#ifdef __BUILDING_WITH_TSAN__
		__tsan_switch_to_fiber(fiber->tsan_fiber, 0);
#endif
		FIBERS_ASSERT(swapcontext(&tmp_uc, &child_uc) == 0, "fibers_create: swapcontext");
	}

#ifdef __BUILDING_WITH_ASAN__
	// fibers_create_trampoline did not change fibers_current
	__sanitizer_finish_switch_fiber(&fibers_current->sanitizer_fake_stack, &fibers_current->stack_bottom, &fibers_current->stack_size);
#endif

	fibers_queue_push(&fibers_run_queue, fiber);
	fibers_existing_push(fiber);

	FIBERS_LOG(FIBERS_LOG_INFO, "fiber %d created", fiber->id);

	/* chance to schedule the newly created fiber */
	fibers_may_yield_internal_with_reason(FIBERS_YIELD_REASON_CREATE);
	return fiber;
}

static void
fibers_dispose(fiber_t fiber)
{
	FIBERS_LOG(FIBERS_LOG_DEBUG, "dispose %d", fiber->id);

	fibers_existing_remove(fiber);

#ifdef __BUILDING_WITH_TSAN__
	__tsan_destroy_fiber(fiber->tsan_fiber);
#endif

	if (fiber->extra_cleanup_routine) {
		fiber->extra_cleanup_routine(fiber->extra);
	}

	free((void*)fiber->stack_bottom);
	free(fiber);
}

void
fibers_exit(void *ret_value)
{
	FIBERS_ASSERT(fibers_current->may_yield_disabled == 0, "fibers_exit: fibers_current->may_yield_disabled is not 0");

	fibers_current->ret_value = ret_value;
	if (fibers_current->joiner) {
		FIBERS_LOG(FIBERS_LOG_INFO, "exiting, joined by %d", fibers_current->joiner->id);
		fibers_queue_push(&fibers_run_queue, fibers_current->joiner);
	} else {
		FIBERS_LOG(FIBERS_LOG_INFO, "exiting, no joiner");
	}

	fibers_choose_next(FIBER_DEAD);
	FIBERS_ASSERT(false, "fibers_exit: unreachable");
}

void *
fibers_join(fiber_t target)
{
	FIBERS_ASSERT(fibers_current->may_yield_disabled == 0, "fibers_join: fibers_current->may_yield_disabled is not 0");

	fibers_may_yield_internal_with_reason(FIBERS_YIELD_REASON_JOIN | FIBERS_YIELD_REASON_ORDER_PRE);

	FIBERS_LOG(FIBERS_LOG_INFO, "join %d", target->id);
	if (target->state != FIBER_DEAD) {
		FIBERS_ASSERT(target->joiner == NULL, "fibers_join: %d already joined by %d", target->id, target->joiner->id);

		target->joiner = fibers_current;
		fibers_current->joining = target;

		// RANGELOCKINGTODO rdar://150845975 maybe have a queue for fibers in join to output debug info in case of deadlock
		fibers_choose_next(FIBER_JOIN);
	}

	FIBERS_LOG(FIBERS_LOG_INFO, "finish joining %d", target->id);
	FIBERS_ASSERT(target->state == FIBER_DEAD, "fibers_join: not dead");

	void *ret_value = target->ret_value;
	fibers_dispose(target);

	fibers_may_yield_internal_with_reason(FIBERS_YIELD_REASON_JOIN | FIBERS_YIELD_REASON_ORDER_POST);
	return ret_value;
}

void
fibers_switch_helper(fiber_t target, int state)
{
	if (target == fibers_current) {
		target->state = FIBER_RUN;
		return;
	}
	FIBERS_LOG(FIBERS_LOG_TRACE, "switch to %d, state=%d", target->id, state);

	fibers_current->state = state;
	fiber_t save = fibers_current;

	if (setjmp(save->env) == 0) {
#ifdef __BUILDING_WITH_ASAN__
		__sanitizer_start_switch_fiber(&target->sanitizer_fake_stack, target->stack_bottom, target->stack_size);
#endif
#ifdef __BUILDING_WITH_TSAN__
		__tsan_switch_to_fiber(target->tsan_fiber, state == FIBER_DEAD ? 0 : __tsan_switch_to_fiber_no_sync);
#endif
		longjmp(target->env, 1337);
	}
#ifdef __BUILDING_WITH_ASAN__
	__sanitizer_finish_switch_fiber(&save->sanitizer_fake_stack, &save->stack_bottom, &save->stack_size);
#endif

	fibers_current = save;
	save->state = FIBER_RUN;
}

void
fibers_choose_next(int state)
{
	fibers_scheduler->fibers_choose_next(fibers_scheduler_context, state);
}

void
fibers_switch_to(fiber_t target, int state)
{
	FIBERS_ASSERT(fibers_queue_remove(&fibers_run_queue, target), "fibers_switch_to");
	fibers_switch_helper(target, state);
}

void
fibers_switch_to_by_id(int target_id, int state)
{
	fiber_t target = fibers_queue_remove_by_id(&fibers_run_queue, target_id);
	FIBERS_ASSERT(target != NULL, "fibers_switch_to_by_id");
	fibers_switch_helper(target, state);
}

void
fibers_switch_top(int state)
{
	fiber_t target = fibers_queue_pop(&fibers_run_queue, 0);
	fibers_switch_helper(target, state);
}

void
fibers_switch_random(int state)
{
	fiber_t target = fibers_queue_pop(&fibers_run_queue, random_below(fibers_run_queue.count));
	fibers_switch_helper(target, state);
}

void
fibers_yield_to(int fiber_id)
{
	fibers_queue_push(&fibers_run_queue, fibers_current);
	fibers_switch_to_by_id(fiber_id, FIBER_STOP);
}

void
fibers_yield(void)
{
	fibers_queue_push(&fibers_run_queue, fibers_current);
	fibers_choose_next(FIBER_STOP);
}

bool
fibers_may_yield_internal(void)
{
	return fibers_may_yield_with_prob_and_reason(FIBERS_INTERNAL_YIELD_PROB, FIBERS_YIELD_REASON_UNKNOWN);
}

bool
fibers_may_yield_internal_with_reason(fiber_yield_reason_t reason)
{
	return fibers_may_yield_with_prob_and_reason(FIBERS_INTERNAL_YIELD_PROB, reason);
}

bool
fibers_may_yield(void)
{
	return fibers_may_yield_with_prob(fibers_may_yield_probability);
}

bool
fibers_may_yield_with_prob(uint64_t probability)
{
	return fibers_may_yield_with_prob_and_reason(probability, FIBERS_YIELD_REASON_UNKNOWN);
}

bool
fibers_may_yield_with_reason(fiber_yield_reason_t reason)
{
	return fibers_may_yield_with_prob_and_reason(fibers_may_yield_probability, reason);
}

bool
fibers_may_yield_with_prob_and_reason(uint64_t probability, fiber_yield_reason_t reason)
{
	if (fibers_current->may_yield_disabled) {
		return false;
	}

	if (fibers_scheduler->fibers_should_yield(fibers_scheduler_context, probability, reason)) {
		fibers_queue_push(&fibers_run_queue, fibers_current);
		fibers_choose_next(FIBER_STOP);
		return true;
	}

	return false;
}
