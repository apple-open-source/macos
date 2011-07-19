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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

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

#ifdef HAVE_WORKING_POLL
#ifdef HAVE_POLL_H
#include <poll.h>
#else
#include <sys/poll.h>
#endif
#undef HAVE_SELECT
#endif

#ifdef HAVE_SELECT
#if !(defined(_MINIX) || defined(__BEOS__))
#define HAVE_WORKING_SELECT 1
#endif
#endif

#ifdef HAVE_WORKING_SELECT
#if defined(HAVE_SYS_SELECT_H) && defined(HAVE_SYS_TIME_SELECT)
#include <sys/select.h>
#endif
#endif

#ifdef HAVE_PTY_H
#include <pty.h>
#endif

#ifdef HAVE_STROPTS_H
#include <stropts.h>
#endif

#ifdef HAVE_SYS_PARAM
#include <sys/param.h>
#endif

#ifdef HAVE_OPENPTY
#if defined(HAVE_UTIL_H)
#include <util.h>
#elif defined(HAVE_LIBUTIL_H)
#include <libutil.h>
#elif defined(HAVE_PTY_H)
#include <pty.h>
#endif
#endif /* HAVE_OPENPTY */

#include "sys.h"

#ifdef USE_IGNORE_RC
int ignore_unused;
#endif

#if defined(HAVE_OPENPTY)
static int opened_tty = -1;
#endif

static int saved_tio_valid = 0;
static struct termios saved_tio;

int
waitForOutput(int fd)
{
    int ret = 0;

#if defined(HAVE_WORKING_POLL)
    struct pollfd pfd[1];
    int rc;

    pfd[0].fd = fd;
    pfd[0].events = POLLOUT;
    pfd[0].revents = 0;

    rc = poll(pfd, 1, -1);
    if (rc < 0)
	ret = -1;
    else if (pfd[0].revents & (POLLOUT | POLLERR | POLLHUP))
	ret = 1;

#elif defined(HAVE_WORKING_SELECT)
    fd_set fds;
    int rc;

    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    rc = select(FD_SETSIZE, NULL, &fds, NULL, NULL);
    if (rc < 0)
	ret = -1;
    else if (FD_ISSET(fd, &fds))
	ret = 1;

#else
    ret = 1;
#endif

    return ret;
}

int
waitForInput(int fd1, int fd2)
{
    int ret = 0;

#if defined(HAVE_WORKING_POLL)
    struct pollfd pfd[2];
    int rc;

    pfd[0].fd = fd1;
    pfd[1].fd = fd2;
    pfd[0].events = pfd[1].events = POLLIN;
    pfd[0].revents = pfd[1].revents = 0;

    rc = poll(pfd, 2, -1);
    if (rc < 0) {
	ret = -1;
    } else {
	if (pfd[0].revents & (POLLIN | POLLERR | POLLHUP))
	    ret |= 1;
	if (pfd[1].revents & (POLLIN | POLLERR | POLLHUP))
	    ret |= 2;
    }

#elif defined(HAVE_WORKING_SELECT)
    fd_set fds;
    int rc;

    FD_ZERO(&fds);
    FD_SET(fd1, &fds);
    FD_SET(fd2, &fds);
    rc = select(FD_SETSIZE, &fds, NULL, NULL, NULL);
    if (rc < 0) {
	ret = -1;
    } else {
	if (FD_ISSET(fd1, &fds))
	    ret |= 1;
	if (FD_ISSET(fd2, &fds))
	    ret |= 2;
    }
#else
    ret = (1 | 2);
#endif

    return ret;
}

int
setWindowSize(int sfd, int dfd)
{
#ifdef TIOCGWINSZ
    int rc;
    struct winsize ws;
    rc = ioctl(sfd, TIOCGWINSZ, (char *) &ws);
    if (rc < 0)
	return -1;
    rc = ioctl(dfd, TIOCSWINSZ, (char *) &ws);
    if (rc < 0)
	return -1;
#endif
    return 0;
}

int
installHandler(int signum, void (*handler) (int))
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
    if (rc < 0)
	return -1;

    rc = tcsetattr(dfd, TCSAFLUSH, &tio);
    if (rc < 0)
	return -1;

    return 0;
}

int
saveTermios(void)
{
    int rc;
    rc = tcgetattr(0, &saved_tio);
    if (rc >= 0)
	saved_tio_valid = 1;
    return rc;
}

int
restoreTermios(void)
{
    if (!saved_tio_valid)
	return -1;
    return tcsetattr(0, TCSAFLUSH, &saved_tio);
}

int
setRawTermios(void)
{
    struct termios tio;
    int rc;

    if (!saved_tio_valid)
	saveTermios();
    rc = tcgetattr(0, &tio);
    if (rc < 0)
	return rc;
    tio.c_lflag &= (unsigned) ~(ECHO | ICANON | ISIG);
    tio.c_iflag &= (unsigned) ~(ICRNL | IXOFF | IXON | ISTRIP);
#ifdef ONLCR
    tio.c_oflag &= (unsigned) ~ONLCR;
#endif
#ifdef OCRNL
    tio.c_oflag &= (unsigned) ~OCRNL;
#endif
#ifdef ONOCR
    tio.c_oflag &= (unsigned) ~ONOCR;
#endif

#ifdef VMIN
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;
#endif
    rc = tcsetattr(0, TCSAFLUSH, &tio);
    if (rc < 0)
	return rc;
    return 0;
}

