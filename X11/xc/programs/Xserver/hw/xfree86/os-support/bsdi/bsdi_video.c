/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/bsdi/bsdi_video.c,v 3.9 2000/10/24 22:45:10 dawes Exp $ */
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
/* $XConsortium: bsdi_video.c /main/4 1996/02/21 17:51:22 kaleb $ */

#include "X.h"
#include "input.h"
#include "scrnintstr.h"

#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"
#include "xf86OSpriv.h"

#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif

/***************************************************************************/
/* Video Memory Mapping section                                            */
/***************************************************************************/

pointer
xf86MapVidMem(int ScreenNum, int Flags, unsigned long Base, unsigned long Size)
{
        pointer base;

	if (Base >= 0xA0000)
	{
		base = mmap(0, Size, PROT_READ|PROT_WRITE, MAP_FILE,
			     xf86Info.screenFd,
			     Base - 0xA0000);
		if (base == MAP_FAILED)
		{
		    FatalError("xf86MapVidMem: Could not mmap /dev/vga (%s)\n",
			       strerror(errno));
		}
		return(base);
	}
	else /* Base < 0xA0000 */
	{
	    FatalError("xf86MapVidMem: cannot mmap /dev/vga below 0xA0000\n");
	}
}

void
xf86UnMapVidMem(int ScreenNum, pointer Base, unsigned long Size)
{
	munmap((caddr_t)Base, Size);
}

Bool
xf86LinearVidMem()
{
	return(TRUE);
}

/***************************************************************************/
/* I/O Permissions section                                                 */
/***************************************************************************/

/*
 * BSDI has a single system-wide TSS I/O bitmap that covers ports up to
 * 0xFFFF.  By default, the TSS has ports 0x3B0-0x3DF enabled.
 *
 * It also allows the IOPL to be enabled or disabled on a per-process
 * basis.  Here, we use the IOPL only.
 */

static Bool ExtendedEnabled = FALSE;

void
xf86EnableIO()
{
	if (ExtendedEnabled)
		return;

	if (ioctl(xf86Info.consoleFd, PCCONENABIOPL, 0) < 0)
	{
		FatalError("%s: Failed to set IOPL for extended I/O\n",
			   "xf86EnableIOPorts");
	}
	ExtendedEnabled = TRUE;
}

void
xf86DisableIO()
{
	if (!ExtendedEnabled)
		return;

	ioctl(xf86Info.consoleFd, PCCONDISABIOPL, 0);
	ExtendedEnabled = FALSE;
}


/***************************************************************************/
/* Interrupt Handling section                                              */
/***************************************************************************/

Bool
xf86DisableInterrupts()
{
	if (!ExtendedEnabled)
	{
		if (ioctl(xf86Info.consoleFd, PCCONENABIOPL, 0) < 0)
		{
			return(FALSE);
		}
	}

#ifdef __GNUC__
	__asm__ __volatile__("cli");
#else 
	asm("cli");
#endif /* __GNUC__ */

	if (!ExtendedEnabled)
	{
		ioctl(xf86Info.consoleFd, PCCONDISABIOPL, 0);
	}

	return(TRUE);
}

void
xf86EnableInterrupts()
{
	if (!ExtendedEnabled)
	{
		ioctl(xf86Info.consoleFd, PCCONENABIOPL, 0);
	}

#ifdef __GNUC__
	__asm__ __volatile__("sti");
#else 
	asm("sti");
#endif /* __GNUC__ */

	if (!ExtendedEnabled)
	{
		ioctl(xf86Info.consoleFd, PCCONDISABIOPL, 0);
	}

	return;
}

void
xf86MapReadSideEffects(int ScreenNum, int Flags, pointer Base,
	unsigned long Size)
{
}

Bool
xf86CheckMTRR(int ScreenNum)
{
	return FALSE;
}

