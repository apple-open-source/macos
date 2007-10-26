/*
 * Copyright (c) 1999, 2004, 2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*-
 * Copyright (c) 1980, 1987, 1988, 1991, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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

#if 0
static char copyright[] =
"@(#) Copyright (c) Apple Computer, Inc. 1997\n\n";
#endif

/*
 * login [ name ]
 * login -h hostname	(for telnetd, etc.)
 * login -f name	(for pre-authenticated login: datakit, xterm, etc.)
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <ttyent.h>
#include <tzfile.h>
#include <unistd.h>
#ifdef USE_PAM
#include <utmpx.h>
#else /* !USE_PAM */
#include <utmp.h>
#endif /* USE_PAM */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <TargetConditionals.h>

#ifdef USE_BSM
#include <bsm/libbsm.h>
#include <bsm/audit_uevents.h>
#endif

#ifdef USE_PAM
#include <pam/pam_appl.h>
#include <pam/pam_misc.h>
#endif

#include <mach/mach_types.h>

#include <servers/bootstrap.h>

#include "pathnames.h"

void	 badlogin __P((char *));
void	 checknologin __P((void));
void	 dolastlog __P((int));
void	 getloginname __P((void));
void	 motd __P((void));
int	 rootterm __P((char *));
void	 sigint __P((int));
void	 sleepexit __P((int));
char	*stypeof __P((char *));
void	 timedout __P((int));
#ifdef KERBEROS
int	 klogin __P((struct passwd *, char *, char *, char *));
#endif
void  au_success();
void  au_fail(char *, int);


#ifndef USE_PAM
extern void login __P((struct utmp *));
#endif /* !USE_PAM */
static void bail(int, int);
static void refused(const char *, const char *, int);

#define	TTYGRPNAME	"tty"		/* name of group to own ttys */
#define NO_SLEEP_EXIT	0
#define SLEEP_EXIT	5

/*
 * This bounds the time given to login.  Not a define so it can
 * be patched on machines where it's too small.
 */
u_int	timeout = 300;

#ifdef KERBEROS
int	notickets = 1;
char	*instance;
char	*krbtkfile_env;
int	authok;
#endif

struct	passwd *pwd;
int	failures;
char	term[64], *hostname, *username = NULL, *tty;
#ifdef USE_PAM
static pam_handle_t *pamh = NULL;
static struct pam_conv conv = { misc_conv, NULL };
static int pam_err;
static int pam_silent = PAM_SILENT;
static int pam_cred_established;
static int pam_session_established;
static struct lastlogx lastlog;
#endif /* USE_PAM */
int hflag;

#ifdef USE_BSM
#define NA_EVENT_STR_SIZE 25
au_tid_t tid;
#endif

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char **environ;
	struct group *gr;
	struct stat st;
#ifndef USE_PAM
	struct utmp utmp;
#endif /* USE_PAM */
	int ask, ch, cnt, oflag = 0, fflag, lflag, pflag;
	int quietlog = 0, rootlogin = 0;
	uid_t uid;
	uid_t euid;
	gid_t egid;
	char *domain, *p, *ttyn;
	char tbuf[MAXPATHLEN + 2], tname[sizeof(_PATH_TTY) + 10];
	char localhost[MAXHOSTNAMELEN];
#ifdef USE_PAM
	char **pmenv;
	pid_t pid;
#else
	int rval;
	char *salt;
