#include <dispatch/dispatch.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <libkern/OSAtomic.h>

#include "dispatch_test.h"

int
main(void)
{
	test_start("Dispatch Apply");

	volatile __block int32_t count = 0;
	const int32_t final = 32;

	dispatch_queue_t queue = dispatch_get_concurrent_queue(0);
	test_ptr_notnull("dispatch_get_concurrent_queue", queue);

	dispatch_apply(final, queue, ^(size_t i __attribute__((unused))) {
		OSAtomicIncrement32(&count);
	});

	test_long("count", count, final);
	test_stop();

	return 0;
}
