#include <dispatch/dispatch.h>
#include <stdio.h>

#include "dispatch_test.h"

uint32_t count = 0;
const uint32_t final = 1000000; // 10M

void pingpongloop(dispatch_group_t group, dispatch_queue_t ping, dispatch_queue_t pong, size_t counter) {
	//printf("[%p] %s: %lu\n", (void*)(uintptr_t)pthread_self(), dispatch_queue_get_label(dispatch_get_current_queue()), counter);
	if (counter < final) {
		dispatch_group_async(group, pong, ^{ pingpongloop(group, pong, ping, counter+1); });
	} else {
		count = counter;
	}
}

int main(void) {
	
	test_start("Dispatch Ping Pong");

	dispatch_queue_t ping = dispatch_queue_create("ping", NULL);
	test_ptr_notnull("dispatch_queue_create(ping)", ping);
	dispatch_queue_t pong = dispatch_queue_create("pong", NULL);
	test_ptr_notnull("dispatch_queue_create(pong)", pong);
	
	dispatch_group_t group = dispatch_group_create();
	test_ptr_notnull("dispatch_group_create", group);
	
	pingpongloop(group, ping, pong, 0);
	dispatch_group_wait(group, DISPATCH_TIME_FOREVER);

	test_long("count", count, final);
	test_stop();

	return 0;
}
