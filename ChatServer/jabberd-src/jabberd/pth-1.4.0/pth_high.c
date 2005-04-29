/*
**  GNU Pth - The GNU Portable Threads
**  Copyright (c) 1999-2001 Ralf S. Engelschall <rse@engelschall.com>
**
**  This file is part of GNU Pth, a non-preemptive thread scheduling
**  library which can be found at http://www.gnu.org/software/pth/.
**
**  This library is free software; you can redistribute it and/or
**  modify it under the terms of the GNU Lesser General Public
**  License as published by the Free Software Foundation; either
**  version 2.1 of the License, or (at your option) any later version.
**
**  This library is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
**  Lesser General Public License for more details.
**
**  You should have received a copy of the GNU Lesser General Public
**  License along with this library; if not, write to the Free Software
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
**  USA, or contact Ralf S. Engelschall <rse@engelschall.com>.
**
**  pth_high.c: Pth high-level replacement functions
*/
                             /* ``The difference between a computer
                                  industry job and open-source software
                                  hacking is about 30 hours a week.''
                                         -- Ralf S. Engelschall     */

/*
 *  These functions used by the applications instead of the
 *  regular Unix/POSIX functions. When the regular functions would
 *  block, these variants let only the thread sleep.
 */

#include "pth_p.h"

/* Pth variant of usleep(3) */
int pth_usleep(unsigned int usec)
{
    pth_time_t until;
    pth_time_t offset;
    pth_event_t ev;
    static pth_key_t ev_key = PTH_KEY_INIT;

    /* short-circuit */
    if (usec == 0)
        return 0;

    /* calculate asleep time */
    offset = pth_time((long)(usec / 1000000), (long)(usec % 1000000));
    pth_time_set(&until, PTH_TIME_NOW);
    pth_time_add(&until, &offset);

    /* and let thread sleep until this time is elapsed */
    ev = pth_event(PTH_EVENT_TIME|PTH_MODE_STATIC, &ev_key, until);
    pth_wait(ev);

    return 0;
}

/* Pth variant of sleep(3) */
unsigned int pth_sleep(unsigned int sec)
{
    pth_time_t until;
    pth_time_t offset;
    pth_event_t ev;
    static pth_key_t ev_key = PTH_KEY_INIT;

    /* consistency check */
    if (sec == 0)
        return 0;

    /* calculate asleep time */
    offset = pth_time(sec, 0);
    pth_time_set(&until, PTH_TIME_NOW);
    pth_time_add(&until, &offset);

    /* and let thread sleep until this time is elapsed */
    ev = pth_event(PTH_EVENT_TIME|PTH_MODE_STATIC, &ev_key, until);
    pth_wait(ev);

    return 0;
}

/* Pth variant of POSIX pthread_sigmask(3) */
int pth_sigmask(int how, const sigset_t *set, sigset_t *oset)
{
    int rv;

    /* change the explicitly remembered signal mask copy for the scheduler */
    if (set != NULL)
        pth_sc(sigprocmask)(how, &(pth_current->mctx.sigs), NULL);

    /* change the real (per-thread saved/restored) signal mask */
    rv = pth_sc(sigprocmask)(how, set, oset);

    return rv;
}

/* Pth variant of POSIX sigwait(3) */
int pth_sigwait(const sigset_t *set, int *sigp)
{
    return pth_sigwait_ev(set, sigp, NULL);
}

/* Pth variant of POSIX sigwait(3) with extra events */
int pth_sigwait_ev(const sigset_t *set, int *sigp, pth_event_t ev_extra)
{
    pth_event_t ev;
    static pth_key_t ev_key = PTH_KEY_INIT;
    sigset_t pending;
    int sig;

    if (set == NULL || sigp == NULL)
        return_errno(EINVAL, EINVAL);

    /* check whether signal is already pending */
    if (sigpending(&pending) < 0)
        sigemptyset(&pending);
    for (sig = 1; sig < PTH_NSIG; sig++) {
        if (sigismember(set, sig) && sigismember(&pending, sig)) {
            pth_util_sigdelete(sig);
            *sigp = sig;
            return 0;
        }
    }

    /* create event and wait on it */
    ev = pth_event(PTH_EVENT_SIGS|PTH_MODE_STATIC, &ev_key, set, sigp);
    if (ev_extra != NULL)
        pth_event_concat(ev, ev_extra, NULL);
    pth_wait(ev);
    if (ev_extra != NULL) {
        pth_event_isolate(ev);
        if (!pth_event_occurred(ev))
            return_errno(EINTR, EINTR);
    }

    /* nothing to do, scheduler has already set *sigp for us */
    return 0;
}

/* Pth variant of waitpid(2) */
pid_t pth_waitpid(pid_t wpid, int *status, int options)
{
    pth_event_t ev;
    static pth_key_t ev_key = PTH_KEY_INIT;
    pid_t pid;

    pth_debug2("pth_waitpid: called from thread \"%s\"", pth_current->name);

    for (;;) {
        /* do a non-blocking poll for the pid */
        while (   (pid = pth_sc(waitpid)(wpid, status, options|WNOHANG)) < 0 
               && errno == EINTR) ;

        /* if pid was found or caller requested a polling return immediately */
        if (pid == -1 || pid > 0 || (pid == 0 && (options & WNOHANG)))
            break;

        /* else wait a little bit */
        ev = pth_event(PTH_EVENT_TIME|PTH_MODE_STATIC, &ev_key, pth_timeout(0,250000));
        pth_wait(ev);
    }

    pth_debug2("pth_waitpid: leave to thread \"%s\"", pth_current->name);
    return pid;
}

