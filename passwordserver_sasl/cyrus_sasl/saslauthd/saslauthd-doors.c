/* MODULE: saslauthd */

/* COPYRIGHT
 * Copyright (c) 1997-2000 Messaging Direct Ltd.
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
 * THIS SOFTWARE IS PROVIDED BY MESSAGING DIRECT LTD. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL MESSAGING DIRECT LTD. OR
 * ITS EMPLOYEES OR AGENTS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * END COPYRIGHT */

/* OVERVIEW
 * saslauthd provides an interface between the SASL library and various
 * external authentication mechanisms. The primary goal is to isolate
 * code that requires superuser privileges (for example, access to
 * the shadow password file) into a single easily audited module. It
 * can also act as an authentication proxy between plaintext-equivelent
 * authentication schemes (i.e. CRAM-MD5) and more secure authentication
 * services such as Kerberos, although such usage is STRONGLY discouraged
 * because it exposes the strong credentials via the insecure plaintext
 * mechanisms.
 *
 * The program listens for connections on a UNIX domain socket. Access to
 * the service is controlled by the UNIX filesystem permissions on the
 * socket.
 *
 * The service speaks a very simple protocol. The client connects and
 * sends the authentication identifier, the plaintext password, the
 * service name and user realm as counted length strings (a 16-bit
 * unsigned integer in network byte order followed by the string
 * itself). The server returns a single response as a counted length
 * string. The response begins with "OK" or "NO", and is followed by
 * an optional text string (separated from the OK/NO by a single space
 * character), and a NUL. The server then closes the connection.
 *
 * An "OK" response indicates the authentication credentials are valid.
 * A "NO" response indicates the authentication failed.
 *
 * The optional text string may be used to indicate an exceptional
 * condition in the authentication environment that should be communicated
 * to the client.
 * END OVERVIEW */

/* HISTORY
 * saslauthd is a re-implementation of the pwcheck utility included
 * with the CMU Cyrus IMAP server circa 1997. This implementation
 * was written by Lyndon Nerenberg of Messaging Direct Inc. (which
 * at that time was the Esys Corporation) and was included in the
 * company's IMAP message store product (Simeon Message Service) as
 * the smsauthd utility.
 *
 * This implementation was contributed to CMU by Messaging Direct Ltd.
 * in September 2000.
 *
 * September 2001 (Ken Murchison of Oceana Matrix Ltd.):
 * - Modified the protocol to use counted length strings instead of
 *   nul delimited strings.
 * - Augmented the protocol to accept the service name and user realm.
 * END HISTORY */

#ifdef __GNUC__
#ident "$Id: saslauthd-doors.c,v 1.1 2002/05/23 18:58:41 snsimon Exp $"
#endif

/* PUBLIC DEPENDENCIES */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#ifdef _AIX
# include <strings.h>
#endif /* _AIX */
#include <syslog.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <door.h>

#include "mechanisms.h"
#include "globals.h"
/* END PUBLIC DEPENDENCIES */

/* PRIVATE DEPENDENCIES */
/* globals */
authmech_t *authmech;		/* Authentication mechanism we're using  */
authmech_t *proxymech;		/* Auth mechanism to proxy accounts from */
int	debug;			/* Debugging level.                      */
int	flag_use_tod;		/* Pay attention to TOD restrictions.    */
char	*r_host;		/* Remote host for rimap driver		 */
char	*r_service;		/* Remote service for rimap driver	 */
#if defined(AUTH_SIA)
int	g_argc;			/* Copy of argc for sia_* routines	 */
char	**g_argv;		/* Copy of argv for sia_* routines       */
#endif /* AUTH_SIA */
/* path_mux needs to be accessable to server_exit() */
char	*path_mux;		/* path to AF_UNIX socket */
int     master_pid;             /* pid of the initial process */
extern char *optarg;		/* getopt() */

/* forward declarations */
void		do_request(int, int);
void            door_request(void *cookie, char *argp, size_t arg_size,
			     door_desc_t *dp, uint_t n_desc);
RETSIGTYPE 	server_exit(int);
RETSIGTYPE 	sigchld_ignore(int);
void		show_version(void);
/* END PRIVATE DEPENDENCIES */

