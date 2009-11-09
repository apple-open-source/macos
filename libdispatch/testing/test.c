#include <mach/mach.h>
#include <mach/mach_time.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <dispatch.h>

int
main(void)
{
	void (^wb)(dispatch_item_t) = ^(dispatch_item_t di) {
		printf("%p\t%p\t%s:\t%llu\n", pthread_self(), di, __func__, mach_absolute_time());
	};
	void (^cb)(dispatch_item_t) = ^(dispatch_item_t di) {
		printf("%p\t%p\t%s:\t%llu\n", pthread_self(), di, __func__, mach_absolute_time());
	};
	dispatch_queue_t q;
	dispatch_item_t di_r;
	size_t i;
	bool r;

	q = dispatch_queue_new("test", 0, NULL, NULL, NULL);
	assert(q != NULL);

	for (i = 0; i < 1000; i++) {
		r = dispatch_call_wait(q, wb, NULL);
		assert(r);
	}

	printf("done with dispatch_call_wait()\n");

	r = dispatch_apply_wait(wb, 10, NULL);
	assert(r);

	r = dispatch_call(q, wb, cb, NULL, &di_r);
	assert(r);
	assert(di_r);

	printf("waiting for dispatch_call() callback\n");

	dispatch_main();

	return 0;
}
