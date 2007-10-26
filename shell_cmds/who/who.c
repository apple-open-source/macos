/*-
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
/* commented as FBSDID not needed for Tiger ......
__FBSDID("$FreeBSD: src/usr.bin/who/who.c,v 1.20 2003/10/26 05:05:48 peter Exp $");
*/

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

/* from utmp.h; used only for print formatting */
#define	UT_NAMESIZE	8
#define	UT_LINESIZE	8
#define	UT_HOSTSIZE	16

static void	heading(void);
static void	process_utmp(void);
static void	process_wtmp();
static void	quick(void);
static void	row(const struct utmpx *);
static int	ttywidth(void);
static void	usage(void);
static void	whoami(void);

static int	bflag;			/* date & time of last reboot */
static int	dflag;			/* dead processes */
static int	Hflag;			/* Write column headings */
static int	lflag;			/* waiting to login */
static int	mflag;			/* Show info about current terminal */
static int	pflag;			/* Processes active & spawned by init */
static int	qflag;			/* "Quick" mode */
static int	rflag;			/* run-level of the init process */
static int	sflag;			/* Show name, line, time */
static int	tflag;			/* time of change to system clock */
static int	Tflag;			/* Show terminal state */
static int	uflag;			/* Show idle time */
#ifdef __APPLE__
#include <get_compat.h>
#else  /* !__APPLE__ */
#define COMPAT_MODE(a,b) (1)
#endif /* __APPLE__ */
static int	unix2003_std;

int
main(int argc, char *argv[])
{
	int ch;

	setlocale(LC_TIME, "");

	unix2003_std = COMPAT_MODE("bin/who", "unix2003");

	while ((ch = getopt(argc, argv, "abdHlmpqrstTu")) != -1) {
		switch (ch) {

		case 'a':		/* -b, -d, -l, -p, -r, -t, -T and -u */
			bflag = dflag = lflag = pflag = 1;
			rflag = tflag = Tflag = uflag = 1;
			break;
		case 'b':		/* date & time of last reboot */
			bflag = 1;
			break;
		case 'd':		/* dead processes */
			dflag = 1;
			break;
		case 'H':		/* Write column headings */
			Hflag = 1;
			break;
		case 'l':		/* waiting to login */
			lflag = 1;
			break;
		case 'm':		/* Show info about current terminal */
			mflag = 1;
			break;
		case 'p':		/* Processes active & spawned by init */
			pflag = 1;
			break;
		case 'q':		/* "Quick" mode */
			qflag = 1;
			break;
		case 'r':		/* run-level of the init process */
			rflag = 1;
			break;
		case 's':		/* Show name, line, time */
			sflag = 1;
			break;
		case 't':		/* time of change to system clock */
			tflag = 1;
			break;
		case 'T':		/* Show terminal state */
			Tflag = 1;
			break;
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
		if (!utmpxname(*argv) || !wtmpxname(*argv))
		    usage();
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
			/* read and process utmpx file for relevant options */
			if( Tflag || uflag || !(bflag || dflag || lflag || pflag || rflag) )
				process_utmp();
	}

	/* read and process wtmp file for relevant options */
	if (bflag || dflag || lflag || pflag || rflag ) {

		process_wtmp();
	}

	endutxent();
	exit(0);
}

static void
usage(void)
{

	fprintf(stderr, "usage: who [-abdHlmpqrstTu] [am I] [file]\n");
	exit(1);
}

static void
heading(void)
{

	printf("%-*s ", UT_NAMESIZE, "NAME");
	if (Tflag)
		printf("S ");
	printf("%-*s ", UT_LINESIZE, "LINE");
	printf("%-*s ", 12, "TIME");
	if (uflag)
		printf("IDLE  ");
	if (unix2003_std && uflag && !Tflag)
		printf("     PID ");
	printf("%-*s", UT_HOSTSIZE, "FROM");
	putchar('\n');
}

static void
row(const struct utmpx *ut)
{
	char buf[80], tty[sizeof(_PATH_DEV) + _UTX_LINESIZE];
	struct stat sb;
	time_t idle, t;
	static int d_first = -1;
	struct tm *tm;
	char state;
	char login_pidstr[20];

	if (d_first < 0)
		d_first = (*nl_langinfo(D_MD_ORDER) == 'd');

	if (Tflag || uflag) {
		snprintf(tty, sizeof(tty), "%s%.*s", _PATH_DEV,
			_UTX_LINESIZE, ut->ut_line);
		state = '?';
		idle = 0;
		if (stat(tty, &sb) == 0) {
			state = sb.st_mode & (S_IWOTH|S_IWGRP) ?
			    '+' : '-';
			idle = time(NULL) - sb.st_mtime;
		}
		if (unix2003_std && !Tflag) {
			/* uflag without Tflag */
			if (ut->ut_pid) {
				snprintf(login_pidstr,sizeof(login_pidstr),
						"%8d",ut->ut_pid);
			} else {
				strcpy(login_pidstr,"       ?");
			}
		}
	}

	printf("%-*.*s ", UT_NAMESIZE, _UTX_USERSIZE, ut->ut_user);
	if (Tflag)
		printf("%c ", state);
	printf("%-*.*s ", UT_LINESIZE, _UTX_LINESIZE, ut->ut_line);
	t = _time32_to_time(ut->ut_tv.tv_sec);
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
		if (unix2003_std && !Tflag) {
			printf("%s ", login_pidstr);
		}
	}
	if (*ut->ut_host != '\0')
		printf("(%.*s)", _UTX_HOSTSIZE, ut->ut_host);
	putchar('\n');

}

