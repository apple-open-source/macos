#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#include "dispatch_test.h"

int
main(void)
{
	test_start("Dispatch dead-name notification");

	dispatch_async(dispatch_get_concurrent_queue(0), ^{
		mach_port_t mp = pthread_mach_thread_np(pthread_self());
		dispatch_source_t ds0;
		kern_return_t kr;

		assert(mp);

		kr = mach_port_mod_refs(mach_task_self(), mp, MACH_PORT_RIGHT_SEND, 1);

		assert(kr == 0);

		ds0 = dispatch_source_machport_create(mp, DISPATCH_MACHPORT_DEAD, NULL, dispatch_get_main_queue(),
					^(dispatch_event_t de) {
			dispatch_release(dispatch_event_get_source(de));
			test_stop();
			exit(EXIT_SUCCESS);
		});

		test_ptr_notnull("dispatch_source_machport_create", ds0);

		// give the mgr queue time to start, otherwise the mgr queue will run
		// on this thread, thus defeating the test which assumes that this
		// thread will die.
		sleep(1);
	});

	dispatch_main();

	return 0;
}
