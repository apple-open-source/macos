/* 
 * Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved
 *
 * Copyright (c) 1987, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * The NEXTSTEP Software License Agreement specifies the terms
 * and conditions for redistribution.
 *
 *	@(#)last.c	8.2 (Berkeley) 4/2/94
 */


#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1987, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tzfile.h>
#include <unistd.h>
#include <utmpx.h>
#include <ctype.h>

#define	NO	0				/* false/no */
#define	YES	1				/* true/yes */

static const struct utmpx	*prev;		/* previous utmpx structure */

/* values from utmp.h, used for print formatting */
#define	UT_NAMESIZE	8			/* old utmp.h value */
#define	UT_LINESIZE	8
#define	UT_HOSTSIZE	16

typedef struct arg {
	const char	*name;			/* argument */
#define	HOST_TYPE	-2
#define	TTY_TYPE	-3
#define	USER_TYPE	-4
	int	type;				/* type of arg */
	struct arg	*next;			/* linked list pointer */
} ARG;
ARG	*arglist;				/* head of linked list */

typedef struct ttytab {
	long	logout;				/* log out time */
	char	id[_UTX_IDSIZE];		/* terminal id */
	pid_t	pid;
	struct ttytab	*next;			/* linked list pointer */
} TTY;
TTY	*ttylist;				/* head of linked list */

static const	char *crmsg;			/* cause of last reboot */
static long	currentout,			/* current logout value */
		maxrec;				/* records to display */
time_t	now;

void	 addarg __P((int, const char *));
TTY	*addtty __P((const char *, pid_t));
void	 hostconv __P((const char *));
void	 onintr __P((int));
const char	*ttyconv __P((const char *));
int	 want __P((struct utmpx *, int));
void	 wtmp __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern int optind;
	extern char *optarg;
	int ch;
	char *p;
	char *myname = *argv;

	if ((p = strrchr(myname, '/')) != NULL)
		myname = p + 1;
	maxrec = -1;
	while ((ch = getopt(argc, argv, "0123456789f:h:t:")) != EOF)
		switch (ch) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			/*
			 * kludge: last was originally designed to take
			 * a number after a dash.
			 */
			if (maxrec == -1) {
				p = argv[optind - 1];
				if (p[0] == '-' && p[1] == ch && !p[2])
					maxrec = atol(++p);
				else
					maxrec = atol(argv[optind] + 1);
				if (!maxrec)
					exit(0);
			}
			break;
		case 'h':
			hostconv(optarg);
			addarg(HOST_TYPE, optarg);
			break;
		case 't':
			addarg(TTY_TYPE, ttyconv(optarg));
			break;
		case 'f':
			warnx("-f is unsupported");
			break;
		case '?':
		default:
			(void)fprintf(stderr,
	"usage: last [-#] [-t tty] [-h hostname] [user ...]\n");
			exit(1);
		}

	if (argc) {
		setlinebuf(stdout);
		for (argv += optind; *argv; ++argv) {
#define	COMPATIBILITY
#ifdef	COMPATIBILITY
			/* code to allow "last p5" to work */
			addarg(TTY_TYPE, ttyconv(*argv));
#endif
			addarg(USER_TYPE, *argv);
		}
	}
	wtmp();
	exit(0);
}

/*
 * wtmp --
 *	read through the wtmp file
 */

