/*-
 * Copyright (c) 1988, 1989, 1992, 1993, 1994
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
static const char copyright[] =
"@(#) Copyright (c) 1988, 1989, 1992, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static const char sccsid[] = "@(#)rshd.c	8.2 (Berkeley) 4/6/94";
#endif
#endif /* not lint */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/libexec/rshd/rshd.c,v 1.51 2005/05/11 02:41:39 jmallett Exp $");

/*
 * remote shell server:
 *	[port]\0
 *	ruser\0
 *	luser\0
 *	command\0
 *	data
 */
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#ifndef __APPLE__
#include <libutil.h>
#endif
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#ifndef __APPLE__
#include <login_cap.h>
#endif

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#if !TARGET_OS_EMBEDDED
#include <security/pam_appl.h>
#include <security/openpam.h>
#endif

#include <sys/wait.h>

#if !TARGET_OS_EMBEDDED
static struct pam_conv pamc = { openpam_nullconv, NULL };
static pam_handle_t *pamh;
static int pam_err;
#endif

#define PAM_END { \
	if ((pam_err = pam_setcred(pamh, PAM_DELETE_CRED)) != PAM_SUCCESS) \
		syslog(LOG_ERR|LOG_AUTH, "pam_setcred(): %s", pam_strerror(pamh, pam_err)); \
	if ((pam_err = pam_close_session(pamh,0)) != PAM_SUCCESS) \
		syslog(LOG_ERR|LOG_AUTH, "pam_close_session(): %s", pam_strerror(pamh, pam_err)); \
	if ((pam_err = pam_end(pamh, pam_err)) != PAM_SUCCESS) \
		syslog(LOG_ERR|LOG_AUTH, "pam_end(): %s", pam_strerror(pamh, pam_err)); \
}

int	keepalive = 1;
int	check_all;
int	log_success;		/* If TRUE, log all successful accesses */
int	sent_null;
int	no_delay;

#ifdef __APPLE__
#define __printf0like(a,b)
#endif

void	 doit(struct sockaddr *);
static void	 rshd_errx(int, const char *, ...) __printf0like(2, 3);
void	 getstr(char *, int, const char *);
int	 local_domain(char *);
char	*topdomain(char *);
void	 usage(void);

char	 slash[] = "/";
char	 bshell[] = _PATH_BSHELL;

#if defined(KERBEROS)
#include <kerberosIV/des.h>
#include <kerberosIV/krb.h>
#define	VERSION_SIZE	9
#define SECURE_MESSAGE  "This rsh session is using DES encryption for all transmissions.\r\n"
#define	OPTIONS		"alnkvxL"
char	authbuf[sizeof(AUTH_DAT)];
char	tickbuf[sizeof(KTEXT_ST)];
int	doencrypt, use_kerberos, vacuous;
Key_schedule	schedule;
#else
#define	OPTIONS	"aDLln"
#endif

