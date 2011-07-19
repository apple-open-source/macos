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
#include <mach/thread_act.h>
#include <mach/thread_policy.h>

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

void usage(void);


void 
usage(void)
{
		fprintf(stderr, "Usage: mean -[r|s|u] <pid>\n");
		fprintf(stderr,  "\tLower <pid>'s priority.\n");
		fprintf(stderr,  "\t-u: return <pid> to normal priority\n");
		fprintf(stderr,  "\t-r: resume <pid>\n");
		fprintf(stderr,  "\t-s: suspend <pid>\n");
		exit(0);
}

int 
main(int argc, char **argv)
{
	int pid, err, i, ch;
	unsigned int count;
	mach_port_t task;
	thread_act_array_t threads;
	thread_precedence_policy_data_t policy;
	
	boolean_t do_high = 0, do_resume = 0, do_suspend = 0;
	boolean_t do_low = 1;
	
	if (argc < 2) 
		usage();
	
	while ((ch = getopt(argc, argv, "rsu")) != -1)
	switch (ch) {
		case 'u':
			do_high = 1;
			do_low = 0;
			continue;
		case 'r':
			do_resume = 1;
			do_low = 0;
			continue;
		case 's':
			do_suspend = 1;
			do_low = 0;
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
	
	err = task_for_pid(mach_task_self(), pid, &task);
	if (err) {
		fprintf(stderr, "Failed to get task port (%d)\n", err);
		exit(0);
	}

	if (do_low || do_high) {

		err = task_threads(task, &threads, &count);
		if (err) {
			fprintf(stderr, "Failed to get thread list (%d)\n", err);
			exit(0);
		}

		if (do_low)
			policy.importance = -100;
		else
			policy.importance = 0;

		for (i = 0; i < count; i++) {
			err = thread_policy_set(threads[i], 
					THREAD_PRECEDENCE_POLICY, 
					(thread_policy_t) &policy, 
					THREAD_PRECEDENCE_POLICY_COUNT);
			if (err) {
				fprintf(stderr, "Failed to set thread priority (%d)\n", err);
				exit(0);
			}
		}

		printf("Process %d's threads set to %s priority.\n", pid, 
				(do_low ? "lowest" : "highest"));	
	}
	
	if (do_suspend) {
		err = task_suspend(task);
		if (err) {
			fprintf(stderr, "Failed to suspend task (%d)\n", err);
		} else {
			printf("Process %d suspended.\n", pid);
		}
		
	}
	
	if (do_resume) {
		err = task_resume(task);
		if (err) {
			fprintf(stderr, "Failed to resume task (%d)\n", err);
		} else {
			printf("Process %d resumed.\n", pid);
		}
	}

	return 0;
}

