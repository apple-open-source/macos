/*-
 * Copyright (c) 1980, 1991, 1993, 1994
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

#include <sys/cdefs.h>

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1980, 1991, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif

#ifndef lint
static const char sccsid[] = "@(#)w.c	8.4 (Berkeley) 4/16/94";
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
#include <sys/socket.h>
#include <sys/tty.h>

#include <machine/cpu.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <locale.h>
#include <netdb.h>
#include <nlist.h>
#include <paths.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>
#include <vis.h>

#include "extern.h"

struct timeval	boottime;
struct utmp	utmp;
struct winsize	ws;
kvm_t	       *kd;
time_t		now;		/* the current time of day */
int		ttywidth;	/* width of tty */
int		argwidth;	/* width of tty */
int		header = 1;	/* true if -h flag: don't print heading */
int		nflag;		/* true if -n flag: don't convert addrs */
int		dflag;		/* true if -d flag: output debug info */
int		sortidle;	/* sort by idle time */
int		use_ampm;	/* use AM/PM time */
int             use_comma;      /* use comma as floats separator */
char	      **sel_users;	/* login array of particular users selected */

/*
 * One of these per active utmp entry.
 */
struct	entry {
	struct	entry *next;
	struct	utmp utmp;
	dev_t	tdev;			/* dev_t of terminal */
	time_t	idle;			/* idle time of terminal in seconds */
	struct	kinfo_proc *kp;		/* `most interesting' proc */
	char	*args;			/* arg list of interesting process */
	struct	kinfo_proc *dkp;	/* debug option proc list */
} *ep, *ehead = NULL, **nextp = &ehead;

#define debugproc(p) *((struct kinfo_proc **)&(p)->ki_spare[0])

/* W_DISPHOSTSIZE should not be greater than UT_HOSTSIZE */
#define	W_DISPHOSTSIZE	16

static void		 pr_header(time_t *, int);
static struct stat	*ttystat(char *, int);
static void		 usage(int);
static int		 this_is_uptime(const char *s);
static void		 w_getargv(void);

char *fmt_argv(char **, char *, int);	/* ../../bin/ps/fmt.c */

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct kinfo_proc *kp, *kprocbuf;
	struct kinfo_proc *dkp;
	struct stat *stp;
	FILE *ut;
	time_t touched;
	int ch, i, nentries, nusers, wcmd, longidle, dropgid;
	const char *memf, *nlistf, *p;
	char *x_suffix;
	char buf[MAXHOSTNAMELEN], errbuf[_POSIX2_LINE_MAX];
	char fn[MAXHOSTNAMELEN];
	char *dot;
	int local_error = 0, retry_count = 0;
	size_t bufSize = 0;
	size_t orig_bufSize = 0;
	int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };

	(void)setlocale(LC_ALL, "");
	/*
	use_ampm = (*nl_langinfo(T_FMT_AMPM) != '\0');
	use_comma = (*nl_langinfo(RADIXCHAR) != ',');
	*/

	/* Are we w(1) or uptime(1)? */
	if (this_is_uptime(argv[0]) == 0) {
		wcmd = 0;
		p = "";
	} else {
		wcmd = 1;
		p = "dhiflM:N:nsuw";
	}

	dropgid = 0;
	memf = nlistf = NULL;
	while ((ch = getopt(argc, argv, p)) != -1)
		switch (ch) {
		case 'd':
			dflag = 1;
			break;
		case 'h':
			header = 0;
			break;
		case 'i':
			sortidle = 1;
			break;
		case 'M':
			header = 0;
			memf = optarg;
			dropgid = 1;
			break;
		case 'N':
			nlistf = optarg;
			dropgid = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'f': case 'l': case 's': case 'u': case 'w':
			warnx("[-flsuw] no longer supported");
			/* FALLTHROUGH */
		case '?':
		default:
			usage(wcmd);
		}
	argc -= optind;
	argv += optind;

	if (!(_res.options & RES_INIT))
		res_init();
	_res.retrans = 2;	/* resolver timeout to 2 seconds per try */
	_res.retry = 1;		/* only try once.. */

	/*
	 * Discard setgid privileges if not the running kernel so that bad
	 * guys can't print interesting stuff from kernel memory.
	 */
	if (dropgid)
		setgid(getgid());

#ifdef OLD_PROC
	if ((kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY, errbuf)) == NULL)
		errx(1, "%s", errbuf);
