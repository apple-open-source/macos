#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <dispatch.h>
#include <dispatch_private.h>
#include <pthread.h>

int
main(void)
{
	dispatch_block_t wb = ^(dispatch_item_t di) { printf("\t\t%p\tstart\n", pthread_self()); sleep(3); };
	dispatch_block_t cb = ^(dispatch_item_t di) { printf("\t\t%p\tdone\n", pthread_self()); };
	dispatch_queue_t dq;
	bool r;
	int i;

	dq = dispatch_queue_new("conc", DISPATCH_QUEUE_CONCURRENT, NULL, NULL, NULL);
	assert(dq);

	for (i = 0; i < 10; i++) {
		r = dispatch_call(dq, wb, cb, NULL, NULL);
		assert(r);
	}

	dispatch_main();

	return 0;
}