#endif

	char auditsuccess = 1;

	(void)signal(SIGALRM, timedout);
	(void)alarm(timeout);
	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)setpriority(PRIO_PROCESS, 0, 0);

	openlog("login", LOG_ODELAY, LOG_AUTH);

	/*
	 * -p is used by getty to tell login not to destroy the environment
	 * -f is used to skip a second login authentication
	 *    -l is used to indicate a login session to the command that
	 *    follows username
	 * -h is used by other servers to pass the name of the remote
	 *    host to login so that it may be placed in utmp and wtmp
	 * -q is used to force hushlogin
	 */
	domain = NULL;
	if (gethostname(localhost, sizeof(localhost)) < 0)
		syslog(LOG_ERR, "couldn't get local hostname: %m");
	else
		domain = strchr(localhost, '.');
	
	euid = geteuid();
	egid = getegid();

	fflag = hflag = lflag = pflag = 0;
	uid = getuid();
	while ((ch = getopt(argc, argv, "1fh:lpq")) != EOF)
		switch (ch) {
		case '1':
			oflag = 1;
			break;
		case 'f':
			fflag = 1;
			break;
		case 'h':
			if (uid)
				errx(1, "-h option: %s", strerror(EPERM));
			hflag = 1;
			if (domain && (p = strchr(optarg, '.')) &&
			    strcasecmp(p, domain) == 0)
				*p = 0;
			hostname = optarg;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'p':
			pflag = 1;
			break;
		case 'q':
			quietlog = 1;
			break;
		case '?':
		default:
			if (!uid)
				syslog(LOG_ERR, "invalid flag %c", ch);
			(void)fprintf(stderr,
			    "usage: login [-pq] [-h hostname] [username]\n");
			(void)fprintf(stderr,
			    "       login -f [-lpq] [-h hostname] [username [prog [arg ...]]]\n");
			exit(1);
		}
	argc -= optind;
	argv += optind;

	if (*argv) {
		username = *argv++;
		ask = 0;
	} else
		ask = 1;

	for (cnt = getdtablesize(); cnt > 2; cnt--)
		(void)close(cnt);

	ttyn = ttyname(STDIN_FILENO);
	if (ttyn == NULL || *ttyn == '\0') {
		(void)snprintf(tname, sizeof(tname), "%s??", _PATH_TTY);
		ttyn = tname;
	}
	if (tty = strrchr(ttyn, '/'))
		++tty;
	else
		tty = ttyn;

#ifdef USE_BSM
	/* Set the terminal id */
	audit_set_terminal_id(&tid);
	if (fstat(STDIN_FILENO, &st) < 0) {
		fprintf(stderr, "login: Unable to stat terminal\n");
		au_fail("Unable to stat terminal", 1);
		exit(-1);
	}
	if (S_ISCHR(st.st_mode)) {
		tid.port = st.st_rdev;
	} else {
		tid.port = 0;
	}
#endif

#ifdef USE_PAM
	pam_err = pam_start("login", username, &conv, &pamh);
	if( pam_err != PAM_SUCCESS ) {
		fprintf(stderr, "login: PAM Error (line %d):  %s\n", __LINE__, pam_strerror(pamh, pam_err));
		au_fail("PAM Error", 1);
		exit(1);
	}
	pam_err = pam_set_item(pamh, PAM_TTY, tty);
	if( pam_err != PAM_SUCCESS ) {
		fprintf(stderr, "login: PAM Error (line %d): %s\n", __LINE__, pam_strerror(pamh, pam_err));
		au_fail("PAM Error", 1);
		exit(1);
	}

	pam_err = pam_set_item(pamh, PAM_RHOST, hostname);
	if( pam_err != PAM_SUCCESS ) {
		fprintf(stderr, "login: PAM Error (line %d): %s\n", __LINE__, pam_strerror(pamh, pam_err));
		au_fail("PAM Error", 1);
		exit(1);
	}

	pam_err = pam_set_item(pamh, PAM_USER_PROMPT, "login: ");
	if( pam_err != PAM_SUCCESS ) {
		fprintf(stderr, "login: PAM Error (line %d): %s\n", __LINE__, pam_strerror(pamh, pam_err));
		au_fail("PAM Error", 1);
		exit(1);
	}

	if( !username )
		getloginname();
	pam_set_item(pamh, PAM_USER, username);
	pwd = getpwnam(username);
	if( (pwd != NULL) && (pwd->pw_uid == 0) )
		rootlogin = 1;

	if( (pwd != NULL) && fflag && ((uid == 0) || (uid == pwd->pw_uid)) ){
		pam_err = 0;
		auditsuccess = 0; /* we've simply opened a terminal window */
	} else {

		pam_err = pam_authenticate(pamh, 0);
		while( (!oflag) && (cnt++ < 10) && ((pam_err == PAM_AUTH_ERR) ||
				(pam_err == PAM_USER_UNKNOWN) ||
				(pam_err == PAM_CRED_INSUFFICIENT) ||
				(pam_err == PAM_AUTHINFO_UNAVAIL))) {
			/* 
			 * we are not exiting here, but this corresponds to 
		 	 * a failed login event, so set exitstatus to 1 
			 */
			au_fail("Login incorrect", 1);
			badlogin(username);
			printf("Login incorrect\n");
			rootlogin = 0;
			getloginname();
			pwd = getpwnam(username);
			if( (pwd != NULL) && (pwd->pw_uid == 0) )
				rootlogin = 1;
			pam_set_item(pamh, PAM_USER, username);
			pam_err = pam_authenticate(pamh, 0);
		}

		if( pam_err != PAM_SUCCESS ) {
			pam_get_item(pamh, PAM_USER, (void *)&username);
			badlogin(username);
			printf("Login incorrect\n");
			au_fail("Login incorrect", 1);
			exit(1);
		}

		pam_err = pam_acct_mgmt(pamh, 0);
		if( pam_err == PAM_NEW_AUTHTOK_REQD ) {
			pam_err = pam_chauthtok(pamh, PAM_CHANGE_EXPIRED_AUTHTOK);
		}
		if( pam_err != PAM_SUCCESS ) {
			fprintf(stderr, "login: PAM Error (line %d): %s\n", __LINE__, pam_strerror(pamh, pam_err));
			au_fail("PAM error", 1);
			exit(1);
		}
	}

	pam_err = pam_get_item(pamh, PAM_USER, (void *)&username);
	if( (pam_err == PAM_SUCCESS) && username && *username) 
		pwd = getpwnam(username);

	/* get lastlog info before PAM make a new entry */
	if (!quietlog)
		getlastlogxbyname(username, &lastlog);

	pam_err = pam_open_session(pamh, 0);
	if( pam_err != PAM_SUCCESS ) {
		fprintf(stderr, "login: PAM Error (line %d): %s\n", __LINE__, pam_strerror(pamh, pam_err));
		au_fail("PAM error", 1);
		exit(1);
	}

	pam_err = pam_setcred(pamh, PAM_ESTABLISH_CRED);
	if( pam_err != PAM_SUCCESS ) {
		fprintf(stderr, "login: PAM Error (line %d): %s\n", __LINE__, pam_strerror(pamh, pam_err));
		au_fail("PAM error", 1);
		exit(1);
	}

