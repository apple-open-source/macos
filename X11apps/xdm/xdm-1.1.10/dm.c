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

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 *
 * display manager
 */

#include	"dm.h"
#include	"dm_auth.h"
#include	"dm_error.h"

#include	<stdio.h>
#ifdef X_POSIX_C_SOURCE
# define _POSIX_C_SOURCE X_POSIX_C_SOURCE
# include <signal.h>
# undef _POSIX_C_SOURCE
#else
# if defined(X_NOT_POSIX) || defined(_POSIX_SOURCE)
#  include <signal.h>
# else
#  define _POSIX_SOURCE
#  include <signal.h>
#  undef _POSIX_SOURCE
# endif
#endif
#ifdef __NetBSD__
# include <sys/param.h>
#endif
#ifdef USESECUREWARE
# include <prot.h>
#endif

#ifndef sigmask
# define sigmask(m)  (1 << ((m - 1)))
#endif

#include	<sys/stat.h>
#include	<errno.h>
#include	<X11/Xfuncproto.h>
#include	<X11/Xatom.h>
#include	<stdarg.h>
#include	<stdint.h>

#ifndef F_TLOCK
# ifndef X_NOT_POSIX
#  include	<unistd.h>
# endif
#endif

#if defined(HAVE_OPENLOG) && defined(HAVE_SYSLOG_H)
# define USE_SYSLOG
# include <syslog.h>
# ifndef LOG_AUTHPRIV
#  define LOG_AUTHPRIV LOG_AUTH
# endif
# ifndef LOG_PID
#  define LOG_PID 0
# endif
#endif

#if defined(SVR4) && !defined(sun)
extern FILE    *fdopen();
#endif

static SIGVAL	StopAll (int n), RescanNotify (int n);
static void	RescanServers (void);
static void	RestartDisplay (struct display *d, int forceReserver);
static void	ScanServers (void);
static void	SetAccessFileTime (void);
static void	SetConfigFileTime (void);
static void	StartDisplays (void);
static void	TerminateProcess (pid_t pid, int signal);

volatile int	Rescan;
static long	ServersModTime, ConfigModTime, AccessFileModTime;

int nofork_session = 0;

#ifndef NOXDMTITLE
static char *Title;
static int TitleLen;
#endif

#ifndef UNRELIABLE_SIGNALS
static SIGVAL ChildNotify (int n);
#endif

static long StorePid (void);
static void RemovePid (void);

static pid_t parent_pid = -1; 	/* PID of parent xdm process */

