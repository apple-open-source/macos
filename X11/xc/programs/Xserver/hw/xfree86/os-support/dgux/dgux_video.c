/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/dgux/dgux_video.c,v 1.7 2003/03/14 13:46:05 tsi Exp $ */
/*
 * INTEL DG/UX RELEASE 4.20 MU03
 * Copyright 1997 Takis Psarogiannakopoulos Cambridge,UK
 * <takis@dpmms.cam.ac.uk>
 *
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.
 * XFREE86 PROJECT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE.
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FITNESS, IN NO EVENT SHALL XCONSORTIUM BE LIABLE FOR
 * ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "X.h"
#include "input.h"
#include "scrnintstr.h"

#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"

#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

/* Stuff for the SET_IOPL() ,RESET_IOPL() */
/* #include <fcntl.h> */
static int io_takis;
int set_takis;


/***************************************************************************/
/* SET_IOPL() and RESET_IOPL() section for ix86 DG/ux 4.20MU03             */
/***************************************************************************/


int SET_IOPL()
{
    io_takis=open("/dev/console", O_RDWR,0);
    if ((io_takis) < 0)
    {
        return(-1);
    }
    set_takis = ioctl(io_takis,KDENABIO,0);

    if (set_takis < 0)
    {
        return(-1);
    }
    return(1);
}




void RESET_IOPL()
{

    ioctl(io_takis,KDDISABIO,0);
    close(io_takis);
    return;

}

/***************************************************************************/
/* DG/ux Video Memory Mapping part                                         */
/***************************************************************************/

#undef HAS_SVR3_MMAPDRV /* ix86 DG/ux is a typical SVR4 without SVR3_MMAPDRV */

Bool xf86LinearVidMem()
{
   return(TRUE);
}

pointer
xf86MapVidMem(int ScreenNum, int Flags, unsigned long Base, unsigned long Size)
{
        pointer base;
        int fd;

        fd = open(DEV_MEM, (Flags & VIDMEM_READONLY) ? O_RDONLY : O_RDWR);
        if (fd < 0)
        {
                FatalError("xf86MapVidMem: failed to open %s (%s)\n",
                           DEV_MEM, strerror(errno));
        }
        base = mmap((caddr_t)0, Size,
		    (Flags & VIDMEM_READONLY) ?
		    PROT_READ : (PROT_READ | PROT_WRITE),
                    MAP_SHARED, fd, (off_t)Base);
        close(fd);
        if (base == MAP_FAILED)
        {
                FatalError("%s: Could not mmap framebuffer [s=%x,a=%x] (%s)\n",
                           "xf86MapVidMem", Size, Base, strerror(errno));
        }

        return(base);
}

void xf86UnMapVidMem(ScreenNum, Region, Base, Size)
int ScreenNum;
int Region;
pointer Base;
unsigned long Size;
{
	munmap(Base, Size);
}

/***************************************************************************/
/* I/O Permissions section                                                 */
/***************************************************************************/

#define ALWAYS_USE_EXTENDED
#ifdef ALWAYS_USE_EXTENDED

static Bool ScreenEnabled[MAXSCREENS];
static Bool ExtendedEnabled = FALSE;
static Bool InitDone = FALSE;

void
xf86ClearIOPortList(ScreenNum)
int ScreenNum;
{
	if (!InitDone)
	{
		int i;
		for (i = 0; i < MAXSCREENS; i++)
			ScreenEnabled[i] = FALSE;
		InitDone = TRUE;
	}
	return;
}

void
xf86AddIOPorts(ScreenNum, NumPorts, Ports)
int ScreenNum;
int NumPorts;
unsigned *Ports;
{
	return;
}

void
xf86EnableIOPorts(ScreenNum)
int ScreenNum;
{
	int i;

	ScreenEnabled[ScreenNum] = TRUE;

	if (ExtendedEnabled)
		return;

	if (SET_IOPL() < 0)
	{
		FatalError("%s: Failed to set IOPL for extended I/O\n",
			   "xf86EnableIOPorts");
	}
	ExtendedEnabled = TRUE;

	return;
}
	
void
xf86DisableIOPorts(ScreenNum)
int ScreenNum;
{
	int i;

	ScreenEnabled[ScreenNum] = FALSE;

	if (!ExtendedEnabled)
		return;

	for (i = 0; i < MAXSCREENS; i++)
		if (ScreenEnabled[i])
			return;

	RESET_IOPL();
	ExtendedEnabled = FALSE;

	return;
}

#else /* !ALWAYS_USE_EXTENDED */

#define DISABLED	0
#define NON_EXTENDED	1
#define EXTENDED	2

static unsigned *EnabledPorts[MAXSCREENS];
static int NumEnabledPorts[MAXSCREENS];
static Bool ScreenEnabled[MAXSCREENS];
static Bool ExtendedPorts[MAXSCREENS];
static Bool ExtendedEnabled = FALSE;
static Bool InitDone = FALSE;
static struct kd_disparam OrigParams;

void xf86ClearIOPortList(ScreenNum)
int ScreenNum;
{
	if (!InitDone)
	{
		xf86InitPortLists(EnabledPorts, NumEnabledPorts, ScreenEnabled,
				  ExtendedPorts, MAXSCREENS);
		if (ioctl(xf86Info.consoleFd, KDDISPTYPE, &OrigParams) < 0)
		{
			FatalError("%s: Could not get display parameters\n",
				   "xf86ClearIOPortList");
		}
		InitDone = TRUE;
		return;
	}
	ExtendedPorts[ScreenNum] = FALSE;
	if (EnabledPorts[ScreenNum] != (unsigned *)NULL)
		xfree(EnabledPorts[ScreenNum]);
	EnabledPorts[ScreenNum] = (unsigned *)NULL;
	NumEnabledPorts[ScreenNum] = 0;
}

