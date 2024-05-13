/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1980, 1991, 1993, 1994
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

#ifndef __APPLE__
__FBSDID("$FreeBSD$");

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1980, 1991, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif

#ifndef lint
static const char sccsid[] = "@(#)w.c	8.4 (Berkeley) 4/16/94";
#endif
#endif

/*
 * w - print system status (who and what)
 *
 * This program is similar to the systat command on Tenex/Tops 10/20
 *
 */
#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/ioctl.h>
#ifndef __APPLE__
#include <sys/sbuf.h>
#endif
#include <sys/socket.h>
#include <sys/tty.h>
#include <sys/types.h>

#ifndef __APPLE__
#include <machine/cpu.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#if HAVE_KVM
#include <kvm.h>
#endif
#include <langinfo.h>
#include <libgen.h>
#include <libutil.h>
#include <limits.h>
#include <locale.h>
#include <netdb.h>
#include <nlist.h>
#include <paths.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <timeconv.h>
#include <unistd.h>
#ifdef __APPLE__
#include <usbuf.h>
#endif
#include <utmpx.h>
#include <vis.h>
#include <libxo/xo.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#include "extern.h"

#ifdef __APPLE__
#ifndef nitems
#define	nitems(x)	(sizeof((x)) / sizeof((x)[0]))
#endif
#endif

static struct utmpx *utmp;
static struct winsize ws;
#if HAVE_KVM
static kvm_t   *kd;
#endif
static time_t	now;		/* the current time of day */
static size_t	ttywidth;	/* width of tty */
static size_t	fromwidth = 0;	/* max width of "from" field */
static size_t	argwidth;	/* width of arguments */
static int	header = 1;	/* true if -h flag: don't print heading */
static int	nflag;		/* true if -n flag: don't convert addrs */
#ifndef __APPLE__
static int	dflag;		/* true if -d flag: output debug info */
#endif
static int	sortidle;	/* sort by idle time */
int		use_ampm;	/* use AM/PM time */
static int	use_comma;      /* use comma as floats separator */
static char   **sel_users;	/* login array of particular users selected */

/*
 * One of these per active utmp entry.
 */
static struct entry {
	struct	entry *next;
	struct	utmpx utmp;
	dev_t	tdev;			/* dev_t of terminal */
	time_t	idle;			/* idle time of terminal in seconds */
	struct	kinfo_proc *kp;		/* `most interesting' proc */
	char	*args;			/* arg list of interesting process */
	struct	kinfo_proc *dkp;	/* debug option proc list */
	char	*from;			/* "from": name or addr */
	char	*save_from;		/* original "from": name or addr */
} *ep, *ehead = NULL, **nextp = &ehead;

#ifndef __APPLE__
#define	debugproc(p) *(&((struct kinfo_proc *)p)->ki_udata)
#else
#define	debugproc(p) *((struct kinfo_proc **)&(p)->ki_spare[0])
#endif

#define	W_DISPUSERSIZE	10
#define	W_DISPLINESIZE	8
#define	W_MAXHOSTSIZE	40

static void		 pr_header(time_t *, int);
static struct stat	*ttystat(char *);
static void		 usage(int);
#if !HAVE_KVM
static char		*w_getargv(void);
static struct kinfo_proc *w_getprocs(int *);
#endif

char *fmt_argv(char **, char *, char *, size_t);	/* ../../bin/ps/fmt.c */

