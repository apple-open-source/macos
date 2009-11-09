#include <dispatch/dispatch.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <spawn.h>
#include <signal.h>
#include <libkern/OSAtomic.h>

#include "dispatch_test.h"

#define PID_CNT 5

static long event_cnt;
static long cancel_cnt;

int
main(void)
{
	dispatch_source_t proc;
	int res;
	pid_t pid;

	test_start("Dispatch Proc");
	
	// Creates a process and register multiple observers.  Send a signal,
	// exit the process, etc., and verify all observers were notified.
	
	posix_spawnattr_t attr;
	res = posix_spawnattr_init(&attr);
	assert(res == 0);
	res = posix_spawnattr_setflags(&attr, POSIX_SPAWN_START_SUSPENDED);
	assert(res == 0);

	char* args[] = {
		"/bin/sleep", "2", NULL
	};
	
	res = posix_spawnp(&pid, args[0], NULL, &attr, args, NULL);
	if (res < 0) {
		perror(args[0]);
		exit(127);
	}

	res = posix_spawnattr_destroy(&attr);
	assert(res == 0);

	dispatch_queue_t semaphore = dispatch_queue_create("semaphore", NULL);
	
	assert(pid > 0);

	int i;
	for (i = 0; i < PID_CNT; ++i) {
		dispatch_suspend(semaphore);
		proc = dispatch_source_proc_create(pid, DISPATCH_PROC_EXIT, NULL, dispatch_get_main_queue(),
			^(dispatch_event_t ev) {
				long err_dom, err_val;
				if ((err_dom = dispatch_event_get_error(ev, &err_val))) {
					test_long("PROC error domain", err_dom, DISPATCH_ERROR_DOMAIN_POSIX);
					test_long("PROC error value", err_val, ECANCELED);
					cancel_cnt++;
					dispatch_resume(semaphore);
				} else {
					long flags = dispatch_event_get_flags(ev);
					test_long("DISPATCH_PROC_EXIT", flags, DISPATCH_PROC_EXIT);
					event_cnt++;
					dispatch_release(dispatch_event_get_source(ev));
				}
		});
		test_ptr_notnull("dispatch_source_proc_create", proc);
	}

	dispatch_async(semaphore, ^{
		int status;
		int res2 = waitpid(pid, &status, 0);
		assert(res2 != -1);
		//int passed = (WIFEXITED(status) && WEXITSTATUS(status) == 0);
		test_long("Sub-process exited", WEXITSTATUS(status) | WTERMSIG(status), 0);
		test_long("Event count", event_cnt, PID_CNT);
		test_long("Cancel count", cancel_cnt, PID_CNT);
		test_stop();
	});

	kill(pid, SIGCONT);

	dispatch_main();

	return 0;
}
