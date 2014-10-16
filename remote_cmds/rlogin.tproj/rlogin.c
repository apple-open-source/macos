/*
 * Copyright (c) 1983, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * Portions of this software were developed for the FreeBSD Project by
 * ThinkSec AS and NAI Labs, the Security Research Division of Network
 * Associates, Inc.  under DARPA/SPAWAR contract N66001-01-C-8035
 * ("CBOSS"), as part of the DARPA CHATS research program.
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

#ifndef lint
__attribute__((__used__))
static const char copyright[] =
"@(#) Copyright (c) 1983, 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#if 0
#ifndef lint
static const char sccsid[] = "@(#)rlogin.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/usr.bin/rlogin/rlogin.c,v 1.40 2005/11/13 21:03:56 dwmalone Exp $");

/*
 * rlogin - remote login
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#ifndef __APPLE__
#include <libutil.h>
#endif
#include <netdb.h>
#include <paths.h>
#include <pwd.h>
#include <setjmp.h>
#include <termios.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <stdarg.h>

#if defined(KERBEROS)
#include <kerberosIV/des.h>
#include <kerberosIV/krb.h>

#include "krb.h"

CREDENTIALS cred;
Key_schedule schedule;
int use_kerberos = 1, doencrypt;
char dst_realm_buf[REALM_SZ], *dest_realm = NULL;
#endif /* KERBEROS */

#ifndef TIOCPKT_WINDOW
#define	TIOCPKT_WINDOW	0x80
#endif

/* concession to Sun */
#ifndef SIGUSR1
#define	SIGUSR1	30
#endif

int eight, rem;
struct termios deftty;

int family = PF_UNSPEC;

int noescape;
u_char escapechar = '~';

#define	get_window_size(fd, wp)	ioctl(fd, TIOCGWINSZ, wp)
struct	winsize winsize;

void		catch_child(int);
void		copytochild(int);
#ifdef __APPLE__
void		doit(sigset_t *) __dead2;
#else
void		doit(long) __dead2;
#endif
void		done(int) __dead2;
void		echo(char);
u_int		getescape(const char *);
void		lostpeer(int);
void		mode(int);
void		msg(const char *);
void		oob(int);
#ifdef __APPLE__
int		reader(sigset_t *);
#else
int		reader(int);
#endif
void		sendwindow(void);
void		setsignal(int);
void		sigwinch(int);
void		stop(char);
void		usage(void) __dead2;
void		writer(void);
void		writeroob(int);

#if defined(KERBEROS)
void		warning(const char *, ...);
#endif

