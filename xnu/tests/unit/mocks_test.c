/*
 * Copyright (c) 2000-2024 Apple Inc. All rights reserved.
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

#include <stdint.h>
#include "mocks/std_safe.h"
#include "mocks/unit_test_utils.h"
#include "mocks/mock_misc.h"
#include "mocks/dt_proxy.h"
#include <kern/lock_mtx.h>
#include <os/atomic_private.h>
#include <kern/sched_prim.h>

#define UT_MODULE osfmk
T_GLOBAL_META(
	T_META_NAMESPACE("xnu.unit.mocks"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_OWNER("s_shalom"),
	T_META_RUN_CONCURRENTLY(false)
	);


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
	pthread_t mythreads[NUM_THREADS] = {};
	struct inc_state s = {.counter = 0};
	lck_grp_init(&s.grp, "test_mutex", LCK_GRP_ATTR_NULL);
	lck_mtx_init(&s.mtx, &s.grp, LCK_ATTR_NULL);

	// Create threads
	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_create(&mythreads[i], NULL, increment_counter, (void*)&s);
	}

	// Wait for all threads to finish
	for (int i = 0; i < NUM_THREADS; i++) {
		pthread_join(mythreads[i], NULL);
	}
	lck_mtx_destroy(&s.mtx, &s.grp);

	T_LOG("Done counter=%lld", os_atomic_load(&s.counter, relaxed));
	T_ASSERT_EQ(s.counter, (int64_t)(NUM_INCREMENTS * NUM_THREADS), "eq");
}

struct wait_state {
	event_t event;
	volatile bool thread_did_sleep;
};

// from unistd.h.
// This can't be in stdsafe.h since it conflicts with a definition in bsd/sys/proc_internal.h
unsigned int sleep(unsigned int seconds);

void*
do_sleep_and_wake(void *arg)
{
	struct wait_state *s = (struct wait_state *)arg;
	sleep(1);
	s->thread_did_sleep = true;
	kern_return_t ret = thread_wakeup(s->event);
	T_ASSERT_EQ(ret, KERN_SUCCESS, "thread_wakeup");
	return NULL;
}

T_DECL(mocks_can_call_dt, "check that mocks can call T_x macros via PT_x")
{
	T_ASSERT_NOTNULL(get_dt_proxy_mock(), "mock dt_proxy null");
	T_ASSERT_NOTNULL(get_dt_proxy_attached(), "attached dt_proxy null");
}

// this test is meant to fail in order to verify that we're linking with the mock unimplemented sptm functions
// it's useful when debugging the Makefile
void libsptm_init(void);
T_DECL(sptm_link_unimpl, "sptm_link_unimpl", T_META_EXPECTFAIL("fail due to unimplemented sptm mock"))
{
	libsptm_init();
}

// --------------- dynamic mocks ---------------------------------

#if (DEBUG || DEVELOPMENT)
// disabled in release since the kernel_funcX() functions are not defined by xnu in release

T_DECL(mock_with_callback, "mock_with_callback")
{
	size_t ret1 = kernel_func1(1, 2);
	T_ASSERT_EQ(ret1, (size_t)0, "expected return before - default value from mock");
	{
		T_MOCK_SET_CALLBACK(kernel_func1,
		    size_t,
		    (int a, char b),
		{
			T_ASSERT_EQ(a, 3, "expected a");
			T_ASSERT_EQ(b, 4, "expected b");
			return a + b;
		});

		size_t ret2 = kernel_func1(3, 4);
		T_ASSERT_EQ(ret2, (size_t)7, "expected return sum");


		T_MOCK_SET_CALLBACK(kernel_func1,
		    size_t,
		    (int a, char b),
		{
			return a - b;
		});

		size_t ret3 = kernel_func1(40, 30);
		T_ASSERT_EQ(ret3, (size_t)10, "expected return second in the same scope");
	}

	size_t ret4 = kernel_func1(5, 6);
	T_ASSERT_EQ(ret4, (size_t)0, "expected return before - mock default value");
}


T_DECL(mock_with_retval, "mock_with_retval")
{
	size_t r1 = kernel_func1(0, 1);
	T_ASSERT_EQ(r1, (size_t)0, "expected value before - mock default value");

	{
		T_MOCK_SET_RETVAL(kernel_func1, size_t, 42);

		size_t r2 = kernel_func1(0, 1);
		T_ASSERT_EQ(r2, (size_t)42, "expected value with mock");


		T_MOCK_SET_RETVAL(kernel_func1, size_t, 43);

		size_t r3 = kernel_func1(0, 1);
		T_ASSERT_EQ(r3, (size_t)43, "expected value with mock second in the same scope");
	}

	size_t r4 = kernel_func1(0, 1);
	T_ASSERT_EQ(r4, (size_t)0, "expected value after - mock default value");
}


T_MOCK_SET_PERM_FUNC(size_t,
    kernel_func2,
    (int a, char b))
{
	T_ASSERT_EQ((int)a % 2, 0, "a is even");
	return a * 2;
}

T_DECL(mock_with_static_func, "mock_with_static_func")
{
	size_t r = kernel_func2(10, 1);
	T_ASSERT_EQ(r, (size_t)20, "expected return value");
}


T_MOCK_SET_PERM_RETVAL(kernel_func3, size_t, 42);

T_DECL(mock_with_perm_retval, "mock_with_perm_retval")
{
	size_t r = kernel_func3(1, 2);
	T_ASSERT_EQ(r, (size_t)42, "expected return value");
}


T_MOCK_CALL_QUEUE(fb_call, {
	int expected_a;
	char expected_b;
	size_t ret_val;
})

T_DECL(mock_call_queue, "mock_call_queue")
{
	enqueue_fb_call((fb_call){ .expected_a = 1, .expected_b = 2, .ret_val = 3 });
	enqueue_fb_call((fb_call){ .expected_a = 10, .expected_b = 20, .ret_val = 30 });

	{
		fb_call c1 = dequeue_fb_call();
		T_ASSERT_EQ(c1.expected_a, 1, "a arg");
		T_ASSERT_EQ(c1.expected_b, 2, "b arg");
		T_ASSERT_EQ(c1.ret_val, (size_t)3, "a arg");
	}
	{
		fb_call c2 = dequeue_fb_call();
		T_ASSERT_EQ(c2.expected_a, 10, "a arg");
		T_ASSERT_EQ(c2.expected_b, 20, "b arg");
		T_ASSERT_EQ(c2.ret_val, (size_t)30, "a arg");
	}
}


T_MOCK_SET_PERM_FUNC(size_t,
    kernel_func4,
    (int a, char b))
{
	fb_call c = dequeue_fb_call();
	T_ASSERT_EQ(a, c.expected_a, "a arg");
	T_ASSERT_EQ(b, c.expected_b, "b arg");
	return c.ret_val;
}

T_DECL(mock_call_queue_in_a_mock, "mock_call_queue_in_a_mock")
{
	enqueue_fb_call((fb_call){ .expected_a = 1, .expected_b = 2, .ret_val = 3 });
	enqueue_fb_call((fb_call){ .expected_a = 10, .expected_b = 20, .ret_val = 30 });

	size_t r1 = kernel_func4(1, 2);
	T_ASSERT_EQ(r1, (size_t)3, "r1 ret");
	size_t r2 = kernel_func4(10, 20);
	T_ASSERT_EQ(r2, (size_t)30, "r2 ret");
}

// a mock that calls the original function explicitly
T_DECL(mock_default_calling_original, "mock_default_calling_original")
{
	size_t r = kernel_func5(1, 2);
	T_ASSERT_EQ(r, (size_t)5000, "r ret");
}

// a mock that calls the original function implicitly through _T_MOCK_DYNAMIC_DEFAULT_IMPL
T_DECL(mock_default_calling_original_implicit, "mock_default_calling_original_auto_define")
{
	size_t r = kernel_func7(1, 2);
	T_ASSERT_EQ(r, (size_t)7000, "r ret");
}

T_DECL(mock_void_ret, "mock_void_ret")
{
	extern int kernel_func6_was_called;
	kernel_func6_was_called = 0;
	kernel_func6(3, 4);
	T_ASSERT_EQ(kernel_func6_was_called, 3, "original called");

	kernel_func6_was_called = 0;
	T_MOCK_SET_CALLBACK(kernel_func6,
	    void,
	    (int a, char b),
	{
		T_ASSERT_EQ(a, 3, "expected a");
		T_ASSERT_EQ(b, 4, "expected b");
	});
	kernel_func6(3, 4);
	T_ASSERT_EQ(kernel_func6_was_called, 0, "original called");
}

// void function with the default action that calls the original function
T_DECL(mock_void_ret_original_implicit, "mock_void_ret_original_implicit")
{
	extern int kernel_func8_was_called;
	kernel_func8_was_called = 0;
	kernel_func8(3, 4);
	T_ASSERT_EQ(kernel_func8_was_called, 3, "original called");
}

#endif // (DEBUG || DEVELOPMENT)
