/*
 * $Xorg: sessreg.c,v 1.5 2000/08/17 19:54:15 cpqbld Exp $
 *
 * Copyright 1990, 1998  The Open Group
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 * 
 * Except as contained in this notice, the name of The Open Group shall
 * not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization
 * from The Open Group.
 *
 * Author:  Keith Packard, MIT X Consortium
 * Lastlog support and dynamic utmp entry allocation
 *   by Andreas Stolcke <stolcke@icsi.berkeley.edu>
 */

/* $XFree86: xc/programs/xdm/sessreg.c,v 3.19 2002/12/07 20:31:04 herrb Exp $ */

/*
 * sessreg
 *
 * simple wtmp/utmp frobber
 *
 * usage: sessreg [ -w <wtmp-file> ] [ -u <utmp-file> ]
 *		  [ -l <line> ]
 *		  [ -h <host-name> ]				/ BSD only
 *		  [ -s <slot-number> ] [ -x Xservers-file ]	/ BSD only
 *		  [ -t <ttys-file> ]				/ BSD only
 *	          [ -a ] [ -d ] user-name
 *
 * one of -a or -d must be specified
 */


# include	"dm.h"

# include	<X11/Xos.h>
# include	<X11/Xfuncs.h>
# include	<stdio.h>
# include	<utmp.h>

#if defined(SYSV) || defined(SVR4) || defined(Lynx) || defined(__QNX__) || defined(__DARWIN__) || defined(_SEQUENT_)
#define NO_LASTLOG
#endif

#ifdef CSRG_BASED
#include <sys/param.h>
#endif

#ifndef NO_LASTLOG
# ifdef CSRG_BASED
#  if (BSD < 199103)
#   include	<lastlog.h>
#  endif
# else
#  include	<lastlog.h>
# endif
# include	<pwd.h>
#endif

#if defined(SVR4) || defined(linux)
#define SYSV
#endif

#ifdef CSRG_BASED
#if !defined(__FreeBSD__) && !defined(__OpenBSD__)
/* *BSD doesn't like a ':0' type entry in utmp */
#define NO_UTMP
#endif
#endif

#ifndef WTMP_FILE
# ifdef _PATH_WTMP
#  define WTMP_FILE	_PATH_WTMP
# else
#  define WTMP_FILE	"/usr/adm/wtmp"
# endif
#endif
#ifndef UTMP_FILE
# ifdef _PATH_UTMP
#  define UTMP_FILE	_PATH_UTMP
# else
#  define UTMP_FILE	"/etc/utmp"
# endif
#endif
#ifndef NO_LASTLOG
# ifndef LLOG_FILE
#  ifdef _PATH_LASTLOG
#   define LLOG_FILE	_PATH_LASTLOG
#  else
#   define LLOG_FILE	"/usr/adm/lastlog"
#  endif
# endif
#endif
#ifndef SYSV
# ifndef TTYS_FILE
#  define TTYS_FILE	"/etc/ttys"
# endif
#endif

#include <time.h>
#define Time_t time_t
#ifdef X_NOT_POSIX
extern long	lseek ();
extern char	*ttyname ();
#endif

static void set_utmp (struct utmp *u, char *line, char *user, char *host, Time_t date, int addp);

int	wflag, uflag, lflag;
char	*wtmp_file, *utmp_file, *line;
int	utmp_none, wtmp_none;
/*
 * BSD specific variables.  To make life much easier for Xstartup/Xreset
 * maintainers, these arguments are accepted but ignored for sysV
 */
int	hflag, sflag, xflag, tflag;
char	*host_name = NULL;
int	slot_number;
char	*xservers_file, *ttys_file;
char	*user_name;
int	aflag, dflag;
#ifndef NO_LASTLOG
char	*llog_file;
int	llog_none, Lflag;
#endif

char	*program_name;

#ifndef SYSV
static int findslot (char *line_name, char *host_name, int addp, int slot);
static int Xslot (char *ttys_file, char *servers_file, char *tty_line,
		  char *host_name, int addp);
#endif

static int
usage (int x)
{
	if (x) {
		fprintf (stderr, "%s: usage %s {-a -d} [-w wtmp-file] [-u utmp-file]", program_name, program_name);
#ifndef NO_LASTLOG
		fprintf (stderr, " [-L lastlog-file]");
#endif
		fprintf (stderr, "\n");
		fprintf (stderr, "             [-t ttys-file] [-l line-name] [-h host-name]\n");
		fprintf (stderr, "             [-s slot-number] [-x servers-file] user-name\n");
		exit (1);
	}
	return x;
}

static char *
getstring (char ***avp, int *flagp)
{
	char	**a = *avp;

	usage ((*flagp)++);
	if (*++*a)
		return *a;
	++a;
	usage (!*a);
	*avp = a;
	return *a;
}

#ifndef SYSV
static int
syserr (int x, char *s)
{
	if (x == -1) {
		perror (s);
		exit (1);
	}
	return x;
}
#endif