#define LOCK_FILE_MODE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH)
#define LOCK_SUFFIX ".pid"
#define ACCEPT_LOCK_SUFFIX ".accept"
#define MAX_REQ_LEN 256		/* login/pw/service/realm input buffer size */

#ifdef _AIX
# define SALEN_TYPE size_t
#else /* ! _AIX */
# define SALEN_TYPE int
#endif /* ! _AIX */
/* END PRIVATE DEPENDENCIES */

/* FUNCTION: main */

int
main(
  /* PARAMETERS */
  int argc,				/* I: number of cmdline arguments */
  char *argv[]				/* I: array of cmdline arguments */
  /* END PARAMETERS */
  ) 
{
    /* VARIABLES */
    int c;				/* general purpose character holder */
    int	count;				/* loop counter */
    char *cwd;				/* current working directory path */
    int dfd; /* door file descriptor */
    int lfd,alfd=0;      		/* master lock file descriptor */
    char *lockfile;			/* master lock file name       */
    struct flock lockinfo;		/* fcntl locking on lockfile   */
    int rc;

#if defined(AUTH_SIA)
    /*
     * The doc claims this must be the very first thing executed
     * in main(). The doc also claims that if the kernel test for the
     * security features being loaded fails, the program exits! (OSF)
     */
    set_auth_parameters(argc, argv);
#endif /* AUTH_SIA */

    /* initialization */
    authmech = NULL;
    proxymech = NULL;
    debug = 0;
    flag_use_tod = 0;
    path_mux = PATH_SASLAUTHD_RUNDIR "/mux";
    r_host = NULL;
    openlog("saslauthd", LOG_PID|LOG_NDELAY, LOG_AUTH);
    syslog(LOG_INFO, "START: saslauthd %s", VERSION);

    /* parse the command line arguments */
    while ((c = getopt(argc, argv, "a:dF:H:m:n:P:Tv")) != -1)
	switch (c) {

	  case 'a':			/* authentication mechanism */
	    for (authmech = mechanisms; authmech->name != NULL; authmech++) {
		if (!strcasecmp(authmech->name, optarg))
		    break;
	    }
	    if (authmech->name == NULL) {
		syslog(LOG_ERR,
		       "FATAL: unknown authentication mechanism: %s",
		       optarg);
		fprintf(stderr,
			"saslauthd: unknown authentication mechanism: %s\n",
			optarg);
		exit(1);
	    }
	    break;
		
	  case 'd':			/* enable debugging */
	    debug++;
	    break;

	  case 'H':
	    r_host = strdup(optarg);
	    break;
	    
	  case 'm':			/* alternate MUX location */
	    if (*optarg != '/') {
		syslog(LOG_ERR, "FATAL: -m requires an absolute pathname");
		fprintf(stderr, "saslauthd: -m requires an absolute pathname");
		exit(1);
	    }
	    path_mux = optarg;
	    break;

	  case 'P':			/* proxy authentication mechanism */
	    for (proxymech = mechanisms; proxymech->name != NULL; proxymech++)
	    {
		if (!strcasecmp(proxymech->name, optarg))
		    break;
	    }
	    if (proxymech->name == NULL) {
		syslog(LOG_ERR,
		       "FATAL: unknown authentication mechanism: %s",
		       optarg);
		fprintf(stderr,
			"saslauthd: unknown authentication mechanism %s\n",
			optarg);
		exit(1);
	    }
	    break;
		
	  case 'T':			/* honour time-of-day restrictions */
	    flag_use_tod++;
	    break;

	  case 'v':			/* display version info and exit */
	    show_version();	    
	    exit(0);
	    break;

	  default:
	    break;
	}
#if defined(AUTH_SIA)
    g_argc = argc;
    g_argv = argv;
#endif /* AUTH_SIA */

    umask(077);			/* don't leave readable core dumps */
    signal(SIGPIPE, SIG_IGN);	/* take an EPIPE on write(2) */
    
    /*
     * chdir() into the directory containing the named socket file.
     * This ensures any core dumps don't get clobbered by other programs.
     */

    cwd = strdup(path_mux);
    if (cwd == NULL) {
	syslog(LOG_ERR, "FATAL: strdup(path_mux) failure");
	fprintf(stderr, "saslauthd: strdup(path_mux) failure\n");
	exit(1);
    }
    if (strrchr(cwd, '/') != NULL)
	*(strrchr(cwd, '/')) = '\0';

    if (chdir(cwd) == -1) {
	rc = errno;
	syslog(LOG_ERR, "FATAL: chdir(%s): %m", cwd);
	fprintf(stderr, "saslauthd: ");
	errno = rc;
	perror(cwd);
	exit(1);
    }
    free(cwd);

    if (authmech == NULL) {
	syslog(LOG_ERR, "FATAL: no authentication mechanism specified");
	fprintf(stderr, "saslauthd: no authentication mechanism specified\n");
	exit(1);
    }

    /* sanity check authentication proxy */
    if (proxymech != NULL) {

	if (proxymech == authmech) {
	    syslog(LOG_ERR, "FATAL: -a and -P specify identical mechanisms");
	    fprintf(stderr,
		    "saslauthd: -a and -P specify identical mechanisms\n");
	    exit(1);
	}

	/* : For now we can only create CRAM accounts */
	if (strcasecmp("sasldb", authmech->name)) {
	    syslog(LOG_ERR, "FATAL: %s does not support proxy creation",
		   authmech->name);
	    fprintf(stderr, "saslauthd: %s does not support proxy creation",
		    authmech->name);
	    exit(1);
	}
    }

    /* if we are running in debug mode, do not fork and exit */	
    if (!debug) {
	pid_t pid;

	/* fork/exec/setsid into a new process group */
	count = 5;
	while (count--) {
	    pid = fork();
	    
	    if (pid > 0)
		_exit(0);		/* parent dies */
	    
	    if ((pid == -1) && (errno == EAGAIN)) {
		syslog(LOG_WARNING, "master fork failed (sleeping): %m");
		sleep(5);
		continue;
	    }
	}
	if (pid == -1) {
	    rc = errno;
	    syslog(LOG_ERR, "FATAL: master fork failed: %m");
	    fprintf(stderr, "saslauthd: ");
	    errno = rc;
	    perror("fork");
	    exit(1);
	}

	/*
	 * We're now running in the child. Lose our controlling terminal
	 * and obtain a new process group.
	 */
	if (setsid() == -1) {
	    rc = errno;
	    syslog(LOG_ERR, "FATAL: setsid: %m");
	    fprintf(stderr, "saslauthd: ");
	    errno = rc;
	    perror("setsid");
	    exit(1);
	}
	
	{
	    int s;

	    s = open("/dev/null", O_RDWR, 0);
	    if (s == -1) {
		rc = errno;
		syslog(LOG_ERR, "FATAL: /dev/null: %m");
		fprintf(stderr, "saslauthd: ");
		errno = rc;
		perror("/dev/null");
		exit(1);
		
	    }
	    dup2(s, fileno(stdin));
	    dup2(s, fileno(stdout));
	    dup2(s, fileno(stderr));
	    if (s > 2) {
		close(s);
	    }
	}
    } /* end if(!debug) */

    master_pid = getpid();
    syslog(LOG_INFO, "master PID is: %d", master_pid);

    lockfile = malloc(strlen(path_mux) + sizeof(LOCK_SUFFIX));
    if (lockfile == NULL) {
	syslog(LOG_ERR, "malloc(lockfile) failed");
	exit(1);
    }
    
    strcpy(lockfile, path_mux);
    strcat(lockfile, LOCK_SUFFIX);

    lfd = open(lockfile, O_WRONLY|O_CREAT, LOCK_FILE_MODE);
    if (lfd < 0) {
	syslog(LOG_ERR, "FATAL: %s: %m", lockfile);
	exit(1);
    }
    /* try to get an exclusive lock */
    lockinfo.l_type = F_WRLCK;
    lockinfo.l_start = 0;
    lockinfo.l_len = 0;
    lockinfo.l_whence = SEEK_SET;
    rc = fcntl(lfd, F_SETLK, &lockinfo);
    if (rc == -1) {
	/*
	 * Probably another daemon running. Different systems return
	 * different errno values if the file is already locked so we
	 * can't pretty-print an "another daemon is running" message.
	 */
	syslog(LOG_ERR, "FATAL: setting master lock on %s: %m", lockfile);
	exit(1);
    }
    
    /* write in the process id */
    {
	char pid_buf[100];
	int l;
	
	sprintf(pid_buf, "%lu\n", (unsigned long)getpid());
	l = strlen(pid_buf);
	rc = write(lfd, pid_buf, l);
	if (rc < 0) {
	    syslog(LOG_ERR, "FATAL: %s: %m", lockfile);
	    exit(1);
	} else if (rc != l) {
	    syslog(LOG_ERR, "FATAL: %s: short write (%d != %d)",
		   lockfile, rc, l);
	    exit(1);
	}
    }
		      
    /*
     * Exit handlers must be in place before creating the socket.
     */
    signal(SIGHUP,  server_exit);
    signal(SIGINT,  server_exit);
    signal(SIGTERM, server_exit);

    /* unlink any stray turds from a previous run */
    (void)unlink(path_mux);
    
    /* perform any auth mechanism specific initializations */
    if (authmech->initialize != NULL) {
	if (authmech->initialize() != 0) {
	    syslog(LOG_ERR,
		   "FATAL: %s initialization failed",
		   authmech->name);
	    closelog();
	    exit(1);
	}
    }
    if ((proxymech != NULL) && (proxymech->initialize != NULL)) {
	if (proxymech->initialize() != 0) {
	    syslog(LOG_ERR,
		   "FATAL: %s initialization failed",
		   proxymech->name);
	    closelog();
	    exit(1);
	}
    }

    dfd = door_create(&door_request, NULL, 0);
    if (dfd < 0) {
	syslog(LOG_ERR, "FATAL: door_create failed: %m", path_mux);
	closelog();
	exit(1);
    }
    close(open(path_mux, O_CREAT | O_RDWR, 0666));
    if (fattach(dfd, path_mux) < 0) {
	syslog(LOG_ERR, "FATAL: fattach %s failed: %m", path_mux);
	closelog();
	exit(1);
    }
    /* make sure it's readable */
    if (chmod(path_mux, 0644) < 0) {
	syslog(LOG_ERR, "FATAL: chmod %s 0644 failed: %m", path_mux);
	closelog();
	exit(1);
    }

    for (;;) {
	pause();
    }

    /*NOTREACHED*/
    close(alfd);
    exit(0);
}

