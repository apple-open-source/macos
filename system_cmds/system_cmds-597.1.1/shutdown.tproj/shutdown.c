/*
 * Copyright (c) 1988, 1990, 1993
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

#if 0
#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1988, 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)shutdown.c	8.4 (Berkeley) 4/28/95";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
#ifndef __APPLE__
__FBSDID("$FreeBSD: src/sbin/shutdown/shutdown.c,v 1.28 2005/01/25 08:40:51 delphij Exp $");
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/syslog.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __APPLE__
#include <errno.h>
#include <util.h>
#include <bsm/libbsm.h>
#include <bsm/audit_uevents.h>
#include <vproc.h>
#include <vproc_priv.h>

#include "kextmanager.h"
#include <IOKit/kext/kextmanager_types.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <mach/mach_port.h>		// allocate
#include <mach/mach.h>			// task_self, etc
#include <servers/bootstrap.h>	// bootstrap
#include <bootstrap_priv.h>
#include <reboot2.h>
#include <utmpx.h>
#include <sys/sysctl.h>

#include "pathnames.h"
#endif /* __APPLE__ */

#ifdef DEBUG
#undef _PATH_NOLOGIN
#define	_PATH_NOLOGIN	"./nologin"
#endif

#define	H		*60*60
#define	M		*60
#define	S		*1
#define	NOLOG_TIME	5*60
struct interval {
	int timeleft, timetowait;
} tlist[] = {
	{ 10 H,  5 H },
	{  5 H,  3 H },
	{  2 H,  1 H },
	{  1 H, 30 M },
	{ 30 M, 10 M },
	{ 20 M, 10 M },
	{ 10 M,  5 M },
	{  5 M,  3 M },
	{  2 M,  1 M },
	{  1 M, 30 S },
	{ 30 S, 30 S },
	{  0  ,  0   }
};
#undef H
#undef M
#undef S

static time_t offset, shuttime;
#ifdef __APPLE__
static int dohalt, doreboot, doups, killflg, mbuflen, oflag;
#else
static int dohalt, dopower, doreboot, killflg, mbuflen, oflag;
#endif
static char mbuf[BUFSIZ];
static const char *nosync, *whom;
#ifdef __APPLE__
static int dosleep;
#endif

void badtime(void);
#ifdef __APPLE__
void log_and_exec_reboot_or_halt(void);
#else
void die_you_gravy_sucking_pig_dog(void);
#endif
void finish(int);
void getoffset(char *);
void loop(void);
void nolog(void);
void timeout(int);
void timewarn(int);
void usage(const char *);
#ifdef __APPLE__
int audit_shutdown(int);
int reserve_reboot(void);
#endif

extern const char **environ;

