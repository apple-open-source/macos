/*
 *  mean.c
 *  mean - lower process priorities with more force than nice
 *
 *  Created by Lucia Ballard on 9/16/09.
 *  Copyright 2009 Apple Inc. All rights reserved.
 *
 */

#include <mach/mach.h>
#include <mach/task.h>

#include <sys/resource.h>

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

void usage(void);

#ifndef PRIO_DARWIN_PROCESS
#define PRIO_DARWIN_PROCESS 4       /* Second argument is a PID */
#endif

void 
usage(void)
{
		fprintf(stderr, "Usage: mean -t <pid>\n");
		fprintf(stderr, "\tthrottle <pid>'s usage of cpu, I/O, and networking\n");
		fprintf(stderr, "mean -u <pid>\n");
		fprintf(stderr,  "\treturn <pid> to normal priority\n");
		fprintf(stderr, "mean -[s|r] <pid>\n");
		fprintf(stderr,  "\tsuspend or resume <pid>\n");
		exit(0);
}

int 
main(int argc, char **argv)
{
	int pid, err, ch;
	mach_port_t task;
	int priority = 0;
	
	boolean_t do_resume = 0, do_suspend = 0;
	boolean_t do_throttle = 0, do_undo = 0;
	
	if (argc < 2) 
		usage();
	
	while ((ch = getopt(argc, argv, "rstu")) != -1)
	switch (ch) {
		case 'u':
			do_undo = 1;
			continue;
		case 'r':
			do_resume = 1;
			continue;
		case 's':
			do_suspend = 1;
			continue;
		case 't':
			do_throttle = 1;
			continue;
		default:
			usage();
	}
	
	argc -= optind; argv += optind;
	
	if (argc == 0)
		usage();

	pid = atoi(*argv);
	if (!pid) 
		usage();

	if (do_throttle || do_undo) {
		priority = PRIO_DARWIN_BG;

		if (do_undo)
			priority = 0;

		err = setpriority(PRIO_DARWIN_PROCESS, pid, priority);
		if (err) {
			fprintf(stderr, "Failed to set priority (%d)\n", errno);
			exit(0);
		}
	}

	if (do_suspend || do_resume) {
		err = task_for_pid(mach_task_self(), pid, &task);
		if (err) {
			fprintf(stderr, "Failed to get task port (%d)\n", err);
			exit(0);
		}

		if (do_suspend) {
			err = task_suspend(task);
			if (err) {
				fprintf(stderr, "Failed to suspend task (%d)\n", err);
			}

		}

		if (do_resume) {
			err = task_resume(task);
			if (err) {
				fprintf(stderr, "Failed to resume task (%d)\n", err);
			}
		}
	}

	return 0;
}