int
main(int argc, char *argv[])
{
	struct passwd *pw;
	struct servent *sp;
	struct termios tty;
#ifndef __APPLE__
	long omask;
#else
	sigset_t smask;
	struct sigaction sa;
#endif
	int argoff, ch, dflag, Dflag, one;
	uid_t uid;
	char *host, *localname, *p, *user, term[1024];
	speed_t ospeed;
	struct sockaddr_storage ss;
	socklen_t sslen;
	size_t len, len2;
	int i;

	argoff = dflag = Dflag = 0;
	one = 1;
	host = localname = user = NULL;

#ifdef __APPLE__
	if (argv[0] == NULL)
		usage();
#endif
	if ((p = rindex(argv[0], '/')))
		++p;
	else
		p = argv[0];

	if (strcmp(p, "rlogin"))
		host = p;

	/* handle "rlogin host flags" */
	if (!host && argc > 2 && argv[1][0] != '-') {
		host = argv[1];
		argoff = 1;
	}

#if defined(KERBEROS)
#define	OPTIONS	"468EKLde:k:l:x"
#else
#define	OPTIONS	"468DEde:i:l:"
#endif
	while ((ch = getopt(argc - argoff, argv + argoff, OPTIONS)) != -1)
		switch(ch) {
		case '4':
			family = PF_INET;
			break;

		case '6':
			family = PF_INET6;
			break;

		case '8':
			eight = 1;
			break;
		case 'D':
			Dflag = 1;
			break;
		case 'E':
			noescape = 1;
			break;
#if defined(KERBEROS)
		case 'K':
			use_kerberos = 0;
			break;
		case 'L':
			litout = 1;
			break;
#endif
		case 'd':
			dflag = 1;
			break;
		case 'e':
			noescape = 0;
			escapechar = getescape(optarg);
			break;
		case 'i':
			if (getuid() != 0)
				errx(1, "-i user: permission denied");
			localname = optarg;
#if defined(KERBEROS)
		case 'k':
			dest_realm = dst_realm_buf;
			(void)strncpy(dest_realm, optarg, REALM_SZ);
			break;
#endif
		case 'l':
			user = optarg;
			break;
#if defined(KERBEROS) && defined(CRYPT)
		case 'x':
			doencrypt = 1;
			des_set_key(cred.session, schedule);
			break;
#endif
		case '?':
		default:
			usage();
		}
	optind += argoff;

	/* if haven't gotten a host yet, do so */
	if (!host && !(host = argv[optind++]))
		usage();

	if (argv[optind])
		usage();

	if (!(pw = getpwuid(uid = getuid())))
		errx(1, "unknown user id");
#ifdef __APPLE__
	/* Accept user1@host format, though "-l user2" overrides user1 */
	p = strchr(host, '@');
	if (p) {
		*p = '\0';
		if (!user && p > host)
			user = host;
		host = p + 1;
		if (*host == '\0')
			usage();
	}
#endif
	if (!user)
		user = pw->pw_name;
	if (!localname)
		localname = pw->pw_name;

	sp = NULL;
#if defined(KERBEROS)
	if (use_kerberos) {
		sp = getservbyname((doencrypt ? "eklogin" : "klogin"), "tcp");
		if (sp == NULL) {
			use_kerberos = 0;
			warning("can't get entry for %s/tcp service",
			    doencrypt ? "eklogin" : "klogin");
		}
	}
	if (sp == NULL)
#endif
	sp = getservbyname("login", "tcp");
	if (sp == NULL)
		errx(1, "login/tcp: unknown service");

	if ((p = getenv("TERM")) != NULL)
		(void)strlcpy(term, p, sizeof(term));
#if __APPLE__
	else
		(void)strcpy(term, "network");
#endif
	len = strlen(term);
	if (len < (sizeof(term) - 1) && tcgetattr(0, &tty) == 0) {
		/* start at 2 to include the / */
		for (ospeed = i = cfgetospeed(&tty), len2 = 2; i > 9; len2++)
			i /= 10;
		if (len + len2 < sizeof(term))
			(void)snprintf(term + len, len2 + 1, "/%ld", ospeed);
	}

	(void)get_window_size(0, &winsize);

#ifdef __APPLE__
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = lostpeer;
	(void)sigaction(SIGPIPE, &sa, (struct sigaction *) 0);
#else
	(void)signal(SIGPIPE, lostpeer);
#endif
	/* will use SIGUSR1 for window size hack, so hold it off */
#ifdef __APPLE__
	sigemptyset(&smask);
	sigaddset(&smask, SIGURG);
	sigaddset(&smask, SIGUSR1);
	(void)sigprocmask(SIG_SETMASK, &smask, &smask);
#else
	omask = sigblock(sigmask(SIGURG) | sigmask(SIGUSR1));
#endif
	/*
	 * We set SIGURG and SIGUSR1 below so that an
	 * incoming signal will be held pending rather than being
	 * discarded. Note that these routines will be ready to get
	 * a signal by the time that they are unblocked below.
	 */
#ifdef __APPLE__
	sa.sa_handler = copytochild;
	(void)sigaction(SIGURG, &sa, (struct sigaction *) 0);
	sa.sa_handler = writeroob;
	(void)sigaction(SIGUSR1, &sa, (struct sigaction *) 0);
#else
	(void)signal(SIGURG, copytochild);
	(void)signal(SIGUSR1, writeroob);
#endif

#if defined(KERBEROS)
try_connect:
	if (use_kerberos) {
		struct hostent *hp;

		/* Fully qualify hostname (needed for krb_realmofhost). */
		hp = gethostbyname(host);
		if (hp != NULL && !(host = strdup(hp->h_name)))
			errx(1, "%s", strerror(ENOMEM));

		rem = KSUCCESS;
		errno = 0;
		if (dest_realm == NULL)
			dest_realm = krb_realmofhost(host);

#if defined(CRYPT)
		if (doencrypt)
			rem = krcmd_mutual(&host, sp->s_port, user, term, 0,
			    dest_realm, &cred, schedule);
		else
#endif /* CRYPT */
			rem = krcmd(&host, sp->s_port, user, term, 0,
			    dest_realm);
		if (rem < 0) {
			use_kerberos = 0;
			sp = getservbyname("login", "tcp");
			if (sp == NULL)
				errx(1, "unknown service login/tcp");
			if (errno == ECONNREFUSED)
				warning("remote host doesn't support Kerberos");
			if (errno == ENOENT)
				warning("can't provide Kerberos auth data");
			goto try_connect;
		}
	} else {
#if defined(CRYPT)
		if (doencrypt)
			errx(1, "the -x flag requires Kerberos authentication");
#endif /* CRYPT */
		rem = rcmd(&host, sp->s_port, pw->pw_name, user, term, 0);
	}
#else /* KERBEROS */
	rem = rcmd_af(&host, sp->s_port, localname, user, term, 0, family);
//	rem = rcmd(&host, sp->s_port, pw->pw_name, user, term, 0);
#endif /* KERBEROS */

	if (rem < 0)
		exit(1);

	if (dflag &&
	    setsockopt(rem, SOL_SOCKET, SO_DEBUG, &one, sizeof(one)) < 0)
		warn("setsockopt");
	if (Dflag &&
	    setsockopt(rem, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) < 0)
		warn("setsockopt NODELAY (ignored)");

	sslen = sizeof(ss);
	one = IPTOS_LOWDELAY;
	if (getsockname(rem, (struct sockaddr *)&ss, &sslen) == 0 &&
	    ss.ss_family == AF_INET) {
		if (setsockopt(rem, IPPROTO_IP, IP_TOS, (char *)&one,
			       sizeof(int)) < 0)
			warn("setsockopt TOS (ignored)");
	} else
		if (ss.ss_family == AF_INET)
			warn("setsockopt getsockname failed");

	(void)setuid(uid);
#ifdef __APPLE__
	doit(&smask);
#else
	doit(omask);
#endif
	/*NOTREACHED*/
}