int
main(int argc, char **argv)
{
	char *p, *endp;
	struct passwd *pw;
	int arglen, ch, len, readstdin;

#ifndef DEBUG
	if (geteuid())
		errx(1, "NOT super-user");
#endif
	nosync = NULL;
	readstdin = 0;
#ifndef __APPLE__
	while ((ch = getopt(argc, argv, "-hknopr")) != -1)
#else
	while ((ch = getopt(argc, argv, "-hknorsu")) != -1)
#endif
		switch (ch) {
		case '-':
			readstdin = 1;
			break;
		case 'h':
			dohalt = 1;
			break;
		case 'k':
			killflg = 1;
			break;
		case 'n':
			nosync = "-n";
			break;
		case 'o':
			oflag = 1;
			break;
#ifndef __APPLE__
		case 'p':
			dopower = 1;
			break;
#endif
        case 'u':
            doups = 1;
            break;
		case 'r':
			doreboot = 1;
			break;
#ifdef __APPLE__
		case 's':
			dosleep = 1;
			break;
#endif
		case '?':
		default:
			usage((char *)NULL);
		}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage((char *)NULL);

#ifndef __APPLE__
	if (killflg + doreboot + dohalt + dopower > 1)
		usage("incompatible switches -h, -k, -p and -r");

	if (oflag && !(dohalt || dopower || doreboot))
		usage("-o requires -h, -p or -r");

	if (nosync != NULL && !oflag)
		usage("-n requires -o");
#else /* !__APPLE__ */
	if (killflg + doreboot + dohalt + dosleep > 1)
		usage("incompatible switches -h, -k, -r, and -s");

	if (!(dohalt || doreboot || dosleep || killflg))
		usage("-h, -r, -s, or -k is required");
		
	if (doups && !dohalt)
		usage("-u requires -h");
#endif /* !__APPLE__ */

	getoffset(*argv++);

	if (*argv) {
		for (p = mbuf, len = sizeof(mbuf); *argv; ++argv) {
			arglen = strlen(*argv);
			if ((len -= arglen) <= 2)
				break;
			if (p != mbuf)
				*p++ = ' ';
			memmove(p, *argv, arglen);
			p += arglen;
		}
		*p = '\n';
		*++p = '\0';
	}

	if (readstdin) {
		p = mbuf;
		endp = mbuf + sizeof(mbuf) - 2;
		for (;;) {
			if (!fgets(p, endp - p + 1, stdin))
				break;
			for (; *p &&  p < endp; ++p);
			if (p == endp) {
				*p = '\n';
				*++p = '\0';
				break;
			}
		}
	}
	mbuflen = strlen(mbuf);

	if (offset)
		(void)printf("Shutdown at %.24s.\n", ctime(&shuttime));
	else
		(void)printf("Shutdown NOW!\n");

	if (!(whom = getlogin()))
		whom = (pw = getpwuid(getuid())) ? pw->pw_name : "???";

#ifdef DEBUG
	audit_shutdown(0);
	(void)putc('\n', stdout);
#else
	(void)setpriority(PRIO_PROCESS, 0, PRIO_MIN);
#ifdef __APPLE__
	if (offset) {
#else
	{
#endif
		int forkpid;

		forkpid = fork();
		if (forkpid == -1) {
			audit_shutdown(1);
			err(1, "fork");
		}
		if (forkpid)
			errx(0, "[pid %d]", forkpid);
#ifdef __APPLE__
		/* 5863185: reboot2() needs to talk to launchd. */
		if (_vprocmgr_detach_from_console(0) != NULL)
			warnx("can't detach from console");
#endif /* __APPLE__ */
	}
	audit_shutdown(0);
	setsid();
#endif
	openlog("shutdown", LOG_CONS, LOG_AUTH);
	loop();
	return(0);
}

void
loop()
{
	struct interval *tp;
	u_int sltime;
	int logged;

	if (offset <= NOLOG_TIME) {
		logged = 1;
		nolog();
	}
	else
		logged = 0;
	tp = tlist;
	if (tp->timeleft < offset)
		(void)sleep((u_int)(offset - tp->timeleft));
	else {
		while (tp->timeleft && offset < tp->timeleft)
			++tp;
		/*
		 * Warn now, if going to sleep more than a fifth of
		 * the next wait time.
		 */
		if ((sltime = offset - tp->timeleft)) {
			if (sltime > (u_int)(tp->timetowait / 5))
				timewarn(offset);
			(void)sleep(sltime);
		}
	}
	for (;; ++tp) {
		timewarn(tp->timeleft);
		if (!logged && tp->timeleft <= NOLOG_TIME) {
			logged = 1;
			nolog();
		}
		(void)sleep((u_int)tp->timetowait);
		if (!tp->timeleft)
			break;
	}
#ifdef __APPLE__
	log_and_exec_reboot_or_halt();
#else
	die_you_gravy_sucking_pig_dog();
#endif
}

static jmp_buf alarmbuf;

static const char *restricted_environ[] = {
	"PATH=" _PATH_STDPATH,
	NULL
};

void
timewarn(int timeleft)
{
	static int first;
	static char hostname[MAXHOSTNAMELEN + 1];
	FILE *pf;
	char wcmd[MAXPATHLEN + 4];

	/* wall is sometimes missing, e.g. on install media */
	if (access(_PATH_WALL, X_OK) == -1) return;

	if (!first++)
		(void)gethostname(hostname, sizeof(hostname));

	/* undoc -n option to wall suppresses normal wall banner */
	(void)snprintf(wcmd, sizeof(wcmd), "%s -n", _PATH_WALL);
	environ = restricted_environ;
	if (!(pf = popen(wcmd, "w"))) {
		syslog(LOG_ERR, "shutdown: can't find %s: %m", _PATH_WALL);
		return;
	}

	(void)fprintf(pf,
	    "\007*** %sSystem shutdown message from %s@%s ***\007\n",
	    timeleft ? "": "FINAL ", whom, hostname);

	if (timeleft > 10*60)
		(void)fprintf(pf, "System going down at %5.5s\n\n",
		    ctime(&shuttime) + 11);
	else if (timeleft > 59)
		(void)fprintf(pf, "System going down in %d minute%s\n\n",
		    timeleft / 60, (timeleft > 60) ? "s" : "");
	else if (timeleft)
		(void)fprintf(pf, "System going down in 30 seconds\n\n");
	else
		(void)fprintf(pf, "System going down IMMEDIATELY\n\n");

	if (mbuflen)
		(void)fwrite(mbuf, sizeof(*mbuf), mbuflen, pf);

	/*
	 * play some games, just in case wall doesn't come back
	 * probably unnecessary, given that wall is careful.
	 */
	if (!setjmp(alarmbuf)) {
		(void)signal(SIGALRM, timeout);
		(void)alarm((u_int)30);
		(void)pclose(pf);
		(void)alarm((u_int)0);
		(void)signal(SIGALRM, SIG_DFL);
	}
}

void
timeout(int signo __unused)
{
	longjmp(alarmbuf, 1);
}

void
#ifdef __APPLE__
log_and_exec_reboot_or_halt()
#else
die_you_gravy_sucking_pig_dog()
#endif
{
#ifndef __APPLE__
	char *empty_environ[] = { NULL };
#else
	if ((errno = reserve_reboot())) {
		warn("couldn't lock for reboot");
		finish(0);
	}
#endif

	syslog(LOG_NOTICE, "%s%s by %s: %s",
#ifndef __APPLE__
	    doreboot ? "reboot" : dohalt ? "halt" : dopower ? "power-down" : 
#else
	    doreboot ? "reboot" : dohalt ? "halt" : dosleep ? "sleep" :
#endif
	    "shutdown", doups?" with UPS delay":"", whom, mbuf);
#ifndef __APPLE__
	(void)sleep(2);
#endif

	(void)printf("\r\nSystem shutdown time has arrived\007\007\r\n");
	if (killflg) {
		(void)printf("\rbut you'll have to do it yourself\r\n");
		exit(0);
	}
#ifdef DEBUG
	if (doreboot)
		(void)printf("reboot");
	else if (dohalt)
		(void)printf("halt");
#ifndef __APPLE__
	else if (dopower)
		(void)printf("power-down");
	if (nosync != NULL)
		(void)printf(" no sync");
#else
	else if (dosleep)
		(void)printf("sleep");
#endif
	(void)printf("\nkill -HUP 1\n");
#else
#ifdef __APPLE__
	if (dosleep) {
		mach_port_t mp;
		io_connect_t fb;
		kern_return_t kr = IOMasterPort(bootstrap_port, &mp);
		if (kr == kIOReturnSuccess) {
			fb = IOPMFindPowerManagement(mp);
			if (fb != IO_OBJECT_NULL) {
				IOReturn err = IOPMSleepSystem(fb);
				if (err != kIOReturnSuccess) {
					fprintf(stderr, "shutdown: sleep failed (0x%08x)\n", err);
					kr = -1;
				}
			}
		}
	} else {
		int howto = 0;

#if defined(__APPLE__) 
		{
			struct utmpx utx;
			bzero(&utx, sizeof(utx));
			utx.ut_type = SHUTDOWN_TIME;
			gettimeofday(&utx.ut_tv, NULL);
			pututxline(&utx);

			int newvalue = 1;
			sysctlbyname("kern.willshutdown", NULL, NULL, &newvalue, sizeof(newvalue));
		}
#else
		logwtmp("~", "shutdown", "");
#endif

		if (dohalt) howto |= RB_HALT;
		if (doups) howto |= RB_UPSDELAY;
		if (nosync) howto |= RB_NOSYNC;

		// launchd(8) handles reboot.  This call returns NULL on success.
		if (reboot2(howto)) {
			syslog(LOG_ERR, "shutdown: launchd reboot failed.");
		}
	}
#else /* __APPLE__ */
	if (!oflag) {
		(void)kill(1, doreboot ? SIGINT :	/* reboot */
			      dohalt ? SIGUSR1 :	/* halt */
			      dopower ? SIGUSR2 :	/* power-down */
			      SIGTERM);			/* single-user */
	} else {
		if (doreboot) {
			execle(_PATH_REBOOT, "reboot", "-l", nosync, 
				(char *)NULL, empty_environ);
			syslog(LOG_ERR, "shutdown: can't exec %s: %m.",
				_PATH_REBOOT);
			warn(_PATH_REBOOT);
		}
		else if (dohalt) {
			execle(_PATH_HALT, "halt", "-l", nosync,
				(char *)NULL, empty_environ);
			syslog(LOG_ERR, "shutdown: can't exec %s: %m.",
				_PATH_HALT);
			warn(_PATH_HALT);
		}
		else if (dopower) {
			execle(_PATH_HALT, "halt", "-l", "-p", nosync,
				(char *)NULL, empty_environ);
			syslog(LOG_ERR, "shutdown: can't exec %s: %m.",
				_PATH_HALT);
			warn(_PATH_HALT);
		}
		(void)kill(1, SIGTERM);		/* to single-user */
	}
#endif /* __APPLE__ */
#endif
	finish(0);
}

#define	ATOI2(p)	(p[0] - '0') * 10 + (p[1] - '0'); p += 2;

void
getoffset(char *timearg)
{
	struct tm *lt;
	char *p;
	time_t now;
	int this_year;

	(void)time(&now);

	if (!strcasecmp(timearg, "now")) {		/* now */
		offset = 0;
		shuttime = now;
		return;
	}

	if (*timearg == '+') {				/* +minutes */
		if (!isdigit(*++timearg))
			badtime();
		if ((offset = atoi(timearg) * 60) < 0)
			badtime();
		shuttime = now + offset;
		return;
	}

	/* handle hh:mm by getting rid of the colon */
	for (p = timearg; *p; ++p)
		if (!isascii(*p) || !isdigit(*p)) {
			if (*p == ':' && strlen(p) == 3) {
				p[0] = p[1];
				p[1] = p[2];
				p[2] = '\0';
			}
			else
				badtime();
		}

	unsetenv("TZ");					/* OUR timezone */
	lt = localtime(&now);				/* current time val */

	switch(strlen(timearg)) {
	case 10:
		this_year = lt->tm_year;
		lt->tm_year = ATOI2(timearg);
		/*
		 * check if the specified year is in the next century.
		 * allow for one year of user error as many people will
		 * enter n - 1 at the start of year n.
		 */
		if (lt->tm_year < (this_year % 100) - 1)
			lt->tm_year += 100;
		/* adjust for the year 2000 and beyond */
		lt->tm_year += (this_year - (this_year % 100));
		/* FALLTHROUGH */
	case 8:
		lt->tm_mon = ATOI2(timearg);
		if (--lt->tm_mon < 0 || lt->tm_mon > 11)
			badtime();
		/* FALLTHROUGH */
	case 6:
		lt->tm_mday = ATOI2(timearg);
		if (lt->tm_mday < 1 || lt->tm_mday > 31)
			badtime();
		/* FALLTHROUGH */
	case 4:
		lt->tm_hour = ATOI2(timearg);
		if (lt->tm_hour < 0 || lt->tm_hour > 23)
			badtime();
		lt->tm_min = ATOI2(timearg);
		if (lt->tm_min < 0 || lt->tm_min > 59)
			badtime();
		lt->tm_sec = 0;
		if ((shuttime = mktime(lt)) == -1)
			badtime();
		if ((offset = shuttime - now) < 0)
			errx(1, "that time is already past.");
		break;
	default:
		badtime();
	}
}

#define	NOMSG	"\n\nNO LOGINS: System going down at "
void
nolog()
{
	int logfd;
	char *ct;

	(void)unlink(_PATH_NOLOGIN);	/* in case linked to another file */
	(void)signal(SIGINT, finish);
	(void)signal(SIGHUP, finish);
	(void)signal(SIGQUIT, finish);
	(void)signal(SIGTERM, finish);
	if ((logfd = open(_PATH_NOLOGIN, O_WRONLY|O_CREAT|O_TRUNC,
	    0664)) >= 0) {
		(void)write(logfd, NOMSG, sizeof(NOMSG) - 1);
		ct = ctime(&shuttime);
		(void)write(logfd, ct + 11, 5);
		(void)write(logfd, "\n\n", 2);
		(void)write(logfd, mbuf, strlen(mbuf));
		(void)close(logfd);
	}
}

void
finish(int signo __unused)
{
	if (!killflg)
		(void)unlink(_PATH_NOLOGIN);
	exit(0);
}

void
badtime()
{
	errx(1, "bad time format");
}

void
usage(const char *cp)
{
	if (cp != NULL)
		warnx("%s", cp);
	(void)fprintf(stderr,
#ifdef __APPLE__
	    "usage: shutdown [-] [-h [-u] [-n] | -r [-n] | -s | -k]"
#else
	    "usage: shutdown [-] [-h | -p | -r | -k] [-o [-n]]"
#endif
	    " time [warning-message ...]\n");
	exit(1);
}

#ifdef __APPLE__
/*
 * The following tokens are included in the audit record for shutdown
 * header
 * subject
 * return
 */  
int audit_shutdown(int exitstatus)
{
	int aufd;
	token_t *tok;
	long au_cond;

	/* If we are not auditing, don't cut an audit record; just return */
	if (auditon(A_GETCOND, &au_cond, sizeof(long)) < 0) {
		fprintf(stderr, "shutdown: Could not determine audit condition\n");
		return 0;
	}
	if (au_cond == AUC_NOAUDIT)
		return 0;

	if((aufd = au_open()) == -1) {
		fprintf(stderr, "shutdown: Audit Error: au_open() failed\n");
		exit(1);      
	}

	/* The subject that performed the operation */
	if((tok = au_to_me()) == NULL) {
		fprintf(stderr, "shutdown: Audit Error: au_to_me() failed\n");
		exit(1);
	}
	au_write(aufd, tok);

	/* success and failure status */
	if((tok = au_to_return32(exitstatus, errno)) == NULL) {
		fprintf(stderr, "shutdown: Audit Error: au_to_return32() failed\n");
		exit(1);
	}
	au_write(aufd, tok);

	if(au_close(aufd, 1, AUE_shutdown) == -1) {
		fprintf(stderr, "shutdown: Audit Error: au_close() failed\n");
		exit(1);
	}
	return 1;
}


// XX copied from reboot.tproj/reboot.c; it would be nice to share the code

#define WAITFORLOCK 1
/*
 * contact kextd to lock for reboot
 */
int
reserve_reboot()
{
    int rval = ELAST + 1;
    kern_return_t macherr = KERN_FAILURE;
    mach_port_t kxport, tport = MACH_PORT_NULL, myport = MACH_PORT_NULL;
    int busyStatus = ELAST + 1;
    mountpoint_t busyVol;

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
        if (macherr)    goto finish;
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
#endif /* __APPLE__ */

