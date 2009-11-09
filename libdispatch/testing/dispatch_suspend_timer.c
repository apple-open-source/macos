#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <dispatch/dispatch.h>

#include "dispatch_test.h"

dispatch_source_t tweedledee;
dispatch_source_t tweedledum;

int main(void)
{
	test_start("Dispatch Suspend Timer");

	dispatch_queue_t main_q = dispatch_get_main_queue();
	test_ptr("dispatch_get_main_queue", main_q, dispatch_get_current_queue());

	__block int i = 0;
	__block int j = 0;

	dispatch_source_attr_t attr = dispatch_source_attr_create();
	test_ptr_notnull("dispatch_source_attr_create", attr);
	
	dispatch_source_attr_set_finalizer(attr, ^(dispatch_source_t ds) {
		test_ptr_notnull("finalizer ran", ds);
		if (ds == tweedledum) test_stop();
	});

	tweedledee = dispatch_source_timer_create(DISPATCH_TIMER_INTERVAL,
		1000000000ull, 0, attr, main_q, ^(dispatch_event_t ev) {
			long err;
			if (dispatch_event_get_error(ev, &err)) {
				test_errno("dispatch_event_get_error", err, ECANCELED);
				dispatch_release(dispatch_event_get_source(ev));
			} else {
				fprintf(stderr, "%d\n", ++i);
				if (i == 10) {
					dispatch_cancel(dispatch_event_get_source(ev));
				}
			}
		});
	test_ptr_notnull("dispatch_source_timer_create", tweedledee);
	dispatch_retain(tweedledee);

	tweedledum = dispatch_source_timer_create(DISPATCH_TIMER_INTERVAL,
		3000000000ull, 0, attr, main_q, ^(dispatch_event_t ev) {
			long err;
			if (dispatch_event_get_error(ev, &err)) {
				test_errno("dispatch_event_get_error", err, ECANCELED);
				dispatch_release(dispatch_event_get_source(ev));
			} else {
				switch(++j) {
					case 1:
						fprintf(stderr, "suspending timer for 3 seconds\n");
						dispatch_suspend(tweedledee);
						break;
					case 2:
						fprintf(stderr, "resuming timer\n");
						dispatch_resume(tweedledee);
						dispatch_release(tweedledee);
						break;
					default:
						dispatch_cancel(dispatch_event_get_source(ev));
						break;
				}
			}
		});
	test_ptr_notnull("dispatch_source_timer_create", tweedledum);
	
	dispatch_release(attr);

	dispatch_main();

	return 0;
}