/* Pth variant of system(3) */
int pth_system(const char *cmd)
{
    struct sigaction sa_ign, sa_int, sa_quit;
    sigset_t ss_block, ss_old;
    struct stat sb;
    pid_t pid;
    int pstat;

    /* POSIX calling convention: determine whether the
       Bourne Shell ("sh") is available on this platform */
    if (cmd == NULL) {
        if (stat(PTH_PATH_BINSH, &sb) == -1)
            return 0;
        return 1;
    }

    /* temporarily ignore SIGINT and SIGQUIT actions */
    sa_ign.sa_handler = SIG_IGN;
    sigemptyset(&sa_ign.sa_mask);
    sa_ign.sa_flags = 0;
    sigaction(SIGINT,  &sa_ign, &sa_int);
    sigaction(SIGQUIT, &sa_ign, &sa_quit);

    /* block SIGCHLD signal */
    sigemptyset(&ss_block);
    sigaddset(&ss_block, SIGCHLD);
    sigprocmask(SIG_BLOCK, &ss_block, &ss_old);

    /* fork the current process */
    pstat = -1;
    switch (pid = pth_fork()) {
        case -1: /* error */
            break;

        case 0:  /* child */
            /* restore original signal dispositions and execute the command */
            sigaction(SIGINT,  &sa_int,  NULL);
            sigaction(SIGQUIT, &sa_quit, NULL);
            sigprocmask(SIG_SETMASK, &ss_old, NULL);

            /* stop the Pth scheduling */
            pth_scheduler_kill();

            /* execute the command through Bourne Shell */
            execl(PTH_PATH_BINSH, "sh", "-c", cmd, NULL);

            /* POSIX compliant return in case execution failed */
            exit(127);

        default: /* parent */
            /* wait until child process terminates */
            pid = pth_waitpid(pid, &pstat, 0);
            break;
    }

    /* restore original signal dispositions and execute the command */
    sigaction(SIGINT,  &sa_int,  NULL);
    sigaction(SIGQUIT, &sa_quit, NULL);
    sigprocmask(SIG_SETMASK, &ss_old, NULL);

    /* return error or child process result code */
    return (pid == -1 ? -1 : pstat);
}

/* Pth variant of select(2) */
int pth_select(int nfds, fd_set *readfds, fd_set *writefds,
               fd_set *exceptfds, struct timeval *timeout)
{
    return pth_select_ev(nfds, readfds, writefds, exceptfds, timeout, NULL);
}

/* Pth variant of select(2) with extra events */
int pth_select_ev(int nfd, fd_set *rfds, fd_set *wfds,
                  fd_set *efds, struct timeval *timeout, pth_event_t ev_extra)
{
    struct timeval delay;
    pth_event_t ev;
    pth_event_t ev_select;
    pth_event_t ev_timeout;
    static pth_key_t ev_key_select  = PTH_KEY_INIT;
    static pth_key_t ev_key_timeout = PTH_KEY_INIT;
    fd_set rspare, wspare, espare;
    fd_set *rtmp, *wtmp, *etmp;
    int selected;
    int rc;

    pth_implicit_init();
    pth_debug2("pth_select_ev: called from thread \"%s\"", pth_current->name);

    /* first deal with the special situation of a plain microsecond delay */
    if (nfd == 0 && rfds == NULL && wfds == NULL && efds == NULL && timeout != NULL) {
        if (timeout->tv_sec < 0 || timeout->tv_usec < 0)
            return_errno(-1, EINVAL);
        if (timeout->tv_sec == 0 && timeout->tv_usec < 500000) {
            /* very small delays are acceptable to be performed directly */
            while (   pth_sc(select)(0, NULL, NULL, NULL, timeout) < 0
                   && errno == EINTR) ;
        }
        else {
            /* larger delays have to use the scheduler */
            ev = pth_event(PTH_EVENT_TIME|PTH_MODE_STATIC, &ev_key_timeout,
                           pth_timeout(timeout->tv_sec, timeout->tv_usec));
            if (ev_extra != NULL)
                pth_event_concat(ev, ev_extra, NULL);
            pth_wait(ev);
            if (ev_extra != NULL) {
                pth_event_isolate(ev);
                if (!pth_event_occurred(ev))
                    return_errno(-1, EINTR);
            }
        }
        /* POSIX compliance */
        if (rfds != NULL) FD_ZERO(rfds);
        if (wfds != NULL) FD_ZERO(wfds);
        if (efds != NULL) FD_ZERO(efds);
        return 0;
    }

    /* now directly poll filedescriptor sets to avoid unneccessary
       (and resource consuming because of context switches, etc) event
       handling through the scheduler. We've to be carefully here, because not
       all platforms guarranty us that the sets are unmodified when an error
       or timeout occured. */
    delay.tv_sec  = 0;
    delay.tv_usec = 0;
    rtmp = NULL;
    if (rfds != NULL) {
        rspare = *rfds;
        rtmp = &rspare;
    }
    wtmp = NULL;
    if (wfds != NULL) {
        wspare = *wfds;
        wtmp = &wspare;
    }
    etmp = NULL;
    if (efds != NULL) {
        espare = *efds;
        etmp = &espare;
    }
    while ((rc = pth_sc(select)(nfd, rtmp, wtmp, etmp, &delay)) < 0 && errno == EINTR) ;
    if (rc > 0) {
        if (rfds != NULL)
            *rfds = rspare;
        if (wfds != NULL)
            *wfds = wspare;
        if (efds != NULL)
            *efds = espare;
        return rc;
    }
    if (rc == 0 && timeout != NULL) {
        if (pth_time_cmp(timeout, PTH_TIME_ZERO) == 0) {
            /* POSIX compliance */
            if (rfds != NULL) FD_ZERO(rfds);
            if (wfds != NULL) FD_ZERO(wfds);
            if (efds != NULL) FD_ZERO(efds);
            return 0;
        }
    }

    /* suspend current thread until one fd is ready or the timeout occurred */
    rc = -1;
    ev = ev_select = pth_event(PTH_EVENT_SELECT|PTH_MODE_STATIC,
                               &ev_key_select, &rc, nfd, rfds, wfds, efds);
    ev_timeout = NULL;
    if (timeout != NULL) {
        ev_timeout = pth_event(PTH_EVENT_TIME|PTH_MODE_STATIC, &ev_key_timeout,
                               pth_timeout(timeout->tv_sec, timeout->tv_usec));
        pth_event_concat(ev, ev_timeout, NULL);
    }
    if (ev_extra != NULL)
        pth_event_concat(ev, ev_extra, NULL);
    pth_wait(ev);
    selected = FALSE;
    pth_event_isolate(ev_select);
    if (pth_event_occurred(ev_select))
        selected = TRUE;
    if (timeout != NULL) {
        pth_event_isolate(ev_timeout);
        if (pth_event_occurred(ev_timeout)) {
            selected = TRUE;
            /* POSIX compliance */
            if (rfds != NULL) FD_ZERO(rfds);
            if (wfds != NULL) FD_ZERO(wfds);
            if (efds != NULL) FD_ZERO(efds);
            rc = 0;
        }
    }
    if (ev_extra != NULL && !selected)
        return_errno(-1, EINTR);
    return rc;
}

