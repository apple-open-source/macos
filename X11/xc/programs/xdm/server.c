/* $Xorg: server.c,v 1.5 2001/02/09 02:05:40 xorgcvs Exp $ */
/*

Copyright 1988, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/* $XFree86: xc/programs/xdm/server.c,v 3.13 2001/12/14 20:01:23 dawes Exp $ */

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 *
 * server.c - manage the X server
 */

# include	"dm.h"
# include	"dm_error.h"
# include 	"dm_socket.h"

# include	<X11/Xlib.h>
# include	<X11/Xos.h>
# include	<stdio.h>
# include	<signal.h>
# include	<errno.h>
# include 	<sys/socket.h>

static int receivedUsr1;

static int serverPause (unsigned t, int serverPid);

static Display	*dpy;

/* ARGSUSED */
static SIGVAL
CatchUsr1 (int n)
{
    int olderrno = errno;

#ifdef SIGNALS_RESET_WHEN_CAUGHT
    (void) Signal (SIGUSR1, CatchUsr1);
#endif
    Debug ("display manager caught SIGUSR1\n");
    ++receivedUsr1;
    errno = olderrno;
}

char *_SysErrorMsg (int n)
{
    char *s = strerror(n);
    return (s ? s : "unknown error");
}

static int
StartServerOnce (struct display *d)
{
    char	**f;
    char	**argv;
    char	arg[1024];
    int		pid;

    Debug ("StartServer for %s\n", d->name);
    receivedUsr1 = 0;
    (void) Signal (SIGUSR1, CatchUsr1);
    argv = d->argv;
    switch (pid = fork ()) {
    case 0:
	CleanUpChild ();
#ifdef XDMCP
	/* The chooser socket is not closed by CleanUpChild() */
	DestroyWellKnownSockets();
#endif
	if (d->authFile) {
	    sprintf (arg, "-auth %s", d->authFile);
	    argv = parseArgs (argv, arg);
	}
	if (!argv) {
	    LogError ("StartServer: no arguments\n");
	    sleep ((unsigned) d->openDelay);
	    exit (UNMANAGE_DISPLAY);
	}
	for (f = argv; *f; f++)
	    Debug ("'%s' ", *f);
	Debug ("\n");
	/*
	 * give the server SIGUSR1 ignored,
	 * it will notice that and send SIGUSR1
	 * when ready
	 */
	(void) Signal (SIGUSR1, SIG_IGN);
	(void) execv (argv[0], argv);
	LogError ("server %s cannot be executed\n",
			argv[0]);
	sleep ((unsigned) d->openDelay);
	exit (REMANAGE_DISPLAY);
    case -1:
	LogError ("fork failed, sleeping\n");
	return 0;
    default:
	break;
    }
    Debug ("Server Started %d\n", pid);
    d->serverPid = pid;
    if (serverPause ((unsigned) d->openDelay, pid))
	return FALSE;
    return TRUE;
}

int
StartServer (struct display *d)
{
    int	i;
    int	ret = FALSE;

    i = 0;
    while (d->serverAttempts == 0 || i < d->serverAttempts)
    {
	if ((ret = StartServerOnce (d)) == TRUE)
	    break;
	sleep (d->openDelay);
	i++;
    }
    return ret;
}

/*
 * sleep for t seconds, return 1 if the server is dead when
 * the sleep finishes, 0 else
 */

static Jmp_buf	pauseAbort;
static int	serverPauseRet;

/* ARGSUSED */
static SIGVAL
serverPauseAbort (int n)
{
    Longjmp (pauseAbort, 1);
}

/* ARGSUSED */
static SIGVAL
serverPauseUsr1 (int n)
{
    Debug ("display manager paused til SIGUSR1\n");
    ++receivedUsr1;
    Longjmp (pauseAbort, 1);
}

static int
serverPause (unsigned t, int serverPid)
{
    int		pid;

    serverPauseRet = 0;
    if (!Setjmp (pauseAbort)) {
	(void) Signal (SIGALRM, serverPauseAbort);
	(void) Signal (SIGUSR1, serverPauseUsr1);
#ifdef SYSV
	if (receivedUsr1)
	    (void) alarm ((unsigned) 1);
	else
	    (void) alarm (t);
#else
	if (!receivedUsr1)
	    (void) alarm (t);
	else
	    Debug ("Already received USR1\n");
#endif
	for (;;) {
#if defined(SYSV) && defined(X_NOT_POSIX)
	    pid = wait ((waitType *) 0);
#else
	    if (!receivedUsr1)
		pid = wait ((waitType *) 0);
	    else
#ifndef X_NOT_POSIX
		pid = waitpid (-1, (int *) 0, WNOHANG);
#else
		pid = wait3 ((waitType *) 0, WNOHANG,
			     (struct rusage *) 0);
#endif /* X_NOT_POSIX */
#endif /* SYSV */
	    if (pid == serverPid ||
	       (pid == -1 && errno == ECHILD))
	    {
		Debug ("Server dead\n");
		serverPauseRet = 1;
		break;
	    }
#if !defined(SYSV) || !defined(X_NOT_POSIX)
	    if (pid == 0) {
		Debug ("Server alive and kicking\n");
		break;
	    }
#endif
	}
    }
    (void) alarm ((unsigned) 0);
    (void) Signal (SIGALRM, SIG_DFL);
    (void) Signal (SIGUSR1, CatchUsr1);
    if (serverPauseRet) {
	Debug ("Server died\n");
	LogError ("server unexpectedly died\n");
    }
    return serverPauseRet;
}


