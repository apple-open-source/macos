/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/bsdi/bsdi_init.c,v 3.6 1998/07/25 16:56:38 dawes Exp $ */
/*
 * Copyright 1992 by Rich Murphey <Rich@Rice.edu>
 * Copyright 1993 by David Wexelblat <dwex@goblin.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Rich Murphey and David Wexelblat 
 * not be used in advertising or publicity pertaining to distribution of 
 * the software without specific, written prior permission.  Rich Murphey and
 * David Wexelblat make no representations about the suitability of this 
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * RICH MURPHEY AND DAVID WEXELBLAT DISCLAIM ALL WARRANTIES WITH REGARD TO 
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND 
 * FITNESS, IN NO EVENT SHALL RICH MURPHEY OR DAVID WEXELBLAT BE LIABLE FOR 
 * ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER 
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF 
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */
/* $XConsortium: bsdi_init.c /main/5 1996/02/21 17:51:15 kaleb $ */

#include "X.h"
#include "Xmd.h"
#include "input.h"
#include "scrnintstr.h"

#include "compiler.h"

#include <sys/param.h>
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"

extern Bool RunFromSmartParent;

static Bool KeepTty = FALSE;

#if BSD >= 199306
static void
NonBlockConsoleOff()
{
    register int i;

    i = fcntl(2, F_GETFL, 0);
    if (i >= 0)
        (void) fcntl(2, F_SETFL, i & ~FNDELAY);
}
#endif

void
xf86OpenConsole()
{
    int i, fd;

    if (serverGeneration == 1)
    {
	/* check if we're run with euid==0 */
	if (geteuid() != 0)
	{
	    FatalError("xf86OpenConsole: Server must be suid root\n");
	}

	if (!KeepTty)
	{
#if BSD >= 199306
	    if (RunFromSmartParent) {
              if (atexit(NonBlockConsoleOff))
                xf86Msg(X_WARNING,
			"InitOutput: can't register NBIO exit handler\n");
	      i = fcntl(2, F_GETFL, 0);
	      if (i >= 0)
	        i = fcntl(2, F_SETFL, i | FNDELAY);
	      if (i < 0)
	        xf86Msg(X_WARNING,
			"InitOutput: can't put stderr in non-block mode\n");
	    }
#else
	    /*
	     * detaching the controlling tty solves problems of kbd character
	     * loss.  This is not interesting for CO driver, because it is 
	     * exclusive.
	     */
	    setpgrp(0, getpid());
	    if ((i = open("/dev/tty",O_RDWR)) >= 0)
	    {
		ioctl(i,TIOCNOTTY,(char *)0);
		close(i);
	    }
#endif
	}

	if ((xf86Info.consoleFd = open("/dev/kbd", O_RDWR|O_NDELAY,0)) < 0)
	{
	    FatalError("xf86OpenConsole: Cannot open /dev/kbd (%s)\n",
		       strerror(errno));
	}
	if ((xf86Info.screenFd = open("/dev/vga", O_RDWR|O_NDELAY,0)) < 0)
	{
	    FatalError("xf86OpenConsole: Cannot open /dev/vga (%s)\n",
		       strerror(errno));
	}

	if (ioctl(xf86Info.consoleFd, PCCONIOCRAW, 0) < 0)
	{
	    FatalError("%s: PCCONIOCRAW failed (%s)\n", 
		       "xf86OpenConsole", strerror(errno));
	}
    }
    return;
}

void
xf86CloseConsole()
{
    ioctl (xf86Info.consoleFd, PCCONIOCCOOK, 0);

    if (xf86Info.screenFd != xf86Info.consoleFd)
    {
	close(xf86Info.screenFd);
    }
    close(xf86Info.consoleFd);
    return;
}

int
xf86ProcessArgument (int argc, char *argv[], int i)
{
	/*
	 * Keep server from detaching from controlling tty.  This is useful 
	 * when debugging (so the server can receive keyboard signals.
	 */
	if (!strcmp(argv[i], "-keeptty"))
	{
		KeepTty = TRUE;
		return(1);
	}
	return(0);
}

void
xf86UseMsg()
{
	ErrorF("-keeptty               ");
	ErrorF("don't detach controlling tty (for debugging only)\n");
	return;
}