int
main (int argc, char **argv)
{
    int	oldpid;
    mode_t oldumask;
    char cmdbuf[1024];

    /* make sure at least world write access is disabled */
    if (((oldumask = umask(022)) & 002) == 002)
	(void) umask (oldumask);
#ifndef NOXDMTITLE
    Title = argv[0];
    TitleLen = (argv[argc - 1] + strlen(argv[argc - 1])) - Title;
#endif

#ifdef USESECUREWARE
    set_auth_parameters (argc, argv);
#endif

    /*
     * Step 1 - load configuration parameters
     */
    InitResources (argc, argv);
    SetConfigFileTime ();
    LoadDMResources ();
    /*
     * Only allow root to run in non-debug mode to avoid problems
     */
    if (debugLevel == 0 && getuid() != 0)
    {
	fprintf (stderr, "Only root wants to run %s\n", argv[0]);
	exit (1);
    }
    if (debugLevel == 0 && daemonMode)
	BecomeDaemon ();
    if (debugLevel == 0)
	InitErrorLog ();
    if (debugLevel >= 10)
	nofork_session = 1;
    /* SUPPRESS 560 */
    if ((oldpid = StorePid ()))
    {
	if (oldpid == -1)
	    LogError ("Can't create/lock pid file %s\n", pidFile);
	else
	    LogError ("Can't lock pid file %s, another xdm is running (pid %d)\n",
		 pidFile, oldpid);
	exit (1);
    }

    LogInfo ("Starting\n");

    if (atexit (RemovePid))
	LogError ("could not register RemovePid() with atexit()\n");

    if (nofork_session == 0) {
	/* Clean up any old Authorization files */
	/* AUD: all good? */
	snprintf(cmdbuf, sizeof(cmdbuf), "/bin/rm -f %s/authdir/authfiles/A*", authDir);
	system(cmdbuf);
    }
#if!defined(ARC4_RANDOM) && !defined(DEV_RANDOM)
    AddOtherEntropy ();
#endif
#ifdef XDMCP
    init_session_id ();
    CreateWellKnownSockets ();
#else
    Debug ("xdm: not compiled for XDMCP\n");
#endif
    parent_pid = getpid ();
    (void) Signal (SIGTERM, StopAll);
    (void) Signal (SIGINT, StopAll);
    /*
     * Step 2 - Read /etc/Xservers and set up
     *	    the socket.
     *
     *	    Keep a sub-daemon running
     *	    for each entry
     */
    SetAccessFileTime ();
#ifdef XDMCP
    ScanAccessDatabase ();
    UpdateListenSockets ();
#endif
    ScanServers ();
    StartDisplays ();
#if !defined(ARC4_RANDOM) && !defined(DEV_RANDOM)
    AddOtherEntropy();
#endif
    (void) Signal (SIGHUP, RescanNotify);
#ifndef UNRELIABLE_SIGNALS
    (void) Signal (SIGCHLD, ChildNotify);
#endif
    Debug ("startup successful; entering main loop\n");
    while (
#ifdef XDMCP
	   AnyWellKnownSockets() ||
#endif
	   AnyDisplaysLeft ())
    {
	if (Rescan)
	{
	    RescanServers ();
	    Rescan = 0;
	}
#if defined(UNRELIABLE_SIGNALS) || !defined(XDMCP)
	WaitForChild ();
#else
	WaitForSomething ();
#endif
    }
    Debug ("Nothing left to do, exiting\n");
    LogInfo ("Exiting\n");
    exit(0);
    /*NOTREACHED*/
}

/* ARGSUSED */
static SIGVAL
RescanNotify (int n)
{
    int olderrno = errno;

    Debug ("Caught SIGHUP\n");
    Rescan = 1;
#ifdef SIGNALS_RESET_WHEN_CAUGHT
    (void) Signal (SIGHUP, RescanNotify);
#endif
    errno = olderrno;
}

static void
ScanServers (void)
{
    char	lineBuf[10240];
    int		len;
    FILE	*serversFile;
    struct stat	statb;
    static DisplayType	acceptableTypes[] =
	    { { Local, Permanent, FromFile },
	      { Foreign, Permanent, FromFile },
	    };

#define NumTypes    (sizeof (acceptableTypes) / sizeof (acceptableTypes[0]))

    if (servers[0] == '/')
    {
	serversFile = fopen (servers, "r");
	if (serversFile == NULL)
 	{
	    LogError ("cannot access servers file %s\n", servers);
	    return;
	}
	if (ServersModTime == 0)
	{
	    fstat (fileno (serversFile), &statb);
	    ServersModTime = statb.st_mtime;
	}
	while (fgets (lineBuf, sizeof (lineBuf)-1, serversFile))
	{
	    len = strlen (lineBuf);
	    if (lineBuf[len-1] == '\n')
		lineBuf[len-1] = '\0';
	    ParseDisplay (lineBuf, acceptableTypes, NumTypes);
	}
	fclose (serversFile);
    }
    else
    {
	ParseDisplay (servers, acceptableTypes, NumTypes);
    }
}

static void
MarkDisplay (struct display *d)
{
    d->state = MissingEntry;
}

static void
RescanServers (void)
{
    Debug ("rescanning servers\n");
    LogInfo ("Rescanning both config and servers files\n");
    ForEachDisplay (MarkDisplay);
    SetConfigFileTime ();
    ReinitResources ();
    LoadDMResources ();
    ScanServers ();
    SetAccessFileTime ();
#ifdef XDMCP
    ScanAccessDatabase ();
    UpdateListenSockets ();
#endif
    StartDisplays ();
}