/* END FUNCTION: main */

/* FUNCTION: do_request */

/* SYNOPSIS
 * do_request: handle an incoming authentication request.
 *
 *	This function is the I/O interface between the socket
 *	and auth mechanism. It reads the login id, password,
 *	service name and user realm from the socket, calls the
 *	mechanism-specific authentication routine, and sends
 *	the result out to the client.
 * END SYNOPSIS */

void
door_request(void *cookie, char *data, size_t datasize, 
	     door_desc_t *dp, size_t ndesc)
{
    /* VARIABLES */
    char *reply;			/* authentication response message.
					   This is a malloc()ed string that
					   is free()d at the end. If you assign
					   this internally, make sure to
					   strdup() the string you assign. */
    unsigned short count;		/* input/output data byte count */
    char login[MAX_REQ_LEN + 1];	/* account name to authenticate */
    char password[MAX_REQ_LEN + 1];	/* password for authentication */
    char service[MAX_REQ_LEN + 1];	/* service name for authentication */
    char realm[MAX_REQ_LEN + 1];	/* user realm for authentication */
    int error_condition;		/* 1: error occured, can't continue */
    char *dataend = data + datasize;
/* END VARIABLES */

    /* initialization */
    error_condition = 0;
    reply = NULL;

    /*
     * The input data stream consists of the login id, password,
     * service name and user realm as counted length strings.
     * We read() each string, then dispatch the data.
     */
    memcpy(&count, data, sizeof(unsigned short));
    count = ntohs(count);
    data += sizeof(unsigned short);
    if (data + count > dataend || count > MAX_REQ_LEN) {
	goto sizeerror;
    }
    memcpy(login, data, count);
    login[count] = '\0';
    data += count;

    memcpy(&count, data, sizeof(unsigned short));
    count = ntohs(count);
    data += sizeof(unsigned short);
    if (data + count > dataend || count > MAX_REQ_LEN) {
	goto sizeerror;
    }
    memcpy(password, data, count);
    password[count] = '\0';
    data += count;

    memcpy(&count, data, sizeof(unsigned short));
    count = ntohs(count);
    data += sizeof(unsigned short);
    if (data + count > dataend || count > MAX_REQ_LEN) {
	goto sizeerror;
    }
    memcpy(service, data, count);
    service[count] = '\0';
    data += count;

    memcpy(&count, data, sizeof(unsigned short));
    count = ntohs(count);
    data += sizeof(unsigned short);
    if (data + count > dataend || count > MAX_REQ_LEN) {
	goto sizeerror;
    }
    memcpy(realm, data, count);
    realm[count] = '\0';
    data += count;

    if (error_condition) {
    sizeerror:
	/*
	 * One of parameter lengths is too long.
	 * This is probably someone trying to exploit a buffer overflow ...
	 */
	syslog(LOG_ERR,
	       "ALERT: input data exceeds %d bytes! Possible intrusion attempt?",
	       MAX_REQ_LEN);
	reply = strdup("NO");		/* Don't give them any hint as to why*/
					/* this failed, in the hope that they*/
					/* will keep banging on the door long*/
					/* enough for someone to take action.*/

	error_condition = 1;
    }
	    
    if (!error_condition) {
	if ((*login == '\0') || (*password == '\0')) {
	    error_condition = 1;
	    syslog(LOG_NOTICE, "null login/password received");
	    reply = strdup("NO Null login/password (saslauthd)");
	} else {
	    if (debug) {
		syslog(LOG_DEBUG, "authenticating %s", login);
	    }
	}
    }

    if (!error_condition) {
	reply = authmech->authenticate(login, password, service, realm);
	memset(password, 0, strlen(password));

	if (reply == NULL) {
	    error_condition = 1;
	    syslog(LOG_ERR,
		   "AUTHFAIL: mechanism %s doesn't grok this environment",
		   authmech->name);
	    reply = strdup("NO authentication mechanism failed to cope! (saslauthd)");
	}
    }

    if (!strncmp(reply, "NO", sizeof("NO")-1)) {
	if (strlen(reply) < sizeof("NO "))
	    syslog(LOG_WARNING, "AUTHFAIL: user=%s service=%s realm=%s",
		   login, service, realm);
	else
	    syslog(LOG_WARNING, "AUTHFAIL: user=%s service=%s realm=%s [%s]",
		   login, service, realm, reply + 3);
    } else {
	if (debug) {
	    syslog(LOG_INFO, "OK: user=%s service=%s realm=%s",
		   login, service, realm);
	}
    }

    door_return(reply, strlen(reply), NULL, 0);

    free(reply);
    return;
}