static int
sysnerr (int x, char *s)
{
	if (x == 0) {
		perror (s);
		exit (1);
	}
	return x;
}

int
main (int argc, char **argv)
{
#ifndef SYSV
	int		utmp;
#endif
	char		*line_tmp;
	int		wtmp;
	Time_t		current_time;
	struct utmp	utmp_entry;

	program_name = argv[0];
	while (*++argv && **argv == '-') {
		switch (*++*argv) {
		case 'w':
			wtmp_file = getstring (&argv, &wflag);
			if (!strcmp (wtmp_file, "none"))
				wtmp_none = 1;
			break;
		case 'u':
			utmp_file = getstring (&argv, &uflag);
			if (!strcmp (utmp_file, "none"))
				utmp_none = 1;
			break;
#ifndef NO_LASTLOG
		case 'L':
			llog_file = getstring (&argv, &Lflag);
			if (!strcmp (llog_file, "none"))
				llog_none = 1;
			break;
#endif
		case 't':
			ttys_file = getstring (&argv, &tflag);
			break;
		case 'l':
			line = getstring (&argv, &lflag);
			break;
		case 'h':
			host_name = getstring (&argv, &hflag);
			break;
		case 's':
			slot_number = atoi (getstring (&argv, &sflag));
			break;
		case 'x':
			xservers_file = getstring (&argv, &xflag);
			break;
		case 'a':
			aflag++;
			break;
		case 'd':
			dflag++;
			break;
		default:
			usage (1);
		}
	}
	usage (!(user_name = *argv++));
	usage (*argv != 0);
	/*
	 * complain if neither aflag nor dflag are set,
	 * or if both are set.
	 */
	usage (!(aflag ^ dflag));
	usage (xflag && !lflag);
	/* set up default file names */
	if (!wflag)
		wtmp_file = WTMP_FILE;
#ifndef NO_UTMP
	if (!uflag)
		utmp_file = UTMP_FILE;
#else
	utmp_none = 1;
#endif
#ifndef NO_LASTLOG
	if (!Lflag)
		llog_file = LLOG_FILE;
#endif
#if !defined(SYSV) && !defined(linux) && !defined(__QNX__)
	if (!tflag)
		ttys_file = TTYS_FILE;
	if (!sflag && !utmp_none) {
		if (xflag)
			sysnerr (slot_number = Xslot (ttys_file, xservers_file, line, host_name, aflag), "Xslot");
		else
			sysnerr (slot_number = ttyslot (), "ttyslot");
	}
#endif
	if (!lflag) {
		sysnerr ((line_tmp = ttyname (0)) != NULL, "ttyname");
		line = strrchr(line_tmp, '/');
		if (line)
			line = line + 1;
		else
			line = line_tmp;
	}
	time (&current_time);
	set_utmp (&utmp_entry, line, user_name, host_name, current_time, aflag);
	if (!utmp_none) {
#ifdef SYSV
		utmpname (utmp_file);
		setutent ();
		(void) getutid (&utmp_entry);
		pututline (&utmp_entry);
		endutent ();
#else
		utmp = open (utmp_file, O_RDWR);
		if (utmp != -1) {
			syserr ((int) lseek (utmp, (long) slot_number * sizeof (struct utmp), 0), "lseek");
			sysnerr (write (utmp, (char *) &utmp_entry, sizeof (utmp_entry))
				        == sizeof (utmp_entry), "write utmp entry");
			close (utmp);
		}
#endif
	}
	if (!wtmp_none) {
		wtmp = open (wtmp_file, O_WRONLY|O_APPEND);
		if (wtmp != -1) {
			sysnerr (write (wtmp, (char *) &utmp_entry, sizeof (utmp_entry))
				        == sizeof (utmp_entry), "write wtmp entry");
			close (wtmp);
		}
	}
#ifndef NO_LASTLOG
	if (aflag && !llog_none) {
	        int llog;
	        struct passwd *pwd = getpwnam(user_name);

	        sysnerr( pwd != NULL, "get user id");
	        llog = open (llog_file, O_RDWR);

		if (llog != -1) {
			struct lastlog ll;

			sysnerr (lseek(llog, (long) pwd->pw_uid*sizeof(ll), 0)
				        != -1, "seeking lastlog entry");
			bzero((char *)&ll, sizeof(ll));
			ll.ll_time = current_time;
			if (line)
			 (void) strncpy (ll.ll_line, line, sizeof (ll.ll_line));
			if (host_name)
			 (void) strncpy (ll.ll_host, host_name, sizeof (ll.ll_host));

			sysnerr (write (llog, (char *) &ll, sizeof (ll))
				        == sizeof (ll), "write lastlog entry");
			close (llog);
		}
	}
#endif
	return 0;
}

/*
 * fill in the appropriate records of the utmp entry
 */