#else /* USE_PAM */
	for (cnt = 0;; ask = 1) {
		if (ask) {
			fflag = 0;
			getloginname();
		}
		rootlogin = 0;
#ifdef	KERBEROS
		if ((instance = strchr(username, '.')) != NULL) {
			if (strncmp(instance, ".root", 5) == 0)
				rootlogin = 1;
			*instance++ = '\0';
		} else
			instance = "";
#endif
		if (strlen(username) > UT_NAMESIZE)
			username[UT_NAMESIZE] = '\0';

		/*
		 * Note if trying multiple user names; log failures for
		 * previous user name, but don't bother logging one failure
		 * for nonexistent name (mistyped username).
		 */
		if (failures && strcmp(tbuf, username)) {
			if (failures > (pwd ? 0 : 1)) {
				badlogin(tbuf);
			}
			failures = 0;
		}
		(void)strcpy(tbuf, username);

		if (pwd = getpwnam(username))
			salt = pwd->pw_passwd;
		else
			salt = "xx";

		/*
		 * if we have a valid account name, and it doesn't have a
		 * password, or the -f option was specified and the caller
		 * is root or the caller isn't changing their uid, don't
		 * authenticate.
		 */
		if (pwd && (*pwd->pw_passwd == '\0' ||
		    fflag && (uid == 0 || uid == pwd->pw_uid)))
			break;
		fflag = 0;
		if (pwd && pwd->pw_uid == 0)
			rootlogin = 1;

		(void)setpriority(PRIO_PROCESS, 0, -4);

		p = getpass("Password:");

		if (pwd) {
#ifdef KERBEROS
			rval = klogin(pwd, instance, localhost, p);
			if (rval != 0 && rootlogin && pwd->pw_uid != 0)
				rootlogin = 0;
			if (rval == 0)
				authok = 1;
			else if (rval == 1)
				rval = strcmp(crypt(p, salt), pwd->pw_passwd);
#else
			rval = strcmp(crypt(p, salt), pwd->pw_passwd);
#endif
		}
		memset(p, 0, strlen(p));

		(void)setpriority(PRIO_PROCESS, 0, 0);

		/*
		 * If trying to log in as root without Kerberos,
		 * but with insecure terminal, refuse the login attempt.
		 */
#ifdef KERBEROS
		if (authok == 0)
#endif
		if (pwd && rootlogin && !rootterm(tty)) {
			(void)fprintf(stderr,
			    "%s login refused on this terminal.\n",
			    pwd->pw_name);
			if (hostname)
				syslog(LOG_NOTICE,
				    "LOGIN %s REFUSED FROM %s ON TTY %s",
				    pwd->pw_name, hostname, tty);
			else
				syslog(LOG_NOTICE,
				    "LOGIN %s REFUSED ON TTY %s",
				     pwd->pw_name, tty);
			au_fail("Login refused on terminal", 0);
			continue;
		}

		if (pwd && !rval)
			break;

		(void)printf("Login incorrect\n");
		failures++;
		/* we allow 10 tries, but after 3 we start backing off */
		if (++cnt > 3) {
			if (cnt >= 10) {
				badlogin(username);
				au_fail("Login incorrect", 1);
				sleepexit(1);
			}
			au_fail("Login incorrect", 1);
			sleep((u_int)((cnt - 3) * 5));
		}
	}
