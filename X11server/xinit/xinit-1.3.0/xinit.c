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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <ctype.h>
#include <stdint.h>

#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <setjmp.h>
#include <stdarg.h>

#ifdef __APPLE__
#include <AvailabilityMacros.h>
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
#include <vproc.h>
#endif
#endif

/* For PRIO_PROCESS and setpriority() */
#ifdef __DragonFly__
#include <sys/time.h>
#include <sys/resource.h>
#endif /* __DragonFly__ */

#include <stdlib.h>

#ifndef SHELL
#define SHELL "sh"
#endif

const char *bindir = BINDIR;
const char * const server_names[] = {
#ifdef __APPLE__
    "Xquartz     Mac OSX Quartz displays.",
#else
# ifdef __CYGWIN__
    "XWin        X Server for the Cygwin environment on Microsoft Windows",
# else
    "Xorg        Common X server for most displays",
# endif
#endif
    "Xvfb        Virtual frame buffer",
    "Xfake       kdrive-based virtual frame buffer",
    "Xnest       X server nested in a window on another X server",
    "Xephyr      kdrive-based nested X server",
    "Xvnc        X server accessed over VNC's RFB protocol",
    "Xdmx        Distributed Multi-head X server",
    NULL};

#ifndef XINITRC
#define XINITRC ".xinitrc"
#endif
char xinitrcbuf[256];

#ifndef XSERVERRC
#define XSERVERRC ".xserverrc"
#endif
char xserverrcbuf[256];

#define TRUE 1
#define FALSE 0

static char *default_server = "X";
static char *default_display = ":0";        /* choose most efficient */
static char *default_client[] = {"xterm", "-geometry", "+1+1", "-n", "login", NULL};
static char *serverargv[100];
static char *clientargv[100];
static char **server = serverargv + 2;        /* make sure room for sh .xserverrc args */
static char **client = clientargv + 2;        /* make sure room for sh .xinitrc args */
static char *displayNum = NULL;
static char *program = NULL;
static Display *xd = NULL;            /* server connection */
int status;
int serverpid = -1;
int clientpid = -1;
volatile int gotSignal = 0;

static void Execute(char **vec);
static Bool waitforserver(void);
static Bool processTimeout(int timeout, char *string);
static int startServer(char *server[]);
static int startClient(char *client[]);
static int ignorexio(Display *dpy);
static void shutdown(void);
static void set_environment(void);

static void Fatal(const char *fmt, ...);
static void Error(const char *fmt, ...);
static void Fatalx(const char *fmt, ...);
static void Errorx(const char *fmt, ...);

static void
sigCatch(int sig)
{
    /* On system with POSIX signals, just interrupt the system call */
    gotSignal = sig;
}

static void
sigIgnore(int sig)
{
}

static void
Execute(char **vec)		/* has room from up above */
{
    execvp(vec[0], vec);
    if (access(vec[0], R_OK) == 0) {
	vec--;				/* back it up to stuff shell in */
	vec[0] = SHELL;
	execvp(vec[0], vec);
    }
    return;
}

