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
**  pth_syscall.c: Pth direct syscall support
*/
                             /* ``Free Software: generous programmers
                                  from around the world all join
                                  forces to help you shoot yourself
                                  in the foot for free.''
                                                 -- Unknown         */
#include "pth_p.h"

#if cpp

#if PTH_SYSCALL_HARD
#include <sys/syscall.h>
#ifdef HAVE_SYS_SOCKETCALL_H
#include <sys/socketcall.h>
#endif
#define pth_sc(func) PTH_SC_##func
#else
#define pth_sc(func) func
#endif

#endif /* cpp */

/* some exported variables for object layer checks */
int pth_syscall_soft = PTH_SYSCALL_SOFT;
int pth_syscall_hard = PTH_SYSCALL_HARD;

/* Pth hard wrapper for syscall fork(2) */
#if cpp
#if defined(SYS_fork)
#define PTH_SC_fork() ((pid_t)syscall(SYS_fork))
#else
#define PTH_SC_fork fork
#endif
#endif /* cpp */
#if PTH_SYSCALL_HARD && defined(SYS_fork)
pid_t fork(void)
{
    pth_implicit_init();
    return pth_fork();
}
#endif

/* Pth hard wrapper for sleep(3) [internally fully emulated] */
#if PTH_SYSCALL_HARD
unsigned int sleep(unsigned int sec)
{
    pth_implicit_init();
    return pth_sleep(sec);
}
#endif

/* Pth hard wrapper for system(3) [internally fully emulated] */
#if PTH_SYSCALL_HARD
int system(const char *cmd)
{
    pth_implicit_init();
    return pth_system(cmd);
}
#endif

/* Pth hard wrapper for sigprocmask(2) */
#if cpp
#if defined(SYS_sigprocmask)
#define PTH_SC_sigprocmask(a1,a2,a3) ((int)syscall(SYS_sigprocmask,(a1),(a2),(a3)))
#else
#define PTH_SC_sigprocmask sigprocmask
#endif
#endif /* cpp */
#if PTH_SYSCALL_HARD && defined(SYS_sigprocmask)
int sigprocmask(int how, const sigset_t *set, sigset_t *oset)
{
    pth_implicit_init();
    return pth_sigmask(how, set, oset);
}
#endif

/* Pth hard wrapper for sigwait(3) [internally fully emulated] */
#if PTH_SYSCALL_HARD
int sigwait(const sigset_t *set, int *sigp)
{
    pth_implicit_init();
    return pth_sigwait(set, sigp);
}
#endif

/* Pth hard wrapper for syscall waitpid(2) */
#if cpp
#if defined(SYS_waitpid)
#define PTH_SC_waitpid(a1,a2,a3) ((int)syscall(SYS_waitpid,(a1),(a2),(a3)))
#else
#define PTH_SC_waitpid waitpid
#endif
#endif /* cpp */
#if PTH_SYSCALL_HARD && defined(SYS_waitpid)
pid_t waitpid(pid_t wpid, int *status, int options)
{
    pth_implicit_init();
    return pth_waitpid(wpid, status, options);
}
#endif

#if defined(SYS_socketcall) && defined(SOCKOP_connect) && defined(SOCKOP_accept) /* mainly Linux */
intern int _pth_socketcall(int call, ...)
{
    va_list ap;
    unsigned long args[3];

    va_start(ap, call);
    switch (call) {
        case SOCKOP_connect:
            args[0] = (unsigned long)va_arg(ap, int);
            args[1] = (unsigned long)va_arg(ap, struct sockaddr *);
            args[2] = (unsigned long)va_arg(ap, int);
            break;
        case SOCKOP_accept:
            args[0] = (unsigned long)va_arg(ap, int);
            args[1] = (unsigned long)va_arg(ap, struct sockaddr *);
            args[2] = (unsigned long)va_arg(ap, int *);
            break;
    }
    va_end(ap);
    return syscall(SYS_socketcall, call, args);
}
#endif

/* Pth hard wrapper for syscall connect(2) */
#if cpp
#if defined(SYS_connect)
#define PTH_SC_connect(a1,a2,a3) ((int)syscall(SYS_connect,(a1),(a2),(a3)))
#elif defined(SYS_socketcall) && defined(SOCKOP_connect) /* mainly Linux */
#define PTH_SC_connect(a1,a2,a3) ((int)_pth_socketcall(SOCKOP_connect,(a1),(a2),(a3)))
#else
#define PTH_SC_connect connect
#endif
#endif /* cpp */
#if PTH_SYSCALL_HARD &&\
    (defined(SYS_connect) || (defined(SYS_socketcall) && defined(SOCKOP_connect)))
int connect(int s, const struct sockaddr *addr, socklen_t addrlen)
{
    pth_implicit_init();
    return pth_connect(s, addr, addrlen);
}
#endif

/* Pth hard wrapper for syscall accept(2) */
#if cpp
#if defined(SYS_accept)
#define PTH_SC_accept(a1,a2,a3) ((int)syscall(SYS_accept,(a1),(a2),(a3)))
#elif defined(SYS_socketcall) && defined(SOCKOP_accept) /* mainly Linux */
#define PTH_SC_accept(a1,a2,a3) ((int)_pth_socketcall(SOCKOP_accept,(a1),(a2),(a3)))
#else
#define PTH_SC_accept accept
#endif
#endif /* cpp */
#if PTH_SYSCALL_HARD &&\
    (defined(SYS_accept) || (defined(SYS_socketcall) && defined(SOCKOP_accept)))
int accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    pth_implicit_init();
    return pth_accept(s, addr, addrlen);
}
#endif

