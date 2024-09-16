/*
 * Copyright (c) 2008, 2009, 2019 Apple Computer, Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <curses.h>
#include <fcntl.h>
#include <inttypes.h>
#include <mach/clock_types.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include "libtop.h"
#include "log.h"
#include "logging.h"
#include "options.h"
#include "preferences.h"
#include "sig.h"
#include "top.h"
#include "userinput.h"

const libtop_tsamp_t *tsamp;

static volatile sig_atomic_t resized = 1;

static int cached_lines = 0, cached_columns = 0;

static void init(void);

enum { MICROSECONDS = 1000000 };

static void
event_loop(void *tinst)
{
	sigset_t sset, oldsset;
	int samples;
	struct timeval tlimit;

	if (sigemptyset(&sset)) {
		perror("sigemptyset");
		exit(EXIT_FAILURE);
	}

	if (sigaddset(&sset, SIGWINCH)) {
		perror("sigaddset");
		exit(EXIT_FAILURE);
	}

	uint64_t beforens = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW);

	while (1) {
		bool sleep_expired = false;
		fd_set fset;
		int ready;

		FD_ZERO(&fset);
		FD_SET(STDIN_FILENO, &fset);

		tlimit.tv_sec = top_prefs_get_sleep();
		tlimit.tv_usec = 0;

		ready = select(STDIN_FILENO + 1, &fset, NULL, NULL, &tlimit);

		uint64_t nowns = clock_gettime_nsec_np(CLOCK_MONOTONIC_RAW);

		if (nowns >= beforens + (uint64_t)tlimit.tv_sec * NSEC_PER_SEC) {
			/*
			 * The sleep has expired, so we should insert new
			 * data for all stats.  This is different than just
			 * the case where we handle user input and the rest
			 * of the data is awaiting a sleep interval update.
			 */
			sleep_expired = true;
			beforens = nowns;
		}

		if (sleep_expired) {
			samples = top_prefs_get_samples();

			if (samples > -1) {
				/* Samples was set in the preferences. */
				if (0 == samples) {
					/* We had N samples and now it's time to exit. */
					endwin();
					exit(EXIT_SUCCESS);
				}

				top_prefs_set_samples(samples - 1);
			}
		}

		if (top_signal_is_exit_set()) {
			exit(EXIT_SUCCESS);
		}

		if (ready && FD_ISSET(STDIN_FILENO, &fset))
			(void)user_input_process(tinst);

		if (sleep_expired)
			top_insert(tinst);

		/* Block SIGWINCH signals while we are in a relayout. */
		if (-1 == sigprocmask(SIG_BLOCK, &sset, &oldsset)) {
			perror("sigprocmask");
			exit(EXIT_FAILURE);
		}

		if (top_need_relayout() || resized || LINES != cached_lines || COLS != cached_columns) {
			cached_lines = LINES;
			cached_columns = COLS;

			if (top_layout(tinst)) {
				resized = 1;
			} else {
				resized = 0;
			}
		}

		if (-1 == sigprocmask(SIG_SETMASK, &oldsset, NULL)) {
			perror("sigprocmask");
			exit(EXIT_FAILURE);
		}

		top_draw(tinst);
	}
}

void
exit_handler(void)
{
	endwin();
}

void
init(void)
{
	if (NULL == initscr()) {
		fprintf(stderr, "error: unable to initscr!\n");
		exit(EXIT_FAILURE);
	}

	atexit(exit_handler);

	if (ERR == cbreak()        /* disable line buffering */
			|| ERR == noecho() /* disable echoing what the user types */
			|| ERR == nonl()   /* no newline */
			|| ERR == intrflush(stdscr, FALSE) || ERR == meta(stdscr, TRUE)
			|| ERR == keypad(stdscr, TRUE)) {
		fprintf(stderr, "error: initializing curses\n");
		exit(EXIT_FAILURE);
	}

	timeout(0);
}

int
main(int argc, char *argv[])
{
	void *tinst;

	top_prefs_init();
	top_options_init();

	if (top_options_parse(argc, argv)) {
		top_options_usage(stderr, argv[0]);
		return EXIT_FAILURE;
	}

	/* 18007048: If output isn't a tty and -l isn't specified, imply -l 0. */
	if ((!isatty(STDOUT_FILENO)) && (top_prefs_get_samples() < 0)) {
		top_prefs_set_samples(0);
	}

	if (top_prefs_get_samples() > -1)
		top_prefs_set_logging_mode(true);

	if (!top_prefs_get_logging_mode())
		init();

	top_signal_init();

	if (libtop_init(NULL, NULL)) {
		endwin();
		fprintf(stderr, "libtop_init failed!\n");
		return EXIT_FAILURE;
	}

	if (top_prefs_get_frameworks() && libtop_set_interval(top_prefs_get_frameworks_interval())) {
		endwin();
		fprintf(stderr, "error: setting framework update interval.\n");
		exit(EXIT_FAILURE);
	}

	tinst = top_create(stdscr);

	if (!top_prefs_get_logging_mode()) {
		top_insert(tinst);
		top_layout(tinst);
		top_draw(tinst);
		event_loop(tinst);
	} else {
		top_logging_loop(tinst);
	}

	return EXIT_SUCCESS;
}
