/* $Xorg: daemon.c,v 1.4 2001/02/09 02:05:40 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/xdm/daemon.c,v 3.22 2004/01/11 00:19:30 dawes Exp $ */

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 */

#include <X11/Xos.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>


#if defined(USG)
# include <termios.h>
#else
# include <sys/ioctl.h>
#endif
#ifdef hpux
# include <sys/ptyio.h>
#endif

#ifdef X_NOT_POSIX
# define Pid_t int
#else
# define Pid_t pid_t
#endif

#include "dm.h"
#include "dm_error.h"

#if defined(__GLIBC__) || defined(CSRG_BASED)
#define HAS_DAEMON
#endif

#ifndef X_NOT_POSIX
#define HAS_SETSID
#endif

#ifndef HAS_SETSID
#define setsid() MySetsid()

static Pid_t
MySetsid(void)
{
#if defined(TIOCNOTTY) || defined(TCCLRCTTY) || defined(TIOCTTY)
    int fd;
#endif
    int stat;

    fd = open("/dev/tty", O_RDWR);
    if (fd >= 0) {
#if defined(USG) && defined(TCCLRCTTY)
       int zero = 0;
       (void) ioctl (fd, TCCLRCTTY, &zero);
#elif (defined(SYSV) || defined(SVR4)) && defined(TIOCTTY)
       int zero = 0;
       (void) ioctl (i, TIOCTTY, &zero);
#elif defined(TIOCNOTTY)
       (void) ioctl (i, TIOCNOTTY, (char *) 0);    /* detach, BSD style */
#endif
        close(fd);
    }

#if defined(SYSV) || defined(__QNXNTO__)
    return setpgrp();
#else
    return setpgid(0, getpid());
#endif
}
#endif /* !HAS_SETSID */

/* detach */
void
BecomeDaemon (void)
{

    /* If our C library has the daemon() function, just use it. */
#ifdef HAS_DAEMON
    daemon (0, 0);
#else
    switch (fork()) {
    case -1:
       /* error */
       LogError("daemon fork failed, %s\n", strerror(errno));
       exit(1);
       break;
    case 0:
       /* child */
       break;
    default:
       /* parent */
       exit(0);
    }

    if (setsid() == -1) {
       LogError("setting session id for daemon failed: %s\n",
                  strerror(errno));
       exit(1);
    }

    chdir("/");

    close (0);
    close (1);
    close (2);


    /*
     * Set up the standard file descriptors.
     */
    (void) open ("/dev/null", O_RDWR);
    (void) dup2 (0, 1);
    (void) dup2 (0, 2);
#endif /* HAS_DAEMON */
}
