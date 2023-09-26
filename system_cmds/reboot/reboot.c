/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1980, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Portions copyright (c) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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

#include <sys/cdefs.h>

#ifndef lint
__COPYRIGHT(
"@(#) Copyright (c) 1980, 1986, 1993\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)reboot.c	8.1 (Berkeley) 6/5/93";
#endif
#endif /* not lint */
__FBSDID("$FreeBSD$");

#include <sys/reboot.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <signal.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmpx.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#if !(TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
#include "kextmanager.h"
#include <IOKit/kext/kextmanager_types.h>
#endif
#include <mach/mach_port.h>		// allocate
#include <mach/mach.h>			// task_self, etc
#include <servers/bootstrap.h>	// bootstrap
#include <bootstrap_priv.h>
#include <reboot2.h>
#endif

static void usage(void);
#ifndef __APPLE__
static u_int get_pageins(void);
#endif
#if defined(__APPLE__) && !(TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
static int reserve_reboot(void);
#endif


static int dohalt;

int
main(int argc, char *argv[])
{
	struct utmpx utx;
	const struct passwd *pw;
	int ch, howto, lflag, nflag, qflag, Nflag;
#ifdef __APPLE__
	int uflag;
#else
	int i, fd, sverrno;
	u_int pageins;
	const char *kernel = NULL;
#endif
	const char *user;

	if (strstr(getprogname(), "halt") != NULL) {
		dohalt = 1;
		howto = RB_HALT;
	} else
		howto = 0;
	lflag = nflag = qflag = Nflag = 0;
	while ((ch = getopt(argc, argv,
		"lnNq"
#ifdef __APPLE__
		"u"
#else
		"cdk:pr"
#endif
	    )) != -1)

		switch(ch) {
#ifndef __APPLE__
		case 'c':
			howto |= RB_POWERCYCLE;
			break;
		case 'd':
			howto |= RB_DUMP;
			break;
		case 'k':
			kernel = optarg;
			break;
#endif
		case 'l':
			lflag = 1;
			break;
		case 'n':
			nflag = 1;
			howto |= RB_NOSYNC;
			break;
		case 'N':
			nflag = 1;
			Nflag = 1;
			break;
/* -p is irrelevant on OS X.  It does that anyway. */
#ifndef __APPLE__
		case 'p':
			howto |= RB_POWEROFF;
			break;
		case 'r':
			howto |= RB_REROOT;
			break;
#endif
#ifdef __APPLE__
		case 'u':
			uflag = 1;
			howto |= RB_UPSDELAY;
			break;
#endif
		case 'q':
			qflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;
#ifndef __APPLE__
	if (argc != 0)
		usage();
#endif

#ifndef __APPLE__
	if ((howto & (RB_DUMP | RB_HALT)) == (RB_DUMP | RB_HALT))
		errx(1, "cannot dump (-d) when halting; must reboot instead");
#endif
	if (Nflag && (howto & RB_NOSYNC) != 0)
		errx(1, "-N cannot be used with -n");
#ifndef __APPLE__
	if ((howto & RB_POWEROFF) && (howto & RB_POWERCYCLE))
		errx(1, "-c and -p cannot be used together");
	if ((howto & RB_REROOT) != 0 && howto != RB_REROOT)
		errx(1, "-r cannot be used with -c, -d, -n, or -p");
#endif
	if (geteuid()) {
		errno = EPERM;
		err(1, NULL);
	}

#if defined(__APPLE__) && !(TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
	if (!qflag && !lflag) {	// shutdown(8) has already checked w/kextd
		if ((errno = reserve_reboot()))
			err(1, "couldn't lock for reboot");
	}
#endif

	if (qflag) {
		reboot(howto);
		err(1, NULL);
	}

#ifndef __APPLE__
	if (kernel != NULL) {
		fd = open("/boot/nextboot.conf", O_WRONLY | O_CREAT | O_TRUNC,
		    0444);
		if (fd > -1) {
			(void)write(fd, "nextboot_enable=\"YES\"\n", 22);
			(void)write(fd, "kernel=\"", 8L);
			(void)write(fd, kernel, strlen(kernel));
			(void)write(fd, "\"\n", 2);
			close(fd);
		}
	}
#endif

	/* Log the reboot. */
	if (!lflag)  {
		if ((user = getlogin()) == NULL)
			user = (pw = getpwuid(getuid())) ?
			    pw->pw_name : "???";
		if (dohalt) {
			openlog("halt", 0, LOG_AUTH | LOG_CONS);
			syslog(LOG_CRIT, "halted by %s%s", user, 
			     (howto & RB_UPSDELAY) ? " with UPS delay":"");
#ifndef __APPLE__
		} else if (howto & RB_REROOT) {
			openlog("reroot", 0, LOG_AUTH | LOG_CONS);
			syslog(LOG_CRIT, "rerooted by %s", user);
		} else if (howto & RB_POWEROFF) {
			openlog("reboot", 0, LOG_AUTH | LOG_CONS);
			syslog(LOG_CRIT, "powered off by %s", user);
		} else if (howto & RB_POWERCYCLE) {
			openlog("reboot", 0, LOG_AUTH | LOG_CONS);
			syslog(LOG_CRIT, "power cycled by %s", user);
#endif
		} else {
			openlog("reboot", 0, LOG_AUTH | LOG_CONS);
			syslog(LOG_CRIT, "rebooted by %s", user);
		}
	}
	bzero(&utx, sizeof(utx));
	utx.ut_type = SHUTDOWN_TIME;
	gettimeofday(&utx.ut_tv, NULL);
	pututxline(&utx);

	/*
	 * Do a sync early on, so disks start transfers while we're off
	 * killing processes.  Don't worry about writes done before the
	 * processes die, the reboot system call syncs the disks.
	 */
	if (!nflag)
		sync();

	/*
	 * Ignore signals that we can get as a result of killing
	 * parents, group leaders, etc.
	 */
	(void)signal(SIGHUP,  SIG_IGN);
	(void)signal(SIGINT,  SIG_IGN);
	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGTERM, SIG_IGN);
	(void)signal(SIGTSTP, SIG_IGN);

	/*
	 * If we're running in a pipeline, we don't want to die
	 * after killing whatever we're writing to.
	 */
	(void)signal(SIGPIPE, SIG_IGN);

#ifndef __APPLE__
	/*
	 * Only init(8) can perform rerooting.
	 */
	if (howto & RB_REROOT) {
		if (kill(1, SIGEMT) == -1)
			err(1, "SIGEMT init");

		return (0);
	}

	/* Just stop init -- if we fail, we'll restart it. */
	if (kill(1, SIGTSTP) == -1)
		err(1, "SIGTSTP init");

	/* Send a SIGTERM first, a chance to save the buffers. */
	if (kill(-1, SIGTERM) == -1 && errno != ESRCH)
		err(1, "SIGTERM processes");

	/*
	 * After the processes receive the signal, start the rest of the
	 * buffers on their way.  Wait 5 seconds between the SIGTERM and
	 * the SIGKILL to give everybody a chance. If there is a lot of
	 * paging activity then wait longer, up to a maximum of approx
	 * 60 seconds.
	 */
	sleep(2);
	for (i = 0; i < 20; i++) {
		pageins = get_pageins();
		if (!nflag)
			sync();
		sleep(3);
		if (get_pageins() == pageins)
			break;
	}

	for (i = 1;; ++i) {
		if (kill(-1, SIGKILL) == -1) {
			if (errno == ESRCH)
				break;
			goto restart;
		}
		if (i > 5) {
			(void)fprintf(stderr,
			    "WARNING: some process(es) wouldn't die\n");
			break;
		}
		(void)sleep(2 * i);
	}
#endif

#ifdef __APPLE__
	// launchd(8) handles reboot.  This call returns NULL on success.
	exit(reboot3(howto) == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
#else /* !__APPLE__ */
	reboot(howto);
	/* FALLTHROUGH */

restart:
	sverrno = errno;
	errx(1, "%s%s", kill(1, SIGHUP) == -1 ? "(can't restart init): " : "",
	    strerror(sverrno));
	/* NOTREACHED */
#endif /* __APPLE__ */
}

static void
usage(void)
{

	(void)fprintf(stderr, dohalt ?
#ifndef __APPLE__
	    "usage: halt [-clNnpq] [-k kernel]\n" :
	    "usage: reboot [-cdlNnpqr] [-k kernel]\n"
#else
	    "usage: halt [-lNnqu]\n" :
	    "usage: reboot [-lNnqu]\n"
#endif
	    );
	exit(1);
}

#ifndef __APPLE__
static u_int
get_pageins(void)
{
	u_int pageins;
	size_t len;

	len = sizeof(pageins);
	if (sysctlbyname("vm.stats.vm.v_swappgsin", &pageins, &len, NULL, 0)
	    != 0) {
		warnx("v_swappgsin");
		return (0);
	}
	return pageins;
}
#endif

#if defined(__APPLE__) && !(TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
// XX this routine is also in shutdown; it would be nice to share

static bool
kextdDisabled(void)
{
	uint32_t disabled = 0;
	size_t   sizeOfDisabled = sizeof(disabled);
	if (sysctlbyname("hw.use_kernelmanagerd", &disabled, &sizeOfDisabled, NULL, 0) != 0) {
		return false;
	}
	return (disabled != 0);
}

#define WAITFORLOCK 1
/*
 * contact kextd to lock for reboot
 */
int
reserve_reboot(void)
{
	int rval = ELAST + 1;
	kern_return_t macherr = KERN_FAILURE;
	mach_port_t kxport, tport = MACH_PORT_NULL, myport = MACH_PORT_NULL;
	int busyStatus = ELAST + 1;
	mountpoint_t busyVol;

	if (kextdDisabled()) {
		/* no need to talk with kextd if it's not running */
		return 0;
	}

	macherr = bootstrap_look_up2(bootstrap_port, KEXTD_SERVER_NAME, &kxport, 0, BOOTSTRAP_PRIVILEGED_SERVER);
	if (macherr)  goto finish;

	// allocate a port to pass to kextd (in case we die)
	tport = mach_task_self();
	if (tport == MACH_PORT_NULL)  goto finish;
	macherr = mach_port_allocate(tport, MACH_PORT_RIGHT_RECEIVE, &myport);
	if (macherr)  goto finish;

	// try to lock for reboot
	macherr = kextmanager_lock_reboot(kxport, myport, !WAITFORLOCK, busyVol,
                                      &busyStatus);
	if (macherr)  goto finish;

	if (busyStatus == EBUSY) {
		warnx("%s is busy updating; waiting for lock", busyVol);
		macherr = kextmanager_lock_reboot(kxport, myport, WAITFORLOCK,
										  busyVol, &busyStatus);
		if (macherr)	goto finish;
	}

	if (busyStatus == EALREADY) {
		// reboot already in progress
		rval = 0;
	} else {
		rval = busyStatus;
	}

finish:
	// in general, we want to err on the side of allowing the reboot
	if (macherr) {
		if (macherr != BOOTSTRAP_UNKNOWN_SERVICE)
			warnx("WARNING: couldn't lock kext manager for reboot: %s",
					mach_error_string(macherr));
		rval = 0;
	}
	// unless we got the lock, clean up our port
	if (busyStatus != 0 && myport != MACH_PORT_NULL)
		mach_port_mod_refs(tport, myport, MACH_PORT_RIGHT_RECEIVE, -1);

	return rval;
}
#endif