int child;

void
#ifdef __APPLE__
doit(sigset_t *omask)
#else
doit(long omask)
#endif
{

//	int i;
//	for (i = 0; i < NCCS; i++)
//		nott.c_cc[i] = _POSIX_VDISABLE;
//	tcgetattr(0, &deftt);
//	nott.c_cc[VSTART] = deftt.c_cc[VSTART];
//	nott.c_cc[VSTOP] = deftt.c_cc[VSTOP];
#ifdef __APPLE__
	struct sigaction sa;

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = SIG_IGN;
	(void)sigaction(SIGINT, &sa, (struct sigaction *) 0);
#else
	(void)signal(SIGINT, SIG_IGN);
#endif
	setsignal(SIGHUP);
	setsignal(SIGQUIT);
	mode(1);
	child = fork();
	if (child == -1) {
		warn("fork");
		done(1);
	}
	if (child == 0) {
		if (reader(omask) == 0) {
			msg("connection closed");
			exit(0);
		}
		sleep(1);
		msg("\007connection closed");
		exit(1);
	}

	/*
	 * We may still own the socket, and may have a pending SIGURG (or might
	 * receive one soon) that we really want to send to the reader.  When
	 * one of these comes in, the trap copytochild simply copies such
	 * signals to the child. We can now unblock SIGURG and SIGUSR1
	 * that were set above.
	 */
#ifdef __APPLE__
	(void)sigprocmask(SIG_SETMASK, omask, (sigset_t *) 0);
	sa.sa_handler = catch_child;
	(void)sigaction(SIGCHLD, &sa, (struct sigaction *) 0);
#else
	(void)sigsetmask(omask);
	(void)signal(SIGCHLD, catch_child);
#endif
	writer();
	msg("closed connection");
	done(0);
}