int
main(int argc, char *argv[])
{
#if !HAVE_KVM
	struct kinfo_proc *kprocbuf;
#endif
	struct kinfo_proc *kp;
	struct kinfo_proc *dkp;
	struct stat *stp;
	time_t touched;
	size_t width;
	int ch, i, nentries, nusers, wcmd, longidle, longattime;
#if HAVE_KVM
	const char *memf, *nlistf, *p, *save_p;
#else
	const char *p, *save_p;
#endif /* HAVE_KVM */
	char *x_suffix;
#if HAVE_KVM
	char errbuf[_POSIX2_LINE_MAX];
#endif
	char buf[MAXHOSTNAMELEN], fn[MAXHOSTNAMELEN];
	char *dot;

	(void)setlocale(LC_ALL, "");
#ifndef __APPLE__
	use_ampm = (*nl_langinfo(T_FMT_AMPM) != '\0');
	use_comma = (*nl_langinfo(RADIXCHAR) != ',');
#endif

	argc = xo_parse_args(argc, argv);
	if (argc < 0)
		exit(1);

	/* Are we w(1) or uptime(1)? */
	if (strcmp(basename(argv[0]), "uptime") == 0) {
		wcmd = 0;
		p = "";
	} else {
		wcmd = 1;
		p = "dhiflM:N:nsuw";
	}

#if HAVE_KVM
	memf = _PATH_DEVNULL;
	nlistf = NULL;
#endif
	while ((ch = getopt(argc, argv, p)) != -1)
		switch (ch) {
#ifndef __APPLE__
		case 'd':
			dflag = 1;
			break;
#endif
		case 'h':
			header = 0;
			break;
		case 'i':
			sortidle = 1;
			break;
#if HAVE_KVM
		case 'M':
			header = 0;
			memf = optarg;
			break;
		case 'N':
			nlistf = optarg;
			break;
#endif /* HAVE_KVM */
		case 'n':
			nflag += 1;
			break;
#ifdef __APPLE__
		case 'd':
#endif
		case 'f': case 'l': case 's': case 'u': case 'w':
#if !HAVE_KVM
		case 'M': case 'N':
#endif
			xo_warnx("-%c no longer supported", ch);
			/* FALLTHROUGH */
		case '?':
		default:
			usage(wcmd);
		}
	argc -= optind;
	argv += optind;

#if !(TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
	if (!(_res.options & RES_INIT))
		res_init();
	_res.retrans = 2;	/* resolver timeout to 2 seconds per try */
	_res.retry = 1;		/* only try once.. */
#endif

#if HAVE_KVM
	if ((kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, errbuf)) == NULL)
		xo_errx(1, "%s", errbuf);
#endif

	(void)time(&now);

	if (*argv)
		sel_users = argv;

	setutxent();
	for (nusers = 0; (utmp = getutxent()) != NULL;) {
		struct addrinfo hints, *res;
		struct sockaddr_storage ss;
		struct sockaddr *sa = (struct sockaddr *)&ss;
		struct sockaddr_in *lsin = (struct sockaddr_in *)&ss;
		struct sockaddr_in6 *lsin6 = (struct sockaddr_in6 *)&ss;
		int isaddr;

		if (utmp->ut_type != USER_PROCESS)
			continue;
		if (!(stp = ttystat(utmp->ut_line)))
			continue;	/* corrupted record */
		++nusers;
		if (wcmd == 0)
			continue;
		if (sel_users) {
			int usermatch;
			char **user;

			usermatch = 0;
			for (user = sel_users; !usermatch && *user; user++)
				if (!strcmp(utmp->ut_user, *user))
					usermatch = 1;
			if (!usermatch)
				continue;
		}
		if ((ep = calloc(1, sizeof(struct entry))) == NULL)
			xo_errx(1, "calloc");
		*nextp = ep;
		nextp = &ep->next;
		memmove(&ep->utmp, utmp, sizeof *utmp);
		ep->tdev = stp->st_rdev;
#ifndef __APPLE__
		/*
		 * If this is the console device, attempt to ascertain
		 * the true console device dev_t.
		 */
		if (ep->tdev == 0) {
			size_t size;

			size = sizeof(dev_t);
			(void)sysctlbyname("machdep.consdev", &ep->tdev, &size, NULL, 0);
		}
#endif
		touched = stp->st_atime;
		if (touched < ep->utmp.ut_tv.tv_sec) {
			/* tty untouched since before login */
			touched = ep->utmp.ut_tv.tv_sec;
		}
		if ((ep->idle = now - touched) < 0)
			ep->idle = 0;

		save_p = p = *ep->utmp.ut_host ? ep->utmp.ut_host : "-";
		if ((x_suffix = strrchr(p, ':')) != NULL) {
			if ((dot = strchr(x_suffix, '.')) != NULL &&
			    strchr(dot+1, '.') == NULL)
				*x_suffix++ = '\0';
			else
				x_suffix = NULL;
		}

		isaddr = 0;
		memset(&ss, '\0', sizeof(ss));
		if (inet_pton(AF_INET6, p, &lsin6->sin6_addr) == 1) {
			lsin6->sin6_len = sizeof(*lsin6);
			lsin6->sin6_family = AF_INET6;
			isaddr = 1;
		} else if (inet_pton(AF_INET, p, &lsin->sin_addr) == 1) {
			lsin->sin_len = sizeof(*lsin);
			lsin->sin_family = AF_INET;
			isaddr = 1;
		}
		if (nflag == 0) {
			/* Attempt to change an IP address into a name */
			if (isaddr && realhostname_sa(fn, sizeof(fn), sa,
			    sa->sa_len) == HOSTNAME_FOUND)
				p = fn;
		} else if (!isaddr && nflag > 1) {
			/*
			 * If a host has only one A/AAAA RR, change a
			 * name into an IP address
			 */
			memset(&hints, 0, sizeof(hints));
			hints.ai_flags = AI_PASSIVE;
			hints.ai_family = AF_UNSPEC;
			hints.ai_socktype = SOCK_STREAM;
			if (getaddrinfo(p, NULL, &hints, &res) == 0) {
				if (res->ai_next == NULL &&
				    getnameinfo(res->ai_addr, res->ai_addrlen,
					fn, sizeof(fn), NULL, 0,
					NI_NUMERICHOST) == 0)
					p = fn;
				freeaddrinfo(res);
			}
		}

		if (x_suffix) {
			(void)snprintf(buf, sizeof(buf), "%s:%s", p, x_suffix);
			p = buf;
		}
		ep->from = strdup(p);
		if ((width = strlen(p)) > fromwidth)
			fromwidth = width;
		if (save_p != p)
			ep->save_from = strdup(save_p);
	}
	endutxent();

