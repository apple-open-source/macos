#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <dispatch/dispatch.h>

#include "dispatch_test.h"

int main(void)
{
	test_start("Dispatch Source Timer, bit 63");

	//uint64_t interval = 0xffffffffffffffffull;
	uint64_t interval = 0x8000000000000001ull;

	dispatch_queue_t mainq = dispatch_get_main_queue();

	__block int i = 0;
	struct timeval start_time;

	gettimeofday(&start_time, NULL);

	dispatch_source_t ds;
	ds = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, mainq);
	assert(ds);
	dispatch_source_set_event_handler(ds, ^{
		assert(i < 1);
		printf("%d\n", i++);
	});
	dispatch_source_set_timer(ds, DISPATCH_TIME_NOW, interval, 0);
	dispatch_resume(ds);

	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 1*NSEC_PER_SEC),
		dispatch_get_main_queue(), ^{
		test_stop();
	});

	dispatch_main();

	return 0;
}
