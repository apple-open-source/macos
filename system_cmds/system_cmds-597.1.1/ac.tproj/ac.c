/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *      Copyright (c) 1994 Christopher G. Demetriou.
 *      @(#)Copyright (c) 1994, Simon J. Gerraty.
 *      
 *      This is free software.  It comes with NO WARRANTY.
 *      Permission to use, modify and distribute this source code 
 *      is granted subject to the following conditions.
 *      1/ that the above copyright notice and this notice 
 *      are preserved in all copies and that due credit be given 
 *      to the author.  
 *      2/ that any changes to this code are clearly commented 
 *      as such so that the author does not get blamed for bugs 
 *      other than his own.
 */

#ifndef lint
#include <sys/cdefs.h>
__unused static char rcsid[] = "$Id: ac.c,v 1.2 2006/02/07 05:51:22 lindak Exp $";
#endif

#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>
#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utmpx.h>
#include <unistd.h>

#define UT_NAMESIZE 8 /* from utmp.h; only for formatting */

/*
 * this is for our list of currently logged in sessions
 */
struct utmp_list {
	struct utmp_list *next;
	struct utmpx usr;
};

/*
 * this is for our list of users that are accumulating time.
 */
struct user_list {
	struct user_list *next;
	char	name[_UTX_USERSIZE+1];
	time_t	secs;
};

/*
 * this is for chosing whether to ignore a login
 */
struct tty_list {
	struct tty_list *next;
	char	name[_UTX_USERSIZE+3];
	int	len;
	int	ret;
};

/*
 * globals - yes yuk
 */
#ifdef CONSOLE_TTY
static char 	*Console = CONSOLE_TTY;
#endif
static time_t	Total = 0;
static time_t	FirstTime = 0;
static int	Flags = 0;
static struct user_list *Users = NULL;
static struct tty_list *Ttys = NULL;

#define NEW(type) (type *)malloc(sizeof (type))

#define	AC_W	1				/* not _PATH_WTMP */
#define	AC_D	2				/* daily totals (ignore -p) */
#define	AC_P	4				/* per-user totals */
#define	AC_U	8				/* specified users only */
#define	AC_T	16				/* specified ttys only */

#ifdef DEBUG
static int Debug = 0;
#endif

int			main __P((int, char **));
int			ac __P((void));
struct tty_list		*add_tty __P((char *));
int			do_tty __P((char *));
struct utmp_list	*log_in __P((struct utmp_list *, struct utmpx *));
struct utmp_list	*log_out __P((struct utmp_list *, struct utmpx *));
int			on_console __P((struct utmp_list *));
void			show __P((char *, time_t));
void			show_today __P((struct user_list *, struct utmp_list *,
			    time_t));
void			show_users __P((struct user_list *));
struct user_list	*update_user __P((struct user_list *, char *, time_t));
void			usage __P((void));

struct tty_list *
add_tty(name)
	char *name;
{
	struct tty_list *tp;
	register char *rcp;

	Flags |= AC_T;
	
	if ((tp = NEW(struct tty_list)) == NULL)
		err(1, "malloc");
	tp->len = 0;				/* full match */
	tp->ret = 1;				/* do if match */
	if (*name == '!') {			/* don't do if match */
		tp->ret = 0;
		name++;
	}
	(void)strncpy(tp->name, name, sizeof (tp->name) - 1);
	tp->name[sizeof (tp->name) - 1] = '\0';
	if ((rcp = strchr(tp->name, '*')) != NULL) {	/* wild card */
		*rcp = '\0';
		tp->len = strlen(tp->name);	/* match len bytes only */
	}
	tp->next = Ttys;
	Ttys = tp;
	return Ttys;
}

/*
 * should we process the named tty?
 */
int
do_tty(name)
	char *name;
{
	struct tty_list *tp;
	int def_ret = 0;
	
	for (tp = Ttys; tp != NULL; tp = tp->next) {
		if (tp->ret == 0)		/* specific don't */
			def_ret = 1;		/* default do */
		if (tp->len != 0) {
			if (strncmp(name, tp->name, tp->len) == 0)
				return tp->ret;
		} else {
			if (strncmp(name, tp->name, sizeof (tp->name)) == 0)
				return tp->ret;
		}
	}
	return def_ret;
}

#ifdef CONSOLE_TTY
/*
 * is someone logged in on Console?
 */
int
on_console(head)
	struct utmp_list *head;
{
	struct utmp_list *up;

	for (up = head; up; up = up->next) {
		if (strncmp(up->usr.ut_line, Console,
		    sizeof (up->usr.ut_line)) == 0)
			return 1;
	}
	return 0;
}
#endif

/*
 * update user's login time
 */
struct user_list *
update_user(head, name, secs)
	struct user_list *head;
	char	*name;
	time_t	secs;
{
	struct user_list *up;

	for (up = head; up != NULL; up = up->next) {
		if (strncmp(up->name, name, sizeof (up->name)) == 0) {
			up->secs += secs;
			Total += secs;
			return head;
		}
	}
	/*
	 * not found so add new user unless specified users only
	 */
	if (Flags & AC_U)
		return head;
	
	if ((up = NEW(struct user_list)) == NULL)
		err(1, "malloc");
	up->next = head;
	(void)strncpy(up->name, name, sizeof (up->name) - 1);
	up->name[sizeof (up->name) - 1] = '\0';	/* paranoid! */
	up->secs = secs;
	Total += secs;
	return up;
}

int
main(argc, argv)
	int	argc;
	char	**argv;
{
	FILE *fp;
	int c;

	fp = NULL;
	while ((c = getopt(argc, argv, "Dc:dpt:w:")) != EOF) {
		switch (c) {
#ifdef DEBUG
		case 'D':
			Debug++;
			break;
#endif
		case 'c':
#ifdef CONSOLE_TTY
			Console = optarg;
#else
			usage();		/* XXX */
#endif
			break;
		case 'd':
			Flags |= AC_D;
			break;
		case 'p':
			Flags |= AC_P;
			break;
		case 't':			/* only do specified ttys */
			add_tty(optarg);
			break;
		case 'w':
			wtmpxname(optarg);
			break;
		case '?':
		default:
			usage();
			break;
		}
	}
	if (optind < argc) {
		/*
		 * initialize user list
		 */
		for (; optind < argc; optind++) {
			Users = update_user(Users, argv[optind], 0L);
		}
		Flags |= AC_U;			/* freeze user list */
	}
	if (Flags & AC_D)
		Flags &= ~AC_P;
	ac();
	
	return 0;
}

/*
 * print login time in decimal hours
 */
void
show(name, secs)
	char *name;
	time_t secs;
{
	(void)printf("\t%-*s %8.2f\n", UT_NAMESIZE, name,
	    ((double)secs / 3600));
}

void
show_users(list)
	struct user_list *list;
{
	struct user_list *lp;

	for (lp = list; lp; lp = lp->next)
		show(lp->name, lp->secs);
}

/*
 * print total login time for 24hr period in decimal hours
 */
void
show_today(users, logins, secs)
	struct user_list *users;
	struct utmp_list *logins;
	time_t secs;
{
	struct user_list *up;
	struct utmp_list *lp;
	char date[64];
	time_t yesterday = secs - 1;

	(void)strftime(date, sizeof (date), "%b %e  total",
	    localtime(&yesterday));

	/* restore the missing second */
	yesterday++;
	
	for (lp = logins; lp != NULL; lp = lp->next) {
		secs = yesterday - lp->usr.ut_tv.tv_sec;
		Users = update_user(Users, lp->usr.ut_user, secs);
		lp->usr.ut_tv.tv_sec = yesterday;	/* as if they just logged in */
	}
	secs = 0;
	for (up = users; up != NULL; up = up->next) {
		secs += up->secs;
		up->secs = 0;			/* for next day */
	}
 	if (secs)
		(void)printf("%s %11.2f\n", date, ((double)secs / 3600));
}

/*
 * log a user out and update their times.
 * if ut_line is "~", we log all users out as the system has
 * been shut down.
 */
struct utmp_list *
log_out(head, up)
	struct utmp_list *head;
	struct utmpx *up;
{
	struct utmp_list *lp, *lp2, *tlp;
	time_t secs;
	
	for (lp = head, lp2 = NULL; lp != NULL; )
		if (up->ut_type == BOOT_TIME || up->ut_type == SHUTDOWN_TIME || strncmp(lp->usr.ut_line, up->ut_line,
		    sizeof (up->ut_line)) == 0) {
			secs = up->ut_tv.tv_sec - lp->usr.ut_tv.tv_sec;
			Users = update_user(Users, lp->usr.ut_user, secs);
#ifdef DEBUG
			if (Debug)
				printf("%-.*s %-.*s: %-.*s logged out (%2d:%02d:%02d)\n",
				    19, ctime(&up->ut_tv.tv_sec),
				    (int)sizeof (lp->usr.ut_line), lp->usr.ut_line,
				    (int)sizeof (lp->usr.ut_user), lp->usr.ut_user,
				    secs / 3600, (secs % 3600) / 60, secs % 60);
#endif
			/*
			 * now lose it
			 */
			tlp = lp;
			lp = lp->next;
			if (tlp == head)
				head = lp;
			else if (lp2 != NULL)
				lp2->next = lp;
			free(tlp);
		} else {
			lp2 = lp;
			lp = lp->next;
		}
	return head;
}


/*
 * if do_tty says ok, login a user
 */
struct utmp_list *
log_in(head, up)
	struct utmp_list *head;
	struct utmpx *up;
{
	struct utmp_list *lp;

	/*
	 * this could be a login. if we're not dealing with
	 * the console name, say it is.
	 *
	 * If we are, and if ut_host==":0.0" we know that it
	 * isn't a real login. _But_ if we have not yet recorded
	 * someone being logged in on Console - due to the wtmp
	 * file starting after they logged in, we'll pretend they
	 * logged in, at the start of the wtmp file.
	 */

#ifdef CONSOLE_TTY
	if (up->ut_host[0] == ':') {
		/*
		 * SunOS 4.0.2 does not treat ":0.0" as special but we
		 * do. 
		 */
		if (on_console(head))
			return head;
		/*
		 * ok, no recorded login, so they were here when wtmp
		 * started!  Adjust ut_tv.tv_sec! 
		 */
		up->ut_tv.tv_sec = FirstTime;
		/*
		 * this allows us to pick the right logout
		 */
		(void)strncpy(up->ut_line, Console, sizeof (up->ut_line) - 1);
		up->ut_line[sizeof (up->ut_line) - 1] = '\0'; /* paranoid! */
	}
#endif
	/*
	 * If we are doing specified ttys only, we ignore
	 * anything else.
	 */
	if (Flags & AC_T)
		if (!do_tty(up->ut_line))
			return head;

	/*
	 * go ahead and log them in
	 */
	if ((lp = NEW(struct utmp_list)) == NULL)
		err(1, "malloc");
	lp->next = head;
	head = lp;
	memmove((char *)&lp->usr, (char *)up, sizeof (struct utmpx));
#ifdef DEBUG
	if (Debug) {
		printf("%-.*s %-.*s: %-.*s logged in", 19,
		    ctime(&lp->usr.ut_tv.tv_sec), (int)sizeof (up->ut_line),
		       up->ut_line, (int)sizeof (up->ut_user), up->ut_user);
		if (*up->ut_host)
			printf(" (%-.*s)", (int)sizeof (up->ut_host), up->ut_host);
		putchar('\n');
	}
#endif
	return head;
}

int
ac()
{
	struct utmp_list *lp, *head = NULL;
	struct utmpx *u, end;
	struct tm *ltm;
	time_t secs = 0;
	int day = -1;
	
	setutxent_wtmp(1); /* read in forward direction */
	while ((u = getutxent_wtmp()) != NULL) {
		if (!FirstTime)
			FirstTime = u->ut_tv.tv_sec;
		if (Flags & AC_D) {
			ltm = localtime(&u->ut_tv.tv_sec);
			if (day >= 0 && day != ltm->tm_yday) {
				day = ltm->tm_yday;
				/*
				 * print yesterday's total
				 */
				secs = u->ut_tv.tv_sec;
				secs -= ltm->tm_sec;
				secs -= 60 * ltm->tm_min;
				secs -= 3600 * ltm->tm_hour;
				show_today(Users, head, secs);
			} else
				day = ltm->tm_yday;
		}
		switch(u->ut_type) {
		case OLD_TIME:
			secs = u->ut_tv.tv_sec;
			break;
		case NEW_TIME:
			secs -= u->ut_tv.tv_sec;
			/*
			 * adjust time for those logged in
			 */
			for (lp = head; lp != NULL; lp = lp->next)
				lp->usr.ut_tv.tv_sec -= secs;
			break;
		case BOOT_TIME:			/* reboot or shutdown */
		case SHUTDOWN_TIME:
			head = log_out(head, u);
			FirstTime = u->ut_tv.tv_sec; /* shouldn't be needed */
			break;
		case USER_PROCESS:
			/*
			 * if they came in on tty[p-y]*, then it is only
			 * a login session if the ut_host field is non-empty
			 */
			if (strncmp(u->ut_line, "tty", 3) != 0 ||
			    strchr("pqrstuvwxy", u->ut_line[3]) == 0 ||
			    *u->ut_host != '\0')
				head = log_in(head, u);
			break;
		case DEAD_PROCESS:
			head = log_out(head, u);
			break;
		}
	}
	endutxent_wtmp();
	bzero(&end, sizeof(end));
	end.ut_tv.tv_sec = time((time_t *)0);
	end.ut_type = SHUTDOWN_TIME;
	
	if (Flags & AC_D) {
		ltm = localtime(&end.ut_tv.tv_sec);
		if (day >= 0 && day != ltm->tm_yday) {
			/*
			 * print yesterday's total
			 */
			secs = end.ut_tv.tv_sec;
			secs -= ltm->tm_sec;
			secs -= 60 * ltm->tm_min;
			secs -= 3600 * ltm->tm_hour;
			show_today(Users, head, secs);
		}
	}
	/*
	 * anyone still logged in gets time up to now
	 */
	head = log_out(head, &end);

	if (Flags & AC_D)
		show_today(Users, head, time((time_t *)0));
	else {
		if (Flags & AC_P)
			show_users(Users);
		show("total", Total);
	}
	return 0;
}

void
usage()
{
	(void)fprintf(stderr,
#ifdef CONSOLE_TTY
	    "ac [-dp] [-c console] [-t tty] [-w wtmp] [users ...]\n");
#else
	    "ac [-dp] [-t tty] [-w wtmp] [users ...]\n");
#endif
	exit(1);
}
