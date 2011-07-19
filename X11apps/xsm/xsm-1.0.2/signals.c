/* $Xorg: signals.c,v 1.4 2001/02/09 02:06:01 xorgcvs Exp $ */
/******************************************************************************

Copyright 1994, 1998  The Open Group

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
******************************************************************************/
/* $XFree86: xc/programs/xsm/signals.c,v 3.5 2001/12/08 18:33:45 herrb Exp $ */

#include <stdlib.h>

#include <X11/Xos.h>
#include <X11/Xfuncs.h>
#include <X11/Intrinsic.h>

#include <X11/SM/SMlib.h>

#include "save.h"

#include <errno.h>
#ifdef USG
#ifndef __TYPES__
#include <sys/types.h>			/* forgot to protect it... */
#define __TYPES__
#endif /* __TYPES__ */
#else
#if defined(_POSIX_SOURCE) && defined(MOTOROLA)
#undef _POSIX_SOURCE
#include <sys/types.h>
#define _POSIX_SOURCE
#else
#include <sys/types.h>
#endif
#endif /* USG */

#ifdef X_POSIX_C_SOURCE
#define _POSIX_C_SOURCE X_POSIX_C_SOURCE
#include <signal.h>
#include <sys/wait.h>
#undef _POSIX_C_SOURCE
#else
#if defined(X_NOT_POSIX) || defined(_POSIX_SOURCE)
#include <signal.h>
#include <sys/wait.h>
#else
#define _POSIX_SOURCE
#include <signal.h>
#ifdef SCO325
#include <sys/procset.h>
#include <sys/siginfo.h>
#endif
#include <sys/wait.h>
#undef _POSIX_SOURCE
#endif
#endif
#include "list.h"
#include "save.h"

#if defined(X_NOT_POSIX) && defined(SIGNALRETURNSINT)
#define SIGVAL int
#else
#define SIGVAL void
#endif

#ifndef X_NOT_POSIX
#define USE_POSIX_WAIT
#endif

#if defined(linux) || defined(SYSV)
#define USE_SYSV_SIGNALS
#endif

#if defined(SCO) || defined(ISC)
#undef SIGTSTP			/* defined, but not the BSD way */
#endif

#if defined(X_NOT_POSIX) && defined(SYSV)
#define SIGNALS_RESET_WHEN_CAUGHT
#endif

#include <stddef.h>

#include "xsm.h"

int checkpoint_from_signal = 0;


static SIGVAL
Signal(int sig, SIGVAL (*handler)(int))
{
#ifndef X_NOT_POSIX
    struct sigaction sigact, osigact;
    sigact.sa_handler = handler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(sig, &sigact, &osigact);
#  if defined(SIGNALRETURNSINT)
    return osigact.sa_handler;
#  endif
#else
#  if defined(SIGNALRETURNSINT)
    return
#  endif
    signal(sig, handler);
#endif
}


void
sig_child_handler (int sig)

{
    int pid, olderrno = errno;

#if !defined(USE_POSIX_WAIT) && (defined(USE_SYSV_SIGNALS) && \
    (defined(CRAY) || !defined(SIGTSTP)))
    wait (NULL);
#endif

#ifdef SIGNALS_RESET_WHEN_CAUGHT
    Signal (SIGCHLD, sig_child_handler);
#endif

    /*
     * The wait() above must come before re-establishing the signal handler.
     * In between this time, a new child might have died.  If we can do
     * a non-blocking wait, we can check for this race condition.  If we
     * don't have non-blocking wait, we lose.
     */

    do
    {
#ifdef USE_POSIX_WAIT
	pid = waitpid (-1, NULL, WNOHANG);
#else
#if defined(USE_SYSV_SIGNALS) && (defined(CRAY) || !defined(SIGTSTP))
	/* cannot do non-blocking wait */
	pid = 0;
#else
	union wait status;

	pid = wait3 (&status, WNOHANG, (struct rusage *)NULL);
#endif
#endif /* USE_POSIX_WAIT else */
    }
    while (pid > 0);
    errno = olderrno;
}


void 
sig_term_handler(int sig)
{
    XtNoticeSignal(sig_term_id);
}

void
xt_sig_term_handler (XtPointer closure, XtSignalId *id)

{
    wantShutdown = 1;
    checkpoint_from_signal = 1;
    DoSave (SmSaveLocal, SmInteractStyleNone, 1 /* fast */);
}

void sig_usr1_handler(int sig)
{
    XtNoticeSignal(sig_usr1_id);
}

void
xt_sig_usr1_handler (XtPointer closure, XtSignalId *id)

{
    wantShutdown = 0;
    checkpoint_from_signal = 1;
    DoSave (SmSaveLocal, SmInteractStyleNone, 0 /* fast */);
}



void
register_signals (XtAppContext appContext)

{
    /*
     * Ignore SIGPIPE
     */

    Signal (SIGPIPE, SIG_IGN);


    /*
     * If child process dies, call our handler
     */

    Signal (SIGCHLD, sig_child_handler);


    /*
     * If we get a SIGTERM, do shutdown, fast, local, no interact
     */

    Signal (SIGTERM, sig_term_handler);
    sig_term_id = XtAppAddSignal(appContext, xt_sig_term_handler, NULL);


    /*
     * If we get a SIGUSR1, do checkpoint, local, no interact
     */

    Signal (SIGUSR1, sig_usr1_handler);
    sig_usr1_id = XtAppAddSignal(appContext, xt_sig_usr1_handler, NULL);
}



int
execute_system_command (char *s)
{
    int stat;

#ifdef X_NOT_POSIX
    /*
     * Non-POSIX system() uses wait().  We must disable our sig child
     * handler because if it catches the signal, system() will block
     * forever in wait().
     */

    int pid;

    Signal (SIGCHLD, SIG_IGN);
#endif

    stat = system (s);

#ifdef X_NOT_POSIX
    /*
     * Re-enable our sig child handler.  We might have missed some signals,
     * so do non-blocking waits until there are no signals left.
     */

    Signal (SIGCHLD, sig_child_handler);

#if !(defined(USE_SYSV_SIGNALS) && (defined(CRAY) || !defined(SIGTSTP)))
    do
    {
	union wait status;

	pid = wait3 (&status, WNOHANG, (struct rusage *)NULL);
    } while (pid > 0);
#endif
#endif   /* X_NOT_POSIX */

    return (stat);
}


