#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <dispatch/dispatch.h>

#include "dispatch_test.h"

int main(void)
{
	test_start("Dispatch Source Timer, bit 31");

	dispatch_queue_t main_q = dispatch_get_main_queue();
	test_ptr("dispatch_get_main_queue", main_q, dispatch_get_current_queue());

	__block int i = 0;
	struct timeval start_time;

	gettimeofday(&start_time, NULL);
	dispatch_source_attr_t attr = dispatch_source_attr_create();
	dispatch_source_attr_set_finalizer(attr, ^(dispatch_source_t ds) {
		struct timeval end_time;
		gettimeofday(&end_time, NULL);
		test_ptr_notnull("finalizer ran", ds);
		// XXX: check, s/b 2.0799... seconds, which is <4 seconds
		// when it could end on a bad boundry.
		test_long_less_than("needs to finish faster than 4 seconds", end_time.tv_sec - start_time.tv_sec, 4);
		// And it has to take at least two seconds...
		test_long_less_than("can't finish faster than 2 seconds", 1, end_time.tv_sec - start_time.tv_sec);
		test_stop();
	});

	dispatch_source_t s = dispatch_source_timer_create(DISPATCH_TIMER_INTERVAL,
		0x80000000ull,
		0,
		attr,
		main_q,
		^(dispatch_event_t ev) {
			long err;
			if (dispatch_event_get_error(ev, &err)) {
				test_errno("dispatch_event_get_error", err, ECANCELED);
				dispatch_release(dispatch_event_get_source(ev));
			} else {
				fprintf(stderr, "%d\n", ++i);
				dispatch_cancel(dispatch_event_get_source(ev));
			}
		});
	test_ptr_notnull("dispatch_source_timer_create", s);

	dispatch_release(attr);

	dispatch_main();

	return 0;
}
