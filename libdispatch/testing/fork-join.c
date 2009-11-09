#include <mach/mach.h>
#include <mach/mach_time.h>
#include <dispatch/dispatch.h>
#include <dispatch/private.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int
main(void)
{
	long double nano_per_lap;
	size_t i, cnt = 1000000;
	dispatch_future_t *df;
	uint64_t s, e;

	df = malloc(cnt * sizeof(df));
	assert(df);

	s = mach_absolute_time();

	for (i = 0; i < cnt; i++) {
		df[i] = dispatch_fork(dispatch_get_concurrent_queue(0), ^{
		});
		assert(df[i]);
	}

	for (i = 0; i < cnt; i++) {
		dispatch_join(df[i]);
	}

	e = mach_absolute_time();

	nano_per_lap = (e - s);
	nano_per_lap /= cnt;

	printf("%Lf nanoseconds per lap\n", nano_per_lap);

	return 0;
}