/* END FUNCTION: do_request */


/* FUNCTION: show_version */

/* SYNOPSIS
 * print the program version number on stderr, then exit.
 * END SYNOPSIS */

void					/* R: none */
show_version(
  /* PARAMETERS */
  void					/* I: none */
  /* END PARAMETERS */
  )
{
    /* VARIABLES */
    authmech_t *authmech;		/* authmech table entry pointer */
    /* END VARIABLES */
    
    fprintf(stderr, "saslauthd %s\nauthentication mechanisms:", 
            VERSION);
    for (authmech = mechanisms; authmech->name != NULL; authmech++) {
	fprintf(stderr, " %s", authmech->name);
    }
    fputs("\n", stderr);
    exit(0);
    /* NOTREACHED */
}

/* END FUNCTION: show_version */

/* FUNCTION: server_exit */

/* SYNOPSIS
 * Terminate the server upon receipt of a signal.
 * END SYNOPSIS */

RETSIGTYPE				/* R: OS dependent */
server_exit(
  /* PARAMETERS */
  int sig				/* I: signal number */
  /* END PARAMETERS */
  )
{
    if(getpid() == master_pid) {
	syslog(LOG_NOTICE,
	       "Caught signal %d. Cleaning up and terminating.", sig);
	kill(-master_pid, sig);
    }

    exit(0);
    /* NOTREACHED */
}