static void
process_utmp(void)
{
	struct utmpx *ut;

	while ((ut = getutxent()) != NULL)
		if (*ut->ut_user != '\0' && ut->ut_type == USER_PROCESS) {
			row(ut);
		}
}

/* For some options, process the wtmp file to generate output */
static void
process_wtmp(void)
{
	struct utmpx *ut;
	struct utmpx lboot_ut;
	int num = 0;	/* count of user entries */

	setutxent_wtmp(0);	/* zero means reverse chronological */
	lboot_ut.ut_type = 0;
	while (!lboot_ut.ut_type && (ut = getutxent_wtmp()) != NULL) {
		switch(ut->ut_type) {
		case BOOT_TIME:
			lboot_ut = *ut;
			strcpy(lboot_ut.ut_user, "reboot");
			strcpy(lboot_ut.ut_line, "~");
			break;
		case INIT_PROCESS:
		case LOGIN_PROCESS:
		case USER_PROCESS:
		case DEAD_PROCESS:
			num++;
			break;
		}
	}
	endutxent_wtmp();

	if (bflag && lboot_ut.ut_type)
		row(&lboot_ut);

	/* run level of the init process is unknown in BSD system. If multi
	   user, then display the highest run level. Else, no-op.
	*/
	if (rflag && (num > 1))
		printf("   .       run-level 3\n");
}

static void
quick(void)
{
	struct utmpx *ut;
	int col, ncols, num;

	ncols = ttywidth();
	col = num = 0;
	while ((ut = getutxent()) != NULL) {
		if (*ut->ut_user == '\0' || ut->ut_type != USER_PROCESS)
			continue;
		printf("%-*.*s", UT_NAMESIZE, _UTX_USERSIZE, ut->ut_user);
		if (++col < ncols / (UT_NAMESIZE + 1))
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
	struct utmpx ut;
	struct utmpx *u;
	struct passwd *pwd;
	const char *name, *p, *tty;

	if ((tty = ttyname(STDIN_FILENO)) == NULL)
		tty = "tty??";
	else if ((p = strrchr(tty, '/')) != NULL)
		tty = p + 1;

	memset(&ut, 0, sizeof(ut));
	strncpy(ut.ut_line, tty, sizeof(ut.ut_line));
	memcpy(ut.ut_id, tty + (strlen(tty) - sizeof(ut.ut_id)), sizeof(ut.ut_id));
	ut.ut_type = USER_PROCESS;
	/* Search utmp for our tty, dump first matching record. */
	u = getutxid(&ut);
	if (u) {
		row(u);
		return;
	}

	/* Not found; fill the utmpx structure with the information we have. */
	if ((pwd = getpwuid(getuid())) != NULL)
		name = pwd->pw_name;
	else
		name = "?";
	strncpy(ut.ut_user, name, _UTX_USERSIZE);
	ut.ut_tv.tv_sec = _time_to_time32(time(NULL));
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
