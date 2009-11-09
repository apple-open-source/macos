#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <dispatch/dispatch.h>

#include "dispatch_test.h"

static void
oneshot(void* context __attribute__((unused)), dispatch_event_t de)
{
	dispatch_source_t ds = dispatch_event_get_source(de);
	test_ptr_notnull("dispatch_event_get_source", ds);

	if (!dispatch_event_get_error(de, NULL)) {
		long canceled = dispatch_testcancel(ds);
		test_long("dispatch_testcancel", canceled, 0);

		dispatch_release(ds);
		test_stop();
	}
}

int
main(void)
{
	test_start("Dispatch Timer One-Shot");

	dispatch_source_t s;

	s  = dispatch_source_timer_create_f(DISPATCH_TIMER_ONESHOT,
		(uint64_t)1000000000ull, // 1s
		0,
		NULL,
		dispatch_get_concurrent_queue(0),
		NULL,
		&oneshot);

	dispatch_main();

	return 0;
}