/* trap a signal, unless it is being ignored. */
void
setsignal(int sig)
{
#ifdef __APPLE__
	struct sigaction sa;
	sigset_t sigs;

	sigemptyset(&sigs);
	sigaddset(&sigs, sig);
	sigprocmask(SIG_BLOCK, &sigs, &sigs);

	sigemptyset(&sa.sa_mask);
	sa.sa_handler = exit;
	sa.sa_flags = SA_RESTART;
	(void)sigaction(sig, &sa, &sa);
	if (sa.sa_handler == SIG_IGN)
		(void)sigaction(sig, &sa, (struct sigaction *) 0);

	(void)sigprocmask(SIG_SETMASK, &sigs, (sigset_t *) 0);
#else
	int omask = sigblock(sigmask(sig));

	if (signal(sig, exit) == SIG_IGN)
		(void)signal(sig, SIG_IGN);
	(void)sigsetmask(omask);
#endif
}

void
done(int status)
{
	int w, wstatus;

	mode(0);
	if (child > 0) {
		/* make sure catch_child does not snap it up */
#if __APPLE__
		struct sigaction sa;
		sigemptyset(&sa.sa_mask);
		sa.sa_handler = SIG_DFL;
		sa.sa_flags = 0;
		(void)sigaction(SIGCHLD, &sa, (struct sigaction *) 0);
#else
		(void)signal(SIGCHLD, SIG_DFL);
#endif
		if (kill(child, SIGKILL) >= 0)
			while ((w = wait(&wstatus)) > 0 && w != child);
	}
	exit(status);
}

int dosigwinch;

/*
 * This is called when the reader process gets the out-of-band (urgent)
 * request to turn on the window-changing protocol.
 */
/* ARGSUSED */
void
writeroob(int signo __unused)
{
	if (dosigwinch == 0) {
		sendwindow();
#ifdef __APPLE__
		struct sigaction sa;
		sigemptyset(&sa.sa_mask);
		sa.sa_handler = sigwinch;
		sa.sa_flags = SA_RESTART;
		(void)sigaction(SIGWINCH, &sa, (struct sigaction *) 0);
#else
		(void)signal(SIGWINCH, sigwinch);
#endif
	}
	dosigwinch = 1;
}

/* ARGSUSED */
void
catch_child(int signo __unused)
{
	pid_t pid;
	int status;

	for (;;) {
		pid = wait3(&status, WNOHANG|WUNTRACED, NULL);
		if (pid == 0)
			return;
		/* if the child (reader) dies, just quit */
		if (pid < 0 || (pid == child && !WIFSTOPPED(status)))
			done(WTERMSIG(status) | WEXITSTATUS(status));
	}
	/* NOTREACHED */
}

/*
 * writer: write to remote: 0 -> line.
 * ~.				terminate
 * ~^Z				suspend rlogin process.
 * ~<delayed-suspend char>	suspend rlogin process, but leave reader alone.
 */