/*
 * this code is complicated by some TCP failings.  On
 * many systems, the connect will occasionally hang forever,
 * this trouble is avoided by setting up a timeout to Longjmp
 * out of the connect (possibly leaving piles of garbage around
 * inside Xlib) and give up, terminating the server.
 */

static Jmp_buf	openAbort;

/* ARGSUSED */
static SIGVAL
abortOpen (int n)
{
	Longjmp (openAbort, 1);
}

#ifdef XDMCP

#ifdef STREAMSCONN
#include <tiuser.h>
#endif

static void
GetRemoteAddress (struct display *d, int fd)
{
    char    buf[512];
    int	    len = sizeof (buf);
#ifdef STREAMSCONN
    struct netbuf	netb;
#endif

    if (d->peer)
	free ((char *) d->peer);
#ifdef STREAMSCONN
    netb.maxlen = sizeof(buf);
    netb.buf = buf;
    t_getname(fd, &netb, REMOTENAME);
    len = 8;
    /* lucky for us, t_getname returns something that looks like a sockaddr */
#else
    getpeername (fd, (struct sockaddr *) buf, (void *)&len);
#endif
    d->peerlen = 0;
    if (len)
    {
	d->peer = (XdmcpNetaddr) malloc (len);
	if (d->peer)
	{
	    memmove( (char *) d->peer, buf, len);
	    d->peerlen = len;
	}
    }
    Debug ("Got remote address %s %d\n", d->name, d->peerlen);
}

#endif /* XDMCP */

static int
openErrorHandler (Display *dpy)
{
    LogError ("IO Error in XOpenDisplay\n");
    exit (OPENFAILED_DISPLAY);
    /*NOTREACHED*/
    return(0);
}

int
WaitForServer (struct display *d)
{
    static int i;

    for (i = 0; i < (d->openRepeat > 0 ? d->openRepeat : 1); i++) {
    	(void) Signal (SIGALRM, abortOpen);
    	(void) alarm ((unsigned) d->openTimeout);
    	if (!Setjmp (openAbort)) {
	    Debug ("Before XOpenDisplay(%s)\n", d->name);
	    errno = 0;
	    (void) XSetIOErrorHandler (openErrorHandler);
	    dpy = XOpenDisplay (d->name);
#ifdef STREAMSCONN
	    {
		/* For some reason, the next XOpenDisplay we do is
		   going to fail, so we might as well get that out
		   of the way.  There is something broken here. */
		Display *bogusDpy = XOpenDisplay (d->name);
		Debug ("bogus XOpenDisplay %s\n",
		       bogusDpy ? "succeeded" : "failed");
		if (bogusDpy) XCloseDisplay(bogusDpy); /* just in case */
	    }
#endif
	    (void) alarm ((unsigned) 0);
	    (void) Signal (SIGALRM, SIG_DFL);
	    (void) XSetIOErrorHandler ((int (*)(Display *)) 0);
	    Debug ("After XOpenDisplay(%s)\n", d->name);
	    if (dpy) {
#ifdef XDMCP
	    	if (d->displayType.location == Foreign)
		    GetRemoteAddress (d, ConnectionNumber (dpy));
#endif
	    	RegisterCloseOnFork (ConnectionNumber (dpy));
		(void) fcntl (ConnectionNumber (dpy), F_SETFD, 0);
	    	return 1;
	    } else {
	    	Debug ("OpenDisplay failed %d (%s) on \"%s\"\n",
		       errno, strerror (errno), d->name);
	    }
	    Debug ("waiting for server to start %d\n", i);
	    sleep ((unsigned) d->openDelay);
    	} else {
	    Debug ("hung in open, aborting\n");
	    LogError ("Hung in XOpenDisplay(%s), aborting\n", d->name);
	    (void) Signal (SIGALRM, SIG_DFL);
	    break;
    	}
    }
    Debug ("giving up on server\n");
    LogError ("server open failed for %s, giving up\n", d->name);
    return 0;
}

void
ResetServer (struct display *d)
{
    if (dpy && d->displayType.origin != FromXDMCP)
	pseudoReset (dpy);
}

static Jmp_buf	pingTime;

static void
PingLost (void)
{
    Longjmp (pingTime, 1);
}

/* ARGSUSED */
static int
PingLostIOErr (Display *dpy)
{
    PingLost();
    return 0;
}

/* ARGSUSED */
static SIGVAL
PingLostSig (int n)
{
    PingLost();
}

int
PingServer (struct display *d, Display *alternateDpy)
{
    int	    (*oldError)(Display *);
    SIGVAL  (*oldSig)(int);
    int	    oldAlarm;
    static Display *aDpy;
    
    aDpy = (alternateDpy != NULL ? alternateDpy : dpy);
    oldError = XSetIOErrorHandler (PingLostIOErr);
    oldAlarm = alarm (0);
    oldSig = Signal (SIGALRM, PingLostSig);
    (void) alarm (d->pingTimeout * 60);
    if (!Setjmp (pingTime))
    {
	Debug ("Ping server\n");
	XSync (aDpy, 0);
    }
    else
    {
	Debug ("Server dead\n");
	(void) alarm (0);
	(void) Signal (SIGALRM, SIG_DFL);
	XSetIOErrorHandler (oldError);
	return 0;
    }
    (void) alarm (0);
    (void) Signal (SIGALRM, oldSig);
    (void) alarm (oldAlarm);
    Debug ("Server alive\n");
    XSetIOErrorHandler (oldError);
    return 1;
}
