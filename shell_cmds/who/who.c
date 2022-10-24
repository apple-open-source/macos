/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2002 Tim J. Robbins.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <langinfo.h>
#include <limits.h>
#include <locale.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <timeconv.h>
#include <unistd.h>
#include <utmpx.h>

static void	heading(void);
static void	process_utmp(void);
static void	quick(void);
static void	row(const struct utmpx *);
static int	ttywidth(void);
static void	usage(void);
static void	whoami(void);

static int	Hflag;			/* Write column headings */
static int	aflag;			/* Print all entries */
static int	bflag;			/* Show date of the last reboot */
#ifdef __APPLE__
static int	lflag;			/* waiting to login */
#endif
static int	mflag;			/* Show info about current terminal */
#ifdef __APPLE__
static int	pflag;			/* Processes active & spawned by init */
#endif
#ifdef __APPLE__
static int	dflag;			/* dead processes */
#endif
static int	qflag;			/* "Quick" mode */
#ifdef __APPLE__
static int	rflag;			/* run-level of the init process */
#endif
static int	sflag;			/* Show name, line, time */
#ifdef __APPLE__
static int	tflag;			/* time of change to system clock */
#endif
static int	Tflag;			/* Show terminal state */
static int	uflag;			/* Show idle time */
#ifdef __APPLE__
#include <get_compat.h>
static int     unix2003_std;
#endif