/* Pth variant of select(2) */
int pth_poll(struct pollfd *pfd, nfds_t nfd, int timeout)
{
    return pth_poll_ev(pfd, nfd, timeout, NULL);
}

/* Pth variant of poll(2) with extra events:
   NOTICE: THIS HAS TO BE BASED ON pth_select(2) BECAUSE
           INTERNALLY THE SCHEDULER IS ONLY select(2) BASED!! */
int pth_poll_ev(struct pollfd *pfd, nfds_t nfd, int timeout, pth_event_t ev_extra)
{
    fd_set rfds, wfds, efds;
    struct timeval tv, *ptv;
    int maxfd, rc, ok;
    unsigned int i;
    char data[64];

    pth_implicit_init();
    pth_debug2("pth_poll_ev: called from thread \"%s\"", pth_current->name);

    /* poll(2) semantics */
    if (pfd == NULL)
        return_errno(-1, EFAULT);

    /* convert timeout number into a timeval structure */
    ptv = &tv;
    if (timeout == 0) {
        /* return immediately */
        ptv->tv_sec  = 0;
        ptv->tv_usec = 0;
    }
    else if (timeout == INFTIM /* (-1) */) {
        /* wait forever */
        ptv = NULL;
    }
    else if (timeout > 0) {
        /* return after timeout */
        ptv->tv_sec  = timeout / 1000;
        ptv->tv_usec = (timeout % 1000) * 1000;
    }
    else
        return_errno(-1, EINVAL);

    /* create fd sets and determine max fd */
    maxfd = -1;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&efds);
    for(i = 0; i < nfd; i++) {
        if (pfd[i].fd < 0)
            continue;
        if (pfd[i].events & POLLIN)
            FD_SET(pfd[i].fd, &rfds);
        if (pfd[i].events & POLLOUT)
            FD_SET(pfd[i].fd, &wfds);
        if (pfd[i].events & POLLPRI)
            FD_SET(pfd[i].fd, &efds);
        if (pfd[i].fd >= maxfd && (pfd[i].events & (POLLIN|POLLOUT|POLLPRI)))
            maxfd = pfd[i].fd;
    }
    if (maxfd == -1)
        return_errno(-1, EINVAL);

    /* examine fd sets */
    rc = pth_select_ev(maxfd+1, &rfds, &wfds, &efds, ptv, ev_extra);

    /* establish results */
    if (rc > 0) {
        rc = 0;
        for (i = 0; i < nfd; i++) {
            ok = 0;
            pfd[i].revents = 0;
            if (pfd[i].fd < 0) {
                pfd[i].revents |= POLLNVAL;
                continue;
            }
            if (FD_ISSET(pfd[i].fd, &rfds)) {
                pfd[i].revents |= POLLIN;
                ok++;
                /* support for POLLHUP */
                if (recv(pfd[i].fd, data, 64, MSG_PEEK) == -1) {
                    if (   errno == ESHUTDOWN    || errno == ECONNRESET
                        || errno == ECONNABORTED || errno == ENETRESET) {
                        pfd[i].revents &= ~(POLLIN);
                        pfd[i].revents |= POLLHUP;
                        ok--;
                    }
                }
            }
            if (FD_ISSET(pfd[i].fd, &wfds)) {
                pfd[i].revents |= POLLOUT;
                ok++;
            }
            if (FD_ISSET(pfd[i].fd, &efds)) {
                pfd[i].revents |= POLLPRI;
                ok++;
            }
            if (ok)
                rc++;
        }
    }
    return rc;
}

/* Pth variant of connect(2) */
int pth_connect(int s, const struct sockaddr *addr, socklen_t addrlen)
{
    return pth_connect_ev(s, addr, addrlen, NULL);
}