void
writer(void)
{
	int bol, local, n;
	char c;

	bol = 1;			/* beginning of line */
	local = 0;
	for (;;) {
		n = read(STDIN_FILENO, &c, 1);
		if (n <= 0) {
			if (n < 0 && errno == EINTR)
				continue;
			break;
		}
		/*
		 * If we're at the beginning of the line and recognize a
		 * command character, then we echo locally.  Otherwise,
		 * characters are echo'd remotely.  If the command character
		 * is doubled, this acts as a force and local echo is
		 * suppressed.
		 */
		if (bol) {
			bol = 0;
			if (!noescape && c == escapechar) {
				local = 1;
				continue;
			}
		} else if (local) {
			local = 0;
			if (c == '.' || CCEQ(deftty.c_cc[VEOF], c)) {
				echo(c);
				break;
			}
			if (CCEQ(deftty.c_cc[VSUSP], c) ||
			    CCEQ(deftty.c_cc[VDSUSP], c)) {
				bol = 1;
				echo(c);
				stop(c);
				continue;
			}
			if (c != escapechar)
#if defined(KERBEROS) && defined(CRYPT)
				if (doencrypt)
					(void)des_write(rem,
					    (char *)&escapechar, 1);
				else
#endif
				(void)write(rem, &escapechar, 1);
		}

#if defined(KERBEROS) && defined(CRYPT)
		if (doencrypt) {
			if (des_write(rem, &c, 1) == 0) {
				msg("line gone");
				break;
			}
		} else
#endif
		if (write(rem, &c, 1) == 0) {
			msg("line gone");
			break;
		}
		bol = CCEQ(deftty.c_cc[VKILL], c) ||
		    CCEQ(deftty.c_cc[VEOF], c) ||
		    CCEQ(deftty.c_cc[VINTR], c) ||
		    CCEQ(deftty.c_cc[VSUSP], c) ||
		    c == '\r' || c == '\n';
	}
}

void
echo(char c)
{
	char *p;
	char buf[8];

	p = buf;
	c &= 0177;
	*p++ = escapechar;
	if (c < ' ') {
		*p++ = '^';
		*p++ = c + '@';
	} else if (c == 0177) {
		*p++ = '^';
		*p++ = '?';
	} else
		*p++ = c;
	*p++ = '\r';
	*p++ = '\n';
	(void)write(STDOUT_FILENO, buf, p - buf);
}

void
stop(char cmdc)
{
	mode(0);
#ifdef __APPLE__
	struct sigaction sa;

	sigemptyset(&sa.sa_mask);
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = SA_RESTART;
	(void)sigaction(SIGCHLD, &sa, (struct sigaction *) 0);
#else
	(void)signal(SIGCHLD, SIG_IGN);
#endif
	(void)kill(CCEQ(deftty.c_cc[VSUSP], cmdc) ? 0 : getpid(), SIGTSTP);
#ifdef __APPLE__
	sa.sa_handler = catch_child;
	(void)sigaction(SIGCHLD, &sa, (struct sigaction *) 0);
#else
	(void)signal(SIGCHLD, catch_child);
#endif
	mode(1);
	sigwinch(0);			/* check for size changes */
}

/* ARGSUSED */
void
sigwinch(int signo __unused)
{
	struct winsize ws;

	if (dosigwinch && get_window_size(0, &ws) == 0 &&
	    bcmp(&ws, &winsize, sizeof(ws))) {
		winsize = ws;
		sendwindow();
	}
}

/*
 * Send the window size to the server via the magic escape
 */
void
sendwindow(void)
{
	struct winsize *wp;
	char obuf[4 + sizeof (struct winsize)];

	wp = (struct winsize *)(obuf+4);
	obuf[0] = 0377;
	obuf[1] = 0377;
	obuf[2] = 's';
	obuf[3] = 's';
	wp->ws_row = htons(winsize.ws_row);
	wp->ws_col = htons(winsize.ws_col);
	wp->ws_xpixel = htons(winsize.ws_xpixel);
	wp->ws_ypixel = htons(winsize.ws_ypixel);

#if defined(KERBEROS) && defined(CRYPT)
	if(doencrypt)
		(void)des_write(rem, obuf, sizeof(obuf));
	else
#endif
	(void)write(rem, obuf, sizeof(obuf));
}

/*
 * reader: read from remote: line -> 1
 */
#define	READING	1
#define	WRITING	2

jmp_buf rcvtop;
int rcvcnt, rcvstate;
pid_t ppid;
char rcvbuf[8 * 1024];

