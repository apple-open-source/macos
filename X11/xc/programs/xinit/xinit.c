/* $Xorg: xinit.c,v 1.5 2001/02/09 02:05:49 xorgcvs Exp $ */

/*

Copyright 1986, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/
/* $XFree86: xc/programs/xinit/xinit.c,v 3.33 2003/06/10 17:03:56 dawes Exp $ */

#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <stdio.h>
#include <ctype.h>

#ifdef X_POSIX_C_SOURCE
#define _POSIX_C_SOURCE X_POSIX_C_SOURCE
#include <signal.h>
#undef _POSIX_C_SOURCE
#else
#if defined(X_NOT_POSIX) || defined(_POSIX_SOURCE)
#include <signal.h>
#else
#define _POSIX_SOURCE
#include <signal.h>
#undef _POSIX_SOURCE
#endif
#endif

#ifndef SYSV
#include <sys/wait.h>
#endif
#include <errno.h>
#include <setjmp.h>
#include <stdarg.h>

#if !defined(SIGCHLD) && defined(SIGCLD)
#define SIGCHLD SIGCLD
#endif
#ifdef __UNIXOS2__
#define INCL_DOSMODULEMGR
#include <os2.h>
#define setpgid(a,b)
#define setuid(a)
#define setgid(a)
#define SHELL "cmd.exe"
#define XINITRC "xinitrc.cmd"
#define XSERVERRC "xservrc.cmd"
char **envsave;	/* to circumvent an UNIXOS2 problem */
#define environ envsave
#endif

#include <stdlib.h>
extern char **environ;
char **newenviron = NULL;

#ifndef SHELL
#define SHELL "sh"
#endif

#ifndef HAS_VFORK
#define vfork() fork()
#else
#if defined(sun) && !defined(SVR4)
#include <vfork.h>
#endif
#endif

/* A/UX setpgid incorrectly removes the controlling terminal.
   Per Posix, only setsid should do that. */
#if !defined(X_NOT_POSIX) && !defined(macII)
#define setpgrp setpgid
#endif

#ifdef __UNIXOS2__
#define HAS_EXECVPE
#endif

#ifdef HAS_EXECVPE
#define Execvpe(path, argv, envp) execvpe(path, argv, envp)
#else
#define Execvpe(path, argv, envp) execvp(path, argv)
#endif

char *bindir = BINDIR;
char *server_names[] = {
#if defined(ultrix) && defined(mips)
    "Xdec        Digital color display on DECstation",
#endif
#ifdef sun				/* Sun */
    "Xsun        Sun BW2, CG2, CG3, CG4, or CG6 on Sun 2, 3, 4, or 386i",
    "Xsunmono    Sun BW2 on Sun 2, 3, 4, or 386i ",
    "Xsun24      Sun BW2, CG2, CG3, CG4, CG6, or CG8 on Sun 4",
#endif
#ifdef hpux				/* HP */
    "Xhp         HP monochrome and colors displays on 9000/300 series",
#endif
#ifdef ibm				/* IBM */
    "Xibm        IBM AED, APA, 8514a, megapel, VGA displays on PC/RT",
#endif
#ifdef macII				/* MacII */
    "XmacII      Apple monochrome display on Macintosh II",
#endif
#ifdef XFREE86
    "XFree86     XFree86 displays",
#endif
#ifdef __DARWIN__
    "XDarwin         Darwin/Mac OS X IOKit displays",
    "XDarwinQuartz   Mac OS X Quartz displays",
    "XDarwinStartup  Auto-select between XDarwin and XDarwinQuartz",
#endif
    
    NULL};

#ifndef XINITRC
#define XINITRC ".xinitrc"
#endif
char xinitrcbuf[256];

#ifndef XSERVERRC
#define XSERVERRC ".xserverrc"
#endif
char xserverrcbuf[256];

#define	TRUE		1
#define	FALSE		0
#define	OK_EXIT		0
#define	ERR_EXIT	1

char *default_server = "X";
char *default_display = ":0";		/* choose most efficient */
char *default_client[] = {"xterm", "-geometry", "+1+1", "-n", "login", NULL};
char *serverargv[100];
char *clientargv[100];
char **server = serverargv + 2;		/* make sure room for sh .xserverrc args */
char **client = clientargv + 2;		/* make sure room for sh .xinitrc args */
char *displayNum;
char *program;
Display *xd;			/* server connection */
#ifndef SYSV
#if defined(__CYGWIN__) || defined(SVR4) || defined(_POSIX_SOURCE) || defined(CSRG_BASED) || defined(__UNIXOS2__) || defined(Lynx)
int status;
#else
union wait	status;
#endif
#endif /* SYSV */
int serverpid = -1;
int clientpid = -1;
#ifndef X_NOT_POSIX
volatile int gotSignal = 0;
#endif

