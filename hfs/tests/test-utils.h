/*
 * Copyright (c) 2014-2015 Apple Inc. All rights reserved.
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

#ifndef TEST_UTILS_H_
#define TEST_UTILS_H_

#include <mach-o/dyld.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/syslimits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <pthread.h>
#include <libgen.h>

__BEGIN_DECLS

#if HAVE_ASSERT_FAIL

void  __attribute__((format (printf, 3, 4), noreturn))
	assert_fail_(const char *file, int line, const char *assertion, ...);

#else // !HAVE_ASSERT_FAIL

static inline void  __attribute__((format (printf, 3, 4), noreturn))
assert_fail_(const char *file, int line, const char *assertion, ...)
{
	va_list args;
	va_start(args, assertion);
	char *msg;
	vasprintf(&msg, assertion, args);
	va_end(args);
	printf("\n%s:%u: error: %s\n", file, line, msg);
	exit(1);
}

#endif // !HAVE_ASSERT_FAIL

int test_cleanup(bool (^)(void));
void do_cleanups(void);

#define assert_fail(str, ...)						\
	assert_fail_(__FILE__, __LINE__, str, ## __VA_ARGS__)

#undef assert
#define assert(condition)											\
	do {															\
		if (!(condition))											\
			assert_fail_(__FILE__, __LINE__,						\
						 "assertion failed: %s", #condition);		\
	} while (0)

#define assert_with_errno_(condition, condition_str)					\
	do {																\
		if (!(condition))												\
			assert_fail_(__FILE__, __LINE__, "%s failed: %s",			\
						 condition_str, strerror(errno));				\
	} while (0)

#define assert_with_errno(condition)			\
	assert_with_errno_((condition), #condition)

#define assert_no_err(condition) \
	assert_with_errno_(!(condition), #condition)

#define assert_equal(lhs, rhs, fmt)										\
	do {																\
		typeof (lhs) lhs_ = (lhs);										\
		typeof (lhs) rhs_ = (rhs);										\
		if (lhs_ != rhs_)												\
			assert_fail(#lhs " (" fmt ") != " #rhs " (" fmt ")",		\
						lhs_, rhs_);									\
	} while (0)

#define assert_equal_int(lhs, rhs)	assert_equal(lhs, rhs, "%d")
#define assert_equal_ll(lhs, rhs)	assert_equal(lhs, rhs, "%lld");

#define check_io(fn, len) check_io_((fn), len, __FILE__, __LINE__, #fn)

static inline ssize_t check_io_(ssize_t res, ssize_t len, const char *file,
								int line, const char *fn_str)
{
	if (res < 0)
		assert_fail_(file, line, "%s failed: %s", fn_str, strerror(errno));
	else if (len != -1 && res != len)
		assert_fail_(file, line, "%s != %ld", fn_str, len);
	return res;
}

#define ignore_eintr(x, error_val)								\
	({															\
		typeof(x) eintr_ret_;									\
		do {													\
			eintr_ret_ = (x);									\
		} while (eintr_ret_ == (error_val) && errno == EINTR);	\
		eintr_ret_;												\
	})

#define lengthof(x)		(sizeof(x) / sizeof(*x))

/*
 * Convenience macro to check if a pthread function fails.
 *
 * pthread functions directly return an errno instead of 0/-1. This makes use
 * of standard assert_no_err() unsuitable.
 */
#define assert_pthread_ok(x) assert_equal_int((x), 0)