#define HEADER_USER		"USER"
#define HEADER_TTY		"TTY"
#define HEADER_FROM		"FROM"
#define HEADER_LOGIN_IDLE	"LOGIN@  IDLE "
#define HEADER_WHAT		"WHAT\n"
#define WUSED  (W_DISPUSERSIZE + W_DISPLINESIZE + fromwidth + \
		sizeof(HEADER_LOGIN_IDLE) + 3)	/* header width incl. spaces */

	if (sizeof(HEADER_FROM) > fromwidth)
		fromwidth = sizeof(HEADER_FROM);
	fromwidth++;
	if (fromwidth > W_MAXHOSTSIZE)
		fromwidth = W_MAXHOSTSIZE;

	xo_open_container("uptime-information");

	if (header || wcmd == 0) {
		pr_header(&now, nusers);
		if (wcmd == 0) {
			xo_close_container("uptime-information");
			if (xo_finish() < 0)
				xo_err(1, "stdout");
#if HAVE_KVM
			(void)kvm_close(kd);
#endif
			exit(0);
		}

		xo_emit("{T:/%-*.*s} {T:/%-*.*s} {T:/%-*.*s}  {T:/%s}",
				W_DISPUSERSIZE, W_DISPUSERSIZE, HEADER_USER,
				W_DISPLINESIZE, W_DISPLINESIZE, HEADER_TTY,
				fromwidth, fromwidth, HEADER_FROM,
				HEADER_LOGIN_IDLE HEADER_WHAT);
	}

#if HAVE_KVM
	if ((kp = kvm_getprocs(kd, KERN_PROC_ALL, 0, &nentries)) == NULL)
		xo_err(1, "%s", kvm_geterr(kd));
#else
	if ((kp = kprocbuf = w_getprocs(&nentries)) == NULL)
		xo_err(1, "failed to get processes");
#endif /* !HAVE_KVM */

