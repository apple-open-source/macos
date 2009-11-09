#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <dispatch/dispatch.h>

#include "dispatch_test.h"

int main(void)
{
	test_start("Dispatch Source Timer");

	dispatch_queue_t main_q = dispatch_get_main_queue();
	test_ptr("dispatch_get_main_queue", main_q, dispatch_get_current_queue());

	uint64_t j;

	// create several timers and release them.
	for (j = 1; j <= 5; ++j)
	{
		dispatch_source_t s = dispatch_source_timer_create(DISPATCH_TIMER_INTERVAL,
			(uint64_t)j * (uint64_t)1000000000ull, 0, NULL, dispatch_get_concurrent_queue(0),
			^(dispatch_event_t ev) {
				if (!dispatch_event_get_error(ev, NULL)) {
					fprintf(stderr, "timer[%lld]\n", j);
					dispatch_release(dispatch_event_get_source(ev));
				}
		});
		test_ptr_notnull("dispatch_source_timer_create", s);
	}

	dispatch_source_attr_t attr = dispatch_source_attr_create();
	dispatch_source_attr_set_finalizer(attr, ^(dispatch_source_t ds) {
		test_ptr_notnull("finalizer ran", ds);
		test_stop();
	});
	
	__block int i = 0;
	
	dispatch_source_t s = dispatch_source_timer_create(DISPATCH_TIMER_INTERVAL,
		1000000000ull,
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
				if (i >= 3) {
					dispatch_cancel(dispatch_event_get_source(ev));
				}
			}
		});
	test_ptr_notnull("dispatch_source_timer_create", s);

	dispatch_release(attr);

	dispatch_main();

	return 0;
}