/*
 * Thread barrier suitable for use in a standard control flow loop.
 *
 * The basic idea is that a group of `num_threads' threads must each enter the
 * thread barrier before any one is permitted to proceed. Use of this primitive
 * in a race condition test permits two disproportionately slow operations to
 * race against each other.
 * E.g., consider a test (viz. 41004803) that races calls to chmod(2) and
 * getxattr(2) on a list of files across two separate threads. Because chmod(2)
 * is significantly faster than getxattr(2), the chmod-thread is
 * likely to complete processing all files before the getxattr-thread does. At
 * that point, the test is not very effective at racing two operations because
 * its two threads are unlikely to operate on the same file at the same time.
 * To counteract this, both threads much synchronize with each other on each
 * iteration: no thread is permitted to proceed to iteration `i' + 1 until both
 * have completed iteration `i'.
 *
 * The typical one-shot thread barrier may be implemented as a condition
 * variable combined with a single, shared counter that represents the number
 * of threads that have entered.
 * However, this approach is insufficient for use in a loop because threads
 * from different iterations may attempt to wait on the barrier:
 *
 * thread 1             thread 2
 * enter()
 *   set ready=1
 *   sleep
 *                      enter()
 *                        set ready=2
 *                        issue broadcast
 *                      enter()
 *                        set ready=1 // assuming thread 1 woke up
 *                        sleep
 *   wake up
 *   sleep // ready == 1
 *            <DEADLOCK>
 *
 * This loop barrier is implemented similarly that what is described above
 * except that `num_ready' is comprised of two counters:
 * - one counter for the number of threads that have entered on even iterations
 * - one counter for odd iterations
 * This works because the difference between any two threads' iterations may
 * only equal one (otherwise the barrier isn't doing its job and has permitted
 * a thread to complete a successive iteration before other threads have
 * completed a prior iteration).
 *
 * Example use:
 * main() {
 *   loop_barrier_t barrier;
 *   loop_barrier_init(&barrier, 2);
 *   // race foo() across two threads
 *   pthread_create(foo, ..., &barrier);
 *   pthread_create(foo, ..., &barrier);
 *   ...
 *   loop_barrier_free(&barrier);
 * }
 *
 * // race calls to printf()
 * void foo(loop_barrier_t *barrier) {
 *   for (unsigned i = 0; i < 10; ++i) {
 *     loop_barrier_enter(&barrier, i);
 *     printf("foo %u\n", i);
 *   }
 * }
 */
typedef struct {
	unsigned num_threads;
	unsigned num_ready[2];
	pthread_mutex_t lock;
	pthread_cond_t cv;
} loop_barrier_t;

/*
 * Initialize a loop barrier for use by `num_threads' number of threads.
 *
 * This must be followed with call to loop_barrier_free().
 */
static inline void
loop_barrier_init(loop_barrier_t *barrier, unsigned num_threads)
{
	barrier->num_threads = num_threads;
	memset(barrier->num_ready, 0, sizeof(barrier->num_ready));
	assert_pthread_ok(pthread_mutex_init(&barrier->lock, NULL));
	assert_pthread_ok(pthread_cond_init(&barrier->cv, NULL));
}

/*
 * Free the internal resources of a barrier initialized from a previous call to
 * loop_barrier_init().
 */
static inline void
loop_barrier_free(loop_barrier_t *barrier)
{
	assert_pthread_ok(pthread_mutex_destroy(&barrier->lock));
	assert_pthread_ok(pthread_cond_destroy(&barrier->cv));
}

/*
 * Enter the loop barrier on iteration number `iter'.
 *
 * For any given iteration, the first `num_threads' - 1 calls will block. The
 * next call will wake up all sleeping threads.
 */
static inline void
loop_barrier_enter(loop_barrier_t *barrier, unsigned iter)
{
	assert_pthread_ok(pthread_mutex_lock(&barrier->lock));

	const unsigned num_threads = barrier->num_threads;

	// select the ready-counter for this iteration
	unsigned *ready = &barrier->num_ready[iter & 1];

	/*
	 * This thread is ready, so increment the counter.
	 * If the counter is initially `num_threads', this means that
	 * - the current thread is the first to reach the barrier
	 * - this barrier was reused from a previous iteration
	 *
	 * In this case, set `ready' = 1 to represent the current thread.
	 */
	*ready = (*ready == num_threads) ? 1 : (*ready+1);

	// sleep until all threads are ready
	while(*ready != num_threads) {
		pthread_cond_wait(&barrier->cv, &barrier->lock);
	}

	// this may spuriously wake up threads waiting on the next iteration's
	// barrier - but that's ok
	assert_pthread_ok(pthread_cond_broadcast(&barrier->cv));

	assert_pthread_ok(pthread_mutex_unlock(&barrier->lock));
}

__END_DECLS

#endif // TEST_UTILS_H_
