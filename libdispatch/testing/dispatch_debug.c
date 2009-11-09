#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <dispatch/dispatch.h>

#include "dispatch_test.h"

int main(void)
{
	test_start("Dispatch Debug");

	dispatch_queue_t main_q = dispatch_get_main_queue();
	dispatch_debug(main_q, "dispatch_queue_t");
	
	dispatch_queue_t default_q = dispatch_get_concurrent_queue(0);
	dispatch_debug(default_q, "dispatch_queue_t");
	
	dispatch_source_attr_t attr = dispatch_source_attr_create();
	dispatch_debug(attr, "dispatch_source_attr_t");

	dispatch_source_t s = dispatch_source_timer_create(DISPATCH_TIMER_INTERVAL,
		1000000000ull, 0, attr, main_q, ^(dispatch_event_t ev __attribute__((unused))) {});
	dispatch_debug(s, "dispatch_source_t");

	dispatch_group_t g = dispatch_group_create();
	dispatch_debug(g, "dispatch_group_t");

	return 0;
}