static void
SetConfigFileTime (void)
{
    struct stat	statb;

    if (stat (config, &statb) != -1)
	ConfigModTime = statb.st_mtime;
}

static void
SetAccessFileTime (void)
{
    struct stat	statb;

    if (stat (accessFile, &statb) != -1)
	AccessFileModTime = statb.st_mtime;
}

static void
RescanIfMod (void)
{
    struct stat	statb;

    if (config && stat (config, &statb) != -1)
    {
	if (statb.st_mtime != ConfigModTime)
	{
	    Debug ("Config file %s has changed, rereading\n", config);
	    LogInfo ("Rereading configuration file %s\n", config);
	    ConfigModTime = statb.st_mtime;
	    ReinitResources ();
	    LoadDMResources ();
	}
    }
    if (servers[0] == '/' && stat(servers, &statb) != -1)
    {
	if (statb.st_mtime != ServersModTime)
	{
	    Debug ("Servers file %s has changed, rescanning\n", servers);
	    LogInfo ("Rereading servers file %s\n", servers);
	    ServersModTime = statb.st_mtime;
	    ForEachDisplay (MarkDisplay);
	    ScanServers ();
	}
    }
#ifdef XDMCP
    if (accessFile && accessFile[0] && stat (accessFile, &statb) != -1)
    {
	if (statb.st_mtime != AccessFileModTime)
	{
	    Debug ("Access file %s has changed, rereading\n", accessFile);
	    LogInfo ("Rereading access file %s\n", accessFile);
	    AccessFileModTime = statb.st_mtime;
	    ScanAccessDatabase ();
	    UpdateListenSockets();
	}
    }
#endif
}

/*
 * catch a SIGTERM, kill all displays and exit
 */

/* ARGSUSED */
static SIGVAL
StopAll (int n)
{
    int olderrno = errno;

    if (parent_pid != getpid())
    {
	/*
	 * We are a child xdm process that was killed by the
	 * master xdm before we were able to return from fork()
	 * and remove this signal handler.
	 *
	 * See defect XWSog08655 for more information.
	 */
	Debug ("Child xdm caught SIGTERM before it removed that signal.\n");
	(void) Signal (n, SIG_DFL);
	TerminateProcess (getpid(), SIGTERM);
	errno = olderrno;
	return;
    }
    Debug ("Shutting down entire manager\n");
    LogInfo ("Shutting down\n");
#ifdef XDMCP
    DestroyWellKnownSockets ();
#endif
    ForEachDisplay (StopDisplay);
#ifdef SIGNALS_RESET_WHEN_CAUGHT
    /* to avoid another one from killing us unceremoniously */
    (void) Signal (SIGTERM, StopAll);
    (void) Signal (SIGINT, StopAll);
#endif
    errno = olderrno;
}

/*
 * notice that a child has died and may need another
 * sub-daemon started
 */

int	ChildReady;

#ifndef UNRELIABLE_SIGNALS
/* ARGSUSED */
static SIGVAL
ChildNotify (int n)
{
    int olderrno = errno;

    ChildReady = 1;
# ifdef ISC
    (void) Signal (SIGCHLD, ChildNotify);
# endif
    errno = olderrno;
}
#endif