#endif

	/* committed to login -- turn off timeout */
	(void)alarm((u_int)0);

	endpwent();

	if (!pwd) {
		fprintf(stderr, "login: Unable to find user: %s\n", username);
		exit(1);
	}

	/* if user not super-user, check for disabled logins */
	if (!rootlogin)
		checknologin();

	/* Audit successful login */
	if (auditsuccess)
		au_success();

	setegid(pwd->pw_gid);
	seteuid(rootlogin ? 0 : pwd->pw_uid);

	if (!lflag) {
		/* First do a stat in case the homedir is automounted */
		stat(pwd->pw_dir,&st);

		if (!*pwd->pw_dir || chdir(pwd->pw_dir) < 0) {
	    		printf("No home directory %s!\n", pwd->pw_dir);
	    		if (chdir("/")) 
	      			refused("Cannot find root directory", "HOMEDIR", 1);
	   	 	pwd->pw_dir = strdup("/");
	    		if (pwd->pw_dir == NULL) {
				syslog(LOG_NOTICE, "strdup(): %m");
				bail(SLEEP_EXIT, 1);
			}
			printf("Logging in with home = \"/\".\n");
		}
	}
	seteuid(euid);
	setegid(egid);

	if (!quietlog)
		quietlog = access(_PATH_HUSHLOGIN, F_OK) == 0;

	/* Nothing else left to fail -- really log in. */
#ifndef USE_PAM
	memset((void *)&utmp, 0, sizeof(utmp));
	(void)time(&utmp.ut_time);
	(void)strncpy(utmp.ut_name, username, sizeof(utmp.ut_name));
	if (hostname)
		(void)strncpy(utmp.ut_host, hostname, sizeof(utmp.ut_host));
	(void)strncpy(utmp.ut_line, tty, sizeof(utmp.ut_line));
	login(&utmp);
#endif /* USE_PAM */

	dolastlog(quietlog);

	(void)chown(ttyn, pwd->pw_uid,
	    (gr = getgrnam(TTYGRPNAME)) ? gr->gr_gid : pwd->pw_gid);
	(void)chmod(ttyn, 0620);
	(void)setgid(pwd->pw_gid);

	if (initgroups(username, pwd->pw_gid) == -1)
		syslog(LOG_ERR, "login: initgroups() failed");

	if (*pwd->pw_shell == '\0')
		pwd->pw_shell = strdup(_PATH_BSHELL);
        if (pwd->pw_shell == NULL) {
                syslog(LOG_NOTICE, "strdup(): %m");
                bail(SLEEP_EXIT, 1);
        }

#if TARGET_OS_EMBEDDED
	/* on embedded, allow a shell to live in /var/debug_mount/bin/sh */
#define _PATH_DEBUGSHELL	"/var/debug_mount/bin/sh"
        if (stat(pwd->pw_shell, &st) != 0) {
        	if (stat(_PATH_DEBUGSHELL, &st) == 0) {
        		pwd->pw_shell = strdup(_PATH_DEBUGSHELL);
        	}
        }
#endif

	/* Destroy environment unless user has requested its preservation. */
	if (!pflag) {
		environ = malloc(sizeof(char *));
		*environ = NULL;
	}
	(void)setenv("HOME", pwd->pw_dir, 1);
	(void)setenv("SHELL", pwd->pw_shell, 1);
	if (term[0] == '\0')
		(void)strncpy(term, stypeof(tty), sizeof(term));
	(void)setenv("TERM", term, 0);
	(void)setenv("LOGNAME", pwd->pw_name, 1);
	(void)setenv("USER", pwd->pw_name, 1);
	(void)setenv("PATH", _PATH_DEFPATH, 0);