int
main(int argc, char *argv[])
{
	int ch;

	setlocale(LC_TIME, "");

#ifdef __APPLE__
	unix2003_std = COMPAT_MODE("bin/who", "unix2003");
#endif

#ifndef __APPLE__
	while ((ch = getopt(argc, argv, "HTabmqsu")) != -1) {
#else
	while ((ch = getopt(argc, argv, "abdHlmpqrstTu")) != -1) {
#endif
		switch (ch) {
		case 'H':		/* Write column headings */
			Hflag = 1;
			break;
		case 'T':		/* Show terminal state */
			Tflag = 1;
			break;
		case 'a':		/* Same as -bdlprtTu */
			aflag = bflag = Tflag = uflag = 1;
#ifdef __APPLE__
			dflag = lflag = pflag = rflag = sflag = tflag = 1;
#endif
			break;
		case 'b':		/* Show date of the last reboot */
			bflag = 1;
			break;
#ifdef __APPLE__
		case 'd':		/* dead processes */
			dflag = 1;
			break;
		case 'l':		/* waiting to login */
			lflag = 1;
			break;
#endif
		case 'm':		/* Show info about current terminal */
			mflag = 1;
			break;
#ifdef __APPLE__
		case 'p':		/* Processes active & spawned by init */
			pflag = 1;
			break;
#endif
		case 'q':		/* "Quick" mode */
			qflag = 1;
			break;
#ifdef __APPLE__
		case 'r':		/* run-level of the init process */
			rflag = 1;
			break;
#endif
		case 's':		/* Show name, line, time */
			sflag = 1;
			break;
#ifdef __APPLE__
		case 't':		/* time of change to system clock */
			tflag = 1;
			break;
#endif
		case 'u':		/* Show idle time */
			uflag = 1;
			break;
		default:
			usage();
			/*NOTREACHED*/
		}
	}
	argc -= optind;
	argv += optind;

	if (argc >= 2 && strcmp(argv[0], "am") == 0 &&
	    (strcmp(argv[1], "i") == 0 || strcmp(argv[1], "I") == 0)) {
		/* "who am i" or "who am I", equivalent to -m */
		mflag = 1;
		argc -= 2;
		argv += 2;
	}
	if (argc > 1)
		usage();

	if (*argv != NULL) {
#ifndef __APPLE__
		if (setutxdb(UTXDB_ACTIVE, *argv) != 0)
			err(1, "%s", *argv);
#else
		if (!utmpxname(*argv) || !wtmpxname(*argv))
			usage();
#endif
	}

	if (qflag)
		quick();
	else {
		if (sflag)
			Tflag = uflag = 0;
		if (Hflag)
			heading();
		if (mflag)
			whoami();
		else
			process_utmp();
	}

	endutxent();

#ifdef __APPLE__
	if (ferror(stdout) != 0 || fflush(stdout) != 0)
		err(1, "stdout");
#endif
	exit(0);
}

static void
usage(void)
{

#ifdef __APPLE__
	fprintf(stderr, "usage: who [-abdHlmpqrstTu] [am I] [file]\n");
#else
	fprintf(stderr, "usage: who [-abHmqsTu] [am I] [file]\n");
#endif
	exit(1);
}

static void
heading(void)
{

	printf("%-16s ", "NAME");
	if (Tflag)
		printf("S ");
	printf("%-12s %-12s ", "LINE", "TIME");
	if (uflag)
		printf("IDLE  ");
#ifdef __APPLE__
	if (unix2003_std && uflag && !Tflag)
		printf("     PID ");
#endif
	printf("%-16s\n", "FROM");
}

static void
row(const struct utmpx *ut)
{
#ifndef __APPLE__
	char buf[80], tty[PATH_MAX];
#else
        char buf[80], tty[sizeof(_PATH_DEV) + _UTX_LINESIZE];
#endif
	struct stat sb;
	time_t idle, t;
	static int d_first = -1;
	struct tm *tm;
	char state;
#ifdef __APPLE__
	char login_pidstr[20];
#endif

	if (d_first < 0)
		d_first = (*nl_langinfo(D_MD_ORDER) == 'd');

	state = '?';
	idle = 0;
	if (Tflag || uflag) {
		snprintf(tty, sizeof(tty), "%s%s", _PATH_DEV, ut->ut_line);
		if (stat(tty, &sb) == 0) {
			state = sb.st_mode & (S_IWOTH|S_IWGRP) ?
			    '+' : '-';
			idle = time(NULL) - sb.st_mtime;
		}
#ifdef __APPLE__
		if (unix2003_std && !Tflag) {
			if (ut->ut_pid) {
				snprintf(login_pidstr,sizeof(login_pidstr),
					"%8d",ut->ut_pid);
			} else {
				strcpy(login_pidstr,"       ?");
			}
		}
#endif
	}

#ifdef __APPLE__
    if (ut->ut_type == BOOT_TIME) {
        printf("                 ");
    } else {
        printf("%-16s ", ut->ut_user);
    }
#else
	printf("%-16s ", ut->ut_user);
#endif /* __APPLE__ */
	if (Tflag)
		printf("%c ", state);
	if (ut->ut_type == BOOT_TIME)
		printf("%-12s ", "system boot");
#ifdef __APPLE__
	else if (ut->ut_type == LOGIN_PROCESS)
		printf("%-12s ", "LOGIN");
#endif
	else
		printf("%-12s ", ut->ut_line);
	t = ut->ut_tv.tv_sec;
	tm = localtime(&t);
	strftime(buf, sizeof(buf), d_first ? "%e %b %R" : "%b %e %R", tm);
	printf("%-*s ", 12, buf);
	if (uflag) {
		if (idle < 60)
			printf("  .   ");
		else if (idle < 24 * 60 * 60)
			printf("%02d:%02d ", (int)(idle / 60 / 60),
			    (int)(idle / 60 % 60));
		else
			printf(" old  ");
#ifdef __APPLE__
		if (unix2003_std && !Tflag) {
			printf("%s ", login_pidstr);
		}
#endif
	}
	if (*ut->ut_host != '\0')
		printf("(%s)", ut->ut_host);
#ifdef __APPLE__
	if (dflag && ut->ut_type == DEAD_PROCESS) {
		printf("\tterm=%d exit=%d", 0, 0);
	}
#endif
	putchar('\n');
}

static int
ttystat(char *line)
{
	struct stat sb;
	char ttybuf[MAXPATHLEN];

	(void)snprintf(ttybuf, sizeof(ttybuf), "%s%s", _PATH_DEV, line);
	if (stat(ttybuf, &sb) == 0) {
		return (0);
	} else
		return (-1);
}

static void
process_utmp(void)
{
	struct utmpx *utx;

	while ((utx = getutxent()) != NULL) {
#ifdef __APPLE__
		if (aflag) {
		    row(utx);
		}
		else if (!bflag && utx->ut_type == USER_PROCESS) {
			if (ttystat(utx->ut_line) == 0) {
				if (!rflag && !lflag && !dflag) {
					row(utx);
				}
			}
		}
		else if (bflag && utx->ut_type == BOOT_TIME) {
#ifndef __APPLE__
			if (ttystat(utx->ut_line) == 0) {
#endif /* __APPLE__ */
				row(utx);
#ifndef __APPLE__
			}
#endif /* __APPLE__ */
		}
		else if (dflag && utx->ut_type == DEAD_PROCESS) {
			row(utx);
		}
		else if (lflag && utx->ut_type == LOGIN_PROCESS) {
			row(utx);
		}
#else
		if (((aflag || !bflag) && utx->ut_type == USER_PROCESS) ||
		    (bflag && utx->ut_type == BOOT_TIME))
			if (ttystat(utx->ut_line) == 0)
				row(utx);
#endif
	}
#ifdef __APPLE__
	if (rflag) {
		printf("   .       run-level 3\n");
	}
#endif
}


static void
quick(void)
{
	struct utmpx *utx;
	int col, ncols, num;

	ncols = ttywidth();
	col = num = 0;
	while ((utx = getutxent()) != NULL) {
#ifndef __APPLE__
		if (utx->ut_type != USER_PROCESS)
#else
		if (*utx->ut_user == '\0' || utx->ut_type != USER_PROCESS)
#endif
			continue;
		printf("%-16s", utx->ut_user);
		if (++col < ncols / (16 + 1))
			putchar(' ');
		else {
			col = 0;
			putchar('\n');
		}
		num++;
	}
	if (col != 0)
		putchar('\n');

	printf("# users = %d\n", num);
}

static void
whoami(void)
{
	struct utmpx ut, *utx;
	struct passwd *pwd;
	const char *name, *tty;

	if ((tty = ttyname(STDIN_FILENO)) == NULL)
		tty = "tty??";
	else if (strncmp(tty, _PATH_DEV, sizeof _PATH_DEV - 1) == 0)
		tty += sizeof _PATH_DEV - 1;
	strlcpy(ut.ut_line, tty, sizeof ut.ut_line);

	/* Search utmp for our tty, dump first matching record. */
	if ((utx = getutxline(&ut)) != NULL && utx->ut_type == USER_PROCESS) {
		row(utx);
		return;
	}

	/* Not found; fill the utmp structure with the information we have. */
	memset(&ut, 0, sizeof(ut));
	if ((pwd = getpwuid(getuid())) != NULL)
		name = pwd->pw_name;
	else
		name = "?";
	strlcpy(ut.ut_user, name, sizeof ut.ut_user);
	gettimeofday(&ut.ut_tv, NULL);
	row(&ut);
}

static int
ttywidth(void)
{
	struct winsize ws;
	long width;
	char *cols, *ep;

	if ((cols = getenv("COLUMNS")) != NULL && *cols != '\0') {
		errno = 0;
		width = strtol(cols, &ep, 10);
		if (errno || width <= 0 || width > INT_MAX || ep == cols ||
		    *ep != '\0')
			warnx("invalid COLUMNS environment variable ignored");
		else
			return (width);
	}
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1)
		return (ws.ws_col);

	return (80);
}
