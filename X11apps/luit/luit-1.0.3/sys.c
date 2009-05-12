/*
Copyright (c) 2001 by Juliusz Chroboczek

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
/* $XFree86: xc/programs/luit/sys.c,v 1.9 2003/08/17 20:39:58 dawes Exp $ */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <termios.h>
#include <signal.h>
#include <errno.h>

#ifdef SVR4
#define HAVE_POLL
#endif

#ifndef HAVE_POLL
#ifndef _MINIX
#define HAVE_SELECT
#endif
#endif

#ifdef HAVE_POLL
#include <sys/poll.h>
#undef HAVE_SELECT
#endif

#ifdef __QNX__
#include <sys/select.h>
#endif


#if (defined(__GLIBC__) && \
     (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 1))) || \
    defined(SVR4) || defined(__APPLE__)
#define HAVE_GRANTPT
#endif

#ifdef __GLIBC__
#include <pty.h>
#endif

#ifdef SVR4
#include <stropts.h>
#endif

#if (defined(__unix__) || defined(unix)) && !defined(USG)
#include <sys/param.h>
#endif

#include "sys.h"

static int saved_tio_valid = 0;
static struct termios saved_tio;


#ifdef HAVE_POLL
int
waitForOutput(int fd)
{
    struct pollfd pfd[1];
    int rc;

    pfd[0].fd = fd;
    pfd[0].events = POLLOUT;
    pfd[0].revents = 0;

    rc = poll(pfd, 1, -1);
    if(rc < 0)
        return -1;

    if(pfd[0].revents & POLLOUT)
        return 1;

    return 0;
}

int
waitForInput(int fd1, int fd2)
{
    struct pollfd pfd[2];
    int ret, rc;

    pfd[0].fd = fd1;
    pfd[1].fd = fd2;
    pfd[0].events = pfd[1].events = POLLIN;
    pfd[0].revents = pfd[1].revents = 0;

    rc = poll(pfd, 2, -1);
    if(rc < 0)
        return -1;

    ret = 0;
    if(pfd[0].revents & POLLIN)
        ret |= 1;
    if(pfd[1].revents & POLLIN)
        ret |= 2;
    return ret;
}
#endif

#ifdef HAVE_SELECT
int
waitForOutput(int fd)
{
    fd_set fds;
    int rc;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    rc = select(FD_SETSIZE, NULL, &fds, NULL, NULL);
    if(rc < 0)
        return -1;

    if(FD_ISSET(fd, &fds))
        return 1;

    return 0;
}

int
waitForInput(int fd1, int fd2)
{
    fd_set fds;
    int ret, rc;

    FD_ZERO(&fds);
    FD_SET(fd1, &fds);
    FD_SET(fd2, &fds);
    rc = select(FD_SETSIZE, &fds, NULL, NULL, NULL);
    if(rc < 0)
        return -1;

    ret = 0;
    if(FD_ISSET(fd1, &fds))
        ret |= 1;
    if(FD_ISSET(fd2, &fds))
        ret |= 2;
    return ret;
}
#endif

#ifndef HAVE_POLL
#ifndef HAVE_SELECT
/* Busy looping implementation */
int
waitForOutput(int fd)
{
    return 1;
}

int
waitForInput(int fd1, int fd2)
{
    return 1|2;
}
#endif
#endif


int
setWindowSize(int sfd, int dfd)
{
#ifdef TIOCGWINSZ
    int rc;
    struct winsize ws;
    rc = ioctl(sfd, TIOCGWINSZ, (char*)&ws);
    if(rc < 0)
        return -1;
    rc = ioctl(dfd, TIOCSWINSZ, (char*)&ws);
    if(rc < 0)
        return -1;
#endif
    return 0;
}

int
installHandler(int signum, void (*handler)(int))
{
    struct sigaction sa;
    sigset_t ss;
    int rc;

    sigemptyset(&ss);

    sa.sa_handler = handler;
    sa.sa_mask = ss;
    sa.sa_flags = 0;
    rc = sigaction(signum, &sa, NULL);
    return rc;
}

int
copyTermios(int sfd, int dfd)
{
    struct termios tio;
    int rc;

    rc = tcgetattr(sfd, &tio);
    if(rc < 0)
        return -1;

    rc = tcsetattr(dfd, TCSAFLUSH, &tio);
    if(rc < 0)
        return -1;

    return 0;
}

int
saveTermios(void)
{
    int rc;
    rc = tcgetattr(0, &saved_tio);
    if(rc >= 0)
        saved_tio_valid = 1;
    return rc;
}

int
restoreTermios(void)
{
    if(!saved_tio_valid)
        return -1;
    return tcsetattr(0, TCSAFLUSH, &saved_tio);
}

int
setRawTermios(void)
{
    struct termios tio;
    int rc;

    if(!saved_tio_valid)
        saveTermios();
    rc = tcgetattr(0, &tio);
    if(rc < 0)
        return rc;
    tio.c_lflag &= ~(ECHO|ICANON|ISIG);
    tio.c_iflag &= ~(ICRNL|IXOFF|IXON|ISTRIP);
#ifdef ONLCR
    tio.c_oflag &= ~ONLCR;
#endif
#ifdef OCRNL
    tio.c_oflag &= ~OCRNL;
#endif
#ifdef ONOCR
    tio.c_oflag &= ~ONOCR;
#endif

#ifdef VMIN
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;
#endif
    rc = tcsetattr(0, TCSAFLUSH, &tio);
    if(rc < 0)
        return rc;
    return 0;
}