#ifdef KERBEROS
	if (krbtkfile_env)
		(void)setenv("KRBTKFILE", krbtkfile_env, 1);
#endif

#ifdef USE_PAM
	pmenv = pam_getenvlist(pamh);
	for( cnt = 0; pmenv && pmenv[cnt]; cnt++ ) 
		putenv(pmenv[cnt]);

	/* Ignore SIGHUP so that the parent's call to waitpid will
	   succeed and the tty ownership can be reset. */
	(void)signal(SIGHUP, SIG_IGN);

	pid = fork();
	if ( pid < 0 ) {
		err(1, "fork");
	} else if( pid != 0 ) {
		waitpid(pid, NULL, 0);
		pam_setcred(pamh, PAM_DELETE_CRED);
		pam_err = pam_close_session(pamh, 0);
		pam_end(pamh,pam_err);
		chown(ttyn, 0, 0);
		chmod(ttyn, 0666);
		exit(0);
	}

	/* Restore the default SIGHUP handler for the child. */
	(void)signal(SIGHUP, SIG_DFL);

#endif

	if (tty[sizeof("tty")-1] == 'd')
		syslog(LOG_INFO, "DIALUP %s, %s", tty, pwd->pw_name);

	/* If fflag is on, assume caller/authenticator has logged root login. */
	if (rootlogin && fflag == 0)
		if (hostname)
			syslog(LOG_NOTICE, "ROOT LOGIN (%s) ON %s FROM %s",
			    username, tty, hostname);
		else
			syslog(LOG_NOTICE, "ROOT LOGIN (%s) ON %s", username, tty);

#ifdef KERBEROS
	if (!quietlog && notickets == 1)
		(void)printf("Warning: no Kerberos tickets issued.\n");
#endif

	if (!quietlog) {
		motd();
		(void)snprintf(tbuf,
		    sizeof(tbuf), "%s/%s", _PATH_MAILDIR, pwd->pw_name);
		if (stat(tbuf, &st) == 0 && st.st_size != 0)
			(void)printf("You have %smail.\n",
			    (st.st_mtime > st.st_atime) ? "new " : "");
	}

	(void)signal(SIGALRM, SIG_DFL);
	(void)signal(SIGQUIT, SIG_DFL);
	(void)signal(SIGINT, SIG_DFL);
	(void)signal(SIGTSTP, SIG_IGN);

	if (setlogin(pwd->pw_name) < 0)
		syslog(LOG_ERR, "setlogin() failure: %m");

	/* Discard permissions last so can't get killed and drop core. */
	if (rootlogin)
		(void) setuid(0);
	else
		(void) setuid(pwd->pw_uid);

	/* Re-enable crash reporter */
	do {
		kern_return_t kr;
		mach_port_t bp, ep, mts;
		thread_state_flavor_t flavor = 0;

#if defined(__ppc__)
		flavor = PPC_THREAD_STATE64;
#elif defined(__i386__)
		flavor = x86_THREAD_STATE;
#endif

		mts = mach_task_self();

		kr = task_get_bootstrap_port(mts, &bp);
		if (kr != KERN_SUCCESS) {
		  syslog(LOG_ERR, "task_get_bootstrap_port() failure: %s (%d)",
			bootstrap_strerror(kr), kr);
		  break;
		}

		const char* bs = "com.apple.ReportCrash";
		kr = bootstrap_look_up(bp, (char*)bs, &ep);
		if (kr != KERN_SUCCESS) {
		  syslog(LOG_ERR, "bootstrap_look_up(%s) failure: %s (%d)",
			bs, bootstrap_strerror(kr), kr);
		  break;
		}

		kr = task_set_exception_ports(mts, EXC_MASK_CRASH, ep, EXCEPTION_STATE_IDENTITY | MACH_EXCEPTION_CODES, flavor);
		if (kr != KERN_SUCCESS) {
		  syslog(LOG_ERR, "task_set_exception_ports() failure: %d", kr);
		  break;
		}
	} while (0);

	if (fflag && *argv) {
		char *arg0 = *argv;
		if (lflag)
			(void)strlcpy(tbuf, (p = strrchr(*argv, '/')) ?
			    p + 1 : *argv, sizeof(tbuf));
		else {
			tbuf[0] = '-';
			(void)strlcpy(tbuf + 1, (p = strrchr(*argv, '/')) ?
			    p + 1 : *argv, sizeof(tbuf) - 1);
		}
		*argv = tbuf;
		execvp(arg0, argv);
		err(1, "%s", arg0);
	} else {
		if (lflag)
			(void)strlcpy(tbuf, (p = strrchr(pwd->pw_shell, '/')) ?
			    p + 1 : pwd->pw_shell, sizeof(tbuf));
		else {
			tbuf[0] = '-';
			(void)strlcpy(tbuf + 1, (p = strrchr(pwd->pw_shell, '/')) ?
			    p + 1 : pwd->pw_shell, sizeof(tbuf) - 1);
		}
		execlp(pwd->pw_shell, tbuf, (char *)NULL);
		err(1, "%s", pwd->pw_shell);
	}
}

