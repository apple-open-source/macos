/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/pmax/pmax_init.c,v 1.3 1998/07/25 16:56:55 dawes Exp $ */
/*
 * Copyright 1998 by Concurrent Computer Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Concurrent Computer
 * Corporation not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Concurrent Computer Corporation makes no representations
 * about the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 *
 * CONCURRENT COMPUTER CORPORATION DISCLAIMS ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CONCURRENT COMPUTER CORPORATION BE
 * LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 * Copyright 1998 by Metro Link Incorporated
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Metro Link
 * Incorporated not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Metro Link Incorporated makes no representations
 * about the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 *
 * METRO LINK INCORPORATED DISCLAIMS ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL METRO LINK INCORPORATED BE
 * LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *
 * This file was derived in part from the original XFree86 sysv OS
 * support which contains the following copyright notice:
 *
 * Copyright 1990,91 by Thomas Roell, Dinkelscherben, Germany
 * Copyright 1993 by David Wexelblat <dwex@goblin.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Thomas Roell and David Wexelblat 
 * not be used in advertising or publicity pertaining to distribution of 
 * the software without specific, written prior permission.  Thomas Roell and
 * David Wexelblat makes no representations about the suitability of this 
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * THOMAS ROELL AND DAVID WEXELBLAT DISCLAIMS ALL WARRANTIES WITH REGARD TO 
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND 
 * FITNESS, IN NO EVENT SHALL THOMAS ROELL OR DAVID WEXELBLAT BE LIABLE FOR 
 * ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER 
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF 
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <sys/types.h>
#include <time.h>
#include <errno.h>

#include <sys/prosrfs.h>
#include <sys/cpu.h>
#include <sys/ipl.h>

#include "X.h"
#include "Xmd.h"

#include "compiler.h"

#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"

static Bool KeepTty = FALSE;
static Bool Protect0 = FALSE;
static Bool pmaxInitialized = FALSE;

#define VT_DEFAULT -2
#define VT_NONE    -1

static int VTnum = VT_DEFAULT;

extern void pmax_init_splmap(void);

int  pmax_sys_type;  /* Also used by pmax_pci.c */

/*
 *      PowerMAXOS_sys_type()
 *
 *      Determine type of PowerHawk, PowerStack, PowerMaxion, or NightHawk
 */
int
PowerMAXOS_sys_type(void)
{ 
  int             fd;
  procfile_t      procfile;
  
  fd = open("/system/processor/0",O_RDONLY);
  if (fd<0) {
    FatalError("Cannot open '%s'\n", "/system/processor/0");
  }
  
  if (read(fd, &procfile, sizeof(procfile)) < 0) {
    FatalError("Cannot read '%s'\n", "/system/processor/0");
  }
  close(fd);
  
  return(procfile.cpu_model);
}

void
pmaxInit(void)
{
	 char *mach;
	 
     if (pmaxInitialized)
	  return;

     pmaxInitialized = TRUE;
     
     /*
      * Determine type of machine
      */
     pmax_sys_type = PowerMAXOS_sys_type();
     switch(pmax_sys_type) {
	  
     case MODEL_NH6400:
	  mach ="PowerMAXION (NH6400)";
	  break;
	  
     case MODEL_NH6408:
	  mach = "PowerMAXION (NH6408)";
	  break;      

     case MODEL_NH6800T:
	  mach = "TurboHawk";
	  break;      
	  
     case MODEL_MPWR:
	  mach = "PowerStack";
	  break;
	  
     case MODEL_PH610:
	  mach = "PowerHawk 610";
	  break;
	  
     case MODEL_MPWR2:
	  mach = "PowerStack II (utah)";
	  break;
	  
     case MODEL_PH620:
	  mach = "PowerHawk 620";
	  break;
	  
     case MODEL_PH640:
	  mach = "PowerHawk 640";
	  break;

     case MODEL_MMTX:
	  mach = "PowerStack II (MTX)";
	  break;
	  
     default:
	  FatalError("pmaxInit: Unknown/unsupported machine type 0x%x\n",
		     pmax_sys_type);
	  /*NOTREACHED*/
     }
	 
     xf86Msg(X_INFO, "pmaxInit: Machine type: %s\n", mach);

     /*
      * Map IPL hardware so that interrupts can be (temporarily) disabled
      * (see pmax_video.c) 
      */
     pmax_init_splmap();

     /*
      * Now that we know the system type, initialize the
      * pci access routines
      */
     pciInit();
}