int
main(int argc, char *argv[])
{
    register char **sptr = server;
    register char **cptr = client;
    register char **ptr;
    int pid;
    int client_given = 0, server_given = 0;
    int client_args_given = 0, server_args_given = 0;
    int start_of_client_args, start_of_server_args;
    struct sigaction sa, si;
#ifdef __APPLE__
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
    vproc_transaction_t vt;
#endif
#endif

    program = *argv++;
    argc--;
    /*
     * copy the client args.
     */
    if (argc == 0 ||
        (**argv != '/' && **argv != '.')) {
        for (ptr = default_client; *ptr; )
            *cptr++ = *ptr++;
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
        (**argv != '/' && **argv != '.')) {
        *sptr++ = default_server;
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
        if ((cp = getenv("XINITRC")) != NULL) {
            snprintf(xinitrcbuf, sizeof(xinitrcbuf), "%s", cp);
            required = True;
        } else if ((cp = getenv("HOME")) != NULL) {
            snprintf(xinitrcbuf, sizeof(xinitrcbuf),
                     "%s/%s", cp, XINITRC);
        }
        if (xinitrcbuf[0]) {
            if (access(xinitrcbuf, F_OK) == 0) {
                client += start_of_client_args - 1;
                client[0] = xinitrcbuf;
            } else if (required) {
                Error("warning, no client init file \"%s\"", xinitrcbuf);
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
        if ((cp = getenv("XSERVERRC")) != NULL) {
            snprintf(xserverrcbuf, sizeof(xserverrcbuf), "%s", cp);
            required = True;
        } else if ((cp = getenv("HOME")) != NULL) {
            snprintf(xserverrcbuf, sizeof(xserverrcbuf),
                     "%s/%s", cp, XSERVERRC);
        }
        if (xserverrcbuf[0]) {
            if (access(xserverrcbuf, F_OK) == 0) {
                server += start_of_server_args - 1;
                server[0] = xserverrcbuf;
            } else if (required) {
                Error("warning, no server init file \"%s\"", xserverrcbuf);
            }
        }
    }

    /*
     * Start the server and client.
     */
    signal(SIGCHLD, SIG_DFL);    /* Insurance */

    /* Let those signal interrupt the wait() call in the main loop */
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = sigCatch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;    /* do not set SA_RESTART */

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGPIPE, &sa, NULL);

    memset(&si, 0, sizeof(si));
    si.sa_handler = sigIgnore;
    sigemptyset(&si.sa_mask);
    si.sa_flags = SA_RESTART;

    sigaction(SIGALRM, &si, NULL);
    sigaction(SIGUSR1, &si, NULL);

#ifdef __APPLE__
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
    vt = vproc_transaction_begin(NULL);
#endif
#endif

    if (startServer(server) > 0
        && startClient(client) > 0) {
        pid = -1;
        while (pid != clientpid && pid != serverpid
               && gotSignal == 0
            )
            pid = wait(NULL);
    }

#ifdef __APPLE__
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
    vproc_transaction_end(NULL, vt);
#endif
#endif

    signal(SIGTERM, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    shutdown();

    if (gotSignal != 0) {
        Errorx("unexpected signal %d", gotSignal);
        exit(EXIT_FAILURE);
    }

    if (serverpid < 0)
        Fatalx("server error");
    if (clientpid < 0)
        Fatalx("client error");
    exit(EXIT_SUCCESS);
}


/*
 *    waitforserver - wait for X server to start up
 */
static Bool
waitforserver(void)
{
    int    ncycles     = 120;        /* # of cycles to wait */
    int    cycles;            /* Wait cycle count */

#ifdef __APPLE__
    /* For Apple, we don't get signaled by the server when it's ready, so we just
     * want to sleep now since we're going to sleep later anyways and this allows us
     * to avoid the awkard, "why is there an error message in the log" questions
     * from users.
     */

    sleep(2);
#endif

    for (cycles = 0; cycles < ncycles; cycles++) {
        if ((xd = XOpenDisplay(displayNum))) {
            return(TRUE);
        }
        else {
            if (!processTimeout(1, "X server to begin accepting connections"))
              break;
        }
    }

    Errorx("giving up");

    return(FALSE);
}

/*
 * return TRUE if we timeout waiting for pid to exit, FALSE otherwise.
 */
static Bool
processTimeout(int timeout, char *string)
{
    int    i = 0, pidfound = -1;
    static char    *laststring;

    for (;;) {
        if ((pidfound = waitpid(serverpid, &status, WNOHANG)) == serverpid)
            break;
        if (timeout) {
            if (i == 0 && string != laststring)
                fprintf(stderr, "\r\nwaiting for %s ", string);
            else
                fprintf(stderr, ".");
            fflush(stderr);
        }
        if (timeout)
            sleep(1);
        if (++i > timeout)
            break;
    }
    if (i > 0) fputc('\n', stderr);     /* tidy up after message */
    laststring = string;
    return (serverpid != pidfound);
}

static int
startServer(char *server[])
{
    sigset_t mask, old;
    const char * const *cpp;

    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, &old);

    serverpid = fork();

    switch(serverpid) {
    case 0:
        /* Unblock */
        sigprocmask(SIG_SETMASK, &old, NULL);

        /*
         * don't hang on read/write to control tty
         */
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        /*
         * ignore SIGUSR1 in child.  The server
         * will notice this and send SIGUSR1 back
         * at xinit when ready to accept connections
         */
        signal(SIGUSR1, SIG_IGN);
        /*
         * prevent server from getting sighup from vhangup()
         * if client is xterm -L
         */
        setpgid(0,getpid());
        Execute(server);

        Error("unable to run server \"%s\"", server[0]);

        fprintf(stderr, "Use the -- option, or make sure that %s is in your path and\n", bindir);
        fprintf(stderr, "that \"%s\" is a program or a link to the right type of server\n", server[0]);
        fprintf(stderr, "for your display.  Possible server names include:\n\n");
        for (cpp = server_names; *cpp; cpp++)
            fprintf(stderr, "    %s\n", *cpp);
        fprintf(stderr, "\n");

        exit(EXIT_FAILURE);

        break;
    case -1:
        break;
    default:
        /*
         * don't nice server
         */
        setpriority(PRIO_PROCESS, serverpid, -1);

        errno = 0;
        if(! processTimeout(0, "")) {
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
        alarm(15);

        sigsuspend(&old);
        alarm(0);
        sigprocmask(SIG_SETMASK, &old, NULL);

        if (waitforserver() == 0) {
            Error("unable to connect to X server");
            shutdown();
            serverpid = -1;
        }
        break;
    }

    return(serverpid);
}

static void
setWindowPath(void)
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
    char nums[10];
    int numn;
    size_t len;
    prop = XInternAtom(xd, "XFree86_VT", False);
    if (prop == None) {
        Errorx("Unable to intern XFree86_VT atom");
        return;
    }
    if (XGetWindowProperty(xd, DefaultRootWindow(xd), prop, 0, 1,
        False, AnyPropertyType, &actualtype, &actualformat,
        &nitems, &bytes_after, &buf)) {
        Errorx("No XFree86_VT property detected on X server, WINDOWPATH won't be set");
        return;
    }
    if (nitems != 1) {
        Errorx("XFree86_VT property unexpectedly has %lu items instead of 1", nitems);
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
            Errorx("XFree86_VT property has unexpected format %d", actualformat);
            XFree(buf);
            return;
        }
        break;
    default:
        Errorx("XFree86_VT property has unexpected type %lx", actualtype);
        XFree(buf);
        return;
    }
    XFree(buf);
    windowpath = getenv("WINDOWPATH");
    numn = snprintf(nums, sizeof(nums), "%lu", num);
    if (!windowpath) {
        len = numn + 1;
        newwindowpath = malloc(len);
        if (newwindowpath == NULL)
            return;
        snprintf(newwindowpath, len, "%s", nums);
    } else {
        len = strlen(windowpath) + 1 + numn + 1;
        newwindowpath = malloc(len);
        if (newwindowpath == NULL)
            return;
        snprintf(newwindowpath, len, "%s:%s",
                 windowpath, nums);
    }
    if (setenv("WINDOWPATH", newwindowpath, TRUE) == -1)
        Error("unable to set WINDOWPATH");


    free(newwindowpath);
}

