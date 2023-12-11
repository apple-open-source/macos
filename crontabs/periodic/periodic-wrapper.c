/*
 * periodic-wrapper.c
 * Copyright (c) 2012-2013 Apple Inc. All rights reserved.
 */

#include <xpc/xpc.h>
#include <xpc/private.h>
#include <asl.h>
#include <vproc_priv.h>
#include <dirent.h>
#include <CoreAnalytics/CoreAnalytics.h>

extern char **environ;

#define DAILY_PATH			"/etc/periodic/daily"
#define WEEKLY_PATH			"/etc/periodic/weekly"
#define MONTHLY_PATH		"/etc/periodic/monthly"
#define EXPECTED_SCRIPT		"999.local"

static unsigned int
get_file_count(char *directory_path)
{
	DIR *directory = NULL;
	struct dirent* dir_ent = NULL;
	unsigned int count = 0;

	if (directory_path) {
		directory = opendir(directory_path);

		// If the periodic directory doesn't exist or we can't access it
		// it's the same as there not being any jobs for our purposes.
		if (!directory) {
			return 0;
		}

		while ((dir_ent = readdir(directory)) != NULL) {
			if (!strncmp(".", dir_ent->d_name, FILENAME_MAX)
				|| !strncmp("..", dir_ent->d_name, FILENAME_MAX)
				|| !strncmp(EXPECTED_SCRIPT, dir_ent->d_name, FILENAME_MAX)) {
				continue;
			}

			count++;
		}

		if (directory) {
			closedir(directory);
		}
	}

	return count;
}

static void
report_job_count(void) {
	unsigned int daily = 0;
	unsigned int weekly = 0;
	unsigned int monthly = 0;

	daily = get_file_count(DAILY_PATH);
	weekly = get_file_count(WEEKLY_PATH);
	monthly = get_file_count(MONTHLY_PATH);

	const char* keys[3] = {"daily", "weekly", "monthly"};
	const xpc_object_t values[3] = {xpc_uint64_create(daily), xpc_uint64_create(weekly), xpc_uint64_create(monthly)};
	xpc_object_t dict = xpc_dictionary_create(keys, values, 3);

	xpc_release(values[0]);
	xpc_release(values[1]);
	xpc_release(values[2]);

	analytics_send_event_lazy("com.apple.crontabs.periodic.jobcount", ^xpc_object_t(void) {return dict;});
}

int
main(int argc, char *argv[])
{
	int64_t is_managed;
	char activity_name[32];
	static dispatch_source_t sigterm_source;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated"
	xpc_track_activity();
#pragma clang diagnostic pop

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

	sigterm_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGTERM, 0, dispatch_get_main_queue());
	dispatch_source_set_event_handler(sigterm_source, ^{
		exit(0);
	});
	dispatch_resume(sigterm_source);

	snprintf(activity_name, sizeof(activity_name), "com.apple.periodic-%s", argv[1]);

	xpc_activity_register(activity_name, XPC_ACTIVITY_CHECK_IN, ^(xpc_activity_t activity) {
		xpc_activity_state_t state;

		state = xpc_activity_get_state(activity);
		if (state == XPC_ACTIVITY_STATE_RUN) {
			pid_t pid;
			int status;
			bool success = false;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated"
			asl_log(NULL, NULL, ASL_LEVEL_NOTICE, "Running %s periodic task.", argv[1]);
#pragma clang diagnostic pop

			const char *args[] = { "periodic", argv[1], NULL };
			if (posix_spawn(&pid, "/usr/sbin/periodic", NULL, NULL, (char * const *)args, environ) == 0) {
				if (waitpid(pid, &status, 0) != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0) {
					success = true;
				}
			}

			if (!success) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated"
				asl_log(NULL, NULL, ASL_LEVEL_NOTICE, "Error running %s periodic task.", argv[1]);
#pragma clang diagnostic pop
			}
		}
	});

	report_job_count();

	dispatch_main();
}