/* Pth variant of connect(2) with extra events */
int pth_connect_ev(int s, const struct sockaddr *addr, socklen_t addrlen, pth_event_t ev_extra)
{
    pth_event_t ev;
    static pth_key_t ev_key = PTH_KEY_INIT;
    int rv, err;
    socklen_t errlen;
    int fdmode;

    pth_implicit_init();
    pth_debug2("pth_connect_ev: enter from thread \"%s\"", pth_current->name);

    /* force filedescriptor into non-blocking mode */
    fdmode = pth_fdmode(s, PTH_FDMODE_NONBLOCK);

    /* try to connect */
    while (   (rv = pth_sc(connect)(s, (struct sockaddr *)addr, addrlen)) == -1
           && errno == EINTR)
        ;

    /* restore filedescriptor mode */
    errno_shield { pth_fdmode(s, fdmode); }

    /* when it is still on progress wait until socket is really writeable */
    if (rv == -1 && errno == EINPROGRESS) {
        ev = pth_event(PTH_EVENT_FD|PTH_UNTIL_FD_WRITEABLE|PTH_MODE_STATIC, &ev_key, s);
        if (ev_extra != NULL)
            pth_event_concat(ev, ev_extra, NULL);
        pth_wait(ev);
        if (ev_extra != NULL) {
            pth_event_isolate(ev);
            if (!pth_event_occurred(ev))
                return_errno(-1, EINTR);
        }
        errlen = sizeof(err);
        if (getsockopt(s, SOL_SOCKET, SO_ERROR, (void *)&err, &errlen) == -1)
            return -1;
        if (err == 0)
            return 0;
        return_errno(rv, err);
    }

    pth_debug2("pth_connect_ev: leave to thread \"%s\"", pth_current->name);
    return rv;
}

/* Pth variant of accept(2) */
int pth_accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    return pth_accept_ev(s, addr, addrlen, NULL);
}

/* Pth variant of accept(2) with extra events */
int pth_accept_ev(int s, struct sockaddr *addr, socklen_t *addrlen, pth_event_t ev_extra)
{
    pth_event_t ev;
    static pth_key_t ev_key = PTH_KEY_INIT;
    int fdmode;
    int rv;

    pth_implicit_init();
    pth_debug2("pth_accept_ev: enter from thread \"%s\"", pth_current->name);

    /* force filedescriptor into non-blocking mode */
    fdmode = pth_fdmode(s, PTH_FDMODE_NONBLOCK);

    /* poll socket via accept */
    ev = NULL;
    while ((rv = pth_sc(accept)(s, addr, addrlen)) == -1
           && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        /* do lazy event allocation */
        if (ev == NULL) {
            ev = pth_event(PTH_EVENT_FD|PTH_UNTIL_FD_READABLE|PTH_MODE_STATIC, &ev_key, s);
            if (ev_extra != NULL)
                pth_event_concat(ev, ev_extra, NULL);
        }
        /* wait until accept has a chance */
        pth_wait(ev);
        /* check for the extra events */
        if (ev_extra != NULL) {
            pth_event_isolate(ev);
            if (!pth_event_occurred(ev)) {
                pth_fdmode(s, fdmode);
                return_errno(-1, EINTR);
            }
        }
    }

    /* restore filedescriptor mode */
    errno_shield {
        pth_fdmode(s, fdmode);
        if (rv != -1)
            pth_fdmode(rv, fdmode);
    }

    pth_debug2("pth_accept_ev: leave to thread \"%s\"", pth_current->name);
    return rv;
}

/* Pth variant of read(2) */
ssize_t pth_read(int fd, void *buf, size_t nbytes)
{
    return pth_read_ev(fd, buf, nbytes, NULL);
}

/* Pth variant of read(2) with extra event(s) */
ssize_t pth_read_ev(int fd, void *buf, size_t nbytes, pth_event_t ev_extra)
{
    struct timeval delay;
    pth_event_t ev;
    static pth_key_t ev_key = PTH_KEY_INIT;
    fd_set fds;
    int n;

    pth_implicit_init();
    pth_debug2("pth_read_ev: enter from thread \"%s\"", pth_current->name);

    /* POSIX compliance */
    if (nbytes == 0)
        return 0;

    /* poll filedescriptor when not already in non-blocking operation */
    if (pth_fdmode(fd, PTH_FDMODE_POLL) == PTH_FDMODE_BLOCK) {

        /* now directly poll filedescriptor for readability
           to avoid unneccessary (and resource consuming because of context
           switches, etc) event handling through the scheduler */
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        delay.tv_sec  = 0;
        delay.tv_usec = 0;
        while ((n = pth_sc(select)(fd+1, &fds, NULL, NULL, &delay)) < 0
               && errno == EINTR) ;

        /* when filedescriptor is still not readable,
           let thread sleep until it is or the extra event occurs */
        if (n < 1) {
            ev = pth_event(PTH_EVENT_FD|PTH_UNTIL_FD_READABLE|PTH_MODE_STATIC, &ev_key, fd);
            if (ev_extra != NULL)
                pth_event_concat(ev, ev_extra, NULL);
            n = pth_wait(ev);
            if (ev_extra != NULL) {
                pth_event_isolate(ev);
                if (!pth_event_occurred(ev))
                    return_errno(-1, EINTR);
            }
        }
    }

    /* Now perform the actual read. We're now guarrantied to not block,
       either because we were already in non-blocking mode or we determined
       above by polling that the next read(2) call will not block.  But keep
       in mind, that only 1 next read(2) call is guarrantied to not block
       (except for the EINTR situation). */
    while ((n = pth_sc(read)(fd, buf, nbytes)) < 0
           && errno == EINTR) ;

    pth_debug2("pth_read_ev: leave to thread \"%s\"", pth_current->name);
    return n;
}

/* Pth variant of write(2) */
ssize_t pth_write(int fd, const void *buf, size_t nbytes)
{
    return pth_write_ev(fd, buf, nbytes, NULL);
}