void
WaitForChild (void)
{
    pid_t		pid;
    struct display	*d;
    waitType	status;
#if !defined(X_NOT_POSIX) && !defined(__UNIXOS2__)
    sigset_t mask, omask;
#else
    int		omask;
#endif

#ifdef UNRELIABLE_SIGNALS
    /* XXX classic System V signal race condition here with RescanNotify */
    if ((pid = wait (&status)) != -1)
#else
# ifndef X_NOT_POSIX
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigaddset(&mask, SIGHUP);
    sigprocmask(SIG_BLOCK, &mask, &omask);
    Debug ("signals blocked\n");
# else
    omask = sigblock (sigmask (SIGCHLD) | sigmask (SIGHUP));
    Debug ("signals blocked, mask was 0x%x\n", omask);
# endif
    if (!ChildReady && !Rescan)
# ifndef X_NOT_POSIX
	sigsuspend(&omask);
# else
	sigpause (omask);
# endif
    ChildReady = 0;
# ifndef X_NOT_POSIX
    sigprocmask(SIG_SETMASK, &omask, (sigset_t *)NULL);
# else
    sigsetmask (omask);
# endif
    while ((pid = waitpid (-1, &status, WNOHANG)) > 0)
#endif
    {
	Debug ("Manager wait returns pid: %d sig %d core %d code %d\n",
	       pid, waitSig(status), waitCore(status), waitCode(status));
	if (autoRescan)
	    RescanIfMod ();
	/* SUPPRESS 560 */
	if ((d = FindDisplayByPid (pid))) {
	    d->pid = -1;
	    switch (waitVal (status)) {
	    case UNMANAGE_DISPLAY:
		Debug ("Display exited with UNMANAGE_DISPLAY\n");
		StopDisplay (d);
		break;
	    case OBEYSESS_DISPLAY:
		d->startTries = 0;
		Debug ("Display exited with OBEYSESS_DISPLAY\n");
		if (d->displayType.lifetime != Permanent ||
		    d->status == zombie)
		    StopDisplay (d);
		else
		    RestartDisplay (d, FALSE);
		break;
	    case OPENFAILED_DISPLAY:
		Debug ("Display exited with OPENFAILED_DISPLAY, try %d of %d\n",
		       d->startTries, d->startAttempts);
		LogError ("Display %s cannot be opened\n", d->name);
		/*
 		 * no display connection was ever made, tell the
		 * terminal that the open attempt failed
 		 */
#ifdef XDMCP
		if (d->displayType.origin == FromXDMCP)
		    SendFailed (d, "Cannot open display");
#endif
		if (d->displayType.origin == FromXDMCP ||
		    d->status == zombie ||
		    ++d->startTries >= d->startAttempts)
		{
		    LogError ("Display %s is being disabled\n", d->name);
		    StopDisplay (d);
		}
		else
		{
		    RestartDisplay (d, TRUE);
		}
		break;
	    case RESERVER_DISPLAY:
		d->startTries = 0;
		Debug ("Display exited with RESERVER_DISPLAY\n");
		if (d->displayType.origin == FromXDMCP || d->status == zombie)
		    StopDisplay(d);
		else
		    RestartDisplay (d, TRUE);
		{
		  Time_t Time;
		  time(&Time);
		  Debug("time %i %i\n",Time,d->lastCrash);
		  if (d->lastCrash &&
		      ((Time - d->lastCrash) < XDM_BROKEN_INTERVAL)) {
		    Debug("Server crash frequency too high:"
			  " removing display %s\n",d->name);
		    LogError("Server crash rate too high:"
			     " removing display %s\n",d->name);
#if !defined(ARC4_RANDOM) && !defined(DEV_RANDOM)
		    AddTimerEntropy();
#endif
		    RemoveDisplay (d);
		  } else
		    d->lastCrash = Time;
		}
		break;
	    case waitCompose (SIGTERM,0,0):
		Debug ("Display exited on SIGTERM, try %d of %d\n",
			d->startTries, d->startAttempts);
		if (d->displayType.origin == FromXDMCP ||
		    d->status == zombie ||
		    ++d->startTries >= d->startAttempts)
		{
		    /*
		     * During normal xdm shutdown, killed local X servers
		     * can be zombies; this is not an error.
		     */
		    if (d->status == zombie &&
			(d->startTries < d->startAttempts))
			LogInfo ("display %s is being disabled\n", d->name);
		    else
			LogError ("display %s is being disabled\n", d->name);
		    StopDisplay(d);
		} else
		    RestartDisplay (d, TRUE);
		break;
	    case REMANAGE_DISPLAY:
		d->startTries = 0;
		Debug ("Display exited with REMANAGE_DISPLAY\n");
		/*
 		 * XDMCP will restart the session if the display
		 * requests it
		 */
		if (d->displayType.origin == FromXDMCP || d->status == zombie)
		    StopDisplay(d);
		else
		    RestartDisplay (d, FALSE);
		break;
	    default:
		Debug ("Display exited with unknown status %d\n", waitVal(status));
		LogError ("Unknown session exit code %d from process %d\n",
			  waitVal (status), pid);
		StopDisplay (d);
		break;
	    }
	}
	/* SUPPRESS 560 */
	else if ((d = FindDisplayByServerPid (pid)))
	{
	    d->serverPid = -1;
	    switch (d->status)
	    {
	    case zombie:
		Debug ("Zombie server reaped, removing display %s\n", d->name);
		RemoveDisplay (d);
		break;
	    case phoenix:
		Debug ("Phoenix server arises, restarting display %s\n", d->name);
		d->status = notRunning;
		break;
	    case running:
		Debug ("Server for display %s terminated unexpectedly, status %d %d\n", d->name, waitVal (status), status);
		LogError ("Server for display %s terminated unexpectedly: %d\n", d->name, waitVal (status));
		if (d->pid != -1)
		{
		    Debug ("Terminating session pid %d\n", d->pid);
		    TerminateProcess (d->pid, SIGTERM);
		}
		break;
	    case notRunning:
		Debug ("Server exited for notRunning session on display %s\n", d->name);
		break;
	    }
	}
	else
	{
	    Debug ("Unknown child termination, status %d\n", waitVal (status));
	}
    }
    StartDisplays ();
}