static void
set_utmp (struct utmp *u, char *line, char *user, char *host, Time_t date, int addp)
{
	if (line)
		(void) strncpy (u->ut_line, line, sizeof (u->ut_line));
	else
		bzero (u->ut_line, sizeof (u->ut_line));
	if (addp && user)
		(void) strncpy (u->ut_name, user, sizeof (u->ut_name));
	else
		bzero (u->ut_name, sizeof (u->ut_name));
#ifdef SYSV
	if (line) {
		int	i;
		/*
		 * this is a bit crufty, but
		 * follows the apparent conventions in
		 * the ttys file.  ut_id is only 4 bytes
		 * long, and the last 4 bytes of the line
		 * name are written into it, left justified.
		 */
		i = strlen (line);
		if (i >= sizeof (u->ut_id))
			i -= sizeof (u->ut_id);
		else
			i = 0;
		(void) strncpy (u->ut_id, line + i, sizeof (u->ut_id));
	} else
		bzero (u->ut_id, sizeof (u->ut_id));
	if (addp) {
		u->ut_pid = getppid ();
		u->ut_type = USER_PROCESS;
	} else {
		u->ut_pid = 0;
		u->ut_type = DEAD_PROCESS;
	}
#endif
#if (!defined(SYSV) && !defined(__QNX__)) || defined(linux)
	if (addp && host)
		(void) strncpy (u->ut_host, host, sizeof (u->ut_host));
	else
		bzero (u->ut_host, sizeof (u->ut_host));
#endif
	u->ut_time = date;
}

#ifndef SYSV
/*
 * compute the slot-number for an X display.  This is computed
 * by counting the lines in /etc/ttys and adding the line-number
 * that the display appears on in Xservers.  This is a poor
 * design, but is limited by the non-existant interface to utmp.
 * If host_name is non-NULL, assume it contains the display name,
 * otherwise use the tty_line argument (i.e., the tty name).
 */

static int
Xslot (char *ttys_file, char *servers_file, char *tty_line, char *host_name,
       int addp)
{
	FILE	*ttys, *servers;
	int	c;
	int	slot = 1;
	int	column0 = 1;
	char	servers_line[1024];
	char	disp_name[512];
	int	len;
	char	*pos;

	/* remove screen number from the display name */
	memset(disp_name, 0, sizeof(disp_name));
	strncpy(disp_name, host_name ? host_name : tty_line, sizeof(disp_name)-1);
	pos = strrchr(disp_name, ':');
	if (pos) {
	    pos = strchr(pos, '.');
	    if (pos)
		*pos = '\0';
	}
	sysnerr ((int)(long)(ttys = fopen (ttys_file, "r")), ttys_file);
	while ((c = getc (ttys)) != EOF)
		if (c == '\n') {
			++slot;
			column0 = 1;
		} else
			column0 = 0;
	if (!column0)
		++slot;
	(void) fclose (ttys);
	sysnerr ((int)(long)(servers = fopen (servers_file, "r")), servers_file);

	len = strlen (disp_name);
	column0 = 1;
	while (fgets (servers_line, sizeof (servers_line), servers)) {
		if (column0 && *servers_line != '#') {
			if (!strncmp (disp_name, servers_line, len) &&
			    (servers_line[len] == ' ' ||
			     servers_line[len] == '\t'))
				return slot;
			++slot;
		}
		if (servers_line[strlen(servers_line)-1] != '\n')
			column0 = 0;
		else
			column0 = 1;
	}
	/*
	 * display not found in Xservers file - allocate utmp entry dinamically
	 */
	return findslot (tty_line, host_name, addp, slot);
}

/*
 * find a free utmp slot for the X display.  This allocates a new entry
 * past the regular tty entries if necessary, reusing existing entries
 * (identified by (line,hostname)) if possible.
 */
static int
findslot (char *line_name, char *host_name, int addp, int slot)
{
	int	utmp;
	struct	utmp entry;
	int	found = 0;
	int	freeslot = -1;

	syserr(utmp = open (utmp_file, O_RDONLY), "open utmp");

	/*
	 * first, try to locate a previous entry for this display
	 * also record location of a free slots in case we need a new one
	 */
	syserr ((int) lseek (utmp, (long) slot * sizeof (struct utmp), 0), "lseek");

	if (!host_name)
		host_name = "";

	while (read (utmp, (char *) &entry, sizeof (entry)) == sizeof (entry)) {
		if (strncmp(entry.ut_line, line_name,
			sizeof(entry.ut_line)) == 0 
#ifndef __QNX__
                    &&
		    strncmp(entry.ut_host, host_name,
			sizeof(entry.ut_host)) == 0
#endif
                   ) {
			found = 1;
			break;
		}
		if (freeslot < 0 && *entry.ut_name == '\0')
			freeslot = slot;
		++slot;
	}

	close (utmp);

	if (found)
		return slot;
	else if (!addp)
		return 0;	/* trying to delete a non-existing entry */
	else if (freeslot < 0)
		return slot;	/* first slot past current entries */
	else
		return freeslot;
}
#endif