#ifdef	KERBEROS
#define	NBUFSIZ		(MAXLOGNAME + 1 + 5)	/* .root suffix */
#else
#define	NBUFSIZ		(MAXLOGNAME + 1)
#endif

/*
 * The following tokens are included in the audit record for successful login attempts
 * header
 * subject
 * return
 */ 
void au_success()
{
#ifdef USE_BSM
	token_t *tok;
	int aufd;
	au_mask_t aumask;
	auditinfo_t auinfo;
	uid_t uid = pwd->pw_uid;
	gid_t gid = pwd->pw_gid;
	pid_t pid = getpid();
	long au_cond;

	/* If we are not auditing, don't cut an audit record; just return */
 	if (auditon(A_GETCOND, &au_cond, sizeof(long)) < 0) {
		fprintf(stderr, "login: Could not determine audit condition\n");
		exit(1);
	}
	if (au_cond == AUC_NOAUDIT)
		return;

	/* Compute and Set the user's preselection mask */ 
	if(au_user_mask(pwd->pw_name, &aumask) == -1) {
		fprintf(stderr, "login: Could not set audit mask\n");
		exit(1);
	}

	/* Set the audit info for the user */
	auinfo.ai_auid = uid;
	auinfo.ai_asid = pid;
	bcopy(&tid, &auinfo.ai_termid, sizeof(auinfo.ai_termid));
	bcopy(&aumask, &auinfo.ai_mask, sizeof(auinfo.ai_mask));
	if(setaudit(&auinfo) != 0) {
		fprintf(stderr, "login: setaudit failed:  %s\n", strerror(errno));
		exit(1);
	}

	if((aufd = au_open()) == -1) {
		fprintf(stderr, "login: Audit Error: au_open() failed\n");
		exit(1);
	}

	/* The subject that is created (euid, egid of the current process) */
	if((tok = au_to_subject32(uid, geteuid(), getegid(), 
			uid, gid, pid, pid, &tid)) == NULL) {
		fprintf(stderr, "login: Audit Error: au_to_subject32() failed\n");
		exit(1);
	}
	au_write(aufd, tok);

	if((tok = au_to_return32(0, 0)) == NULL) {
		fprintf(stderr, "login: Audit Error: au_to_return32() failed\n");
		exit(1);
	}
	au_write(aufd, tok);

	if(au_close(aufd, 1, AUE_login) == -1) {
		fprintf(stderr, "login: Audit Record was not committed.\n");
		exit(1);
	}
#endif
}

/*
 * The following tokens are included in the audit record for successful login attempts
 * header
 * subject
 * text
 * return
 */ 