void
wtmp()
{
	
	struct utmpx	*bp;			/* current structure */
	TTY	*T;				/* tty list entry */
	long	delta;				/* time difference */
	char	*ct;

	(void)time(&now);
	(void)signal(SIGINT, onintr);
	(void)signal(SIGQUIT, onintr);

	setutxent_wtmp(0);	/* zero means reverse chronological order */
	while ((bp = getutxent_wtmp()) != NULL) {
		prev = bp;
		switch(bp->ut_type) {
		case BOOT_TIME:
		case SHUTDOWN_TIME:
			/* everybody just logged out */
			for (T = ttylist; T; T = T->next)
				T->logout = -bp->ut_tv.tv_sec;
			currentout = -bp->ut_tv.tv_sec;
			crmsg = bp->ut_type == BOOT_TIME ? "crash" : "shutdown";
			if (want(bp, NO)) {
				ct = ctime(&bp->ut_tv.tv_sec);
				printf("%-*s  %-*s %-*.*s %10.10s %5.5s \n",
				    UT_NAMESIZE, bp->ut_type == BOOT_TIME ? "reboot" : "shutdown",
				    UT_LINESIZE, "~",
				    UT_HOSTSIZE, _UTX_HOSTSIZE,
				    bp->ut_host, ct, ct + 11);
				if (maxrec != -1 && !--maxrec)
					return;
			}
			continue;
		case OLD_TIME:
		case NEW_TIME:
			if (want(bp, NO)) {
				ct = ctime(&bp->ut_tv.tv_sec);
				printf("%-*s  %-*s %-*.*s %10.10s %5.5s \n",
				    UT_NAMESIZE, "date",
				    UT_LINESIZE, bp->ut_type == OLD_TIME ? "|" : "{",
				    UT_HOSTSIZE, _UTX_HOSTSIZE, bp->ut_host,
				    ct, ct + 11);
				if (maxrec && !--maxrec)
					return;
			}
			continue;
		case USER_PROCESS:
		case DEAD_PROCESS:
			/* find associated tty */
			for (T = ttylist;; T = T->next) {
				if (!T) {
					/* add new one */
					T = addtty(bp->ut_id, bp->ut_pid);
					break;
				}
				if (!memcmp(T->id, bp->ut_id, _UTX_IDSIZE) && T->pid == bp->ut_pid)
					break;
			}
			if (bp->ut_type != DEAD_PROCESS && want(bp, YES)) {
				ct = ctime(&bp->ut_tv.tv_sec);
				printf("%-*.*s  %-*.*s %-*.*s %10.10s %5.5s ",
				UT_NAMESIZE, _UTX_USERSIZE, bp->ut_user,
				UT_LINESIZE, _UTX_LINESIZE, bp->ut_line,
				UT_HOSTSIZE, _UTX_HOSTSIZE, bp->ut_host,
				ct, ct + 11);
				if (!T->logout)
					puts("  still logged in");
				else {
					if (T->logout < 0) {
						T->logout = -T->logout;
						printf("- %s", crmsg);
					}
					else
						printf("- %5.5s",
						    ctime(&T->logout)+11);
					delta = T->logout - bp->ut_tv.tv_sec;
					if (delta < SECSPERDAY)
						printf("  (%5.5s)\n",
						    asctime(gmtime(&delta))+11);
					else
						printf(" (%ld+%5.5s)\n",
						    delta / SECSPERDAY,
						    asctime(gmtime(&delta))+11);
				}
				if (maxrec != -1 && !--maxrec)
					return;
			}
			T->logout = bp->ut_tv.tv_sec;
			continue;
		}
	}
	endutxent_wtmp();
	ct = ctime(prev ? &prev->ut_tv.tv_sec : &now);
	printf("\nwtmp begins %10.10s %5.5s \n", ct, ct + 11);
}

/*
 * want --
 *	see if want this entry
 */
int
want(bp, check)
	struct utmpx *bp;
	int check;
{
	ARG *step;

	if (!arglist)
		return (YES);

	for (step = arglist; step; step = step->next)
		switch(step->type) {
		case HOST_TYPE:
			if (!strncasecmp(step->name, bp->ut_host, _UTX_HOSTSIZE))
				return (YES);
			break;
		case TTY_TYPE:
		{
			char *line = bp->ut_line;
			if (check) {
				/*
				 * when uucp and ftp log in over a network, the entry in
				 * the utmpx file is the name plus their process id.  See
				 * etc/ftpd.c and usr.bin/uucp/uucpd.c for more information.
				 */
				if (!strncmp(line, "ftp", sizeof("ftp") - 1))
					line = "ftp";
				else if (!strncmp(line, "uucp", sizeof("uucp") - 1))
					line = "uucp";
			}
			if (!strncmp(step->name, line, _UTX_LINESIZE))
				return (YES);
			break;
		}
		case USER_TYPE:
			if (bp->ut_type == BOOT_TIME && !strncmp(step->name, "reboot", _UTX_USERSIZE))
				return (YES);
			if (bp->ut_type == SHUTDOWN_TIME && !strncmp(step->name, "shutdown", _UTX_USERSIZE))
				return (YES);
			if (!strncmp(step->name, bp->ut_user, _UTX_USERSIZE))
				return (YES);
			break;
	}
	return (NO);
}