#if !HAVE_KVM
#define ki_stat		kp_proc.p_stat
#define ki_pgid		kp_eproc.e_pgid
#define ki_tpgid	kp_eproc.e_tpgid
#define ki_tdev		kp_eproc.e_tdev
#endif /* !HAVE_KVM */
	for (i = 0; i < nentries; i++, kp++) {
		if (kp->ki_stat == SIDL || kp->ki_stat == SZOMB ||
		    kp->ki_tdev == NODEV)
			continue;
		for (ep = ehead; ep != NULL; ep = ep->next) {
			if (ep->tdev == kp->ki_tdev) {
				/*
				 * proc is associated with this terminal
				 */
				if (ep->kp == NULL && kp->ki_pgid == kp->ki_tpgid) {
					/*
					 * Proc is 'most interesting'
					 */
					if (proc_compare(ep->kp, kp))
						ep->kp = kp;
				}
				/*
				 * Proc debug option info; add to debug
				 * list using kinfo_proc ki_spare[0]
				 * as next pointer; ptr to ptr avoids the
				 * ptr = long assumption.
				 */
				dkp = ep->dkp;
				ep->dkp = kp;
#ifndef __APPLE__
				debugproc(kp) = dkp;
#endif
			}
		}
	}
	if ((ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 &&
	     ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == -1 &&
	     ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1) || ws.ws_col == 0)
	       ttywidth = 79;
        else
	       ttywidth = ws.ws_col - 1;
	argwidth = ttywidth - WUSED;
	if (argwidth < 4)
		argwidth = 8;
	/* Don't truncate if we're outputting json or XML. */
	if (xo_get_style(NULL) != XO_STYLE_TEXT)
		argwidth = ARG_MAX;
	for (ep = ehead; ep != NULL; ep = ep->next) {
		if (ep->kp == NULL) {
			ep->args = strdup("-");
			continue;
		}
#if HAVE_KVM
		ep->args = fmt_argv(kvm_getargv(kd, ep->kp, argwidth),
		    ep->kp->ki_comm, NULL, MAXCOMLEN);
#else
		ep->args = w_getargv();
#endif /* HAVE_KVM */
		if (ep->args == NULL)
			xo_err(1, "fmt_argv");
	}
	/* sort by idle time */
	if (sortidle && ehead != NULL) {
		struct entry *from, *save;

		from = ehead;
		ehead = NULL;
		while (from != NULL) {
			for (nextp = &ehead;
			    (*nextp) && from->idle >= (*nextp)->idle;
			    nextp = &(*nextp)->next)
				continue;
			save = from;
			from = from->next;
			save->next = *nextp;
			*nextp = save;
		}
	}

	xo_open_container("user-table");
	xo_open_list("user-entry");

	for (ep = ehead; ep != NULL; ep = ep->next) {
		time_t t;

		xo_open_instance("user-entry");

#ifndef __APPLE__
		if (dflag) {
			xo_open_container("process-table");
			xo_open_list("process-entry");

			for (dkp = ep->dkp; dkp != NULL; dkp = debugproc(dkp)) {
				const char *ptr;

				ptr = fmt_argv(kvm_getargv(kd, dkp, argwidth),
				    dkp->ki_comm, NULL, MAXCOMLEN);
				if (ptr == NULL)
					ptr = "-";
				xo_open_instance("process-entry");
				xo_emit("\t\t{:process-id/%-9d/%d} "
				    "{:command/%hs}\n", dkp->ki_pid, ptr);
				xo_close_instance("process-entry");
			}
			xo_close_list("process-entry");
			xo_close_container("process-table");
		}
#endif /* !__APPLE__ */
		xo_emit("{:user/%-*.*s/%@**@s} {:tty/%-*.*s/%@**@s} ",
			W_DISPUSERSIZE, W_DISPUSERSIZE, ep->utmp.ut_user,
			W_DISPLINESIZE, W_DISPLINESIZE,
			*ep->utmp.ut_line ?
			(strncmp(ep->utmp.ut_line, "tty", 3) &&
			 strncmp(ep->utmp.ut_line, "cua", 3) ?
			 ep->utmp.ut_line : ep->utmp.ut_line + 3) : "-");

		if (ep->save_from)
		    xo_attr("address", "%s", ep->save_from);
		xo_emit("{:from/%-*.*s/%@**@s} ",
		    (int)fromwidth, (int)fromwidth, ep->from);
		t = ep->utmp.ut_tv.tv_sec;
		longattime = pr_attime(&t, &now);
		longidle = pr_idle(ep->idle);
		xo_emit("{:command/%.*hs/%@*@hs}\n",
		    (int)argwidth - longidle - longattime,
		    ep->args);
#if !HAVE_KVM
		free(ep->args);
#endif

		xo_close_instance("user-entry");
	}

	xo_close_list("user-entry");
	xo_close_container("user-table");
	xo_close_container("uptime-information");
	if (xo_finish() < 0)
		xo_err(1, "stdout");