void au_fail(char *errmsg, int na)
{
#ifdef USE_BSM
	token_t *tok;
	int aufd;
	long au_cond;
	uid_t uid;
	gid_t gid;
	pid_t pid = getpid();

	/* If we are not auditing, don't cut an audit record; just return */
 	if (auditon(A_GETCOND, &au_cond, sizeof(long)) < 0) {
		fprintf(stderr, "login: Could not determine audit condition\n");
		exit(1);
	}
	if (au_cond == AUC_NOAUDIT)
		return;

	if((aufd = au_open()) == -1) {
		fprintf(stderr, "login: Audit Error: au_open() failed\n");
		exit(1);
	}

	if(na) {
		/* Non attributable event */
		/* Assuming that login is not called within a users' session => auid,asid == -1 */
		if((tok = au_to_subject32(-1, geteuid(), getegid(), -1, -1, 
				pid, -1, &tid)) == NULL) {

			fprintf(stderr, "login: Audit Error: au_to_subject32() failed\n");
			exit(1);
		}
	}
	else {
		/* we know the subject -- so use its value instead */
		uid = pwd->pw_uid;
		gid = pwd->pw_gid;
		if((tok = au_to_subject32(uid, geteuid(), getegid(), 
				uid, gid, pid, pid, &tid)) == NULL) {
			fprintf(stderr, "login: Audit Error: au_to_subject32() failed\n");
			exit(1);
		}
	}
	au_write(aufd, tok);

	/* Include the error message */
	if((tok = au_to_text(errmsg)) == NULL) {
		fprintf(stderr, "login: Audit Error: au_to_text() failed\n");
		exit(1);
	}
	au_write(aufd, tok);

	if((tok = au_to_return32(1, errno)) == NULL) {
		fprintf(stderr, "login: Audit Error: au_to_return32() failed\n");
		exit(1);
	}
	au_write(aufd, tok);

	if(au_close(aufd, 1, AUE_login) == -1) {
		fprintf(stderr, "login: Audit Error: au_close()  was not committed\n");
		exit(1);
	}
#endif
}

void
getloginname()
{
	int ch;
	char *p;
	static char nbuf[NBUFSIZ];

	for (;;) {
		(void)printf("login: ");
		for (p = nbuf; (ch = getchar()) != '\n'; ) {
			if (ch == EOF) {
				badlogin(username);
				exit(0);
			}
			if (p < nbuf + (NBUFSIZ - 1))
				*p++ = ch;
		}
		if (p > nbuf) {
			if (nbuf[0] == '-')
				(void)fprintf(stderr,
				    "login names may not start with '-'.\n");
			else {
				*p = '\0';
				username = nbuf;
				break;
			}
		}
	}
}

int
rootterm(ttyn)
	char *ttyn;
{
	struct ttyent *t;

	return ((t = getttynam(ttyn)) && t->ty_status & TTY_SECURE);
}

jmp_buf motdinterrupt;

void
motd()
{
	int fd, nchars;
	sig_t oldint;
	char tbuf[8192];

	if ((fd = open(_PATH_MOTDFILE, O_RDONLY, 0)) < 0)
		return;
	oldint = signal(SIGINT, sigint);
	if (setjmp(motdinterrupt) == 0)
		while ((nchars = read(fd, tbuf, sizeof(tbuf))) > 0)
			(void)write(fileno(stdout), tbuf, nchars);
	(void)signal(SIGINT, oldint);
	(void)close(fd);
}

/* ARGSUSED */
void
sigint(signo)
	int signo;
{

	longjmp(motdinterrupt, 1);
}

/* ARGSUSED */
void
timedout(signo)
	int signo;
{

	(void)fprintf(stderr, "Login timed out after %d seconds\n", timeout);
	exit(0);
}

void
checknologin()
{
	int fd, nchars;
	char tbuf[8192];

	if ((fd = open(_PATH_NOLOGIN, O_RDONLY, 0)) >= 0) {
		while ((nchars = read(fd, tbuf, sizeof(tbuf))) > 0)
			(void)write(fileno(stdout), tbuf, nchars);
		au_fail("No login", 0);
		sleepexit(0);
	}
}