char *
my_basename(char *path)
{
    char *p;

    p = strrchr(path, '/');
    if (!p)
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

    rc = stat(line, &s);
    if (rc < 0)
	return -1;
    if (s.st_uid != getuid() || s.st_gid != getgid()) {
	rc = chown(line, getuid(), getgid());
	if (rc < 0) {
	    fprintf(stderr,
		    "Warning: could not change ownership of tty -- "
		    "pty is insecure!\n");
	    return 0;
	}
    }
    if ((s.st_mode & 0777) != (S_IRUSR | S_IWUSR | S_IWGRP)) {
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
    const char *name1 = "pqrstuvwxyzPQRST";
    char buffer[80];
    char *name2 = strcpy(buffer, "0123456789abcdefghijklmnopqrstuv");
    const char *p1;
    char *p2;

#if defined(HAVE_GRANTPT)
    int rc;

    pty = open("/dev/ptmx", O_RDWR);
    if (pty < 0)
	goto bsd;

    rc = grantpt(pty);
    if (rc < 0) {
	close(pty);
	goto bsd;
    }

    rc = unlockpt(pty);
    if (rc < 0) {
	close(pty);
	goto bsd;
    }

    line = strmalloc(ptsname(pty));
    if (!line) {
	close(pty);
	goto bsd;
    }

    fix_pty_perms(line);

    *pty_return = pty;
    *line_return = line;
    return 0;

  bsd:
#elif defined(HAVE_OPENPTY)
    int rc;
    char ttydev[80];		/* OpenBSD says at least 16 bytes */

    rc = openpty(&pty, &opened_tty, ttydev, NULL, NULL);
    if (rc < 0) {
	close(pty);
	goto bsd;
    }
    line = strmalloc(ttydev);
    if (!line) {
	close(pty);
	goto bsd;
    }

    fix_pty_perms(line);

    *pty_return = pty;
    *line_return = line;
    return 0;

  bsd:
#endif /* HAVE_GRANTPT, etc */

    strcpy(name, "/dev/pty??");
    for (p1 = name1; *p1; p1++) {
	name[8] = *p1;
	for (p2 = name2; *p2; p2++) {
	    name[9] = *p2;
	    pty = open(name, O_RDWR);
	    if (pty >= 0)
		goto found;
	    /* Systems derived from 4.4BSD differ in their pty names,
	       so ENOENT doesn't necessarily imply we're done. */
	    continue;
	}
    }

    goto bail;

  found:
    if ((line = strmalloc(name)) != 0) {
	line[5] = 't';
	fix_pty_perms(line);
	*pty_return = pty;
	*line_return = line;
	return 0;
    }

  bail:
    if (pty >= 0)
	close(pty);
    if (line)
	free(line);
    return -1;
}

int
openTty(char *line)
{
    int rc;
    int tty = -1;

    tty = open(line, O_RDWR
#if defined(TIOCSCTTY) && defined(O_NOCTTY)
    /*
     * Do not make this our controlling terminal, yet just in case it fails
     * in some intermediate state.  But do not add this flag if we haven't
     * the corresponding ioctl.
     */
	       | O_NOCTTY
#endif
	);

    if (tty < 0)
	goto bail;

#if defined(HAVE_OPENPTY)
    if (opened_tty >= 0) {
	close(opened_tty);
	opened_tty = -1;
    }
#endif

#ifdef TIOCSCTTY
    /*
     * Now that we've successfully opened the terminal, make it the controlling
     * terminal.  This call works only if the process does not already have a
     * controlling terminal.
     *
     * Cygwin as of 2009/10/12 lacks this call, but has O_NOCTTY.
     */
    rc = ioctl(tty, TIOCSCTTY, (char *) 0);
    if (rc < 0) {
	goto bail;
    }
#endif

#if defined(I_PUSH) && (defined(SVR4) || defined(__SVR4))
    rc = ioctl(tty, I_PUSH, "ptem");
    if (rc < 0)
	goto bail;

    rc = ioctl(tty, I_PUSH, "ldterm");
    if (rc < 0)
	goto bail;

    rc = ioctl(tty, I_PUSH, "ttcompat");
    if (rc < 0)
	goto bail;
#endif

    return tty;

  bail:
    if (tty >= 0)
	close(tty);
    return -1;
}

/* Post-4.4 BSD systems have POSIX semantics (_POSIX_SAVED_IDS
   or not, depending on the version).  4.3BSD and Minix do not have
   saved IDs at all, so there's no issue. */
int
droppriv(void)
{
    int rc;
#if (defined(BSD) && !defined(_POSIX_SAVED_IDS)) || defined(_MINIX)
    rc = setuid(getuid());
    if (rc >= 0) {
	rc = setgid(getgid());
    }
#elif defined(_POSIX_SAVED_IDS)
    uid_t uid = getuid();
    uid_t euid = geteuid();
    gid_t gid = getgid();
    gid_t egid = getegid();

    if ((uid != euid || gid != egid) && euid != 0) {
	errno = ENOSYS;
	rc = -1;
    } else {
	rc = setuid(uid);
	if (rc >= 0)
	    rc = setgid(gid);
    }
#else
    uid_t uid = getuid();
    uid_t euid = geteuid();
    gid_t gid = getgid();
    gid_t egid = getegid();

    if (uid != euid || gid != egid) {
	errno = ENOSYS;
	rc = -1;
    } else {
	rc = 0;
    }
#endif
    return rc;
}

char *
strmalloc(const char *value)
{
    char *result = 0;

    if (value != 0) {
#ifdef HAVE_STRDUP
	result = strdup(value);
#else
	result = malloc(strlen(value) + 1);
	if (result != 0)
	    strcpy(result, value);
#endif
    }
    return result;
}

#ifdef NO_LEAKS
void
ExitProgram(int code)
{
    luit_leaks();
    iso2022_leaks();
    charset_leaks();
    exit(code);
}
#endif