static void
CheckDisplayStatus (struct display *d)
{
    if (d->displayType.origin == FromFile)
    {
	switch (d->state) {
	case MissingEntry:
	    StopDisplay (d);
	    break;
	case NewEntry:
	    d->state = OldEntry;
	case OldEntry:
	    if (d->status == notRunning)
		StartDisplay (d);
	    break;
	}
    }
}

static void
StartDisplays (void)
{
    ForEachDisplay (CheckDisplayStatus);
}

static void
SetWindowPath(struct display *d)
{
	/* setting WINDOWPATH for clients */
	Atom prop;
	Atom actualtype;
	int actualformat;
	unsigned long nitems;
	unsigned long bytes_after;
	unsigned char *buf;
	const char *windowpath;
	char *newwindowpath;
	unsigned long num;

	prop = XInternAtom(d->dpy, "XFree86_VT", False);
	if (prop == None) {
		fprintf(stderr, "no XFree86_VT atom\n");
		return;
	}
	if (XGetWindowProperty(d->dpy, DefaultRootWindow(d->dpy), prop, 0, 1,
		False, AnyPropertyType, &actualtype, &actualformat,
		&nitems, &bytes_after, &buf)) {
		fprintf(stderr, "no XFree86_VT property\n");
		return;
	}
	if (nitems != 1) {
		fprintf(stderr, "%lu items in XFree86_VT property!\n", nitems);
		XFree(buf);
		return;
	}
	switch (actualtype) {
	case XA_CARDINAL:
	case XA_INTEGER:
	case XA_WINDOW:
		switch (actualformat) {
		case  8:
			num = (*(uint8_t  *)(void *)buf);
			break;
		case 16:
			num = (*(uint16_t *)(void *)buf);
			break;
		case 32:
			num = (*(uint32_t *)(void *)buf);
			break;
		default:
			fprintf(stderr, "format %d in XFree86_VT property!\n", actualformat);
			XFree(buf);
			return;
		}
		break;
	default:
		fprintf(stderr, "type %lx in XFree86_VT property!\n", actualtype);
		XFree(buf);
		return;
	}
	XFree(buf);
	windowpath = getenv("WINDOWPATH");
	if (!windowpath) {
		asprintf(&newwindowpath, "%lu", num);
	} else {
		asprintf(&newwindowpath, "%s:%lu", windowpath, num);
	}
	if (d->windowPath)
		free(d->windowPath);
	d->windowPath = newwindowpath;
}