/* Pth hard wrapper for syscall select(2) */
#if cpp
#if defined(SYS__newselect) /* mainly Linux */
#define PTH_SC_select(a1,a2,a3,a4,a5) ((int)syscall(SYS__newselect,(a1),(a2),(a3),(a4),(a5)))
#elif defined(SYS_select)
#define PTH_SC_select(a1,a2,a3,a4,a5) ((int)syscall(SYS_select,(a1),(a2),(a3),(a4),(a5)))
#else
#define PTH_SC_select select
#endif
#endif /* cpp */
#if PTH_SYSCALL_HARD && (defined(SYS__newselect) || defined(SYS_select))
int select(int nfds, fd_set *readfds, fd_set *writefds,
           fd_set *exceptfds, struct timeval *timeout)
{
    pth_implicit_init();
    return pth_select(nfds, readfds, writefds, exceptfds, timeout);
}
#endif

/* Pth hard wrapper for syscall poll(2) */
#if cpp
#if defined(SYS_poll)
#define PTH_SC_poll(a1,a2,a3) ((int)syscall(SYS_poll,(a1),(a2),(a3)))
#else
#define PTH_SC_poll poll
#endif
#endif /* cpp */
#if PTH_SYSCALL_HARD && defined(SYS_poll)
int poll(struct pollfd *pfd, nfds_t nfd, int timeout)
{
    pth_implicit_init();
    return pth_poll(pfd, nfd, timeout);
}
#endif

/* Pth hard wrapper for syscall read(2) */
#if cpp
#if defined(SYS_read)
#define PTH_SC_read(a1,a2,a3) ((ssize_t)syscall(SYS_read,(a1),(a2),(a3)))
#else
#define PTH_SC_read read
#endif
#endif /* cpp */
#if PTH_SYSCALL_HARD && defined(SYS_read)
ssize_t read(int fd, void *buf, size_t nbytes)
{
    pth_implicit_init();
    return pth_read(fd, buf, nbytes);
}
#endif

/* Pth hard wrapper for syscall write(2) */
#if cpp
#if defined(SYS_write)
#define PTH_SC_write(a1,a2,a3) ((ssize_t)syscall(SYS_write,(a1),(a2),(a3)))
#else
#define PTH_SC_write write
#endif
#endif /* cpp */
#if PTH_SYSCALL_HARD && defined(SYS_write)
ssize_t write(int fd, const void *buf, size_t nbytes)
{
    pth_implicit_init();
    return pth_write(fd, buf, nbytes);
}
#endif

/* Pth hard wrapper for syscall readv(2) */
#if cpp
#if defined(SYS_readv)
#define PTH_SC_readv(a1,a2,a3) ((ssize_t)syscall(SYS_readv,(a1),(a2),(a3)))
#else
#define PTH_SC_readv readv
#endif
#endif /* cpp */
#if PTH_SYSCALL_HARD && defined(SYS_readv)
ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
{
    pth_implicit_init();
    return pth_readv(fd, iov, iovcnt);
}
#endif

/* Pth hard wrapper for syscall writev(2) */
#if cpp
#if defined(SYS_writev)
#define PTH_SC_writev(a1,a2,a3) ((ssize_t)syscall(SYS_writev,(a1),(a2),(a3)))
#else
#define PTH_SC_writev writev
#endif
#endif /* cpp */
#if PTH_SYSCALL_HARD && defined(SYS_writev)
ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
    pth_implicit_init();
    return pth_writev(fd, iov, iovcnt);
}
#endif

/* Pth hard wrapper for pread(3) [internally fully emulated] */
#if PTH_SYSCALL_HARD
ssize_t pread(int, void *, size_t, off_t);
ssize_t pread(int fd, void *buf, size_t nbytes, off_t offset)
{
    pth_implicit_init();
    return pth_pread(fd, buf, nbytes, offset);
}
#endif

/* Pth hard wrapper for pwrite(3) [internally fully emulated] */
#if PTH_SYSCALL_HARD
ssize_t pwrite(int, const void *, size_t, off_t);
ssize_t pwrite(int fd, const void *buf, size_t nbytes, off_t offset)
{
    pth_implicit_init();
    return pth_pwrite(fd, buf, nbytes, offset);
}
#endif

/* Pth hard wrapper for syscall recvfrom(2) */
#if cpp
#if defined(SYS_recvfrom)
#define PTH_SC_recvfrom(a1,a2,a3,a4,a5,a6) ((ssize_t)syscall(SYS_recvfrom,(a1),(a2),(a3),(a4),(a5),(a6)))
#else
#define PTH_SC_recvfrom recvfrom
#endif
#endif /* cpp */
#if PTH_SYSCALL_HARD && defined(SYS_recvfrom)
ssize_t recvfrom(int fd, void *buf, size_t nbytes, int flags, struct sockaddr *from, socklen_t *fromlen)
{
    pth_implicit_init();
    return pth_recvfrom(fd, buf, nbytes, flags, from, fromlen);
}
#endif

/* Pth hard wrapper for syscall sendto(2) */
#if cpp
#if defined(SYS_sendto)
#define PTH_SC_sendto(a1,a2,a3,a4,a5,a6) ((ssize_t)syscall(SYS_sendto,(a1),(a2),(a3),(a4),(a5),(a6)))
#else
#define PTH_SC_sendto sendto
#endif
#endif /* cpp */
#if PTH_SYSCALL_HARD && defined(SYS_sendto)
ssize_t sendto(int fd, const void *buf, size_t nbytes, int flags, const struct sockaddr *to, socklen_t tolen)
{
    pth_implicit_init();
    return pth_sendto(fd, buf, nbytes, flags, to, tolen);
}
#endif