void
xf86OpenConsole()
{
    struct vt_mode VT;
    char vtname[10];
    MessageType from = X_DEFAULT;

    if (serverGeneration == 1) 
    {
    
    	/* check if we're run with euid==0 */
    	if (geteuid() != 0)
	{
      	    FatalError("xf86OpenConsole: Server must be suid root\n");
	}

	/* Protect page 0 to help find NULL dereferencing */
	/* mprotect() doesn't seem to work */
	if (Protect0)
	{
	    int fd = -1;

	    if ((fd = open("/dev/zero", O_RDONLY, 0)) < 0)
	    {
		xf86Msg(X_WARNING,
			"xf86OpenConsole: cannot open /dev/zero (%s)\n",
			strerror(errno));
	    }
	    else
	    {
		if ((int)mmap(0, 0x1000, PROT_NONE,
			      MAP_FIXED | MAP_SHARED, fd, 0) == -1)
		{
		    xf86Msg(X_WARNING,
			"xf86OpenConsole: failed to protect page 0 (%s)\n",
			strerror(errno));
		}
		close(fd);
	    }
	}

	pmaxInit();  /* Initialize OS specific functions */
	
    	/*
     	 * setup the virtual terminal manager
     	 */
	if (VTnum == VT_DEFAULT) {
	    int fd;
	    
	    /*
	     * No specific VT specified, so ask the vtl term mgr
	     * for the next available VT
	     */
	    if ((fd = open("/dev/vt00",O_WRONLY,0)) < 0) {
        	xf86Msg(X_WARNING,
			"xf86OpenConsole: Could not open /dev/vt00 (%s)\n",
			strerror(errno));
		VTnum = VT_NONE;
	    }
	    else {
		if (ioctl(fd, VT_OPENQRY, &VTnum) < 0)
		{
		    xf86Msg(X_WARNING,
			    "xf86OpenConsole: Cannot find a free VT\n");
		    VTnum = VT_NONE;
		}
		close(fd);
	    }
	} else {
	    from = X_CMDLINE;
	}

	xf86Info.vtno = VTnum;

	if (xf86Info.vtno == VT_NONE)
	    strcpy(vtname, "/dev/null");
	else
	    sprintf(vtname,"/dev/vt%02d",xf86Info.vtno);
	    
        xf86Msg(from, "using VT \"%s\"\n\n", vtname);
	
	if (!KeepTty)
    	{
    	    setpgrp();
	}

	if ((xf86Info.consoleFd = open(vtname, O_RDWR|O_NDELAY, 0)) < 0)
	{
            FatalError("xf86OpenConsole: Cannot open %s (%s)\n",
		       vtname, strerror(errno));
	}

	if (xf86Info.vtno != VT_NONE)
	{
	     /* change ownership of the vt */
	     (void) chown(vtname, getuid(), getgid());

	     /*
	      * now get the VT
	      */
	     if (ioctl(xf86Info.consoleFd, VT_ACTIVATE, xf86Info.vtno) != 0)
	     {
		  xf86Msg(X_WARNING, "xf86OpenConsole: VT_ACTIVATE failed\n");
	     }
	     if (ioctl(xf86Info.consoleFd, VT_WAITACTIVE, xf86Info.vtno) != 0)
	     {
		  xf86Msg(X_WARNING, "xf86OpenConsole: VT_WAITACTIVE failed\n");
	     }
	     if (ioctl(xf86Info.consoleFd, VT_GETMODE, &VT) < 0) 
	     {
		  FatalError("xf86OpenConsole: VT_GETMODE failed\n");
	     }

	     signal(SIGUSR1, xf86VTRequest);
	     
	     VT.mode = VT_PROCESS;
	     VT.relsig = SIGUSR1;
	     VT.acqsig = SIGUSR1;
	     if (ioctl(xf86Info.consoleFd, VT_SETMODE, &VT) < 0) 
	     {
		  FatalError("xf86OpenConsole: VT_SETMODE VT_PROCESS failed\n");
	     }
	     if (ioctl(xf86Info.consoleFd, KDSETMODE, KD_GRAPHICS) < 0)
	     {
		  FatalError("xf86OpenConsole: KDSETMODE KD_GRAPHICS failed\n");
	     }
	}
    }
    else 
    {   
	/* serverGeneration != 1 */
	/*
	 * now get the VT
	 */
	if (xf86Info.vtno != VT_NONE)
	{
	     if (ioctl(xf86Info.consoleFd, VT_ACTIVATE, xf86Info.vtno) != 0)
	     {
		  xf86Msg(X_WARNING, "xf86OpenConsole: VT_ACTIVATE failed\n");
	     }
	     if (ioctl(xf86Info.consoleFd, VT_WAITACTIVE, xf86Info.vtno) != 0)
	     {
		  xf86Msg(X_WARNING, "xf86OpenConsole: VT_WAITACTIVE failed\n");
	     }
	     /*
	      * If the server doesn't have the VT when the reset occurs,
	      * this is to make sure we don't continue until the activate
	      * signal is received.
	      */
	     if (!xf86Screens[0]->vtSema)
		  sleep(5);
	}
    }
    return;
}