void
StartDisplay (struct display *d)
{
    pid_t	pid;

    Debug ("StartDisplay %s\n", d->name);
    LogInfo ("Starting X server on %s\n", d->name);
    LoadServerResources (d);
    if (d->displayType.location == Local)
    {
	/* don't bother pinging local displays; we'll
	 * certainly notice when they exit
	 */
	d->pingInterval = 0;
    	if (d->authorize)
    	{
	    Debug ("SetLocalAuthorization %s, auth %s\n",
		    d->name, d->authNames[0]);
	    SetLocalAuthorization (d);
	    /*
	     * reset the server after writing the authorization information
	     * to make it read the file (for compatibility with old
	     * servers which read auth file only on reset instead of
	     * at first connection)
	     */
	    if (d->serverPid != -1 && d->resetForAuth && d->resetSignal)
		kill (d->serverPid, d->resetSignal);
    	}
	if (d->serverPid == -1 && !StartServer (d))
	{
	    LogError ("Server for display %s can't be started, session disabled\n", d->name);
	    RemoveDisplay (d);
	    return;
	}
    }
    else
    {
	/* this will only happen when using XDMCP */
	if (d->authorizations)
	    SaveServerAuthorizations (d, d->authorizations, d->authNum);
    }
    if (!nofork_session)
	pid = fork ();
    else
	pid = 0;
    switch (pid)
    {
    case 0:
	if (!nofork_session) {
	    CleanUpChild ();
	    (void) Signal (SIGPIPE, SIG_IGN);
	}
#ifdef USE_SYSLOG
	openlog("xdm", LOG_PID, LOG_AUTHPRIV);
#endif
	LoadSessionResources (d);
	SetAuthorization (d);
	if (!WaitForServer (d))
	    exit (OPENFAILED_DISPLAY);
	SetWindowPath(d);
#ifdef XDMCP
	if (d->useChooser)
	    RunChooser (d);
	else
#endif
	    ManageSession (d);
	exit (REMANAGE_DISPLAY);
    case -1:
	break;
    default:
	Debug ("pid: %d\n", pid);
	d->pid = pid;
	d->status = running;
	break;
    }
}

static void
TerminateProcess (pid_t pid, int signal)
{
    kill (pid, signal);
#ifdef SIGCONT
    kill (pid, SIGCONT);
#endif
}

/*
 * transition from running to zombie or deleted
 */

void
StopDisplay (struct display *d)
{
    if (d->serverPid != -1)
	d->status = zombie; /* be careful about race conditions */
    if (d->pid != -1)
	TerminateProcess (d->pid, SIGTERM);
    if (d->serverPid != -1)
	TerminateProcess (d->serverPid, d->termSignal);
    else
	RemoveDisplay (d);
}

/*
 * transition from running to phoenix or notRunning
 */

static void
RestartDisplay (struct display *d, int forceReserver)
{
    if (d->serverPid != -1 && (forceReserver || d->terminateServer))
    {
	TerminateProcess (d->serverPid, d->termSignal);
	d->status = phoenix;
    }
    else
    {
	d->status = notRunning;
    }
}

static FD_TYPE	CloseMask;
static int	max;

void
RegisterCloseOnFork (int fd)
{
    FD_SET (fd, &CloseMask);
    if (fd > max)
	max = fd;
}

void
ClearCloseOnFork (int fd)
{
    FD_CLR (fd, &CloseMask);
    if (fd == max) {
	while (--fd >= 0)
	    if (FD_ISSET (fd, &CloseMask))
		break;
	max = fd;
    }
}

void
CloseOnFork (void)
{
    int	fd;

    for (fd = 0; fd <= max; fd++)
	if (FD_ISSET (fd, &CloseMask))
	{
	    close (fd);
        }
    FD_ZERO (&CloseMask);
    max = 0;
}

static int  pidFd;
static FILE *pidFilePtr;

/*
 * Create and populate file storing xdm's process ID.
 */
