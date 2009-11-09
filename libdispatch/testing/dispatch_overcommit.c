#include <dispatch/dispatch.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <libkern/OSAtomic.h>

#include "dispatch_test.h"

int32_t count = 0;
const int32_t final = 32;

int
main(void)
{
	test_start("Dispatch Overcommit");

	dispatch_queue_attr_t attr = dispatch_queue_attr_create();
	test_ptr_notnull("dispatch_queue_attr_create", attr);
	dispatch_queue_attr_set_flags(attr, DISPATCH_QUEUE_OVERCOMMIT);
	
	int i;
	for (i = 0; i < final; ++i) {
		char* name;
		asprintf(&name, "test.overcommit.%d", i);
		
		dispatch_queue_t queue = dispatch_queue_create(name, attr);
		test_ptr_notnull("dispatch_queue_create", queue);
		free(name);
		
		dispatch_async(queue, ^{
			OSAtomicIncrement32(&count);
			if (count == final) {
				test_long("count", count, final);
				test_stop();
			} else {
				while (1); // spin
			}
		});
	}
	
	dispatch_main();

	return 0;
}
