/*

Copyright (c) 1988  X Consortium

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.

*/
/* $XFree86: xc/programs/xfs/os/daemon.c,v 1.12 2002/10/20 21:42:50 tsi Exp $ */

#include <X11/Xos.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>

#ifndef __GLIBC__
# if defined(__osf__) || \
     defined(__GNU__) || \
     defined(__CYGWIN__) || \
     defined(linux)
#  define setpgrp setpgid
# endif
#endif

#if defined(SVR4) || defined(USG) || defined(__GLIBC__)
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

#include "os.h"

void
BecomeOrphan ()
{
    Pid_t child_id;

    chdir("/");
    /*
     * fork so that the process goes into the background automatically. Also
     * has a nice side effect of having the child process get inherited by
     * init (pid 1).
     * Separate the child into its own process group before the parent
     * exits.  This eliminates the possibility that the child might get
     * killed when the init script that's running xfs exits.
     */

    child_id = fork();
    switch (child_id) {
    case 0:
	/* child */
	break;
    case -1:
	/* error */
	FatalError("daemon fork failed, %s\n", strerror(errno));
	break;

    default:
	/* parent */

#if defined(CSRG_BASED) || \
    defined(SYSV) || \
    defined(SVR4) || \
    defined(__QNXNTO__) || \
    defined(__GLIBC__) || \
    defined(linux)
	{
	    int stat;
# if defined(SVR4) || defined(__QNXNTO__)
	    /* This gets error EPERM.  Why? */
	    stat = setpgid (child_id, child_id);
# elif defined(SYSV)
	    stat = 0;	/* don't know how to set child's process group */
# elif defined(__GLIBC__)
	    stat = setpgrp ();
# else
	    stat = setpgrp (child_id, child_id);
# endif
	    if (stat != 0)
		FatalError("setting process group for daemon failed: %s\n",
			   strerror(errno));
	}
#endif /* ! (CSRG_BASED || SYSV || SVR4 || __QNXNTO__ || __GLIBC__) */
	exit (0);
    }
}

void
BecomeDaemon ()
{
    /*
     * Close standard file descriptors and get rid of controlling tty.
     */

    /* If our C library has the daemon() function, just use it. */
#if defined(__GLIBC__) || defined(CSRG_BASED)
    daemon (0, 0);
#else
    register int i;

# if defined(SYSV) || defined(SVR4) || defined(__QNXNTO__)
    setpgrp ();
# else
    setpgrp (0, getpid());
# endif

    close (0);
    close (1);
    close (2);

# if !defined(__UNIXOS2__) && !defined(__CYGWIN__)
#  if !((defined(SYSV) || defined(SVR4)) && defined(i386))
    if ((i = open ("/dev/tty", O_RDWR)) >= 0) {	/* did open succeed? */
#   if defined(USG) && defined(TCCLRCTTY)
	int zero = 0;
	(void) ioctl (i, TCCLRCTTY, &zero);
#   else
#    if (defined(SYSV) || defined(SVR4)) && defined(TIOCTTY)
	int zero = 0;
	(void) ioctl (i, TIOCTTY, &zero);
#    else
	(void) ioctl (i, TIOCNOTTY, (char *) 0);    /* detach, BSD style */
#    endif
#   endif
	(void) close (i);
    }
#  endif /* !((SYSV || SVR4) && i386) */
# endif /* !__UNIXOS2__ && !__CYGWIN__ */

    /*
     * Set up the standard file descriptors.
     */
    (void) open ("/", O_RDONLY);	/* root inode already in core */
    (void) dup2 (0, 1);
    (void) dup2 (0, 2);
#endif
}