void xf86CloseConsole()
{
    struct vt_mode   VT;

    if (xf86Info.vtno != VT_NONE)
    {
	 
#if 0
	 ioctl(xf86Info.consoleFd, VT_ACTIVATE, xf86Info.vtno);
	 ioctl(xf86Info.consoleFd, VT_WAITACTIVE, 0);
#endif
	 ioctl(xf86Info.consoleFd, KDSETMODE, KD_TEXT);  /* Back to text mode ... */
	 if (ioctl(xf86Info.consoleFd, VT_GETMODE, &VT) != -1)
	 {
	      VT.mode = VT_AUTO;
	      ioctl(xf86Info.consoleFd, VT_SETMODE, &VT); /* set dflt vt handling */
	 }
    }
    
    close(xf86Info.consoleFd);                 /* make the vt-manager happy */
    return;
}

int xf86ProcessArgument(argc, argv, i)
int argc;
char *argv[];
int i;
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

	/*
	 * Prevent server from attemping to open a new VT in the "-novt"
	 * flag was specified.
	 */
	if (!strcmp(argv[i], "-novt"))
	{
		VTnum = VT_NONE;
		return(1);
	}

	/*
	 * Undocumented flag to protect page 0 from read/write to help
	 * catch NULL pointer dereferences.  This is purely a debugging
	 * flag.
	 */
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
			VTnum = VT_DEFAULT;
			return(0);
		}
		return(1);
	}
	return(0);
}

void xf86UseMsg()
{
	ErrorF("vtXX                   use the specified VT number\n");
	ErrorF("-keeptty               ");
	ErrorF("don't detach controlling tty (for debugging only)\n");
	ErrorF("-novt                  ");
	ErrorF("don't allocate and open a new virtual terminal\n");
	return;
}


void
xf86_pmax_usleep(unsigned long n)
{
  struct timespec requested,remaining;
  int rv;

  requested.tv_sec = n/1000000;
  requested.tv_nsec = (n % 1000000) * 1000;
  
  while ((rv = nanosleep(&requested,&remaining)) < 0) {
	  if (errno != EINTR)
		  break;
	  
	  remaining = requested; /* structure assignment */
  }
    
  if (rv) {
      ErrorF("xf86_pmax_usleep: nanosleep() failed: rv=%d, errno=%d\n", rv, errno);
  }
}

#ifndef usleep

void
usleep(unsigned long n)
{
	xf86_pmax_usleep(n);
}

#endif