static void Execute ( char **vec, char **envp );
static Bool waitforserver ( void );
static Bool processTimeout ( int timeout, char *string );
static int startServer ( char *server[] );
static int startClient ( char *client[] );
static int ignorexio ( Display *dpy );
static void shutdown ( void );
static void set_environment ( void );
static void Fatal(char *msg);
static void Error ( char *fmt, ... );


#ifdef SIGNALRETURNSINT
#define SIGVAL int
#else
#define SIGVAL void
#endif

#ifdef X_NOT_POSIX
/* Can't use Error() in signal handlers */
#ifndef STDERR_FILENO
#define WRITES(s) write(STDERR_FILENO, (s), strlen(s))
#else
#define WRITES(s) write(fileno(stderr), (s), strlen(s))
#endif
#endif

static SIGVAL 
sigCatch(int sig)
{
#ifdef X_NOT_POSIX
	char buf[1024];

	signal(SIGQUIT, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	snprintf(buf, sizeof buf, "%s: unexpected signal %d\r\n", 
		 program, sig);
	WRITES(buf);
	shutdown();
	_exit(ERR_EXIT);
#else
	/* On system with POSIX signals, just interrupt the system call */
	gotSignal = sig;
#endif
}

static SIGVAL 
sigAlarm(int sig)
{
#if defined(SYSV) || defined(SVR4) || defined(linux) || defined(__UNIXOS2__)
	signal (sig, sigAlarm);
#endif
}

static SIGVAL
sigUsr1(int sig)
{
#if defined(SYSV) || defined(SVR4) || defined(linux) || defined(__UNIXOS2__)
	signal (sig, sigUsr1);
#endif
}

static void 
Execute(char **vec,		/* has room from up above */
	char **envp)
{
    Execvpe (vec[0], vec, envp);
#ifndef __UNIXOS2__
    if (access (vec[0], R_OK) == 0) {
	vec--;				/* back it up to stuff shell in */
	vec[0] = SHELL;
	Execvpe (vec[0], vec, envp);
    }
#endif
    return;
}

#ifndef __UNIXOS2__
int
main(int argc, char *argv[])
#else
int
main(int argc, char *argv[], char *envp[])
#endif
{
	register char **sptr = server;
	register char **cptr = client;
	register char **ptr;
	int pid;
	int client_given = 0, server_given = 0;
	int client_args_given = 0, server_args_given = 0;
	int start_of_client_args, start_of_server_args;
#ifndef X_NOT_POSIX
	struct sigaction sa;
#endif

#ifdef __UNIXOS2__
	envsave = envp;	/* circumvent an EMX problem */

	/* Check whether the system will run at all */
	if (_emx_rev < 50) {
		APIRET rc;
		HMODULE hmod;
		char name[CCHMAXPATH];
		char fail[9];
		fputs ("This program requires emx.dll revision 50 (0.9c) "
			"or later.\n", stderr);
		rc = DosLoadModule (fail, sizeof (fail), "emx", &hmod);
		if (rc == 0) {
			rc = DosQueryModuleName (hmod, sizeof (name), name);
			if (rc == 0)
				fprintf (stderr, "Please delete or update `%s'.\n", name);
			DosFreeModule (hmod);
		}
		exit (2);
	}
#endif
	program = *argv++;
	argc--;
	/*
	 * copy the client args.
	 */
	if (argc == 0 ||
#ifndef __UNIXOS2__
	    (**argv != '/' && **argv != '.')) {
#else
	    (**argv != '/' && **argv != '\\' && **argv != '.' &&
	     !(isalpha(**argv) && (*argv)[1]==':'))) {
#endif
		for (ptr = default_client; *ptr; )
			*cptr++ = *ptr++;
#ifdef sun
		/* 
		 * If running on a sun, and if WINDOW_PARENT isn't defined, 
		 * that means SunWindows isn't running, so we should pass 
		 * the -C flag to xterm so that it sets up a console.
		 */
		if ( getenv("WINDOW_PARENT") == NULL )
		    *cptr++ = "-C";
#endif /* sun */
	} else {
		client_given = 1;
	}
	start_of_client_args = (cptr - client);
	while (argc && strcmp(*argv, "--")) {
		client_args_given++;
		*cptr++ = *argv++;
		argc--;
	}
	*cptr = NULL;
	if (argc) {
		argv++;
		argc--;
	}

	/*
	 * Copy the server args.
	 */
	if (argc == 0 ||
#ifndef __UNIXOS2__
	    (**argv != '/' && **argv != '.')) {
		*sptr++ = default_server;
#else
	    (**argv != '/' && **argv != '\\' && **argv != '.' &&
	     !(isalpha(**argv) && (*argv)[1]==':'))) {
		*sptr = getenv("XSERVER");
		if (!*sptr) {
			Error("No XSERVER environment variable set");
			exit(1);
		}
		*sptr++;
#endif
	} else {
		server_given = 1;
		*sptr++ = *argv++;
		argc--;
	}
	if (argc > 0 && (argv[0][0] == ':' && isdigit(argv[0][1])))
		displayNum = *argv;
	else
		displayNum = *sptr++ = default_display;

	start_of_server_args = (sptr - server);
	while (--argc >= 0) {
		server_args_given++;
		*sptr++ = *argv++;
	}
	*sptr = NULL;

	/*
	 * if no client arguments given, check for a startup file and copy
	 * that into the argument list
	 */
	if (!client_given) {
	    char *cp;
	    Bool required = False;

	    xinitrcbuf[0] = '\0';
	    if ((cp = getenv ("XINITRC")) != NULL) {
		strcpy (xinitrcbuf, cp);
		required = True;
	    } else if ((cp = getenv ("HOME")) != NULL) {
		(void) sprintf (xinitrcbuf, "%s/%s", cp, XINITRC);
	    }
	    if (xinitrcbuf[0]) {
		if (access (xinitrcbuf, F_OK) == 0) {
		    client += start_of_client_args - 1;
		    client[0] = xinitrcbuf;
		} else if (required) {
		    fprintf (stderr,
			     "%s:  warning, no client init file \"%s\"\n",
			     program, xinitrcbuf);
		}
	    }
	}

	/*
	 * if no server arguments given, check for a startup file and copy
	 * that into the argument list
	 */
	if (!server_given) {
	    char *cp;
	    Bool required = False;

	    xserverrcbuf[0] = '\0';
	    if ((cp = getenv ("XSERVERRC")) != NULL) {
		strcpy (xserverrcbuf, cp);
		required = True;
	    } else if ((cp = getenv ("HOME")) != NULL) {
		(void) sprintf (xserverrcbuf, "%s/%s", cp, XSERVERRC);
	    }
	    if (xserverrcbuf[0]) {
		if (access (xserverrcbuf, F_OK) == 0) {
		    server += start_of_server_args - 1;
		    server[0] = xserverrcbuf;
		} else if (required) {
		    fprintf (stderr,
			     "%s:  warning, no server init file \"%s\"\n",
			     program, xserverrcbuf);
		}
	    }
	}


	/*
	 * put the display name into the environment
	 */
	set_environment ();

	/*
	 * Start the server and client.
	 */
#ifdef SIGCHLD
	signal(SIGCHLD, SIG_DFL);	/* Insurance */
#endif
#ifdef X_NOT_POSIX
	signal(SIGQUIT, sigCatch);
	signal(SIGINT, sigCatch);
	signal(SIGHUP, sigCatch);
	signal(SIGPIPE, sigCatch);
#else
	/* Let those signal interrupt the wait() call in the main loop */
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = sigCatch;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;	/* do not set SA_RESTART */
	
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGPIPE, &sa, NULL);
#endif
	signal(SIGALRM, sigAlarm);
	signal(SIGUSR1, sigUsr1);
	if (startServer(server) > 0
	 && startClient(client) > 0) {
		pid = -1;
		while (pid != clientpid && pid != serverpid
#ifndef X_NOT_POSIX
		       && gotSignal == 0
#endif
			)
			pid = wait(NULL);
	}
	signal(SIGQUIT, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	shutdown();
#ifndef X_NOT_POSIX
	if (gotSignal != 0) {
		Error("unexpected signal %d.\n", gotSignal);
		exit(ERR_EXIT);
	}
#endif
	if (serverpid < 0 )
		Fatal("Server error.\n");
	if (clientpid < 0)
		Fatal("Client error.\n");
	exit(OK_EXIT);
}


/*
 *	waitforserver - wait for X server to start up
 */
static Bool
waitforserver(void)
{
	int	ncycles	 = 120;		/* # of cycles to wait */
	int	cycles;			/* Wait cycle count */

	for (cycles = 0; cycles < ncycles; cycles++) {
		if ((xd = XOpenDisplay(displayNum))) {
			return(TRUE);
		}
		else {
#define MSG "X server to begin accepting connections"
		    if (!processTimeout (1, MSG)) 
		      break;
#undef MSG
		}
	}

	fprintf (stderr, "giving up.\r\n");
	return(FALSE);
}

/*
 * return TRUE if we timeout waiting for pid to exit, FALSE otherwise.
 */
static Bool
processTimeout(int timeout, char *string)
{
	int	i = 0, pidfound = -1;
	static char	*laststring;

	for (;;) {
#if defined(SYSV) || defined(__UNIXOS2__)
		alarm(1);
		if ((pidfound = wait(NULL)) == serverpid)
			break;
		alarm(0);
#else /* SYSV */
#if defined(SVR4) || defined(_POSIX_SOURCE) || defined(Lynx)
		if ((pidfound = waitpid(serverpid, &status, WNOHANG)) == serverpid)
			break;
#else
		if ((pidfound = wait3(&status, WNOHANG, NULL)) == serverpid)
			break;
#endif
#endif /* SYSV */
		if (timeout) {
			if (i == 0 && string != laststring)
				fprintf(stderr, "\r\nwaiting for %s ", string);
			else
				fprintf(stderr, ".");
			fflush(stderr);
		}
		if (timeout)
			sleep (1);
		if (++i > timeout)
			break;
	}
	if ( i > 0 ) fputc( '\n', stderr );     /* tidy up after message */
	laststring = string;
	return( serverpid != pidfound );
}

static int
startServer(char *server[])
{
#if !defined(X_NOT_POSIX)
	sigset_t mask, old;
#else
	int old;
#endif
#ifdef __UNIXOS2__
	sigset_t pendings;
#endif

#if !defined(X_NOT_POSIX)
	sigemptyset(&mask);
	sigaddset(&mask, SIGUSR1);
	sigprocmask(SIG_BLOCK, &mask, &old);
#else
	old = sigblock (sigmask (SIGUSR1));
#endif
	serverpid = fork(); 

	switch(serverpid) {
	case 0:
		/* Unblock */
#ifndef X_NOT_POSIX
		sigprocmask(SIG_SETMASK, &old, NULL);
#else
		sigsetmask (old);
#endif

		/*
		 * don't hang on read/write to control tty
		 */
#ifdef SIGTTIN
		(void) signal(SIGTTIN, SIG_IGN);
#endif
#ifdef SIGTTOU
		(void) signal(SIGTTOU, SIG_IGN);
#endif
		/*
		 * ignore SIGUSR1 in child.  The server
		 * will notice this and send SIGUSR1 back
		 * at xinit when ready to accept connections
		 */
		(void) signal(SIGUSR1, SIG_IGN);
		/*
		 * prevent server from getting sighup from vhangup()
		 * if client is xterm -L
		 */
#ifndef __UNIXOS2__
		setpgrp(0,getpid());
#endif
		Execute (server, environ);
		Error ("no server \"%s\" in PATH\n", server[0]);
		{
		    char **cpp;

		    fprintf (stderr,
"\nUse the -- option, or make sure that %s is in your path and\n",
			     bindir);
		    fprintf (stderr,
"that \"%s\" is a program or a link to the right type of server\n",
			     server[0]);
		    fprintf (stderr,
"for your display.  Possible server names include:\n\n");
		    for (cpp = server_names; *cpp; cpp++) {
			fprintf (stderr, "    %s\n", *cpp);
		    }
		    fprintf (stderr, "\n");
		}
		exit (ERR_EXIT);

		break;
	case -1:
		break;
	default:
		/*
		 * don't nice server
		 */
#ifdef PRIO_PROCESS
		setpriority( PRIO_PROCESS, serverpid, -1 );
#endif

		errno = 0;
		if (! processTimeout(0, "")) {
			serverpid = -1;
			break;
		}
		/*
		 * kludge to avoid race with TCP, giving server time to
		 * set his socket options before we try to open it,
		 * either use the 15 second timeout, or await SIGUSR1.
		 *
		 * If your machine is substantially slower than 15 seconds,
		 * you can easily adjust this value.
		 */
		alarm (15);

#ifndef X_NOT_POSIX
#ifdef __UNIXOS2__
		/*
		 * fg2003/05/06: work around a problem in EMX: sigsuspend()
		 * does not deliver pending signals when called but when
		 * returning; so if SIGUSR1 has already been sent by the
		 * server, we would still have to await SIGALRM
		 */
		sigemptyset(&pendings);
		sigpending(&pendings);
		if (!sigismember(&pendings, SIGUSR1))
#endif /* __UNIXOS2__ */
		sigsuspend(&old);
		alarm (0);
		sigprocmask(SIG_SETMASK, &old, NULL);
#else
		sigpause (old);
		alarm (0);
		sigsetmask (old);
#endif

		if (waitforserver() == 0) {
			Error("unable to connect to X server\r\n");
			shutdown();
			serverpid = -1;
		}
		break;
	}

	return(serverpid);
}

static int
startClient(char *client[])
{
	if ((clientpid = vfork()) == 0) {
		setuid(getuid());
		setpgrp(0, getpid());
		environ = newenviron;
#ifdef __UNIXOS2__
#undef environ
		environ = newenviron;
		client[0] = (char*)__XOS2RedirRoot(client[0]);
#endif
		Execute (client,newenviron);
		Error ("no program named \"%s\" in PATH\r\n", client[0]);
		fprintf (stderr,
"\nSpecify a program on the command line or make sure that %s\r\n", bindir);
		fprintf (stderr,
"is in your path.\r\n");
		fprintf (stderr, "\n");
		_exit (ERR_EXIT);
	}
	return (clientpid);
}

#if !defined(X_NOT_POSIX) || defined(SYSV) || defined(__UNIXOS2__)
#define killpg(pgrp, sig) kill(-(pgrp), sig)
#endif

static jmp_buf close_env;

static int 
ignorexio(Display *dpy)
{
    fprintf (stderr, "%s:  connection to X server lost.\r\n", program);
    longjmp (close_env, 1);
    /*NOTREACHED*/
    return 0;
}

static void 
shutdown(void)
{
	/* have kept display opened, so close it now */
	if (clientpid > 0) {
		XSetIOErrorHandler (ignorexio);
		if (! setjmp(close_env)) {
		    XCloseDisplay(xd);
		}

		/* HUP all local clients to allow them to clean up */
		errno = 0;
		if ((killpg(clientpid, SIGHUP) != 0) &&
		    (errno != ESRCH))
			Error("can't send HUP to process group %d\r\n",
				clientpid);
	}

	if (serverpid < 0)
		return;
	errno = 0;
	if (killpg(serverpid, SIGTERM) < 0) {
		if (errno == EPERM)
			Fatal("Can't kill X server\r\n");
		if (errno == ESRCH)
			return;
	}
	if (! processTimeout(10, "X server to shut down")) {
	    fprintf (stderr, "\r\n");
	    return;
	}

	fprintf(stderr, 
	"\r\n%s:  X server slow to shut down, sending KILL signal.\r\n",
		program);
	fflush(stderr);
	errno = 0;
	if (killpg(serverpid, SIGKILL) < 0) {
		if (errno == ESRCH)
			return;
	}
	if (processTimeout(3, "server to die")) {
		fprintf (stderr, "\r\n");
		Fatal("Can't kill server\r\n");
	}
	fprintf (stderr, "\r\n");
	return;
}


/*
 * make a new copy of environment that has room for DISPLAY
 */

static void 
set_environment(void)
{
    int nenvvars;
    char **newPtr, **oldPtr;
    static char displaybuf[256];

    /* count number of environment variables */
    for (oldPtr = environ; *oldPtr; oldPtr++) ;

    nenvvars = (oldPtr - environ);
    newenviron = (char **) malloc ((nenvvars + 2) * sizeof(char **));
    if (!newenviron) {
	fprintf (stderr,
		 "%s:  unable to allocate %d pointers for environment\n",
		 program, nenvvars + 2);
	exit (1);
    }

    /* put DISPLAY=displayname as first element */
    strcpy (displaybuf, "DISPLAY=");
    strcpy (displaybuf + 8, displayNum);
    newPtr = newenviron;
    *newPtr++ = displaybuf;

    /* copy pointers to other variables */
    for (oldPtr = environ; *oldPtr; oldPtr++) {
	if (strncmp (*oldPtr, "DISPLAY=", 8) != 0) {
	    *newPtr++ = *oldPtr;
	}
    }
    *newPtr = NULL;
    return;
}

static void
Fatal(char *msg)
{
	Error(msg);
	exit(ERR_EXIT);
}

static void
Error(char *fmt, ...)
{
        va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "%s:  ", program);
	if (errno > 0)
	  fprintf (stderr, "%s (errno %d):  ", strerror(errno), errno);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}