/* Pth variant of write(2) with extra event(s) */
ssize_t pth_write_ev(int fd, const void *buf, size_t nbytes, pth_event_t ev_extra)
{
    struct timeval delay;
    pth_event_t ev;
    static pth_key_t ev_key = PTH_KEY_INIT;
    fd_set fds;
    int fdmode;
    ssize_t rv;
    ssize_t s;
    int n;

    pth_implicit_init();
    pth_debug2("pth_write_ev: enter from thread \"%s\"", pth_current->name);

    /* POSIX compliance */
    if (nbytes == 0)
        return 0;

    /* force filedescriptor into non-blocking mode */
    fdmode = pth_fdmode(fd, PTH_FDMODE_NONBLOCK);

    /* poll filedescriptor when not already in non-blocking operation */
    if (fdmode != PTH_FDMODE_NONBLOCK) {

        /* now directly poll filedescriptor for writeability
           to avoid unneccessary (and resource consuming because of context
           switches, etc) event handling through the scheduler */
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        delay.tv_sec  = 0;
        delay.tv_usec = 0;
        while ((n = pth_sc(select)(fd+1, NULL, &fds, NULL, &delay)) < 0
               && errno == EINTR) ;

        rv = 0;
        for (;;) {
            /* when filedescriptor is still not writeable,
               let thread sleep until it is or event occurs */
            if (n < 1) {
                ev = pth_event(PTH_EVENT_FD|PTH_UNTIL_FD_WRITEABLE|PTH_MODE_STATIC, &ev_key, fd);
                if (ev_extra != NULL)
                    pth_event_concat(ev, ev_extra, NULL);
                pth_wait(ev);
                if (ev_extra != NULL) {
                    pth_event_isolate(ev);
                    if (!pth_event_occurred(ev)) {
                        pth_fdmode(fd, fdmode);
                        return_errno(-1, EINTR);
                    }
                }
            }

            /* now perform the actual write operation */
            while ((s = pth_sc(write)(fd, buf, nbytes)) < 0
                   && errno == EINTR) ;
            if (s > 0)
                rv += s;

            /* although we're physically now in non-blocking mode,
               iterate unless all data is written or an error occurs, because
               we've to mimic the usual blocking I/O behaviour of write(2). */
            if (s > 0 && s < (ssize_t)nbytes) {
                nbytes -= s;
                buf = (void *)((char *)buf + s);
                n = 0;
                continue;
            }

            /* pass error to caller, but not for partial writes (rv > 0) */
            if (s < 0 && rv == 0)
                rv = -1;

            /* stop looping */
            break;
        }
    }
    else {
        /* Now perform the actual write operation */
        while ((rv = pth_sc(write)(fd, buf, nbytes)) < 0
               && errno == EINTR) ;
    }

    /* restore filedescriptor mode */
    errno_shield { pth_fdmode(fd, fdmode); }

    pth_debug2("pth_write_ev: leave to thread \"%s\"", pth_current->name);
    return rv;
}

/* Pth variant of readv(2) */
ssize_t pth_readv(int fd, const struct iovec *iov, int iovcnt)
{
    return pth_readv_ev(fd, iov, iovcnt, NULL);
}

/* Pth variant of readv(2) with extra event(s) */
ssize_t pth_readv_ev(int fd, const struct iovec *iov, int iovcnt, pth_event_t ev_extra)
{
    struct timeval delay;
    pth_event_t ev;
    static pth_key_t ev_key = PTH_KEY_INIT;
    fd_set fds;
    int n;

    pth_implicit_init();
    pth_debug2("pth_readv_ev: enter from thread \"%s\"", pth_current->name);

    /* POSIX compliance */
    if (iovcnt <= 0 || iovcnt > UIO_MAXIOV)
        return_errno(-1, EINVAL);

    /* poll filedescriptor when not already in non-blocking operation */
    if (pth_fdmode(fd, PTH_FDMODE_POLL) == PTH_FDMODE_BLOCK) {

        /* first directly poll filedescriptor for readability
           to avoid unneccessary (and resource consuming because of context
           switches, etc) event handling through the scheduler */
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        delay.tv_sec  = 0;
        delay.tv_usec = 0;
        while ((n = pth_sc(select)(fd+1, &fds, NULL, NULL, &delay)) < 0
               && errno == EINTR) ;

        /* when filedescriptor is still not readable,
           let thread sleep until it is or event occurs */
        if (n < 1) {
            ev = pth_event(PTH_EVENT_FD|PTH_UNTIL_FD_READABLE|PTH_MODE_STATIC, &ev_key, fd);
            if (ev_extra != NULL)
                pth_event_concat(ev, ev_extra, NULL);
            n = pth_wait(ev);
            if (ev_extra != NULL) {
                pth_event_isolate(ev);
                if (!pth_event_occurred(ev))
                    return_errno(-1, EINTR);
            }
        }
    }

    /* Now perform the actual read. We're now guarrantied to not block,
       either because we were already in non-blocking mode or we determined
       above by polling that the next read(2) call will not block.  But keep
       in mind, that only 1 next read(2) call is guarrantied to not block
       (except for the EINTR situation). */
#if PTH_FAKE_RWV
    while ((n = pth_readv_faked(fd, iov, iovcnt)) < 0
           && errno == EINTR) ;
#else
    while ((n = pth_sc(readv)(fd, iov, iovcnt)) < 0
           && errno == EINTR) ;
#endif

    pth_debug2("pth_readv_ev: leave to thread \"%s\"", pth_current->name);
    return n;
}