#endif

	(void)time(&now);
	if ((ut = fopen(_PATH_UTMP, "r")) == NULL)
		err(1, "%s", _PATH_UTMP);

	if (*argv)
		sel_users = argv;

	for (nusers = 0; fread(&utmp, sizeof(utmp), 1, ut);) {
		if (utmp.ut_name[0] == '\0')
			continue;
		if (!(stp = ttystat(utmp.ut_line, UT_LINESIZE)))
			continue;	/* corrupted record */
		++nusers;
		if (wcmd == 0)
			continue;
		if (sel_users) {
			int usermatch;
			char **user;

			usermatch = 0;
			for (user = sel_users; !usermatch && *user; user++)
				if (!strncmp(utmp.ut_name, *user, UT_NAMESIZE))
					usermatch = 1;
			if (!usermatch)
				continue;
		}
		if ((ep = calloc(1, sizeof(struct entry))) == NULL)
			errx(1, "calloc");
		*nextp = ep;
		nextp = &ep->next;
		memmove(&ep->utmp, &utmp, sizeof(struct utmp));
		ep->tdev = stp->st_rdev;
#ifdef CPU_CONSDEV
		/*
		 * If this is the console device, attempt to ascertain
		 * the true console device dev_t.
		 */
		if (ep->tdev == 0) {
			int mib[2];
			size_t size;

			mib[0] = CTL_MACHDEP;
			mib[1] = CPU_CONSDEV;
			size = sizeof(dev_t);
			(void)sysctl(mib, 2, &ep->tdev, &size, NULL, 0);
		}
#endif
		touched = stp->st_atime;
		if (touched < ep->utmp.ut_time) {
			/* tty untouched since before login */
			touched = ep->utmp.ut_time;
		}
		if ((ep->idle = now - touched) < 0)
			ep->idle = 0;
	}
	(void)fclose(ut);

	if (header || wcmd == 0) {
		pr_header(&now, nusers);
		if (wcmd == 0) {
#ifdef OLD_PROC
			(void)kvm_close(kd);
#endif
			exit(0);
		}

#define HEADER_USER		"USER"
#define HEADER_TTY		"TTY"
#define HEADER_FROM		"FROM"
#define HEADER_LOGIN_IDLE	"LOGIN@  IDLE "
#define HEADER_WHAT		"WHAT\n"
#define WUSED  (UT_NAMESIZE + UT_LINESIZE + W_DISPHOSTSIZE + \
		sizeof(HEADER_LOGIN_IDLE) + 3)	/* header width incl. spaces */ 
		(void)printf("%-*.*s %-*.*s %-*.*s  %s", 
				UT_NAMESIZE, UT_NAMESIZE, HEADER_USER,
				UT_LINESIZE, UT_LINESIZE, HEADER_TTY,
				W_DISPHOSTSIZE, W_DISPHOSTSIZE, HEADER_FROM,
				HEADER_LOGIN_IDLE HEADER_WHAT);
	}

#ifdef OLD_PROC
	if ((kp = kvm_getprocs(kd, KERN_PROC_ALL, 0, &nentries)) == NULL)
		err(1, "%s", kvm_geterr(kd));
#else
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_ALL;
	mib[3] = 0;

	if (sysctl(mib, 4, NULL, &bufSize, NULL, 0) < 0) {
		perror("Failure calling sysctl");
		exit(1);
	}

	kprocbuf = kp = (struct kinfo_proc *)malloc(bufSize);

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
			perror("Failure calling sysctl");
			exit(1);
		} else if (local_error == 0) {
			break;
		}
		sleep(1);
	}
	nentries = bufSize / sizeof(struct kinfo_proc);