int
main(int argc, char *argv[])
{
	extern int __check_rhosts_file;
	struct linger linger;
	socklen_t fromlen;
	int ch, on = 1;
	struct sockaddr_storage from;

	openlog("rshd", LOG_PID | LOG_ODELAY, LOG_DAEMON);

	opterr = 0;
	while ((ch = getopt(argc, argv, OPTIONS)) != -1)
		switch (ch) {
		case 'a':
			/* ignored for compatibility */
			check_all = 1;
			break;
		case 'l':
			__check_rhosts_file = 0;
			break;
		case 'n':
			keepalive = 0;
			break;
#if defined(KERBEROS)
		case 'k':
			use_kerberos = 1;
			break;
		case 'v':
			vacuous = 1;
			break;
#if defined(CRYPT)
		case 'x':
			doencrypt = 1;
			break;
#endif /* CRYPT */
#endif /* KERBEROS */
		case 'D':
			no_delay = 1;
			break;
		case 'L':
			log_success = 1;
			break;
		case '?':
		default:
			usage();
			break;
		}

	argc -= optind;
	argv += optind;

#if defined(KERBEROS)
	if (use_kerberos && vacuous) {
		syslog(LOG_ERR, "only one of -k and -v allowed");
		exit(2);
	}
#if defined(CRYPT)
	if (doencrypt && !use_kerberos) {
		syslog(LOG_ERR, "-k is required for -x");
		exit(2);
	}
#endif /* CRYPT */
#endif /* KERBEROS */

	fromlen = sizeof (from);
	if (getpeername(0, (struct sockaddr *)&from, &fromlen) < 0) {
		syslog(LOG_ERR, "getpeername: %m");
		exit(1);
	}
	if (keepalive &&
	    setsockopt(0, SOL_SOCKET, SO_KEEPALIVE, (char *)&on,
	    sizeof(on)) < 0)
		syslog(LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");
	linger.l_onoff = 1;
	linger.l_linger = 60;			/* XXX */
	if (setsockopt(0, SOL_SOCKET, SO_LINGER, (char *)&linger,
	    sizeof (linger)) < 0)
		syslog(LOG_WARNING, "setsockopt (SO_LINGER): %m");
	if (no_delay &&
	    setsockopt(0, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) < 0)
		syslog(LOG_WARNING, "setsockopt (TCP_NODELAY): %m");
	doit((struct sockaddr *)&from);
	/* NOTREACHED */
	return(0);
}

#ifdef __APPLE__
char	username[20] = "USER=";
char	homedir[64] = "HOME=";
char	shell[64] = "SHELL=";
char	path[100] = "PATH=";
char	*envinit[] =
	    {homedir, shell, path, username, 0};
#endif
extern char **environ;

void
doit(struct sockaddr *fromp)
{
	extern char *__rcmd_errstr;	/* syslog hook from libc/net/rcmd.c. */
	struct passwd *pwd;
	u_short port;
	fd_set ready, readfrom;
	int cc, fd, nfd, pv[2], pid, s;
	int one = 1;
	const char *cp, *errorstr;
	char sig, buf[BUFSIZ];
	char *cmdbuf, luser[16], ruser[16];
	char rhost[2 * MAXHOSTNAMELEN + 1];
	char numericname[INET6_ADDRSTRLEN];
	int af, srcport;
	int maxcmdlen;
#ifndef __APPLE__
	login_cap_t *lc;
#else
	struct hostent *hp;
	char *hostname, *errorhost = NULL;
#endif

	maxcmdlen = (int)sysconf(_SC_ARG_MAX);
	if (maxcmdlen <= 0 || (cmdbuf = malloc(maxcmdlen)) == NULL)
		exit(1);

#if defined(KERBEROS)
	AUTH_DAT	*kdata = (AUTH_DAT *) NULL;
	KTEXT		ticket = (KTEXT) NULL;
	char		instance[INST_SZ], version[VERSION_SIZE];
	struct		sockaddr_in	fromaddr;
	int		rc;
	long		authopts;
	int		pv1[2], pv2[2];
	fd_set		wready, writeto;

	fromaddr = *fromp;
#endif /* KERBEROS */

	(void) signal(SIGINT, SIG_DFL);
	(void) signal(SIGQUIT, SIG_DFL);
	(void) signal(SIGTERM, SIG_DFL);
	af = fromp->sa_family;
	srcport = ntohs(*((in_port_t *)&fromp->sa_data));
	if (af == AF_INET) {
		inet_ntop(af, &((struct sockaddr_in *)fromp)->sin_addr,
		    numericname, sizeof numericname);
	} else if (af == AF_INET6) {
		inet_ntop(af, &((struct sockaddr_in6 *)fromp)->sin6_addr,
		    numericname, sizeof numericname);
	} else {
		syslog(LOG_ERR, "malformed \"from\" address (af %d)", af);
		exit(1);
	}
#ifdef IP_OPTIONS
	if (af == AF_INET) {
		u_char optbuf[BUFSIZ/3];
		socklen_t optsize = sizeof(optbuf), ipproto, i;
		struct protoent *ip;

		if ((ip = getprotobyname("ip")) != NULL)
			ipproto = ip->p_proto;
		else
			ipproto = IPPROTO_IP;
		if (!getsockopt(0, ipproto, IP_OPTIONS, optbuf, &optsize) &&
		    optsize != 0) {
			for (i = 0; i < optsize; ) {
				u_char c = optbuf[i];
				if (c == IPOPT_LSRR || c == IPOPT_SSRR) {
					syslog(LOG_NOTICE,
					    "connection refused from %s with IP option %s",
					    numericname,
					    c == IPOPT_LSRR ? "LSRR" : "SSRR");
					exit(1);
				}
				if (c == IPOPT_EOL)
					break;
				i += (c == IPOPT_NOP) ? 1 : optbuf[i+1];
			}
		}
	}
#endif

#if defined(KERBEROS)
	if (!use_kerberos)
#endif
	if (srcport >= IPPORT_RESERVED ||
	    srcport < IPPORT_RESERVED/2) {
		syslog(LOG_NOTICE|LOG_AUTH,
		    "connection from %s on illegal port %u",
		    numericname,
		    srcport);
		exit(1);
	}

	(void) alarm(60);
	port = 0;
	s = 0;		/* not set or used if port == 0 */
	for (;;) {
		char c;
		if ((cc = read(STDIN_FILENO, &c, 1)) != 1) {
			if (cc < 0)
				syslog(LOG_NOTICE, "read: %m");
			shutdown(0, SHUT_RDWR);
			exit(1);
		}
		if (c == 0)
			break;
		port = port * 10 + c - '0';
	}

	(void) alarm(0);
	if (port != 0) {
		int lport = IPPORT_RESERVED - 1;
		s = rresvport_af(&lport, af);
		if (s < 0) {
			syslog(LOG_ERR, "can't get stderr port: %m");
			exit(1);
		}
#if defined(KERBEROS)
		if (!use_kerberos)
#endif
		if (port >= IPPORT_RESERVED ||
		    port < IPPORT_RESERVED/2) {
			syslog(LOG_NOTICE|LOG_AUTH,
			    "2nd socket from %s on unreserved port %u",
			    numericname,
			    port);
			exit(1);
		}
		*((in_port_t *)&fromp->sa_data) = htons(port);
		if (connect(s, fromp, fromp->sa_len) < 0) {
			syslog(LOG_INFO, "connect second port %d: %m", port);
			exit(1);
		}
	}

#if defined(KERBEROS)
	if (vacuous) {
		error("rshd: remote host requires Kerberos authentication\n");
		exit(1);
	}
#endif

	errorstr = NULL;
#ifndef __APPLE__
	realhostname_sa(rhost, sizeof(rhost) - 1, fromp, fromp->sa_len);
	rhost[sizeof(rhost) - 1] = '\0';
	/* XXX truncation! */
#else
	errorstr = NULL;
	hp = gethostbyaddr((char *)&((struct sockaddr_in *)fromp)->sin_addr, sizeof (struct in_addr),
		((struct sockaddr_in *)fromp)->sin_family);
	if (hp) {
		/*
		 * If name returned by gethostbyaddr is in our domain,
		 * attempt to verify that we haven't been fooled by someone
		 * in a remote net; look up the name and check that this
		 * address corresponds to the name.
		 */
		hostname = hp->h_name;
#if defined(KERBEROS)
		if (!use_kerberos)
#endif
		if (check_all || local_domain(hp->h_name)) {
			strncpy(rhost, hp->h_name, sizeof(rhost) - 1);
			rhost[sizeof(rhost) - 1] = 0;
			errorhost = rhost;
			hp = gethostbyname(rhost);
			if (hp == NULL) {
				syslog(LOG_INFO,
				    "Couldn't look up address for %s",
				    rhost);
				errorstr =
				"Couldn't look up address for your host (%s)\n";
				hostname = inet_ntoa(((struct sockaddr_in *)fromp)->sin_addr);
			} else for (; ; hp->h_addr_list++) {
				if (hp->h_addr_list[0] == NULL) {
					syslog(LOG_NOTICE,
					  "Host addr %s not listed for host %s",
					    inet_ntoa(((struct sockaddr_in *)fromp)->sin_addr),
					    hp->h_name);
					errorstr =
					    "Host address mismatch for %s\n";
					hostname = inet_ntoa(((struct sockaddr_in *)fromp)->sin_addr);
					break;
				}
				if (!bcmp(hp->h_addr_list[0],
				    (caddr_t)&((struct sockaddr_in *)fromp)->sin_addr,
				    sizeof(((struct sockaddr_in *)fromp)->sin_addr))) {
					hostname = hp->h_name;
					break;
				}
			}
		}
	} else
		errorhost = hostname = inet_ntoa(((struct sockaddr_in *)fromp)->sin_addr);

#if defined(KERBEROS)
	if (use_kerberos) {
		kdata = (AUTH_DAT *) authbuf;
		ticket = (KTEXT) tickbuf;
		authopts = 0L;
		strcpy(instance, "*");
		version[VERSION_SIZE - 1] = '\0';
#if defined(CRYPT)
		if (doencrypt) {
			struct sockaddr_in local_addr;
			rc = sizeof(local_addr);
			if (getsockname(0, (struct sockaddr *)&local_addr,
			    &rc) < 0) {
				syslog(LOG_ERR, "getsockname: %m");
				error("rshd: getsockname: %m");
				exit(1);
			}
			authopts = KOPT_DO_MUTUAL;
			rc = krb_recvauth(authopts, 0, ticket,
				"rcmd", instance, &fromaddr,
				&local_addr, kdata, "", schedule,
				version);
			des_set_key(kdata->session, schedule);
		} else
#endif /* CRYPT */
			rc = krb_recvauth(authopts, 0, ticket, "rcmd",
				instance, &fromaddr,
				(struct sockaddr_in *) 0,
				kdata, "", (bit_64 *) 0, version);
		if (rc != KSUCCESS) {
			error("Kerberos authentication failure: %s\n",
				  krb_err_txt[rc]);
			exit(1);
		}
	} else
#endif /* KERBEROS */
#endif

	(void) alarm(60);
	getstr(ruser, sizeof(ruser), "ruser");
	getstr(luser, sizeof(luser), "luser");
	getstr(cmdbuf, maxcmdlen, "command");
	(void) alarm(0);
#if !TARGET_OS_EMBEDDED
	pam_err = pam_start("rshd", luser, &pamc, &pamh);
	if (pam_err != PAM_SUCCESS) {
		syslog(LOG_ERR|LOG_AUTH, "pam_start(): %s",
		    pam_strerror(pamh, pam_err));
		rshd_errx(1, "Login incorrect.");
	}

	if ((pam_err = pam_set_item(pamh, PAM_RUSER, ruser)) != PAM_SUCCESS ||
	    (pam_err = pam_set_item(pamh, PAM_RHOST, rhost) != PAM_SUCCESS)) {
		syslog(LOG_ERR|LOG_AUTH, "pam_set_item(): %s",
		    pam_strerror(pamh, pam_err));
		rshd_errx(1, "Login incorrect.");
	}

	pam_err = pam_authenticate(pamh, 0);
	if (pam_err == PAM_SUCCESS) {
		if ((pam_err = pam_get_user(pamh, &cp, NULL)) == PAM_SUCCESS) {
			strncpy(luser, cp, sizeof(luser));
			luser[sizeof(luser) - 1] = '\0';
			/* XXX truncation! */
		}
		pam_err = pam_acct_mgmt(pamh, 0);
	}
	if (pam_err != PAM_SUCCESS) {
		syslog(LOG_INFO|LOG_AUTH,
		    "%s@%s as %s: permission denied (%s). cmd='%.80s'",
		    ruser, rhost, luser, pam_strerror(pamh, pam_err), cmdbuf);
		rshd_errx(1, "Login incorrect.");
	}
#endif
	setpwent();
	pwd = getpwnam(luser);
	if (pwd == NULL) {
		syslog(LOG_INFO|LOG_AUTH,
		    "%s@%s as %s: unknown login. cmd='%.80s'",
		    ruser, rhost, luser, cmdbuf);
		if (errorstr == NULL)
			errorstr = "Login incorrect.";
		rshd_errx(1, errorstr, rhost);
	}

#ifndef __APPLE__
	lc = login_getpwclass(pwd);
	if (pwd->pw_uid)
		auth_checknologin(lc);
#endif

	if (chdir(pwd->pw_dir) < 0) {
		if (chdir("/") < 0 ||
#ifndef __APPLE__
		    login_getcapbool(lc, "requirehome", !!pwd->pw_uid)) {
#else
			0) {
#endif /* __APPLE__ */
#ifdef notdef
			syslog(LOG_INFO|LOG_AUTH,
			"%s@%s as %s: no home directory. cmd='%.80s'",
			ruser, rhost, luser, cmdbuf);
			rshd_errx(0, "No remote home directory.");
#endif
		}
		pwd->pw_dir = slash;
	}

#if defined(KERBEROS)
	if (use_kerberos) {
		if (pwd->pw_passwd != 0 && *pwd->pw_passwd != '\0') {
			if (kuserok(kdata, luser) != 0) {
				syslog(LOG_INFO|LOG_AUTH,
				    "Kerberos rsh denied to %s.%s@%s",
				    kdata->pname, kdata->pinst, kdata->prealm);
				error("Permission denied.\n");
				exit(1);
			}
		}
	} else
#endif

#ifdef __APPLE__
		if (errorstr ||
		    (pwd->pw_passwd != 0 && *pwd->pw_passwd != '\0' &&
		    iruserok(((struct sockaddr_in *)fromp)->sin_addr.s_addr,
#if TARGET_OS_EMBEDDED
	// rdar://problem/5381734
		    0,
#else
		    pwd->pw_uid == 0,
#endif
		    ruser, luser) < 0)) {
			if (__rcmd_errstr)
			syslog(LOG_INFO|LOG_AUTH,
			    "%s@%s as %s: permission denied (%s). cmd='%.80s'",
			    ruser, rhost, luser, __rcmd_errstr,
				    cmdbuf);
			else
				syslog(LOG_INFO|LOG_AUTH,
			    "%s@%s as %s: permission denied. cmd='%.80s'",
				    ruser, rhost, luser, cmdbuf);
			if (errorstr == NULL)
				errorstr = "Permission denied.";
			rshd_errx(1, errorstr, errorhost);
		}

	if (pwd->pw_uid && !access(_PATH_NOLOGIN, F_OK)) {
		rshd_errx(1, "Logins currently disabled.");
	}
#else
	if (lc != NULL && fromp->sa_family == AF_INET) {	/*XXX*/
		char	remote_ip[MAXHOSTNAMELEN];

		strncpy(remote_ip, numericname,
			sizeof(remote_ip) - 1);
		remote_ip[sizeof(remote_ip) - 1] = 0;
		/* XXX truncation! */
		if (!auth_hostok(lc, rhost, remote_ip)) {
			syslog(LOG_INFO|LOG_AUTH,
			    "%s@%s as %s: permission denied (%s). cmd='%.80s'",
			    ruser, rhost, luser, __rcmd_errstr,
			    cmdbuf);
			rshd_errx(1, "Login incorrect.");
		}
		if (!auth_timeok(lc, time(NULL)))
			rshd_errx(1, "Logins not available right now");
	}

	/*
	 * PAM modules might add supplementary groups in
	 * pam_setcred(), so initialize them first.
	 * But we need to open the session as root.
	 */
	if (setusercontext(lc, pwd, pwd->pw_uid, LOGIN_SETGROUP) != 0) {
		syslog(LOG_ERR, "setusercontext: %m");
		exit(1);
	}
#endif /* !__APPLE__ */

#if !TARGET_OS_EMBEDDED
	if ((pam_err = pam_open_session(pamh, 0)) != PAM_SUCCESS) {
		syslog(LOG_ERR, "pam_open_session: %s", pam_strerror(pamh, pam_err));
	} else if ((pam_err = pam_setcred(pamh, PAM_ESTABLISH_CRED)) != PAM_SUCCESS) {
		syslog(LOG_ERR, "pam_setcred: %s", pam_strerror(pamh, pam_err));
	}
#endif
	(void) write(STDERR_FILENO, "\0", 1);
	sent_null = 1;

	if (port) {
		if (pipe(pv) < 0)
			rshd_errx(1, "Can't make pipe.");
#if defined(KERBEROS) && defined(CRYPT)
		if (doencrypt) {
			if (pipe(pv1) < 0)
				rshd_errx(1, "Can't make 2nd pipe.");
			if (pipe(pv2) < 0)
				rshd_errx(1, "Can't make 3rd pipe.");
		}
#endif /* KERBEROS && CRYPT */
		pid = fork();
		if (pid == -1)
			rshd_errx(1, "Can't fork; try again.");
		if (pid) {
#if defined(KERBEROS) && defined(CRYPT)
			if (doencrypt) {
				static char msg[] = SECURE_MESSAGE;
				(void) close(pv1[1]);
				(void) close(pv2[1]);
				des_write(s, msg, sizeof(msg) - 1);

			} else
#endif /* KERBEROS && CRYPT */
			(void) close(0);
			(void) close(1);
			(void) close(2);
			(void) close(pv[1]);

			FD_ZERO(&readfrom);
			FD_SET(s, &readfrom);
			FD_SET(pv[0], &readfrom);
			if (pv[0] > s)
				nfd = pv[0];
			else
				nfd = s;
#if defined(KERBEROS) && defined(CRYPT)
			if (doencrypt) {
				FD_ZERO(&writeto);
				FD_SET(pv2[0], &writeto);
				FD_SET(pv1[0], &readfrom);

				nfd = MAX(nfd, pv2[0]);
				nfd = MAX(nfd, pv1[0]);
			} else
#endif /* KERBEROS && CRYPT */
				ioctl(pv[0], FIONBIO, (char *)&one);

			/* should set s nbio! */
			nfd++;
			do {
				ready = readfrom;
#if defined(KERBEROS) && defined(CRYPT)
				if (doencrypt) {
					wready = writeto;
					if (select(nfd, &ready,
					    &wready, (fd_set *) 0,
					    (struct timeval *) 0) < 0)
						break;
				} else
#endif /* KERBEROS && CRYPT */
				if (select(nfd, &ready, (fd_set *)0,
				  (fd_set *)0, (struct timeval *)0) < 0)
					break;
				if (FD_ISSET(s, &ready)) {
					int	ret;
#if defined(KERBEROS) && defined(CRYPT)
					if (doencrypt)
						ret = des_read(s, &sig, 1);
					else
#endif /* KERBEROS && CRYPT */
						ret = read(s, &sig, 1);
				if (ret <= 0)
					FD_CLR(s, &readfrom);
				else
					killpg(pid, sig);
				}
				if (FD_ISSET(pv[0], &ready)) {
					errno = 0;
					cc = read(pv[0], buf, sizeof(buf));
					if (cc <= 0) {
						shutdown(s, SHUT_RDWR);
						FD_CLR(pv[0], &readfrom);
					} else {
#if defined(KERBEROS) && defined(CRYPT)
						if (doencrypt)
							(void)
							  des_write(s, buf, cc);
						else
#endif /* KERBEROS && CRYPT */
						(void)write(s, buf, cc);
					}
				}
#if defined(KERBEROS) && defined(CRYPT)
				if (doencrypt && FD_ISSET(pv1[0], &ready)) {
					errno = 0;
					cc = read(pv1[0], buf, sizeof(buf));
					if (cc <= 0) {
						shutdown(pv1[0], 1+1);
						FD_CLR(pv1[0], &readfrom);
					} else
						(void) des_write(STDOUT_FILENO,
						    buf, cc);
				}

				if (doencrypt && FD_ISSET(pv2[0], &wready)) {
					errno = 0;
					cc = des_read(STDIN_FILENO,
					    buf, sizeof(buf));
					if (cc <= 0) {
						shutdown(pv2[0], 1+1);
						FD_CLR(pv2[0], &writeto);
					} else
						(void) write(pv2[0], buf, cc);
				}
#endif /* KERBEROS && CRYPT */

			} while (FD_ISSET(s, &readfrom) ||
#if defined(KERBEROS) && defined(CRYPT)
			    (doencrypt && FD_ISSET(pv1[0], &readfrom)) ||
#endif /* KERBEROS && CRYPT */
			    FD_ISSET(pv[0], &readfrom));
#if !TARGET_OS_EMBEDDED
			PAM_END;
#endif
			exit(0);
		}
#ifdef __APPLE__
		// rdar://problem/4485794
		setpgid(0, getpid());
#endif
		(void) close(s);
		(void) close(pv[0]);
#if defined(KERBEROS) && defined(CRYPT)
		if (doencrypt) {
			close(pv1[0]); close(pv2[0]);
			dup2(pv1[1], 1);
			dup2(pv2[1], 0);
			close(pv1[1]);
			close(pv2[1]);
		}
#endif /* KERBEROS && CRYPT */
		dup2(pv[1], 2);
		close(pv[1]);
	}
#ifndef __APPLE__
	else {
		pid = fork();
		if (pid == -1)
			rshd_errx(1, "Can't fork; try again.");
		if (pid) {
			/* Parent. */
			while (wait(NULL) > 0 || errno == EINTR)
				/* nothing */ ;
			PAM_END;
			exit(0);
		}
	}
#endif

	for (fd = getdtablesize(); fd > 2; fd--)
		(void) close(fd);
	if (setsid() == -1)
		syslog(LOG_ERR, "setsid() failed: %m");
	if (setlogin(pwd->pw_name) < 0)
		syslog(LOG_ERR, "setlogin() failed: %m");

	if (*pwd->pw_shell == '\0')
		pwd->pw_shell = bshell;
#ifdef __APPLE__
	(void) setgid((gid_t)pwd->pw_gid);
	initgroups(pwd->pw_name, pwd->pw_gid);
	(void) setuid((uid_t)pwd->pw_uid);

	environ = envinit;
	strncat(homedir, pwd->pw_dir, sizeof(homedir)-6);
	strcat(path, _PATH_DEFPATH);
	strncat(shell, pwd->pw_shell, sizeof(shell)-7);
	strncat(username, pwd->pw_name, sizeof(username)-6);
#endif
#if !TARGET_OS_EMBEDDED
	(void) pam_setenv(pamh, "HOME", pwd->pw_dir, 1);
	(void) pam_setenv(pamh, "SHELL", pwd->pw_shell, 1);
	(void) pam_setenv(pamh, "USER", pwd->pw_name, 1);
	(void) pam_setenv(pamh, "PATH", _PATH_DEFPATH, 1);
	environ = pam_getenvlist(pamh);
	(void) pam_end(pamh, pam_err);
#endif
	cp = strrchr(pwd->pw_shell, '/');
	if (cp)
		cp++;
	else
		cp = pwd->pw_shell;

#ifndef __APPLE__
	if (setusercontext(lc, pwd, pwd->pw_uid,
		LOGIN_SETALL & ~LOGIN_SETGROUP) < 0) {
		syslog(LOG_ERR, "setusercontext(): %m");
		exit(1);
	}
	login_close(lc);
#endif
	endpwent();
	if (log_success || pwd->pw_uid == 0) {
#if defined(KERBEROS)
		if (use_kerberos)
		    syslog(LOG_INFO|LOG_AUTH,
			"Kerberos shell from %s.%s@%s on %s as %s, cmd='%.80s'",
			kdata->pname, kdata->pinst, kdata->prealm,
			hostname, luser, cmdbuf);
		else
#endif /* KERBEROS */
		    syslog(LOG_INFO|LOG_AUTH, "%s@%s as %s: cmd='%.80s'",
			ruser, rhost, luser, cmdbuf);
	}
	execl(pwd->pw_shell, cp, "-c", cmdbuf, (char *)NULL);
	err(1, "%s", pwd->pw_shell);
	exit(1);
}

/*
 * Report error to client.  Note: can't be used until second socket has
 * connected to client, or older clients will hang waiting for that
 * connection first.
 */

static void
rshd_errx(int errcode, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	if (sent_null == 0)
		write(STDERR_FILENO, "\1", 1);

	verrx(errcode, fmt, ap);
	/* NOTREACHED */
}

void
getstr(char *buf, int cnt, const char *error)
{
	char c;

	do {
		if (read(STDIN_FILENO, &c, 1) != 1)
			exit(1);
		*buf++ = c;
		if (--cnt == 0)
			rshd_errx(1, "%s too long", error);
	} while (c != 0);
}

/*
 * Check whether host h is in our local domain,
 * defined as sharing the last two components of the domain part,
 * or the entire domain part if the local domain has only one component.
 * If either name is unqualified (contains no '.'),
 * assume that the host is local, as it will be
 * interpreted as such.
 */
int
local_domain(char *h)
{
	char localhost[MAXHOSTNAMELEN];
	char *p1, *p2;

	localhost[0] = 0;
	(void) gethostname(localhost, sizeof(localhost) - 1);
	localhost[sizeof(localhost) - 1] = '\0';
	/* XXX truncation! */
	p1 = topdomain(localhost);
	p2 = topdomain(h);
	if (p1 == NULL || p2 == NULL || !strcasecmp(p1, p2))
		return (1);
	return (0);
}

char *
topdomain(char *h)
{
	char *p, *maybe = NULL;
	int dots = 0;

	for (p = h + strlen(h); p >= h; p--) {
		if (*p == '.') {
			if (++dots == 2)
				return (p);
			maybe = p;
		}
	}
	return (maybe);
}

void
usage(void)
{

	syslog(LOG_ERR, "usage: rshd [-%s]", OPTIONS);
	exit(2);
}
