/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/dgux/dgux_init.c,v 1.2 2003/11/17 22:20:40 dawes Exp $ */
/*
 * INTEL DG/UX RELEASE 4.20 MU03
 * Copyright 1997 Takis Psarogiannakopoulos Cambridge,UK
 * <takis@dpmms.cam.ac.uk>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.
 * XCONSORTIUM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE.
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FITNESS, IN NO EVENT SHALL XCONSORTIUM BE LIABLE FOR
 * ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */


#include "X.h"
#include "Xmd.h"
#include "input.h"
#include "scrnintstr.h"

#include "compiler.h"

#include "xf86.h"
#include "xf86Procs.h"
#include "xf86_OSlib.h"

static Bool KeepTty = FALSE;
static Bool Protect0 = FALSE;
static int VTnum = -1;

extern void xf86VTRequest(
	int
);

void xf86OpenConsole()
{
    int i;
    int fd;
    char vtname[14];

    if (serverGeneration == 1) 
    {
    	/* check if we're run with euid==0 */
    	if (geteuid() != 0)
	{
      	    FatalError("xf86OpenConsole: Server must be suid root\n");
	}
	if (Protect0)
	{
	    int fd = -1;

	    if ((fd = open("/dev/zero", O_RDONLY, 0)) < 0)
	    {
		ErrorF("xf86OpenConsole: cannot open /dev/zero (%s)\n",
		       strerror(errno));
	    }
	    else
	    {
		if ((int)mmap(0, 0x1000, PROT_NONE,
			      MAP_FIXED | MAP_SHARED, fd, 0) == -1)
		{
		    ErrorF("xf86OpenConsole: failed to protect page 0 (%s)\n",
		       strerror(errno));
		}
		close(fd);
	    }
	}
    	if (VTnum != -1) 
	{
      	    xf86Info.vtno = VTnum;
    	}
    	else 
	{
      	    if ((fd = open("/dev/console",O_WRONLY,0)) < 0) 
	    {
        	FatalError(
		    "xf86OpenConsole: Cannot open system tty (/dev/console), (%s)\n",
		    strerror(errno));
	    }
           close(fd);
        }
        xf86Info.vtno=0;
	ErrorF("   (Intel DG/ux: using VT number: systty%d)\n\n", xf86Info.vtno);

	sprintf(vtname,"/dev/console");

	xf86Config(FALSE); /* Read XF86Config */

	if (!KeepTty)
    	{
    	    setpgrp();
	}

	if ((xf86Info.consoleFd = open("/dev/console", O_RDWR|O_NDELAY, 0)) < 0)
	{
            FatalError("xf86OpenConsole: Cannot open %s (%s)\n",
		       vtname, strerror(errno));
	}



        if ((xf86Info.kbdFd = open("/dev/keybd", O_RDONLY|O_NDELAY, 0)) < 0)
        {
            FatalError("xf86OpenConsole: Cannot open keyboard (/dev/keybd), (%s)\n", strerror(errno));
        }
	/* change ownerships and Grab all other system consoles  */
	chown(vtname, getuid(), getgid());
        chown("/dev/syscon", getuid(), getgid());
        chown("/dev/systty", getuid(), getgid());
        if (!KeepTty)
        {
            /*
             * Detach from the controlling tty to avoid char loss
             */
            if ((i = open("/dev/tty",O_RDWR)) >= 0)
            {
                ioctl(i, TIOCNOTTY, 0);
                close(i);
            }
        }
    }
    else 
    {   
	/* serverGeneration != 1 */
	if (!xf86VTSema)
	    sleep(5);
    }
    return;
}

void xf86CloseConsole()
{
    close(xf86Info.kbdFd);      /* Close the keyboard */
    close(xf86Info.consoleFd);      /* Close the system console */
    return;
}

int xf86ProcessArgument(argc, argv, i)
int argc;
char *argv[];
int i;
{
	if (!strcmp(argv[i], "-keeptty"))
	{
		KeepTty = TRUE;
		return(1);
	}
	if (!strcmp(argv[i], "-protect0"))
	{
		Protect0 = TRUE;
		return(1);
	}
	if ((argv[i][0] == 'v') && (argv[i][1] == 't'))
	{
		if (sscanf(argv[i], "vt%2d", &VTnum) == 0)
		{
			UseMsg();
			VTnum = -1;
			return(0);
		}
		return(1);
	}
	return(0);
}

void xf86UseMsg()
{
	ErrorF("-keeptty               ");
	ErrorF("don't detach controlling tty (for debugging only)\n");
	return;
}