/* A faked version of readv(2) */
intern ssize_t pth_readv_faked(int fd, const struct iovec *iov, int iovcnt)
{
    char *buffer;
    size_t bytes, copy, rv;
    int i;

    /* determine total number of bytes to read */
    bytes = 0;
    for (i = 0; i < iovcnt; i++) {
        if (iov[i].iov_len <= 0)
            return_errno((ssize_t)(-1), EINVAL);
        bytes += iov[i].iov_len;
    }
    if (bytes <= 0)
        return_errno((ssize_t)(-1), EINVAL);

    /* allocate a temporary buffer */
    if ((buffer = (char *)malloc(bytes)) == NULL)
        return (ssize_t)(-1);

    /* read data into temporary buffer (caller guarrantied us to not block) */
    rv = pth_sc(read)(fd, buffer, bytes);

    /* scatter read data into callers vector */
    if (rv > 0) {
        bytes = rv;
        for (i = 0; i < iovcnt; i++) {
            copy = pth_util_min(iov[i].iov_len, bytes);
            memcpy(iov[i].iov_base, buffer, copy);
            buffer += copy;
            bytes  -= copy;
            if (bytes <= 0)
                break;
        }
    }

    /* remove the temporary buffer */
    errno_shield { free(buffer); }

    /* return number of read bytes */
    return(rv);
}

/* Pth variant of writev(2) */
ssize_t pth_writev(int fd, const struct iovec *iov, int iovcnt)
{
    return pth_writev_ev(fd, iov, iovcnt, NULL);
}

/* Pth variant of writev(2) with extra event(s) */
ssize_t pth_writev_ev(int fd, const struct iovec *iov, int iovcnt, pth_event_t ev_extra)
{
    struct timeval delay;
    pth_event_t ev;
    static pth_key_t ev_key = PTH_KEY_INIT;
    fd_set fds;
    int fdmode;
    struct iovec *liov;
    int liovcnt;
    size_t nbytes;
    ssize_t rv;
    ssize_t s;
    int n;

    pth_implicit_init();
    pth_debug2("pth_writev_ev: enter from thread \"%s\"", pth_current->name);

    /* POSIX compliance */
    if (iovcnt <= 0 || iovcnt > UIO_MAXIOV)
        return_errno(-1, EINVAL);

    /* force filedescriptor into non-blocking mode */
    fdmode = pth_fdmode(fd, PTH_FDMODE_NONBLOCK);

    /* poll filedescriptor when not already in non-blocking operation */
    if (fdmode != PTH_FDMODE_NONBLOCK) {

        /* init return value and number of bytes to write */
        rv      = 0;
        nbytes  = pth_writev_iov_bytes(iov, iovcnt);

        /* init local iovec structure */
        liov    = NULL;
        liovcnt = 0;
        pth_writev_iov_advance(iov, iovcnt, 0, &liov, &liovcnt);

        /* first directly poll filedescriptor for writeability
           to avoid unneccessary (and resource consuming because of context
           switches, etc) event handling through the scheduler */
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        delay.tv_sec  = 0;
        delay.tv_usec = 0;
        while ((n = pth_sc(select)(fd+1, NULL, &fds, NULL, &delay)) < 0
               && errno == EINTR) ;

        for (;;) {
            /* when filedescriptor is still not writeable,
               let thread sleep until it is or event occurs */
            if (n < 1) {
                ev = pth_event(PTH_EVENT_FD|PTH_UNTIL_FD_WRITEABLE|PTH_MODE_STATIC, &ev_key, fd);
                if (ev_extra != NULL)
                    pth_event_concat(ev, ev_extra, NULL);
                pth_wait(ev);
                if (ev_extra != NULL) {
                    pth_event_isolate(ev);
                    if (!pth_event_occurred(ev)) {
                        pth_fdmode(fd, fdmode);
                        return_errno(-1, EINTR);
                    }
                }
            }

            /* Now perform the actual write operation */
#if PTH_FAKE_RWV
            while ((s = pth_writev_faked(fd, liov, liovcnt)) < 0
                   && errno == EINTR) ;
#else
            while ((s = pth_sc(writev)(fd, liov, liovcnt)) < 0
                   && errno == EINTR) ;
#endif
            if (s > 0)
                rv += s;

            /* although we're physically now in non-blocking mode,
               iterate unless all data is written or an error occurs, because
               we've to mimic the usual blocking I/O behaviour of writev(2) */
            if (s > 0 && s < (ssize_t)nbytes) {
                nbytes -= s;
                pth_writev_iov_advance(iov, iovcnt, n, &liov, &liovcnt);
                n = 0;
                continue;
            }

            /* pass error to caller, but not for partial writes (rv > 0) */
            if (s < 0 && rv == 0)
                rv = -1;

            /* stop looping */
            break;
        }
    }
    else {
        /* Now perform the actual write operation */
#if PTH_FAKE_RWV
        while ((rv = pth_writev_faked(fd, iov, iovcnt)) < 0
               && errno == EINTR) ;
#else
        while ((rv = pth_sc(writev)(fd, iov, iovcnt)) < 0
               && errno == EINTR) ;
#endif
    }

    /* restore filedescriptor mode */
    errno_shield { pth_fdmode(fd, fdmode); }

    pth_debug2("pth_writev_ev: leave to thread \"%s\"", pth_current->name);
    return rv;
}

/* calculate number of bytes in a struct iovec */
intern ssize_t pth_writev_iov_bytes(const struct iovec *iov, int iovcnt)
{
    ssize_t bytes;
    int i;

    bytes = 0;
    for (i = 0; i < iovcnt; i++) {
        if (iov[i].iov_len <= 0)
            continue;
        bytes += iov[i].iov_len;
    }
    return bytes;
}