/* END FUNCTION: server_exit */

/* FUNCTION: sigchld_ignore */

/* SYNOPSIS
 * Reap process status from terminated children.
 * END SYNOPSIS */

RETSIGTYPE				/* R: OS dependent */
sigchld_ignore (
  /* PARAMETERS */
  int sig __attribute__((unused))	/* I: signal number */
  /* END PARAMETERS */
  )
{
    /* VARIABLES */
    pid_t pid;				/* process id from waitpid() */
    /* END VARIABLES */

    while ((pid = waitpid(-1, 0, WNOHANG)) > 0) {
	/*
	 * We don't do anything with the results from waitpid(), however
	 * we still need to call it to prevent terminated children from
	 * becoming zombies and filling the proc table.
	 */
    }
    /* Re-load the signal handler. */
    signal(SIGCHLD, sigchld_ignore);
    
#if RETSIGTYPE == void
    return;
#else
    return 0;
#endif
}

/* END FUNCTION: sigchld_ignore */
/* FUNCTION: retry_read */

/* SYNOPSIS
 * Keep calling the read() system call with 'fd', 'buf', and 'nbyte'
 * until all the data is read in or an error occurs.
 * END SYNOPSIS */
int retry_read(int fd, void *buf, unsigned nbyte)
{
    int n;
    int nread = 0;

    if (nbyte == 0) return 0;

    for (;;) {
	n = read(fd, buf, nbyte);
	if (n == 0) {
	    /* end of file */
	    return -1;
	}
	if (n == -1) {
	    if (errno == EINTR) continue;
	    return -1;
	}

	nread += n;

	if (n >= (int) nbyte) return nread;

	buf += n;
	nbyte -= n;
    }
}