static long
StorePid (void)
{
    long	oldpid;
    char	pidstr[11]; /* enough space for a 32-bit pid plus \0 */
    size_t	pidstrlen;

    if (pidFile[0] != '\0')
    {
	Debug ("storing process ID in %s\n", pidFile);
	pidFd = open (pidFile, O_WRONLY|O_CREAT|O_EXCL, 0666);
	if (pidFd == -1 && errno == ENOENT)
	{
	    /* Make sure directory exists if needed
	       Allows setting pidDir to /var/run/xdm
	     */
	    char *pidDir = strdup(pidFile);

	    if (pidDir != NULL)
	    {
		char *p = strrchr(pidDir, '/');
		int r;

		if ((p != NULL) && (p != pidDir)) {
		    *p = '\0';
		}
		r = mkdir(pidDir, 0755);
		if ( (r < 0) && (errno != EEXIST) ) {
		     LogError ("process-id directory %s cannot be created\n",
			       pidDir);
		}
	    }

	    pidFd = open (pidFile, O_WRONLY|O_CREAT|O_EXCL, 0666);
	}
	if (pidFd == -1)
	{
	    if (errno == EEXIST)
	    {
		/* pidFile already exists; see if we can open it */
		pidFilePtr = fopen (pidFile, "r");
		if (pidFilePtr == NULL)
		{
		    LogError ("cannot open process ID file %s for reading: "
			      "%s\n", pidFile, _SysErrorMsg (errno));
		    return -1;
		}
		if (fscanf (pidFilePtr, "%ld\n", &oldpid) != 1)
		{
		    LogError ("existing process ID file %s empty or contains "
			      "garbage\n", pidFile);
		    oldpid = -1;
		}
		fclose (pidFilePtr);
		return oldpid;
	    }
		else
	    {
		LogError ("cannot fdopen process ID file %s for writing: "
			  "%s\n", pidFile, _SysErrorMsg (errno));
		return -1;
	    }
	}
	if ((pidFilePtr = fdopen (pidFd, "w")) == NULL)
	{
	    LogError ("cannot open process ID file %s for writing: %s\n",
		      pidFile, _SysErrorMsg (errno));
	    return -1;
	}
	(void) snprintf (pidstr, 11, "%ld", (long) getpid ());
	pidstrlen = strlen (pidstr);
	if (fprintf (pidFilePtr, "%s\n", pidstr) != ( pidstrlen + 1))
	{
	    LogError ("cannot write to process ID file %s: %s\n", pidFile,
		      _SysErrorMsg (errno));
	    return -1;
	}
	(void) fflush (pidFilePtr);
	(void) fclose (pidFilePtr);
    }
    return 0;
}

/*
 * Remove process ID file.  This function is registered with atexit().
 */
static void
RemovePid (void)
{
    if (parent_pid != getpid())
	return;

    Debug ("unlinking process ID file %s\n", pidFile);
    if (unlink (pidFile))
	if (errno != ENOENT)
	    LogError ("cannot remove process ID file %s: %s\n", pidFile,
		      _SysErrorMsg (errno));
}

#if 0
void
UnlockPidFile (void)
{
    if (lockPidFile)
# ifdef F_SETLK
    {
	struct flock lock_data;
	lock_data.l_type = F_UNLCK;
	lock_data.l_whence = SEEK_SET;
	lock_data.l_start = lock_data.l_len = 0;
	(void) fcntl(pidFd, F_SETLK, &lock_data);
    }
# else
#  ifdef F_ULOCK
	lockf (pidFd, F_ULOCK, 0);
#  else
	flock (pidFd, LOCK_UN);
#  endif
# endif
    close (pidFd);
    fclose (pidFilePtr);
}
#endif

#ifndef HAS_SETPROCTITLE
void SetTitle (char *name, ...)
{
# ifndef NOXDMTITLE
    char	*p = Title;
    int	left = TitleLen;
    char	*s;
    va_list	args;

    va_start(args,name);
    *p++ = '-';
    --left;
    s = name;
    while (s)
    {
	while (*s && left > 0)
	{
	    *p++ = *s++;
	    left--;
	}
	s = va_arg (args, char *);
    }
    while (left > 0)
    {
	*p++ = ' ';
	--left;
    }
    va_end(args);
# endif
}
#endif