/* advance the virtual pointer of a struct iov */
intern void pth_writev_iov_advance(const struct iovec *riov, int riovcnt, size_t advance,
                                   struct iovec **liov, int *liovcnt)
{
    static struct iovec siov[UIO_MAXIOV];
    int i;

    if (*liov == NULL && *liovcnt == 0) {
        /* initialize with real (const) structure on first step */
        *liov = (struct iovec *)riov;
        *liovcnt = riovcnt;
    }
    if (advance > 0) {
        if (*liov == riov && *liovcnt == riovcnt) {
            /* reinitialize with a copy to be able to adjust it */
            *liov = &siov[0];
            for (i = 0; i < riovcnt; i++) {
                siov[i].iov_base = riov[i].iov_base;
                siov[i].iov_len  = riov[i].iov_len;
            }
        }
        /* advance the virtual pointer */
        while (*liovcnt > 0 && advance > 0) {
            if ((*liov)->iov_len > advance) {
                (*liov)->iov_base = (char *)((*liov)->iov_base) + advance;
                (*liov)->iov_len -= advance;
                break;
            }
            else {
                advance -= (*liov)->iov_len;
                (*liovcnt)--;
                (*liov)++;
            }
        }
    }
    return;
}

/* A faked version of writev(2) */
intern ssize_t pth_writev_faked(int fd, const struct iovec *iov, int iovcnt)
{
    char *buffer, *cp;
    size_t bytes, to_copy, copy, rv;
    int i;

    /* determine total number of bytes to write */
    bytes = 0;
    for (i = 0; i < iovcnt; i++) {
        if (iov[i].iov_len <= 0)
            return_errno((ssize_t)(-1), EINVAL);
        bytes += iov[i].iov_len;
    }
    if (bytes <= 0)
        return_errno((ssize_t)(-1), EINVAL);

    /* allocate a temporary buffer to hold the data */
    if ((buffer = (char *)malloc(bytes)) == NULL)
        return (ssize_t)(-1);

    /* concatenate the data from callers vector into buffer */
    to_copy = bytes;
    cp = buffer;
    for (i = 0; i < iovcnt; i++) {
         copy = pth_util_min(iov[i].iov_len, to_copy);
         memcpy(cp, iov[i].iov_base, copy);
         to_copy -= copy;
         if (to_copy <= 0)
             break;
    }

    /* write continuous chunck of data (caller guarrantied us to not block) */
    rv = pth_sc(write)(fd, buffer, bytes);

    /* remove the temporary buffer */
    errno_shield { free(buffer); }

    return(rv);
}

/* Pth variant of POSIX pread(3) */
ssize_t pth_pread(int fd, void *buf, size_t nbytes, off_t offset)
{
    static pth_mutex_t mutex = PTH_MUTEX_INIT;
    off_t old_offset;
    ssize_t rc;

    /* protect us: pth_read can yield! */
    if (!pth_mutex_acquire(&mutex, FALSE, NULL))
        return (-1);

    /* remember current offset */
    if ((old_offset = lseek(fd, 0, SEEK_CUR)) == (off_t)(-1)) {
        pth_mutex_release(&mutex);
        return (-1);
    }
    /* seek to requested offset */
    if (lseek(fd, offset, SEEK_SET) == (off_t)(-1)) {
        pth_mutex_release(&mutex);
        return (-1);
    }

    /* perform the read operation */
    rc = pth_read(fd, buf, nbytes);

    /* restore the old offset situation */
    errno_shield { lseek(fd, old_offset, SEEK_SET); }

    /* unprotect and return result of read */
    pth_mutex_release(&mutex);
    return rc;
}

/* Pth variant of POSIX pwrite(3) */
ssize_t pth_pwrite(int fd, const void *buf, size_t nbytes, off_t offset)
{
    static pth_mutex_t mutex = PTH_MUTEX_INIT;
    off_t old_offset;
    ssize_t rc;

    /* protect us: pth_write can yield! */
    if (!pth_mutex_acquire(&mutex, FALSE, NULL))
        return (-1);

    /* remember current offset */
    if ((old_offset = lseek(fd, 0, SEEK_CUR)) == (off_t)(-1)) {
        pth_mutex_release(&mutex);
        return (-1);
    }
    /* seek to requested offset */
    if (lseek(fd, offset, SEEK_SET) == (off_t)(-1)) {
        pth_mutex_release(&mutex);
        return (-1);
    }

    /* perform the write operation */
    rc = pth_write(fd, buf, nbytes);

    /* restore the old offset situation */
    errno_shield { lseek(fd, old_offset, SEEK_SET); }

    /* unprotect and return result of write */
    pth_mutex_release(&mutex);
    return rc;
}

/* Pth variant of SUSv2 recv(2) */
ssize_t pth_recv(int s, void *buf, size_t len, int flags)
{
    return pth_recv_ev(s, buf, len, flags, NULL);
}

/* Pth variant of SUSv2 recv(2) with extra event(s) */
ssize_t pth_recv_ev(int s, void *buf, size_t len, int flags, pth_event_t ev)
{
    return pth_recvfrom_ev(s, buf, len, flags, NULL, 0, ev);
}

/* Pth variant of SUSv2 recvfrom(2) */
ssize_t pth_recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
    return pth_recvfrom_ev(s, buf, len, flags, from, fromlen, NULL);
}