/* END FUNCTION: retry_read */

/* FUNCTION: retry_writev */

/* SYNOPSIS
 * Keep calling the writev() system call with 'fd', 'iov', and 'iovcnt'
 * until all the data is written out or an error occurs.
 * END SYNOPSIS */

int				/* R: bytes written, or -1 on error */
retry_writev (
  /* PARAMETERS */
  int fd,				/* I: fd to write on */
  struct iovec *iov,			/* U: iovec array base
					 *    modified as data written */
  int iovcnt				/* I: number of iovec entries */
  /* END PARAMETERS */
  )
{
    /* VARIABLES */
    int n;				/* return value from writev() */
    int i;				/* loop counter */
    int written;			/* bytes written so far */
    static int iov_max;			/* max number of iovec entries */
    /* END VARIABLES */

    /* initialization */
#ifdef MAXIOV
    iov_max = MAXIOV;
#else /* ! MAXIOV */
# ifdef IOV_MAX
    iov_max = IOV_MAX;
# else /* ! IOV_MAX */
    iov_max = 8192;
# endif /* ! IOV_MAX */
#endif /* ! MAXIOV */
    written = 0;
    
    for (;;) {

	while (iovcnt && iov[0].iov_len == 0) {
	    iov++;
	    iovcnt--;
	}

	if (!iovcnt) {
	    return written;
	}

	n = writev(fd, iov, iovcnt > iov_max ? iov_max : iovcnt);
	if (n == -1) {
	    if (errno == EINVAL && iov_max > 10) {
		iov_max /= 2;
		continue;
	    }
	    if (errno == EINTR) {
		continue;
	    }
	    return -1;
	} else {
	    written += n;
	}

	for (i = 0; i < iovcnt; i++) {
	    if (iov[i].iov_len > (unsigned) n) {
		iov[i].iov_base = (char *)iov[i].iov_base + n;
		iov[i].iov_len -= n;
		break;
	    }
	    n -= iov[i].iov_len;
	    iov[i].iov_len = 0;
	}

	if (i == iovcnt) {
	    return written;
	}
    }
    /* NOTREACHED */
}

/* END FUNCTION: retry_writev */