void xf86AddIOPorts(ScreenNum, NumPorts, Ports)
int ScreenNum;
int NumPorts;
unsigned *Ports;
{
	int i;

	if (!InitDone)
	{
	    FatalError("xf86AddIOPorts: I/O control lists not initialised\n");
	}
	EnabledPorts[ScreenNum] = xrealloc(EnabledPorts[ScreenNum], 
			(NumEnabledPorts[ScreenNum]+NumPorts)*sizeof(unsigned));
	for (i = 0; i < NumPorts; i++)
	{
		EnabledPorts[ScreenNum][NumEnabledPorts[ScreenNum] + i] =
								Ports[i];
		if (Ports[i] > 0x3FF)
			ExtendedPorts[ScreenNum] = TRUE;
	}
	NumEnabledPorts[ScreenNum] += NumPorts;
}

void xf86EnableIOPorts(ScreenNum)
int ScreenNum;
{
	struct kd_disparam param;
	int i, j;

	if (ScreenEnabled[ScreenNum])
		return;

	for (i = 0; i < MAXSCREENS; i++)
	{
		if (ExtendedPorts[i] && (ScreenEnabled[i] || i == ScreenNum))
		{
		    if (SET_IOPL() < 0)
		    {
			FatalError("%s: Failed to set IOPL for extended I/O\n",
				   "xf86EnableIOPorts");
		    }
		    ExtendedEnabled = TRUE;
		    break;
		}
	}
	if (ExtendedEnabled && i == MAXSCREENS)
	{
		RESET_IOPL();
		ExtendedEnabled = FALSE;
	}
	if (ioctl(xf86Info.consoleFd, KDDISPTYPE, &param) < 0)
	{
		FatalError("%s: Could not get display parameters\n",
			   "xf86EnableIOPorts");
	}
	for (i = 0; i < NumEnabledPorts[ScreenNum]; i++)
	{
		unsigned port = EnabledPorts[ScreenNum][i];

		if (port > 0x3FF)
			continue;

		if (!xf86CheckPorts(port, EnabledPorts, NumEnabledPorts,
				    ScreenEnabled, MAXSCREENS))
		{
			continue;
		}
		for (j=0; j < MKDIOADDR; j++)
		{
			if (param.ioaddr[j] == port)
			{
				break;
			}
		}
		if (j == MKDIOADDR)
		{
			if (ioctl(xf86Info.consoleFd, KDADDIO, port) < 0)
			{
				FatalError("%s: Failed to enable port 0x%x\n",
					   "xf86EnableIOPorts", port);
			}
		}
	}
	if (ioctl(xf86Info.consoleFd, KDENABIO, 0) < 0)
	{
		FatalError("xf86EnableIOPorts: I/O port enable failed (%s)\n",
			   strerror(errno));
	}
	ScreenEnabled[ScreenNum] = TRUE;
	return;
}

void xf86DisableIOPorts(ScreenNum)
int ScreenNum;
{
	struct kd_disparam param;
	int i, j;

	if (!ScreenEnabled[ScreenNum])
		return;

	ScreenEnabled[ScreenNum] = FALSE;
	for (i = 0; i < MAXSCREENS; i++)
	{
		if (ScreenEnabled[i] && ExtendedPorts[i])
			break;
	}
	if (ExtendedEnabled && i == MAXSCREENS)
	{
		RESET_IOPL();
		ExtendedEnabled = FALSE;
	}
	/* Turn off I/O before changing the access list */
	ioctl(xf86Info.consoleFd, KDDISABIO, 0);
	if (ioctl(xf86Info.consoleFd, KDDISPTYPE, &param) < 0)
	{
		ErrorF("%s: Could not get display parameters\n",
		       "xf86DisableIOPorts");
		return;
	}

	for (i=0; i < MKDIOADDR; i++)
	{
		if (param.ioaddr[i] == 0)
		{
			break;
		}
		if (!xf86CheckPorts(param.ioaddr[i], EnabledPorts,
				    NumEnabledPorts, ScreenEnabled, MAXSCREENS))
		{
			continue;
		}
		for (j=0; j < MKDIOADDR; j++)
		{
			if (param.ioaddr[i] == OrigParams.ioaddr[j])
			{
				/*
				 * Port was one of the original ones; don't
				 * touch it.
				 */
				break;
			}
		}
		if (j == MKDIOADDR)
		{
			/*
			 * We added this port, so remove it.
			 */
			ioctl(xf86Info.consoleFd, KDDELIO, param.ioaddr[i]);
		}
	}
	for (i = 0; i < MAXSCREENS; i++)
	{
		if (ScreenEnabled[i])
		{
			ioctl(xf86Info.consoleFd, KDENABIO, 0);
			break;
		}
	}
	return;
}
#endif

void xf86DisableIOPrivs()
{
	if (ExtendedEnabled)
		RESET_IOPL();
	return;
}

/***************************************************************************/
/* Interrupt Handling section                                              */
/***************************************************************************/


Bool xf86DisableInterrupts()
{

#ifdef __GNUC__
        __asm__ __volatile__("cli");
#else
        asm("cli");
#endif /* __GNUC__ */

        return(TRUE);
}

void xf86EnableInterrupts()
{

#ifdef __GNUC__
        __asm__ __volatile__("sti");
#else
        asm("sti");
#endif /* __GNUC__ */

        return;
}


void
xf86MapReadSideEffects(int ScreenNum, int Flags, pointer Base,
	unsigned long Size)
{
}

