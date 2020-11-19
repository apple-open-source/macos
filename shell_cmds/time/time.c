/*	$NetBSD: time.c,v 1.9 1997/10/20 03:28:21 lukem Exp $	*/

/*
 * Copyright (c) 1987, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <errno.h>
#include <err.h>
#include <inttypes.h>
#include <langinfo.h>
#include <libproc.h>
#include <locale.h>
#include <sys/cdefs.h>
#include <sysexits.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

int lflag;
int portableflag;
bool child_running = true;

void
child_handler(int sig)
{
	child_running = false;
}

int
main(int argc, char **argv)
{
	int pid;
	int ch, status, rusage_ret = -1;
	uint64_t before_ns, after_ns, duration_ns, duration_secs, duration_frac_ns;
	struct rusage ru;
	struct rusage_info_v4 ruinfo;
	sigset_t sigmask, suspmask, origmask;

	lflag = 0;
	while ((ch = getopt(argc, argv, "lp")) != -1) {
		switch((char)ch) {
		case 'p':
			portableflag = 1;
			break;
		case 'l':
			lflag = 1;
			break;
		case '?':
		default:
			fprintf(stderr, "usage: time [-lp] <command>\n");
			exit(1);
		}
	}

	if (!(argc -= optind)) {
		exit(0);
	}
	argv += optind;

	sigemptyset(&sigmask);
	/*
	 * Block SIGCHLD so that the check for `child_running` doesn't miss the
	 * handler before calling `sigsuspend` and blocking forever.
	 */
	sigaddset(&sigmask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &sigmask, &origmask);

	/*
	 * Ensure child signals are handled by the parent prior to fork; otherwise,
	 * they could be missed between the child forking and calling `sigsuspend`.
	 */
	(void)signal(SIGCHLD, child_handler);

	sigemptyset(&suspmask);

	before_ns = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
	/*
	 * NB: Don't add anything between these two lines -- measurement is
	 * happening now.
	 */
	switch (pid = vfork()) {
	case -1: /* error */
		err(EX_OSERR, "time");
		__builtin_unreachable();
	case 0: /* child */
		/*
		 * Allow the child to respond to signals by resetting to the original
		 * signal handling behavior.
		 */
		(void)sigprocmask(SIG_SETMASK, &origmask, NULL);
		execvp(*argv, argv);
		perror(*argv);
		_exit((errno == ENOENT) ? 127 : 126);
		__builtin_unreachable();
	default: /* parent */
		break;
	}

	/*
	 * Let the child handle signals that normally exit.
	 */
	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGQUIT, SIG_IGN);

	while (child_running) {
		/*
		 * This would be racy, but SIGCHLD is blocked above (as part of
		 * `sigmask`.
		 */
		sigsuspend(&suspmask);
	}
	/*
	 * NB: Minimize what's added between these statements to preserve the
	 * accuracy of the time measurement.
	 */
	after_ns = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
	if (lflag) {
		rusage_ret = proc_pid_rusage(pid, RUSAGE_INFO_V4, (void **)&ruinfo);
	}
	while (wait3(&status, 0, &ru) != pid) {
	}
	if (!WIFEXITED(status)) {
		fprintf(stderr, "Command terminated abnormally.\n");
	}
	duration_ns = after_ns - before_ns;
	duration_secs = duration_ns / (1000 * 1000 * 1000);
	duration_frac_ns = duration_ns - (duration_secs * 1000 * 1000 * 1000);

	if (portableflag) {
		char *radix = NULL;

		setlocale(LC_ALL, "");

		radix = nl_langinfo(RADIXCHAR);
		if (!radix || radix[0] == '\0') {
			radix = ".";
		}

		fprintf(stderr, "real %9" PRIu64 "%s%02" PRIu64 "\n",
			duration_secs, radix, duration_frac_ns / (10 * 1000 * 1000));
		fprintf(stderr, "user %9ld%s%02ld\n",
			(long)ru.ru_utime.tv_sec, radix, (long)ru.ru_utime.tv_usec/10000);
		fprintf(stderr, "sys  %9ld%s%02ld\n",
			(long)ru.ru_stime.tv_sec, radix, (long)ru.ru_stime.tv_usec/10000);
	} else {
		fprintf(stderr, "%9" PRIu64 ".%02" PRIu64 " real ",
			duration_secs, duration_frac_ns / (10 * 1000 * 1000));
		fprintf(stderr, "%9ld.%02ld user ",
			(long)ru.ru_utime.tv_sec, (long)ru.ru_utime.tv_usec/10000);
		fprintf(stderr, "%9ld.%02ld sys\n",
			(long)ru.ru_stime.tv_sec, (long)ru.ru_stime.tv_usec/10000);
	}

	if (lflag) {
		int hz = 100; /* XXX */
		long ticks;

		ticks = hz * (ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) +
		     hz * (ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) / 1000000;

		fprintf(stderr, "%20ld  %s\n",
			ru.ru_maxrss, "maximum resident set size");
		fprintf(stderr, "%20ld  %s\n", ticks ? ru.ru_ixrss / ticks : 0,
			"average shared memory size");
		fprintf(stderr, "%20ld  %s\n", ticks ? ru.ru_idrss / ticks : 0,
			"average unshared data size");
		fprintf(stderr, "%20ld  %s\n", ticks ? ru.ru_isrss / ticks : 0,
			"average unshared stack size");
		fprintf(stderr, "%20ld  %s\n",
			ru.ru_minflt, "page reclaims");
		fprintf(stderr, "%20ld  %s\n",
			ru.ru_majflt, "page faults");
		fprintf(stderr, "%20ld  %s\n",
			ru.ru_nswap, "swaps");
		fprintf(stderr, "%20ld  %s\n",
			ru.ru_inblock, "block input operations");
		fprintf(stderr, "%20ld  %s\n",
			ru.ru_oublock, "block output operations");
		fprintf(stderr, "%20ld  %s\n",
			ru.ru_msgsnd, "messages sent");
		fprintf(stderr, "%20ld  %s\n",
			ru.ru_msgrcv, "messages received");
		fprintf(stderr, "%20ld  %s\n",
			ru.ru_nsignals, "signals received");
		fprintf(stderr, "%20ld  %s\n",
			ru.ru_nvcsw, "voluntary context switches");
		fprintf(stderr, "%20ld  %s\n",
			ru.ru_nivcsw, "involuntary context switches");

		if (rusage_ret >= 0) {
			if (ruinfo.ri_instructions > 0) {
				fprintf(stderr, "%20" PRIu64 "  %s\n", ruinfo.ri_instructions,
					"instructions retired");
			}
			if (ruinfo.ri_cycles > 0) {
				fprintf(stderr, "%20" PRIu64 "  %s\n", ruinfo.ri_cycles,
					"cycles elapsed");
			}
			if (ruinfo.ri_lifetime_max_phys_footprint > 0) {
				fprintf(stderr, "%20" PRIu64 "  %s\n",
					ruinfo.ri_lifetime_max_phys_footprint,
					"peak memory footprint");
			}
		}
	}

	exit(WIFEXITED(status) ? WEXITSTATUS(status) : EXIT_FAILURE);
}
