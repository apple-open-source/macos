#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <dispatch/dispatch.h>
#include <pthread.h>
#include <assert.h>
#include <CoreFoundation/CoreFoundation.h>

int global_count;

void
main_work(void* ctxt)
{
	if (global_count == 20) {
		exit(0);
	}
	uint64_t time = random() % NSEC_PER_SEC;
	printf("Firing timer on main %d\n", ++global_count);
	dispatch_after_f(dispatch_time(0, time), dispatch_get_main_queue(), NULL, main_work);
}


int main(void) {
	global_count = 0;

	dispatch_queue_t dq = dispatch_queue_create("foo.bar", NULL);
		dispatch_async(dq, ^{
			
			dispatch_async_f(dispatch_get_main_queue(), NULL, main_work);
			
			int i;
			for (i=0; i<5; ++i) {
				dispatch_sync(dispatch_get_main_queue(), ^{
					printf("Calling sync %d\n", i);
					assert(pthread_main_np() == 1);
					if (i==4) {
						global_count = 20;
					}
				});
			}
		});

	//dispatch_main();
	CFRunLoopRun();
	return 0;
}