/* Pth variant of SUSv2 recvfrom(2) with extra event(s) */
ssize_t pth_recvfrom_ev(int fd, void *buf, size_t nbytes, int flags, struct sockaddr *from, socklen_t *fromlen, pth_event_t ev_extra)
{
    struct timeval delay;
    pth_event_t ev;
    static pth_key_t ev_key = PTH_KEY_INIT;
    fd_set fds;
    int n;

    pth_implicit_init();
    pth_debug2("pth_recvfrom_ev: enter from thread \"%s\"", pth_current->name);

    /* POSIX compliance */
    if (nbytes == 0)
        return 0;

    /* poll filedescriptor when not already in non-blocking operation */
    if (pth_fdmode(fd, PTH_FDMODE_POLL) == PTH_FDMODE_BLOCK) {

        /* now directly poll filedescriptor for readability
           to avoid unneccessary (and resource consuming because of context
           switches, etc) event handling through the scheduler */
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        delay.tv_sec  = 0;
        delay.tv_usec = 0;
        while ((n = pth_sc(select)(fd+1, &fds, NULL, NULL, &delay)) < 0
               && errno == EINTR) ;

        /* when filedescriptor is still not readable,
           let thread sleep until it is or the extra event occurs */
        if (n < 1) {
            ev = pth_event(PTH_EVENT_FD|PTH_UNTIL_FD_READABLE|PTH_MODE_STATIC, &ev_key, fd);
            if (ev_extra != NULL)
                pth_event_concat(ev, ev_extra, NULL);
            n = pth_wait(ev);
            if (ev_extra != NULL) {
                pth_event_isolate(ev);
                if (!pth_event_occurred(ev))
                    return_errno(-1, EINTR);
            }
        }
    }

    /* Now perform the actual read. We're now guarrantied to not block,
       either because we were already in non-blocking mode or we determined
       above by polling that the next recvfrom(2) call will not block.  But keep
       in mind, that only 1 next recvfrom(2) call is guarrantied to not block
       (except for the EINTR situation). */
    while ((n = pth_sc(recvfrom)(fd, buf, nbytes, flags, from, fromlen)) < 0
           && errno == EINTR) ;

    pth_debug2("pth_recvfrom_ev: leave to thread \"%s\"", pth_current->name);
    return n;
}

/* Pth variant of SUSv2 send(2) */
ssize_t pth_send(int s, const void *buf, size_t len, int flags)
{
    return pth_send_ev(s, buf, len, flags, NULL);
}

/* Pth variant of SUSv2 send(2) with extra event(s) */
ssize_t pth_send_ev(int s, const void *buf, size_t len, int flags, pth_event_t ev)
{
    return pth_sendto_ev(s, buf, len, flags, NULL, 0, ev);
}

/* Pth variant of SUSv2 sendto(2) */
ssize_t pth_sendto(int s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen)
{
    return pth_sendto_ev(s, buf, len, flags, to, tolen, NULL);
}

/* Pth variant of SUSv2 sendto(2) with extra event(s) */
ssize_t pth_sendto_ev(int fd, const void *buf, size_t nbytes, int flags, const struct sockaddr *to, socklen_t tolen, pth_event_t ev_extra)
{
    struct timeval delay;
    pth_event_t ev;
    static pth_key_t ev_key = PTH_KEY_INIT;
    fd_set fds;
    int fdmode;
    ssize_t rv;
    ssize_t s;
    int n;

    pth_implicit_init();
    pth_debug2("pth_sendto_ev: enter from thread \"%s\"", pth_current->name);

    /* POSIX compliance */
    if (nbytes == 0)
        return 0;

    /* force filedescriptor into non-blocking mode */
    fdmode = pth_fdmode(fd, PTH_FDMODE_NONBLOCK);

    /* poll filedescriptor when not already in non-blocking operation */
    if (fdmode != PTH_FDMODE_NONBLOCK) {

        /* now directly poll filedescriptor for writeability
           to avoid unneccessary (and resource consuming because of context
           switches, etc) event handling through the scheduler */
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        delay.tv_sec  = 0;
        delay.tv_usec = 0;
        while ((n = pth_sc(select)(fd+1, NULL, &fds, NULL, &delay)) < 0
               && errno == EINTR) ;

        rv = 0;
        for (;;) {
            /* when filedescriptor is still not writeable,
               let thread sleep until it is or event occurs */
            if (n < 1) {
                ev = pth_event(PTH_EVENT_FD|PTH_UNTIL_FD_WRITEABLE|PTH_MODE_STATIC, &ev_key, fd);
                if (ev_extra != NULL)
                    pth_event_concat(ev, ev_extra, NULL);
                pth_wait(ev);
                if (ev_extra != NULL) {
                    pth_event_isolate(ev);
                    if (!pth_event_occurred(ev)) {
                        pth_fdmode(fd, fdmode);
                        return_errno(-1, EINTR);
                    }
                }
            }

            /* now perform the actual send operation */
            while ((s = pth_sc(sendto)(fd, buf, nbytes, flags, to, tolen)) < 0
                   && errno == EINTR) ;
            if (s > 0)
                rv += s;

            /* although we're physically now in non-blocking mode,
               iterate unless all data is written or an error occurs, because
               we've to mimic the usual blocking I/O behaviour of write(2). */
            if (s > 0 && s < (ssize_t)nbytes) {
                nbytes -= s;
                buf = (void *)((char *)buf + s);
                n = 0;
                continue;
            }

            /* pass error to caller, but not for partial writes (rv > 0) */
            if (s < 0 && rv == 0)
                rv = -1;

            /* stop looping */
            break;
        }
    }
    else {
        /* Now perform the actual send operation */
        while ((rv = pth_sc(sendto)(fd, buf, nbytes, flags, to, tolen)) < 0
               && errno == EINTR) ;
    }

    /* restore filedescriptor mode */
    errno_shield { pth_fdmode(fd, fdmode); }

    pth_debug2("pth_sendto_ev: leave to thread \"%s\"", pth_current->name);
    return rv;
}