void
dolastlog(quiet)
	int quiet;
{
#ifdef USE_PAM
	if (quiet)
		return;
	if (*lastlog.ll_line) {
		(void)printf("Last login: %.*s ",
		    24-5, (char *)ctime(&lastlog.ll_tv.tv_sec));
		if (*lastlog.ll_host != '\0')
			(void)printf("from %.*s\n",
			    (int)sizeof(lastlog.ll_host),
			    lastlog.ll_host);
		else
			(void)printf("on %.*s\n",
			    (int)sizeof(lastlog.ll_line),
			    lastlog.ll_line);
	}
#else /* !USE_PAM */
	struct lastlog ll;
	int fd;

	/* HACK HACK HACK: This is because HFS doesn't support sparse files
	 * and seeking into the file too far is too slow.  The "solution"
	 * is to just bail if the seek time for a large uid would be too
	 * slow.
	 */
	if(pwd->pw_uid > 100000) {
		syslog(LOG_NOTICE, "User login %s (%d) not logged in lastlog.  UID too large.", pwd->pw_name, pwd->pw_uid);
		return;
	}

	if ((fd = open(_PATH_LASTLOG, O_RDWR, 0)) >= 0) {
		(void)lseek(fd, (off_t)pwd->pw_uid * sizeof(ll), L_SET);
		if (!quiet) {
			if (read(fd, (char *)&ll, sizeof(ll)) == sizeof(ll) &&
			    ll.ll_time != 0) {
				(void)printf("Last login: %.*s ",
				    24-5, (char *)ctime(&ll.ll_time));
				if (*ll.ll_host != '\0')
					(void)printf("from %.*s\n",
					    (int)sizeof(ll.ll_host),
					    ll.ll_host);
				else
					(void)printf("on %.*s\n",
					    (int)sizeof(ll.ll_line),
					    ll.ll_line);
			}
			(void)lseek(fd, (off_t)pwd->pw_uid * sizeof(ll), L_SET);
		}
		memset((void *)&ll, 0, sizeof(ll));
		(void)time(&ll.ll_time);
		(void)strncpy(ll.ll_line, tty, sizeof(ll.ll_line));
		if (hostname)
			(void)strncpy(ll.ll_host, hostname, sizeof(ll.ll_host));
		(void)write(fd, (char *)&ll, sizeof(ll));
		(void)close(fd);
	}
#endif /* USE_PAM */
}

void
badlogin(name)
	char *name;
{

	if (failures == 0)
		return;
	if (hostname) {
		syslog(LOG_NOTICE, "%d LOGIN FAILURE%s FROM %s",
		    failures, failures > 1 ? "S" : "", hostname);
		syslog(LOG_AUTHPRIV|LOG_NOTICE,
		    "%d LOGIN FAILURE%s FROM %s, %s",
		    failures, failures > 1 ? "S" : "", hostname, name);
	} else {
		syslog(LOG_NOTICE, "%d LOGIN FAILURE%s ON %s",
		    failures, failures > 1 ? "S" : "", tty);
		syslog(LOG_AUTHPRIV|LOG_NOTICE,
		    "%d LOGIN FAILURE%s ON %s, %s",
		    failures, failures > 1 ? "S" : "", tty, name);
	}
}

#undef	UNKNOWN
#define	UNKNOWN	"su"

char *
stypeof(ttyid)
	char *ttyid;
{
	struct ttyent *t;

	return (ttyid && (t = getttynam(ttyid)) ? t->ty_type : UNKNOWN);
}

void
sleepexit(eval)
	int eval;
{

	(void)sleep(5);
	exit(eval);
}

static void
refused(const char *msg, const char *rtype, int lout)
{

        if (msg != NULL)
		printf("%s.\n", msg);
        if (hflag)
                syslog(LOG_NOTICE, "LOGIN %s REFUSED (%s) FROM %s ON TTY %s",
		       pwd->pw_name, rtype, hostname, tty);
        else
                syslog(LOG_NOTICE, "LOGIN %s REFUSED (%s) ON TTY %s",
		       pwd->pw_name, rtype, tty);
        if (lout)
                bail(SLEEP_EXIT, 1);
}

#ifdef USE_PAM
/*
 * Log a PAM error
 */
static void
pam_syslog(const char *msg)
{
        syslog(LOG_ERR, "%s: %s", msg, pam_strerror(pamh, pam_err));
}

/*
 * Shut down PAM
 */
static void
pam_cleanup()
{

        if (pamh != NULL) {
                if (pam_session_established) {
                        pam_err = pam_close_session(pamh, 0);
                        if (pam_err != PAM_SUCCESS)
                                pam_syslog("pam_close_session()");
                }
                pam_session_established = 0;
                if (pam_cred_established) {
                        pam_err = pam_setcred(pamh, pam_silent|PAM_DELETE_CRED);
                        if (pam_err != PAM_SUCCESS)
                                pam_syslog("pam_setcred()");
                }
                pam_cred_established = 0;
                pam_end(pamh, pam_err);
                pamh = NULL;
        }
}
#endif /* USE_PAM */
/*
 * Exit, optionally after sleeping a few seconds
 */
void
bail(int sec, int eval)
{
#ifdef USE_PAM
        pam_cleanup();
#endif
        (void)sleep(sec);
        exit(eval);
}