#endif
	for (i = 0; i < nentries; i++, kp++) {
		if (kp->kp_proc.p_stat == SIDL || kp->kp_proc.p_stat == SZOMB)
			continue;
		for (ep = ehead; ep != NULL; ep = ep->next) {
			if (ep->tdev == kp->kp_eproc.e_tdev) {
				/*
				 * proc is associated with this terminal
				 */
				if (ep->kp == NULL && kp->kp_eproc.e_pgid == kp->kp_eproc.e_tpgid) {
					/*
					 * Proc is 'most interesting'
					 */
					if (proc_compare(&ep->kp->kp_proc, &kp->kp_proc))
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
/* --bbraun
				debugproc(kp) = dkp;
*/
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
	for (ep = ehead; ep != NULL; ep = ep->next) {
		if (ep->kp == NULL) {
			ep->args = strdup("-");
			continue;
		}
#ifdef OLD_PROC
		ep->args = fmt_argv(kvm_getargv(kd, ep->kp, argwidth),
		    ep->kp->kp_proc.p_comm, MAXCOMLEN);
#else
		w_getargv();
#endif
		if (ep->args == NULL)
			err(1, NULL);
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

	for (ep = ehead; ep != NULL; ep = ep->next) {
		char host_buf[UT_HOSTSIZE + 1];
		struct sockaddr_storage ss;
		struct sockaddr *sa = (struct sockaddr *)&ss;
		struct sockaddr_in *lsin = (struct sockaddr_in *)&ss;
#ifdef SUCKAGE
		struct hostent *hp;
#else
		struct sockaddr_in6 *lsin6 = (struct sockaddr_in6 *)&ss;
#endif
		int isaddr;

		host_buf[UT_HOSTSIZE] = '\0';
		strncpy(host_buf, ep->utmp.ut_host, UT_HOSTSIZE);
		p = *host_buf ? host_buf : "-";
		if ((x_suffix = strrchr(p, ':')) != NULL) {
			if ((dot = strchr(x_suffix, '.')) != NULL &&
			    strchr(dot+1, '.') == NULL)
				*x_suffix++ = '\0';
			else
				x_suffix = NULL;
		}
		if (!nflag) {
			/* Attempt to change an IP address into a name */
			isaddr = 0;
			memset(&ss, '\0', sizeof(ss));
#ifdef SUCKAGE
			if (inet_aton(p, &lsin->sin_addr) ) {
				lsin->sin_len = sizeof(*lsin);
				lsin->sin_family = AF_INET;
				isaddr = 1;
			}

			hp = gethostbyaddr((char *)&lsin->sin_addr, sizeof(lsin->sin_addr), AF_INET);
			if( hp ) {
 				p = hp->h_name;
			}
#else
			if (inet_pton(AF_INET6, p, &lsin6->sin6_addr) == 1) {
				lsin6->sin6_len = sizeof(*lsin6);
				lsin6->sin6_family = AF_INET6;
				isaddr = 1;
			} else if (inet_pton(AF_INET, p, &lsin->sin_addr) == 1) {
				lsin->sin_len = sizeof(*lsin);
				lsin->sin_family = AF_INET;
				isaddr = 1;
			}
			if (isaddr && realhostname_sa(fn, sizeof(fn), sa,
			    sa->sa_len) == HOSTNAME_FOUND)
				p = fn;
#endif
		}
		if (x_suffix) {
			(void)snprintf(buf, sizeof(buf), "%s:%s", p, x_suffix);
			p = buf;
		}
#ifndef SUCKAGE
		if (dflag) {
			for (dkp = ep->dkp; dkp != NULL; dkp = debugproc(dkp)) {
				const char *ptr;

				ptr = fmt_argv(kvm_getargv(kd, dkp, argwidth),
				    dkp->ki_comm, MAXCOMLEN);
				if (ptr == NULL)
					ptr = "-";
				(void)printf("\t\t%-9d %s\n",
				    dkp->ki_pid, ptr);
			}
		}
#endif
		(void)printf("%-*.*s %-*.*s %-*.*s ",
		    UT_NAMESIZE, UT_NAMESIZE, ep->utmp.ut_name,
		    UT_LINESIZE, UT_LINESIZE,
		    strncmp(ep->utmp.ut_line, "tty", 3) &&
		    strncmp(ep->utmp.ut_line, "cua", 3) ?
		    ep->utmp.ut_line : ep->utmp.ut_line + 3,
		    W_DISPHOSTSIZE, W_DISPHOSTSIZE, *p ? p : "-");
		pr_attime(&ep->utmp.ut_time, &now);
		longidle = pr_idle(ep->idle);
		(void)printf("%.*s\n", argwidth - longidle, ep->args);
#ifndef OLD_PROC
		free(ep->args);
#endif
	}
#ifdef OLD_PROC
	(void)kvm_close(kd);
#else
	free(kprocbuf);
#endif
	exit(0);
}

static void
pr_header(nowp, nusers)
	time_t *nowp;
	int nusers;
{
	double avenrun[3];
	time_t uptime;
	int days, hrs, i, mins, secs;
	int mib[2];
	size_t size;
	char buf[256];

	/*
	 * Print time of day.
	 */
	(void)strftime(buf, sizeof(buf)	- 1,
		       use_ampm	? "%l:%M%p" : "%k:%M", localtime(nowp));
	buf[sizeof(buf) - 1] = '\0';
	(void)printf("%s ", buf);

	/*
	 * Print how long system has been up.
	 * (Found by looking getting "boottime" from the kernel)
	 */
	mib[0] = CTL_KERN;
	mib[1] = KERN_BOOTTIME;
	size = sizeof(boottime);
	if (sysctl(mib, 2, &boottime, &size, NULL, 0) != -1 &&
	    boottime.tv_sec != 0) {
		uptime = now - boottime.tv_sec;
		if (uptime > 60)
			uptime += 30;
		days = uptime / 86400;
		uptime %= 86400;
		hrs = uptime / 3600;
		uptime %= 3600;
		mins = uptime / 60;
		secs = uptime % 60;
		(void)printf(" up");
		if (days > 0)
			(void)printf(" %d day%s,", days, days > 1 ? "s" : "");
		if (hrs > 0 && mins > 0)
			(void)printf(" %2d:%02d,", hrs, mins);
		else if (hrs > 0)
			(void)printf(" %d hr%s,", hrs, hrs > 1 ? "s" : "");
		else if (mins > 0)
			(void)printf(" %d min%s,", mins, mins > 1 ? "s" : "");
		else
			(void)printf(" %d sec%s,", secs, secs > 1 ? "s" : "");
	}

	/* Print number of users logged in to system */
	(void)printf(" %d user%s", nusers, nusers == 1 ? "" : "s");

	/*
	 * Print 1, 5, and 15 minute load averages.
	 */
	if (getloadavg(avenrun, sizeof(avenrun) / sizeof(avenrun[0])) == -1)
		(void)printf(", no load average information available\n");
	else {
		(void)printf(", load averages:");
		for (i = 0; i < (int)(sizeof(avenrun) / sizeof(avenrun[0])); i++) {
			if (use_comma && i > 0)
				(void)printf(",");
			(void)printf(" %.2f", avenrun[i]);
		}
		(void)printf("\n");
	}
}

static struct stat *
ttystat(line, sz)
	char *line;
	int sz;
{
	static struct stat sb;
	char ttybuf[MAXPATHLEN];

	(void)snprintf(ttybuf, sizeof(ttybuf), "%s%.*s", _PATH_DEV, sz, line);
	if (stat(ttybuf, &sb)) {
		warn("%s", ttybuf);
		return (NULL);
	}
	return (&sb);
}

static void
usage(wcmd)
	int wcmd;
{
	if (wcmd)
		(void)fprintf(stderr,
		    "usage: w [-dhin] [-M core] [-N system] [user ...]\n");
	else
		(void)fprintf(stderr, "usage: uptime\n");
	exit(1);
}

static int 
this_is_uptime(s)
	const char *s;
{
	const char *u;

	if ((u = strrchr(s, '/')) != NULL)
		++u;
	else
		u = s;
	if (strcmp(u, "uptime") == 0)
		return (0);
	return (-1);
}

static void
w_getargv(void)
{
	int mib[3], argmax;
	size_t size;
	char *procargs, *sp, *np, *cp;

	mib[0] = CTL_KERN;
	mib[1] = KERN_ARGMAX;

	size = sizeof(argmax);
	if (sysctl(mib, 2, &argmax, &size, NULL, 0) == -1) {
		goto ERROR;
	}

	procargs = malloc(argmax);
	if (procargs == NULL) {
		goto ERROR;
	}

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROCARGS;
	mib[2] = KI_PROC(ep)->p_pid;

	size = (size_t)argmax;
	if (sysctl(mib, 3, procargs, &size, NULL, 0) == -1) {
		goto ERROR_FREE;
	}

	for (cp = procargs; cp < &procargs[size]; cp++) {
		if (*cp == '\0') {
			break;
		}
	}
	if (cp == &procargs[size]) {
		goto ERROR_FREE;
	}

	sp = cp;

	for (np = NULL; cp < &procargs[size]; cp++) {
		if (*cp == '\0') {
			if (np != NULL) {
				*np = ' ';
			}
			np = cp;
		} else if (*cp == '=') {
			break;
		}
	}

	for (np = sp; (np < &procargs[size]) && (*np == ' '); np++);

	ep->args = strdup(np);
	free(procargs);
	return;

ERROR_FREE:
	free(procargs);
ERROR:
/*
	ep->args = malloc(2);
	ep->args[0] = '-';
	ep->args[1] = '\0';
*/
	asprintf(&ep->args, "%s", KI_PROC(ep)->p_comm);
	return;
}
