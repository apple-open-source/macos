#include <dispatch/dispatch.h>
#include <pthread.h>
#include <stdio.h>
#include <assert.h>

#include "dispatch_test.h"

#define LAPS 10000

int
main(void)
{
	static size_t total;
	dispatch_semaphore_t dsema;

	test_start("Dispatch Semaphore");

	dsema = dispatch_semaphore_create(1);
	assert(dsema);

	dispatch_apply(LAPS, dispatch_get_concurrent_queue(0), ^(size_t idx __attribute__((unused))) {
		dispatch_semaphore_wait(dsema, DISPATCH_TIME_FOREVER);
		total++;
		dispatch_semaphore_signal(dsema);
	});

	dispatch_release(dsema);

	test_long("count", total, LAPS);
	test_stop();

	return 0;
}
