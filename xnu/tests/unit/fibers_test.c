/*
 * Copyright (c) 2000-2025 Apple Inc. All rights reserved.
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

#include <darwintest.h>

#include "mocks/std_safe.h"
#include "mocks/mock_thread.h"

#include "mocks/fibers/fibers.h"
#include "mocks/fibers/mutex.h"
#include "mocks/fibers/condition.h"
#include "mocks/fibers/random.h"

// Use FIBERS_PREEMPTION=1 to have simulated preemption at memory operations.
// make -C tests/unit SDKROOT=macosx.internal fibers_test FIBERS_PREEMPTION=1

#define UT_MODULE osfmk
T_GLOBAL_META(
	T_META_NAMESPACE("xnu.unit.fibers"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_OWNER("a_fioraldi"),
	T_META_RUN_CONCURRENTLY(false)
	);
// use fibers for scheduling
UT_USE_FIBERS(1);
// use the data race checker
// UT_FIBERS_USE_CHECKER(1);

static int third_fiber_id = -1;
static void*
coop_fibers_func(void* x)
{
	int *cooperative_counter = (int*)x;

	if (*cooperative_counter == 0) {
		// main thread can jump here just after fibers_create
		fibers_yield_to(0); // switch back to main thread and finish the fibers creation
	}

	T_QUIET; T_ASSERT_EQ(*cooperative_counter, fibers_current->id, "invalid cooperative_counter");
	*cooperative_counter = fibers_current->id + 1;

	// switch to next fiber or to main fiber (id=0) if the current is the last
	if (fibers_current->id == third_fiber_id) {
		fibers_yield_to(0);
	} else {
		fibers_yield_to(fibers_current->id + 1);
	}

	return NULL;
}

T_DECL(coop_fibers, "cooperative scheduling using fibers")
{
	// disable preemption in case FIBERS_PREEMPTION=1 was using to compile
	// context switches will still happen before and after locks / interrupt enable/disable / fibers creation
	fibers_may_yield_probability = 0;

	random_set_seed(1234);

	int cooperative_counter = 0;

	fiber_t first = fibers_create(FIBERS_DEFAULT_STACK_SIZE, coop_fibers_func, (void*)&cooperative_counter);
	fiber_t second = fibers_create(FIBERS_DEFAULT_STACK_SIZE, coop_fibers_func, (void*)&cooperative_counter);
	fiber_t third = fibers_create(FIBERS_DEFAULT_STACK_SIZE, coop_fibers_func, (void*)&cooperative_counter);

	third_fiber_id = third->id;

	// Start the chain of ctxswitches from the main thread and switch to first
	cooperative_counter = first->id;
	fibers_yield_to(first->id);

	T_LOG("Done cooperative_counter=%d", cooperative_counter);
	T_ASSERT_EQ(cooperative_counter, third->id + 1, "invalid cooperative schedule");

	// always join the fibers
	fibers_join(first);
	fibers_join(second);
	fibers_join(third);

	T_PASS("coop_fibers");
}

static int global_var;
static void*
tiny_race_func(void* x)
{
	global_var = 42;
	return x;
}

// Standard ThreadSanitizer example in the llvm doc to showcase a race
// TSan will not fail the test by default, you beed to set halt_on_error=1 in TSAN_OPTIONS
// the test will just run fine without TSan, the data race between fibers can be detected with the fibers data race checker too
T_DECL(tsan_tiny_race, "tsan_tiny_race")
{
	// This sometimes triggers a ThreadSanitizer data race depending on the OS scheduler
	pthread_t thread;
	pthread_create(&thread, NULL, tiny_race_func, NULL);
	global_var = 43;
	pthread_join(thread, NULL);

	T_LOG("Done pthread global_var=%d", global_var);

	// This always triggers a ThreadSanitizer data race thanks to the fixed seed
	fibers_log_level = FIBERS_LOG_INFO;
	fibers_may_yield_probability = 1;
	random_set_seed(1234);

	fiber_t fiber = fibers_create(FIBERS_DEFAULT_STACK_SIZE, tiny_race_func, NULL);
	global_var = 43;
	fibers_join(fiber);

	T_LOG("Done fibers global_var=%d", global_var);
	T_PASS("tsan_tiny_race");
}

#define NUM_INCREMENTS 100000
#define NUM_THREADS 10

struct inc_state {
	volatile int64_t counter;
	//_Atomic int64_t counter;
	lck_mtx_t mtx;
	lck_grp_t grp;
};

void*
increment_counter(void* arg)
{
	struct inc_state *s = (struct inc_state *)arg;
	for (int i = 0; i < NUM_INCREMENTS; i++) {
		// Remove locks to fail the test and trigger a ThreadSanitizer data race
		lck_mtx_lock(&s->mtx);
		//lck_mtx_lock_spin(&s->mtx);
		s->counter++;
		//os_atomic_inc(&s->counter, relaxed);
		lck_mtx_unlock(&s->mtx);
	}
	return NULL;
}

T_DECL(mutex_mock_increment_int, "mutex mock test")
{
	// fibers_log_level = 1;
	// fibers_may_yield_probability = 0;
	random_set_seed(1234);

	fiber_t mythreads[NUM_THREADS] = {};
	struct inc_state s = {.counter = 0};
	lck_grp_init(&s.grp, "test_mutex", LCK_GRP_ATTR_NULL);
	lck_mtx_init(&s.mtx, &s.grp, LCK_ATTR_NULL);

	// Create fibers
	for (int i = 0; i < NUM_THREADS; i++) {
		mythreads[i] = fibers_create(FIBERS_DEFAULT_STACK_SIZE, increment_counter, (void*)&s);
	}

	// Wait for all fibers to finish
	for (int i = 0; i < NUM_THREADS; i++) {
		fibers_join(mythreads[i]);
	}
	lck_mtx_destroy(&s.mtx, &s.grp);

	T_LOG("Done counter=%lld", os_atomic_load(&s.counter, relaxed));
	T_ASSERT_EQ(s.counter, (int64_t)(NUM_INCREMENTS * NUM_THREADS), "race detected on counter");

	T_PASS("mutex_mock_increment_int");
}