/* ARGSUSED */
void
oob(int signo __unused)
{
	struct termios tty;
	int atmark, n, rcvd;
	char waste[BUFSIZ], mark;

	rcvd = 0;
	while (recv(rem, &mark, 1, MSG_OOB) < 0) {
		switch (errno) {
		case EWOULDBLOCK:
			/*
			 * Urgent data not here yet.  It may not be possible
			 * to send it yet if we are blocked for output and
			 * our input buffer is full.
			 */
			if (rcvcnt < (int)sizeof(rcvbuf)) {
				n = read(rem, rcvbuf + rcvcnt,
				    sizeof(rcvbuf) - rcvcnt);
				if (n <= 0)
					return;
				rcvd += n;
			} else {
				n = read(rem, waste, sizeof(waste));
				if (n <= 0)
					return;
			}
			continue;
		default:
			return;
		}
	}
	if (mark & TIOCPKT_WINDOW) {
		/* Let server know about window size changes */
		(void)kill(ppid, SIGUSR1);
	}
	if (!eight && (mark & TIOCPKT_NOSTOP)) {
		(void)tcgetattr(0, &tty);
		tty.c_iflag &= ~IXON;
		(void)tcsetattr(0, TCSANOW, &tty);
//		tt.c_iflag &= ~(IXON | IXOFF);
//		tt.c_cc[VSTOP] = _POSIX_VDISABLE;
//		tt.c_cc[VSTART] = _POSIX_VDISABLE;
	}
	if (!eight && (mark & TIOCPKT_DOSTOP)) {
		(void)tcgetattr(0, &tty);
		tty.c_iflag |= (deftty.c_iflag & IXON);
		(void)tcsetattr(0, TCSANOW, &tty);
//		tt.c_iflag |= (IXON|IXOFF);
//		tt.c_cc[VSTOP] = deftt.c_cc[VSTOP];
//		tt.c_cc[VSTART] = deftt.c_cc[VSTART];
	}
	if (mark & TIOCPKT_FLUSHWRITE) {
		(void)tcflush(1, TCIOFLUSH);
		for (;;) {
			if (ioctl(rem, SIOCATMARK, &atmark) < 0) {
				warn("ioctl");
				break;
			}
			if (atmark)
				break;
			n = read(rem, waste, sizeof (waste));
			if (n <= 0)
				break;
		}
		/*
		 * Don't want any pending data to be output, so clear the recv
		 * buffer.  If we were hanging on a write when interrupted,
		 * don't want it to restart.  If we were reading, restart
		 * anyway.
		 */
		rcvcnt = 0;
		longjmp(rcvtop, 1);
	}

	/* oob does not do FLUSHREAD (alas!) */

	/*
	 * If we filled the receive buffer while a read was pending, longjmp
	 * to the top to restart appropriately.  Don't abort a pending write,
	 * however, or we won't know how much was written.
	 */
	if (rcvd && rcvstate == READING)
		longjmp(rcvtop, 1);
}

/* reader: read from remote: line -> 1 */
int
#if __APPLE__
reader(sigset_t *smask)
#else
reader(int omask)
#endif
{
	int n, remaining;
	char *bufp;
	pid_t pid;

	pid = getpid();
#if __APPLE__
	struct sigaction sa;

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = SIG_IGN;
	(void)sigaction(SIGTTOU, &sa, (struct sigaction *) 0);
	sa.sa_handler = oob;
	(void)sigaction(SIGURG, &sa, (struct sigaction *) 0);
	(void)sigaction(SIGUSR1, &sa, (struct sigaction *) 0);
#else
	(void)signal(SIGTTOU, SIG_IGN);
	(void)signal(SIGURG, oob);
	(void)signal(SIGUSR1, oob); /* When propogating SIGURG from parent */
#endif
	ppid = getppid();
	(void)fcntl(rem, F_SETOWN, pid);
	(void)setjmp(rcvtop);
#if __APPLE__
	(void)sigprocmask(SIG_SETMASK, smask, (sigset_t *) 0);
#else
	(void)sigsetmask(omask);
#endif
	bufp = rcvbuf;
	for (;;) {
		while ((remaining = rcvcnt - (bufp - rcvbuf)) > 0) {
			rcvstate = WRITING;
			n = write(STDOUT_FILENO, bufp, remaining);
			if (n < 0) {
				if (errno != EINTR)
					return (-1);
				continue;
			}
			bufp += n;
		}
		bufp = rcvbuf;
		rcvcnt = 0;
		rcvstate = READING;

#if defined(KERBEROS) && defined(CRYPT)
		if (doencrypt)
			rcvcnt = des_read(rem, rcvbuf, sizeof(rcvbuf));
		else
#endif
		rcvcnt = read(rem, rcvbuf, sizeof (rcvbuf));
		if (rcvcnt == 0)
			return (0);
		if (rcvcnt < 0) {
			if (errno == EINTR)
				continue;
			warn("read");
			return (-1);
		}
	}
}

