#include <stdio.h>
#include <stdlib.h>
#include <pthread/qos_private.h>
#include <dispatch/dispatch.h>
#include <dispatch/private.h>
#define CONFIG_X86_64_COMPAT 0
#include <machine/cpu_capabilities.h>
#include <sys/time.h>
#include <sys/sysctl.h>

#include "darwintest_defaults.h"
#include <darwintest_utils.h>

// for _PTHREAD_QOS_PARALLELISM_AMX
#define __PTHREAD_EXPOSE_INTERNALS__
#include <pthread/bsdthread_private.h>
#undef __PTHREAD_EXPOSE_INTERNALS__

extern int __bsdthread_ctl(uintptr_t cmd, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3);

static uint64_t end_spin;

static uint32_t
get_ncpu(void)
{
	static uint32_t activecpu;
	if (!activecpu) {
		uint32_t n;
		size_t s = sizeof(activecpu);
		sysctlbyname("hw.ncpu", &n, &s, NULL, 0);
		activecpu = n;
	}
	return activecpu;
}

static void
spin_and_pause(void *ctx)
{
	long i = (long)ctx;

	printf("Thread %ld starts\n", i);

	while (clock_gettime_nsec_np(CLOCK_MONOTONIC) < end_spin) {
#if defined(__x86_64__) || defined(__i386__)
		__asm__("pause");
#elif defined(__arm__) || defined(__arm64__)
		__asm__("wfe");
#endif
	}
	printf("Thread %ld blocks\n", i);
	pause();
}

static void
spin(void *ctx)
{
	long i = (long)ctx;

	printf("Thread %ld starts\n", i);

	while (clock_gettime_nsec_np(CLOCK_MONOTONIC)) {
#if defined(__x86_64__) || defined(__i386__)
		__asm__("pause");
#elif defined(__arm__) || defined(__arm64__)
		__asm__("wfe");
#endif
	}
}

T_DECL(thread_request_32848402, "repro for rdar://32848402")
{
	dispatch_queue_attr_t bg_attr, in_attr;

	bg_attr = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_CONCURRENT,
			QOS_CLASS_BACKGROUND, 0);
	in_attr = dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_CONCURRENT,
			QOS_CLASS_USER_INITIATED, 0);

	dispatch_queue_t a = dispatch_queue_create_with_target("in", in_attr, NULL);
	dispatch_queue_t b = dispatch_queue_create_with_target("bg", bg_attr, NULL);

	end_spin = clock_gettime_nsec_np(CLOCK_MONOTONIC) + 2 * NSEC_PER_SEC;

	dispatch_async_f(a, (void *)0, spin_and_pause);
	long n_threads = MIN((long)get_ncpu(),
			pthread_qos_max_parallelism(QOS_CLASS_BACKGROUND, 0));
	for (long i = 1; i < n_threads; i++) {
		dispatch_async_f(b, (void *)i, spin);
	}

	dispatch_async(b, ^{
		T_PASS("The NCPU+1-nth block got scheduled");
		T_END;
	});

	sleep(10);
	T_FAIL("The NCPU+1-nth block didn't get scheduled");
}

static bool
is_amx_supported(void)
{
#if defined(__arm64__)
	if (__AMXVersion() < 1) {
		return false;
	}

	return true;
#else
	return false;
#endif
}

static void
skip_if_amx_unsupported(void)
{
	if (!is_amx_supported()) {
		T_SKIP("amx is not supported on this platform");
	}
}

T_DECL(amx_parallelism, "amx parallelism")
{
	skip_if_amx_unsupported();

	size_t ncpus = get_ncpu();
	int num_amx, num_amx2;
	num_amx = pthread_qos_max_parallelism(QOS_CLASS_BACKGROUND, PTHREAD_MAX_PARALLELISM_AMX);

	T_ASSERT_LE(num_amx, ncpus, "Num AMX units is less than num cpus");
	T_ASSERT_GE(num_amx, 0, "Num AMX units is greater than 0");

	num_amx2 = __bsdthread_ctl(BSDTHREAD_CTL_QOS_MAX_PARALLELISM, THREAD_QOS_BACKGROUND, _PTHREAD_QOS_PARALLELISM_AMX, 0);
	T_ASSERT_EQ(num_amx2, num_amx, "Matches up with previous query");

	num_amx = pthread_qos_max_parallelism(QOS_CLASS_USER_INITIATED, PTHREAD_MAX_PARALLELISM_AMX);

	T_ASSERT_LE(num_amx, ncpus, "Num AMX units is less than num cpus");
	T_ASSERT_GE(num_amx, 0, "Num AMX units is greater than 0");
}

T_DECL(amx_parallelism_on_non_amx_hardware, "amx parallelism on non-amx hardware")
{
	if (is_amx_supported()) {
		T_SKIP("Test is irrelevant on platforms with amx");
	}

	size_t ncpus = get_ncpu();
	int num_amx, num_amx2;

	num_amx = pthread_qos_max_parallelism(QOS_CLASS_BACKGROUND, PTHREAD_MAX_PARALLELISM_AMX);
	T_ASSERT_EQ(num_amx, 0, "Got 0 AMX units on non-amx hardware");

	num_amx2 = __bsdthread_ctl(BSDTHREAD_CTL_QOS_MAX_PARALLELISM, THREAD_QOS_BACKGROUND, _PTHREAD_QOS_PARALLELISM_AMX, 0);
	T_ASSERT_EQ(num_amx2, -1, "No AMX");

	num_amx = pthread_qos_max_parallelism(QOS_CLASS_USER_INITIATED, PTHREAD_MAX_PARALLELISM_AMX);
	T_ASSERT_EQ(num_amx, 0, "Got 0 AMX units on non-amx hardware");
}
