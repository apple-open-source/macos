#include <dispatch/dispatch.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>

#include "dispatch_test.h"

#define LAPS 10000
#define INTERVAL 100

static size_t r_count = LAPS;
static size_t w_count = LAPS / INTERVAL;

static void
writer(void *ctxt __attribute__((unused)))
{
	if (--w_count == 0) {
		if (r_count == 0) {
			test_stop();
		}
	}
}

static void
reader(void *ctxt __attribute__((unused)))
{
	if (__sync_sub_and_fetch(&r_count, 1) == 0) {
		if (r_count == 0) {
			test_stop();
		}
	}
}

int
main(void)
{
	dispatch_queue_t dq;

	test_start("Dispatch Reader/Writer Queues");

	dq = dispatch_queue_create("com.apple.libdispatch.test_readsync", NULL);
	assert(dq);

	dispatch_queue_set_width(dq, LONG_MAX);

	dispatch_apply(LAPS, dispatch_get_concurrent_queue(0), ^(size_t idx) {
		dispatch_sync_f(dq, NULL, reader);

		if (idx % INTERVAL) {
			dispatch_barrier_async_f(dq, NULL, writer);
		}
	});

	dispatch_release(dq);

	dispatch_main();
}
