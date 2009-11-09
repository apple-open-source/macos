#include <dispatch/dispatch.h>
#include <assert.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <mach/clock_types.h>

#include "dispatch_test.h"

extern char **environ;

int
main(int argc, char *argv[])
{
	dispatch_source_t tmp_ds;
	int res;
	pid_t pid;

	if (argc < 2) {
		fprintf(stderr, "usage: harness [...]\n");
		exit(1);
	}

	posix_spawnattr_t attr;
	res = posix_spawnattr_init(&attr);
	assert(res == 0);
	res = posix_spawnattr_setflags(&attr, POSIX_SPAWN_START_SUSPENDED);
	assert(res == 0);

	int i;
	char** newargv = calloc(argc, sizeof(void*));
	for (i = 1; i < argc; ++i) {
		newargv[i-1] = argv[i];
	}
	newargv[i-1] = NULL;

	res = posix_spawnp(&pid, newargv[0], NULL, &attr, newargv, environ);
	if (res) {
		errno = res;
		perror(newargv[0]);
		exit(EXIT_FAILURE);
	}
	//fprintf(stderr, "pid = %d\n", pid);
	assert(pid > 0);

	dispatch_queue_t main_q = dispatch_get_main_queue();

	tmp_ds = dispatch_source_proc_create(pid, DISPATCH_PROC_EXIT, NULL, main_q,
		^(dispatch_event_t ev __attribute__((unused))) {
			int status;
			int res2 = waitpid(pid, &status, 0);
			assert(res2 != -1);
			//int passed = (WIFEXITED(status) && WEXITSTATUS(status) == 0);
			test_long("Process exited", WEXITSTATUS(status) | WTERMSIG(status), 0);
			exit(0);
	});
	assert(tmp_ds);
	
	uint64_t timeout = 30LL * NSEC_PER_SEC;

	tmp_ds = dispatch_source_timer_create(DISPATCH_TIMER_ONESHOT, timeout, 0, NULL, main_q,
		^(dispatch_event_t ev __attribute__((unused))) {
			kill(pid, SIGKILL);
			fprintf(stderr, "Terminating unresponsive process (%0.1lfs)\n", (double)timeout/NSEC_PER_SEC);
	});
	assert(tmp_ds);

	signal(SIGINT, SIG_IGN);
	tmp_ds = dispatch_source_signal_create(SIGINT, NULL, main_q,
		^(dispatch_event_t ev __attribute__((unused))) {
			fprintf(stderr, "Terminating process due to signal\n");
			kill(pid, SIGKILL);
	});
	assert(tmp_ds);

	kill(pid, SIGCONT);

	dispatch_main();

	return 0;
}