void
mode(int f)
{
	struct termios tty;

	switch (f) {
	case 0:
		(void)tcsetattr(0, TCSANOW, &deftty);
//		(void)tcsetattr(0, TCSADRAIN, &deftty);
		break;
	case 1:
		(void)tcgetattr(0, &deftty);
		tty = deftty;
		/* This is loosely derived from sys/kern/tty_compat.c. */
		tty.c_lflag &= ~(ECHO|ICANON|ISIG|IEXTEN);
		tty.c_iflag &= ~ICRNL;
		tty.c_oflag &= ~OPOST;
		tty.c_cc[VMIN] = 1;
		tty.c_cc[VTIME] = 0;
		if (eight) {
			tty.c_iflag &= IXOFF;
			tty.c_cflag &= ~(CSIZE|PARENB);
			tty.c_cflag |= CS8;
//			tt.c_iflag &= ~(IXON | IXOFF | ISTRIP);
//			tt.c_cc[VSTOP] = _POSIX_VDISABLE;
//			tt.c_cc[VSTART] = _POSIX_VDISABLE;
		}
		(void)tcsetattr(0, TCSANOW, &tty);
//		tcsetattr(0, TCSADRAIN, &tt);
		break;
	default:
		return;
	}
}

/* ARGSUSED */
void
lostpeer(int signo __unused)
{
#ifdef __APPLE__
	struct sigaction sa;

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = SIG_IGN;
	(void)sigaction(SIGPIPE, &sa, (struct sigaction *) 0);
#else
	(void)signal(SIGPIPE, SIG_IGN);
#endif
	msg("\007connection closed");
	done(1);
}

/* copy SIGURGs to the child process via SIGUSR1. */
/* ARGSUSED */
void
copytochild(int signo __unused)
{
	(void)kill(child, SIGUSR1);
}

void
msg(const char *str)
{
	(void)fprintf(stderr, "rlogin: %s\r\n", str);
}

#if defined(KERBEROS)
/* VARARGS */
void
warning(const char *fmt, ...)
{
	va_list ap;

	(void)fprintf(stderr, "rlogin: warning, using standard rlogin: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, ".\n");
}
#endif

void
usage(void)
{
	(void)fprintf(stderr,
#ifdef __APPLE__
	"usage: rlogin [-46%s]%s[-e char] [-i localname] [-l username] [username@]host\n",
#else
	"usage: rlogin [-46%s]%s[-e char] [-i localname] [-l username] host\n",
#endif
#if defined(KERBEROS) && defined(CRYPT)
	    "8EKLx", " [-k realm] ");
#elif defined(KERBEROS)
	    "8EKL", " [-k realm] ");
#else
	    "8DEd", " ");
#endif
	exit(1);
}

u_int
getescape(const char *p)
{
	long val;
	size_t len;

	if ((len = strlen(p)) == 1)	/* use any single char, including '\' */
		return ((u_int)*p);
					/* otherwise, \nnn */
	if (*p == '\\' && len >= 2 && len <= 4) {
		val = strtol(++p, NULL, 8);
		for (;;) {
			if (!*++p)
				return ((u_int)val);
			if (*p < '0' || *p > '8')
				break;
		}
	}
	msg("illegal option value -- e");
	usage();
	/* NOTREACHED */
}