/*
 * addarg --
 *	add an entry to a linked list of arguments
 */
void
addarg(type, arg)
	int type;
	const char *arg;
{
	ARG *cur;

	if (!(cur = (ARG *)malloc((u_int)sizeof(ARG))))
		err(1, "malloc failure");
	cur->next = arglist;
	cur->type = type;
	cur->name = arg;
	arglist = cur;
}

/*
 * addtty --
 *	add an entry to a linked list of ttys
 */
TTY *
addtty(id, pid)
	const char *id;
	pid_t pid;
{
	TTY *cur;

	if (!(cur = (TTY *)malloc((u_int)sizeof(TTY))))
		err(1, "malloc failure");
	cur->next = ttylist;
	cur->logout = currentout;
	memmove(cur->id, id, _UTX_IDSIZE);
	cur->pid = pid;
	return (ttylist = cur);
}

/*
 * hostconv --
 *	convert the hostname to search pattern; if the supplied host name
 *	has a domain attached that is the same as the current domain, rip
 *	off the domain suffix since that's what login(1) does.
 */
void
hostconv(arg)
	const char *arg;
{
	static int first = 1;
	static char *hostdot, name[MAXHOSTNAMELEN];
	char *argdot;

	if (!(argdot = strchr(arg, '.')))
		return;
	if (first) {
		first = 0;
		if (gethostname(name, sizeof(name)))
			err(1, "gethostname");
		hostdot = strchr(name, '.');
	}
	if (hostdot && !strcasecmp(hostdot, argdot))
		*argdot = '\0';
}

/*
 * ttyconv --
 *	convert tty to correct name.
 */
const char *
ttyconv(arg)
	const char *arg;
{
	char *mval;
	int kludge = 0;
	int len = strlen(arg);

	/*
	 * kludge -- we assume that most tty's end with
	 * a two character suffix.
	 */
	if (len == 2)
		kludge = 8; /* either 6 for "ttyxx" or 8 for "console"; use largest */
	/*
	 * kludge -- we assume cloning ptys names are "ttys" followed by
	 * more than one digit
	 */
	else if (len > 2 && *arg == 's') {
		const char *cp = arg + 1;
		while(*cp && isdigit(*cp))
			cp++;
		if (*cp == 0)
			kludge = len + sizeof("tty");
	}
	if (kludge) {
		if (!(mval = malloc(kludge)))
			err(1, "malloc failure");
		if (!strcmp(arg, "co"))
			(void)strcpy(mval, "console");
		else {
			(void)strcpy(mval, "tty");
			(void)strcpy(mval + sizeof("tty") - 1, arg);
		}
		return (mval);
	}
	if (!strncmp(arg, _PATH_DEV, sizeof(_PATH_DEV) - 1))
		return (arg + sizeof(_PATH_DEV) - 1);
	return (arg);
}

/*
 * onintr --
 *	on interrupt, we inform the user how far we've gotten
 */
void
onintr(signo)
	int signo;
{
	char *ct;

	ct = ctime(prev ? &prev->ut_tv.tv_sec : &now);
	printf("\ninterrupted %10.10s %5.5s \n", ct, ct + 11);
	if (signo == SIGINT)
		exit(1);
	(void)fflush(stdout);			/* fix required for rsh */
}