static int
startClient(char *client[])
{
    clientpid = fork();
    if (clientpid == 0) {
        set_environment();
        setWindowPath();

        if (setuid(getuid()) == -1) {
            Error("cannot change uid");
            _exit(EXIT_FAILURE);
        }
        setpgid(0, getpid());
        Execute(client);
        Error("Unable to run program \"%s\"", client[0]);

        fprintf(stderr, "Specify a program on the command line or make sure that %s\n", bindir);
        fprintf(stderr, "is in your path.\n\n");

        _exit(EXIT_FAILURE);
    } else {
        return clientpid;
    }
}

static jmp_buf close_env;

static int
ignorexio(Display *dpy)
{
    Errorx("connection to X server lost");
    longjmp(close_env, 1);
    /*NOTREACHED*/
    return 0;
}

static void
shutdown(void)
{
    /* have kept display opened, so close it now */
    if (clientpid > 0) {
        XSetIOErrorHandler(ignorexio);
        if (! setjmp(close_env)) {
            XCloseDisplay(xd);
        }

        /* HUP all local clients to allow them to clean up */
        if (killpg(clientpid, SIGHUP) < 0 && errno != ESRCH)
            Error("can't send HUP to process group %d", clientpid);
    }

    if (serverpid < 0)
        return;

    if (killpg(serverpid, SIGTERM) < 0) {
        if (errno == ESRCH)
            return;
        Fatal("can't kill X server");
    }

    if (!processTimeout(10, "X server to shut down"))
        return;

    Errorx("X server slow to shut down, senging KILL signal");

    if (killpg(serverpid, SIGKILL) < 0) {
        if (errno == ESRCH)
            return;
        Error("can't SIGKILL X server");
    }

    if (processTimeout(3, "server to die"))
        Fatalx("X server refuses to die");
}

static void
set_environment(void)
{
    if (setenv("DISPLAY", displayNum, TRUE) == -1)
        Fatal("unable to set DISPLAY");
}

static void
verror(const char *fmt, va_list ap)
{
    fprintf(stderr, "%s: ", program);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, ": %s\n", strerror(errno));
}

static void
verrorx(const char *fmt, va_list ap)
{
    fprintf(stderr, "%s: ", program);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
}

static void
Fatal(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    verror(fmt, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

static void
Fatalx(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    verrorx(fmt, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

static void
Error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    verror(fmt, ap);
    va_end(ap);
}

static void
Errorx(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    verrorx(fmt, ap);
    va_end(ap);
}
