#include <mach/mach.h>
#include <mach/mach_time.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

static inline uint64_t
rdtsc(void)
{
	uint32_t lo, hi;

	asm volatile("rdtsc" : "=a" (lo), "=d" (hi));

	return (uint64_t)hi << 32 | lo;
}

__attribute__((noinline)) void
apply_p(void (^b)(size_t), size_t offset, size_t count)
{
	/* This would feed through to the existing dispatch_apply() */
	abort();
}

/* a dynamically variable to eventually be added to the kernel/user 'commpage' */
size_t total_active_cpus = 8;

__attribute__((noinline)) void
apply(void (^b)(size_t), size_t offset, size_t count)
{
	const size_t too_long = 100000; /* 100 us */
	const size_t laps = 16;
	uint64_t delta, tmp, now;
	size_t i;

	if (total_active_cpus == 1) {
		for (i = 0; i < count; i++) {
			b(offset + i);
		}
		return;
	}

	now = mach_absolute_time();

	for (i = 0; i < count; i++) {
		b(offset + i);

		if (i % laps) {
			continue;
		}

		tmp = mach_absolute_time();
		delta = tmp - now;
		now = tmp;

		if (delta > (too_long * laps) || (i == 0 && delta > too_long)) {
			apply_p(b, offset + i + 1, count - (i + 1));
			return;
		}
	}
}

int
main(void)
{
	void (^b)(size_t) = ^(size_t index) {
		asm volatile(""); /* defeat compiler optimizations */
	};
	const size_t laps = 10000000;
	mach_timebase_info_data_t tbi;
	kern_return_t kr;
	long double dd;
	uint64_t s, e;
	size_t i;

        kr = mach_timebase_info(&tbi);
	assert(kr == 0);
	assert(tbi.numer == tbi.denom); /* This will fail on PowerPC. */

	s = mach_absolute_time();
	for (i = 0; i < laps; i++) {
		b(i);
	}
	e = mach_absolute_time();
	dd = e - s;
	dd /= laps;
	printf("direct:\t%Lf ns\n", dd);

	s = mach_absolute_time();
	apply(b, 0, laps);
	e = mach_absolute_time();
	dd = e - s;
	dd /= laps;
	printf("apply:\t%Lf ns\n", dd);

	return 0;
}
