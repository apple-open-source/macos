/*****************************************************************************

NAME:
   idle.c -- code for interruptible delays without sleep(3).

ENTRY POINTS:
   interruptible_idle() -- delay for some time, interruptible by signal.

THEORY:
   Sometimes you need more than one time delay per program, so alarm(3)
won't cut it.  This code illustrates time delays with select(2).

AUTHOR:
   Eric S. Raymond <esr@thyrsus.com>, 1997.  This source code example
is part of fetchmail and the Unix Cookbook, and are released under the
MIT license.  Compile with -DMAIN to build the demonstrator.

******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fetchmail.h>	/* for ROOT_UID */

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

volatile int lastsig;		/* last signal received */

#ifdef SLEEP_WITH_ALARM
/*
 * The function of this variable is to remove the window during which a
 * SIGALRM can hose the code (ALARM is triggered *before* pause() is called).
 * This is a bit of a kluge; the real right thing would use sigprocmask(),
 * sigsuspend().  This workaround lets the interval timer trigger the first
 * alarm after the required interval and will then generate alarms
 * seconds until it is certain that the critical section (ie., the window)
 * is exited.
 */
static sig_atomic_t	alarm_latch = FALSE;

RETSIGTYPE gotsigalrm(int sig)
{
    set_signal_handler(sig, gotsigalrm);
    lastsig = sig;
    alarm_latch = TRUE;
}
#endif /* SLEEP_WITH_ALARM */

#ifdef __EMX__
/* Various EMX-specific definitions */
static int itimerflag;

void itimerthread(void* dummy)
{
    if (outlevel >= O_VERBOSE)
	report(stderr, 
	       GT_("fetchmail: thread sleeping for %d sec.\n"), poll_interval);
    while(1)
    {
	_sleep2(poll_interval*1000);
	kill((getpid()), SIGALRM);
    }
}
#endif

int interruptible_idle(int seconds)
/* time for a pause in the action; return TRUE if awakened by signal */
{
    int awoken = FALSE;

#ifndef __EMX__
#ifdef SLEEP_WITH_ALARM		/* not normally on */
    /*
     * We can't use sleep(3) here because we need an alarm(3)
     * equivalent in order to implement server nonresponse timeout.
     * We'll just assume setitimer(2) is available since fetchmail
     * has to have a BSDoid socket layer to work at all.
     */
    /* 
     * This code stopped working under glibc-2, apparently due
     * to the change in signal(2) semantics.  (The siginterrupt
     * line, added later, should fix this problem.) John Stracke
     * <francis@netscape.com> wrote:
     *
     * The problem seems to be that, after hitting the interval
     * timer while talking to the server, the process no longer
     * responds to SIGALRM.  I put in printf()s to see when it
     * reached the pause() for the poll interval, and I checked
     * the return from setitimer(), and everything seemed to be
     * working fine, except that the pause() just ignored SIGALRM.
     * I thought maybe the itimer wasn't being fired, so I hit
     * it with a SIGALRM from the command line, and it ignored
     * that, too.  SIGUSR1 woke it up just fine, and it proceeded
     * to repoll--but, when the dummy server didn't respond, it
     * never timed out, and SIGALRM wouldn't make it.
     *
     * (continued below...)
     */
    {
    struct itimerval ntimeout;

    ntimeout.it_interval.tv_sec = 5; /* repeat alarm every 5 secs */
    ntimeout.it_interval.tv_usec = 0;
    ntimeout.it_value.tv_sec  = seconds;
    ntimeout.it_value.tv_usec = 0;

    alarm_latch = FALSE;
    set_signal_handler(SIGALRM, gotsigalrm);	/* first trap signals */
    setitimer(ITIMER_REAL,&ntimeout,NULL);	/* then start timer */
    /* there is a very small window between the next two lines */
    /* which could result in a deadlock.  But this will now be  */
    /* caught by periodic alarms (see it_interval) */
    if (!alarm_latch)
	pause();
    /* stop timer */
    ntimeout.it_interval.tv_sec = ntimeout.it_interval.tv_usec = 0;
    ntimeout.it_value.tv_sec  = ntimeout.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL,&ntimeout,NULL);	/* now stop timer */
    set_signal_handler(SIGALRM, SIG_IGN);
    }
#else
    /* 
     * So the workaround I used is to make it sleep by using
     * select() instead of setitimer()/pause().  select() is
     * perfectly happy being called with a timeout and
     * no file descriptors; it just sleeps until it hits the
     * timeout.  The only concern I had was that it might
     * implement its timeout with SIGALRM--there are some
     * Unices where this is done, because select() is a library
     * function--but apparently not.
     */
    {
    struct timeval timeout;

    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;
    do {
	lastsig = 0;
	select(0,0,0,0, &timeout);
    } while (lastsig == SIGCHLD);
    }
#endif
#else /* EMX */
    alarm_latch = FALSE;
    set_signal_handler(SIGALRM, gotsigalrm);
    _beginthread(itimerthread, NULL, 32768, NULL);
    /* see similar code above */
    if (!alarm_latch)
	pause();
    set_signal_handler(SIGALRM, SIG_IGN);
#endif /* ! EMX */
    if (lastsig == SIGUSR1 || ((seconds && getuid() == ROOT_UID)
	&& lastsig == SIGHUP))
       awoken = TRUE;

    /* now lock out interrupts again */
    set_signal_handler(SIGUSR1, SIG_IGN);
    if (getuid() == ROOT_UID)
	set_signal_handler(SIGHUP, SIG_IGN);

    return(awoken ? lastsig : 0);
}

#ifdef MAIN
int main(int argc, char **argv)
{
    for (;;)
    {
	printf("How may I serve you, master?\n");
	interruptible_idle(5);
    }
}
#endif /* MAIN */

/* idle.c ends here */