char *
my_basename(char *path)
{
    char *p;

    p = strrchr(path, '/');
    if(!p)
        p = path;
    else
        p++;
    return p;
}

static int
fix_pty_perms(char *line)
{
    int rc;
    struct stat s;
    int uid = getuid(), gid = getgid();

    rc = stat(line, &s);
    if(rc < 0)
        return -1;
    if(s.st_uid != uid || s.st_gid != gid) {
        rc = chown(line, getuid(), getgid());
        if(rc < 0) {
            fprintf(stderr, 
                    "Warning: could not change ownership of tty -- "
                    "pty is insecure!\n");
            return 0;
        }
    }
    if((s.st_mode & 0777) != (S_IRUSR | S_IWUSR | S_IWGRP)) {
        rc = chmod(line, S_IRUSR | S_IWUSR | S_IWGRP);
        if (rc < 0) {
            fprintf(stderr,
                    "Warning: could not change permissions of tty -- "
                    "pty is insecure!\n");
            return 0;
        }
    }
    return 1;
}

int
allocatePty(int *pty_return, char **line_return)
{
    char name[12], *line = NULL;
    int pty = -1;
    char *name1 = "pqrstuvwxyzPQRST", 
        *name2 = "0123456789abcdefghijklmnopqrstuv";
    char *p1, *p2;

#ifdef HAVE_GRANTPT
    char *temp_line;
    int rc;

#ifdef __APPLE__
    pty = posix_openpt(O_RDWR);
#else
    pty = open("/dev/ptmx", O_RDWR);
#endif

    if(pty < 0)
        goto bsd;

    rc = grantpt(pty);
    if(rc < 0) {
        close(pty);
        goto bsd;
    }

    rc = unlockpt(pty);
    if(rc < 0) {
        close(pty);
        goto bsd;
    }

    temp_line = ptsname(pty);
    if(!temp_line) {
        close(pty);
        goto bsd;
    }
    line = strdup(temp_line);
    if(!line) {
        close(pty);
        return -1;
    }

    fix_pty_perms(line);

    *pty_return = pty;
    *line_return = line;
    return 0;

  bsd:
#endif /* HAVE_GRANTPT */

    strcpy(name, "/dev/pty??");
    for(p1 = name1; *p1; p1++) {
        name[8] = *p1;
        for(p2 = name2; *p2; p2++) {
            name[9] = *p2;
            pty = open(name, O_RDWR);
            if(pty >= 0)
                goto found;
            /* Systems derived from 4.4BSD differ in their pty names,
               so ENOENT doesn't necessarily imply we're done. */
            continue;
        }
    }

    goto bail;

  found:
    line = strdup(name);
    if(!line)
	goto bail;
    line[5] = 't';
    fix_pty_perms(line);
    *pty_return = pty;
    *line_return = line;
    return 0;

  bail:
    if(pty >= 0)
        close(pty);
    if(line)
        free(line);
    return -1;
}

int
openTty(char *line)
{
    int rc;
    int tty = -1;

    tty = open(line, O_RDWR | O_NOCTTY);
 
    if(tty < 0)
        goto bail;

#ifdef TIOCSCTTY
    rc = ioctl(tty, TIOCSCTTY, (char *)0);
    if(rc < 0) {
        goto bail;
    }
#endif

#ifdef SVR4
    rc = ioctl(tty, I_PUSH, "ptem");
    if(rc < 0)
        goto bail;

    rc = ioctl(tty, I_PUSH, "ldterm");
    if(rc < 0)
        goto bail;

    rc = ioctl(tty, I_PUSH, "ttcompat");
    if(rc < 0)
        goto bail;
#endif

    return tty;

  bail:
    if(tty >= 0)
        close(tty);
    return -1;
}

/* Post-4.4 BSD systems have POSIX semantics (_POSIX_SAVED_IDS
   or not, depending on the version).  4.3BSD and Minix do not have
   saved IDs at all, so there's no issue. */
#if (defined(BSD) && !defined(_POSIX_SAVED_IDS)) || defined(_MINIX)
int
droppriv(void)
{
    int rc;
    rc = setuid(getuid());
    if(rc < 0)
        return rc;
    return setgid(getgid());
}
#elif defined(_POSIX_SAVED_IDS)
int
droppriv(void)
{
    int uid = getuid();
    int euid = geteuid();
    int gid = getgid();
    int egid = getegid();
    int rc;

    if((uid != euid || gid != egid) && euid != 0) {
        errno = ENOSYS;
        return -1;
    }
    rc = setuid(uid);
    if(rc < 0)
        return rc;
    return setgid(gid);
}
#else
int
droppriv(void)
{
    int uid = getuid();
    int euid = geteuid();
    int gid = getgid();
    int egid = getegid();

    if(uid != euid || gid != egid) {
        errno = ENOSYS;
        return -1;
    }
    return 0;
}
#endif    