#if HAVE_KVM
	(void)kvm_close(kd);
#else
	free(kprocbuf);
#endif /* HAVE_KVM */
	exit(0);
}

static void
pr_header(time_t *nowp, int nusers)
{
	char buf[64];
	struct sbuf upbuf;
	double avenrun[3];
#ifdef __APPLE__
	struct timeval boottime, realtime;
	size_t size = sizeof(boottime);
	int mib[2] = { CTL_KERN, KERN_BOOTTIME };
#endif
	struct timespec tp;
	unsigned long days, hrs, mins, secs;
	unsigned int i;

	/*
	 * Print time of day.
	 */
	if (strftime(buf, sizeof(buf),
	    use_ampm ? "%l:%M%p" : "%k:%M", localtime(nowp)) != 0)
		xo_emit("{:time-of-day/%s} ", buf);
	/*
	 * Print how long system has been up.
	 */
	(void)sbuf_new(&upbuf, buf, sizeof(buf), SBUF_FIXEDLEN);
#ifdef __APPLE__
	/*
	 * FreeBSD uses CLOCK_UPTIME to report uptime.  However,
	 *
	 * 1) Darwin does not have CLOCK_UPTIME.
	 *
	 * 2) It does have CLOCK_UPTIME_RAW, but we actually want the
	 * uptime to include time spent suspended.
	 *
	 * 3) CLOCK_MONOTONIC actually counts up from power-on, which is
	 * not the same thing as boot, and may change in the future.
	 *
	 * Instead, we get the boot wall time from the kern.boottime
	 * sysctl and subtract it from the current wall time.
	 */
	if (sysctl(mib, nitems(mib), &boottime, &size, NULL, 0) == 0 &&
	    size == sizeof(boottime) &&
	    gettimeofday(&realtime, NULL) == 0 &&
	    realtime.tv_sec > boottime.tv_sec) {
		tp.tv_sec = realtime.tv_sec - boottime.tv_sec;
		tp.tv_nsec = realtime.tv_usec - boottime.tv_usec;
		if (tp.tv_nsec < 0) {
			tp.tv_sec -= 1;
			tp.tv_nsec += 1000000;
		}
		tp.tv_nsec *= 1000;
#else
	if (clock_gettime(CLOCK_UPTIME, &tp) != -1) {
#endif
		xo_emit(" up");
		secs = tp.tv_sec;
		xo_emit("{e:uptime/%lu}", secs);
		mins = secs / 60;
		secs %= 60;
		hrs = mins / 60;
		mins %= 60;
		days = hrs / 24;
		hrs %= 24;
		xo_emit("{e:days/%ld}{e:hours/%ld}{e:minutes/%ld}{e:seconds/%ld}",
		    days, hrs, mins, secs);

		/* If we've been up longer than 60 s, round to nearest min */
		if (tp.tv_sec > 60) {
			secs = tp.tv_sec + 30;
			mins = secs / 60;
			secs = 0;
			hrs = mins / 60;
			mins %= 60;
			days = hrs / 24;
			hrs %= 24;
		}

		if (days > 0)
			sbuf_printf(&upbuf, " %ld day%s,",
				days, days > 1 ? "s" : "");
		if (hrs > 0 && mins > 0)
			sbuf_printf(&upbuf, " %2ld:%02ld,", hrs, mins);
		else if (hrs > 0)
			sbuf_printf(&upbuf, " %ld hr%s,",
				hrs, hrs > 1 ? "s" : "");
		else if (mins > 0)
			sbuf_printf(&upbuf, " %ld min%s,",
				mins, mins > 1 ? "s" : "");
		else
			sbuf_printf(&upbuf, " %ld sec%s,",
				secs, secs > 1 ? "s" : "");
		if (sbuf_finish(&upbuf) != 0)
			xo_err(1, "Could not generate output");
		xo_emit("{:uptime-human/%s}", sbuf_data(&upbuf));
		sbuf_delete(&upbuf);
	}

	/* Print number of users logged in to system */
	xo_emit(" {:users/%d} {Np:user,users}", nusers);

	/*
	 * Print 1, 5, and 15 minute load averages.
	 */
	if (getloadavg(avenrun, nitems(avenrun)) == -1)
		xo_emit(", no load average information available\n");
	else {
		static const char *format[] = {
		    " {:load-average-1/%.2f}",
		    " {:load-average-5/%.2f}",
		    " {:load-average-15/%.2f}",
		};
		xo_emit(", load averages:");
		for (i = 0; i < nitems(avenrun); i++) {
			if (use_comma && i > 0)
				xo_emit(",");
			xo_emit(format[i], avenrun[i]);
		}
		xo_emit("\n");
	}
}

static struct stat *
ttystat(char *line)
{
	static struct stat sb;
	char ttybuf[MAXPATHLEN];

	(void)snprintf(ttybuf, sizeof(ttybuf), "%s%s", _PATH_DEV, line);
	if (stat(ttybuf, &sb) == 0 && S_ISCHR(sb.st_mode))
		return (&sb);
	return (NULL);
}

static void
usage(int wcmd)
{
	if (wcmd)
#if HAVE_KVM
		xo_error("usage: w [-dhin] [-M core] [-N system] [user ...]\n");
#else
		xo_error("usage: w [-dhin] [user ...]\n");
#endif
	else
		xo_error("usage: uptime\n");
	xo_finish();
	exit(1);
}

#if !HAVE_KVM
static char *
w_getargv(void)
{
	int mib[3], argc, c;
	size_t size;
	char *args, *procargs, *sp, *np, *cp;

	procargs = malloc(size = 4096);
	if (procargs == NULL) {
		goto ERROR;
	}

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROCARGS2;
	mib[2] = KI_PROC(ep)->p_pid;

	while (sysctl(mib, 3, procargs, &size, NULL, 0) == -1) {
		if (errno != ENOMEM) {
			goto ERROR_FREE;
		}
		procargs = reallocf(procargs, size *= 2);
		if (procargs == NULL) {
			goto ERROR_FREE;
		}
	}

	/* get argc */
	if (size < sizeof(argc)) {
		goto ERROR_FREE;
	}
	memcpy(&argc, procargs, sizeof(argc));

	/* skip binary path and padding */
	cp = procargs + sizeof(argc);
	while (cp < procargs + size && *cp != '\0') {
		cp++;
	}
	while (cp < procargs + size && *cp == '\0') {
		cp++;
	}
	if (cp == procargs + size) {
		goto ERROR_FREE;
	}

	/* iterate over arguments, replacing intervening NULs with blanks */
	sp = cp;
	for (c = 0, np = NULL; c < argc && cp < procargs + size; cp++) {
		if (*cp == '\0') {
			if (np != NULL) {
				*np = ' ';
			}
			np = cp;
			c++;
		}
	}

	/* trim leading blanks */
	for (np = sp; np < procargs + size && *np == ' '; np++)
		/* nothing */ ;

	args = strdup(np);
	free(procargs);
	return (args);

ERROR_FREE:
	free(procargs);
ERROR:
	args = strdup(KI_PROC(ep)->p_comm);
	return (args);
}

static struct kinfo_proc *
w_getprocs(int *nentriesp)
{
	struct kinfo_proc *kp;
	size_t bufSize = 0, orig_bufSize = 0;
	int local_error = 0, retry_count = 0;
	int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };

	if (sysctl(mib, 4, NULL, &bufSize, NULL, 0) < 0) {
		return (NULL);
	}

	kp = malloc(bufSize);
	if (kp == NULL) {
		return (NULL);
	}

	retry_count = 0;
	orig_bufSize = bufSize;
	for (retry_count = 0; ; retry_count++) {
		local_error = 0;
		bufSize = orig_bufSize;
		if ((local_error = sysctl(mib, 4, kp, &bufSize, NULL, 0)) < 0) {
			if (retry_count < 1000) {
				sleep(1);
				continue;
			}
			return NULL;
		} else if (local_error == 0) {
			break;
                }
		sleep(1);
        }
	*nentriesp = (int)(bufSize / sizeof(struct kinfo_proc));
	return (kp);
}
#endif /* HAVE_KVM */
