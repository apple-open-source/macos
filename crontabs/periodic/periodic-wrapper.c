/*
 * periodic-wrapper.c
 * Copyright (c) 2012-2013 Apple Inc. All rights reserved.
 */

#include <xpc/xpc.h>
#include <xpc/private.h>
#include <asl.h>
#include <vproc_priv.h>

extern char **environ;

int
main(int argc, char *argv[])
{
	int64_t is_managed;
	char activity_name[32];

	xpc_track_activity();

	is_managed = 0;
	(void)vproc_swap_integer(NULL, VPROC_GSK_IS_MANAGED, NULL, &is_managed);
	if (!is_managed) {
		exit(1);
	}

	if (argc != 2) {
		exit(1);
	}

	if (strcmp(argv[1], "daily") == 0) {
	} else if (strcmp(argv[1], "weekly") == 0) {
	} else if (strcmp(argv[1], "monthly") == 0) {
	} else {
		exit(1);
	}

	snprintf(activity_name, sizeof(activity_name), "com.apple.periodic-%s", argv[1]);

	xpc_activity_register(activity_name, XPC_ACTIVITY_CHECK_IN, ^(xpc_activity_t activity) {
		xpc_activity_state_t state;

		state = xpc_activity_get_state(activity);
		if (state == XPC_ACTIVITY_STATE_RUN) {
			pid_t pid;
			int status;
			bool success = false;

			asl_log(NULL, NULL, ASL_LEVEL_NOTICE, "Running %s periodic task.", argv[1]);

			const char *args[] = { "periodic", argv[1], NULL };
			if (posix_spawn(&pid, "/usr/sbin/periodic", NULL, NULL, (char * const *)args, environ) == 0) {
				if (waitpid(pid, &status, 0) != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
					success = true;
				}
			}

			if (!success) {
				asl_log(NULL, NULL, ASL_LEVEL_NOTICE, "Error running %s periodic task.", argv[1]);
			}
		}
	});

	dispatch_main();
}
