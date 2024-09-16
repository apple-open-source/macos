#include <darwintest.h>

#include <stdatomic.h>

#include <unistd.h>
#include <pthread.h>
#include <sys/ulock.h>

#include <os/tsd.h>

#ifndef __TSD_MACH_THREAD_SELF
#define __TSD_MACH_THREAD_SELF 3
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wbad-function-cast"
__inline static mach_port_name_t
_os_get_self(void)
{
	mach_port_name_t self = (mach_port_name_t)_os_tsd_get_direct(__TSD_MACH_THREAD_SELF);
	return self;
}
#pragma clang diagnostic pop

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true));

#pragma mark ulock_non_owner_wake

static _Atomic uint32_t test_ulock;

static void *
test_waiter(void *arg __unused)
{
	for (;;) {
		uint32_t test_ulock_owner = atomic_load_explicit(&test_ulock,
		    memory_order_relaxed);
		int rc = __ulock_wait(UL_UNFAIR_LOCK | ULF_NO_ERRNO, &test_ulock,
		    test_ulock_owner, 0);
		if (rc == -EINTR || rc == -EFAULT) {
			continue;
		}
		T_ASSERT_GE(rc, 0, "__ulock_wait");
		break;
	}

	T_PASS("Waiter woke");
	T_END;

	return NULL;
}

static mach_timebase_info_data_t timebase_info;

static uint64_t
nanos_to_abs(uint64_t nanos)
{
	return nanos * timebase_info.denom / timebase_info.numer;
}

static void *
test_waiter_with_timeout(void *arg)
{
	uint64_t deadline = (uint64_t) arg;

	for (;;) {
		uint32_t test_ulock_owner = atomic_load_explicit(&test_ulock,
		    memory_order_relaxed);
		int rc = __ulock_wait2(UL_UNFAIR_LOCK | ULF_NO_ERRNO | ULF_DEADLINE, &test_ulock,
		    test_ulock_owner, deadline, 0);
		if (rc == -EINTR || rc == -EFAULT) {
			continue;
		}
		T_ASSERT_EQ(rc, -ETIMEDOUT, "__ulock_wait");
		break;
	}

	T_ASSERT_GE(mach_absolute_time(), deadline, "Current time is past the deadline specified");

	T_PASS("Waiter woke");

	return NULL;
}

static void *
test_waker(void *arg __unused)
{
	for (;;) {
		int rc = __ulock_wake(UL_UNFAIR_LOCK | ULF_NO_ERRNO | ULF_WAKE_ALLOW_NON_OWNER,
		    &test_ulock, 0);
		if (rc == -EINTR) {
			continue;
		}
		T_ASSERT_EQ(rc, 0, "__ulock_wake");
		break;
	}
	return NULL;
}

T_DECL(ulock_non_owner_wake, "ulock_wake respects non-owner wakes",
    T_META_CHECK_LEAKS(false), T_META_TAG_VM_PREFERRED)
{
	pthread_t waiter, waker;

	atomic_store_explicit(&test_ulock, _os_get_self() & ~0x3u, memory_order_relaxed);

	T_ASSERT_POSIX_ZERO(pthread_create(&waiter, NULL, test_waiter, NULL), "create waiter");

	// wait for the waiter to reach the kernel
	for (;;) {
		int kernel_ulocks = __ulock_wake(UL_DEBUG_HASH_DUMP_PID, NULL, 0);
		T_QUIET; T_ASSERT_NE(kernel_ulocks, -1, "UL_DEBUG_HASH_DUMP_PID");

		if (kernel_ulocks == 1) {
			T_LOG("waiter is now waiting");
			break;
		}
		usleep(100);
	}

	T_ASSERT_POSIX_ZERO(pthread_create(&waker, NULL, test_waker, NULL), "create waker");

	// won't ever actually join
	pthread_join(waiter, NULL);
}

T_DECL(ulock_wait_deadline, "ulock_wait2 with deadline", T_META_CHECK_LEAKS(false), T_META_TAG_VM_PREFERRED)
{
	kern_return_t kr = mach_timebase_info(&timebase_info);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_timebase_info");

	// Take the lock as self
	atomic_store_explicit(&test_ulock, _os_get_self() & ~0x3u, memory_order_relaxed);

	pthread_t waiter;

	// Deadline in the past
	uint64_t deadline = mach_absolute_time() - nanos_to_abs(3 * NSEC_PER_SEC);
	T_ASSERT_POSIX_ZERO(pthread_create(&waiter, NULL, test_waiter_with_timeout, (void *) deadline), "create waiter");

	pthread_join(waiter, NULL);

	// Deadline in the future
	deadline = mach_absolute_time() + nanos_to_abs(3 * NSEC_PER_SEC);
	T_ASSERT_POSIX_ZERO(pthread_create(&waiter, NULL, test_waiter_with_timeout, (void *) deadline), "create waiter");

	pthread_join(waiter, NULL);
	T_END;
}
