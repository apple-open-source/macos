/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/pmax/pmax_map.c,v 1.8 2000/11/19 16:38:06 tsi Exp $ */
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

#include "X.h"

#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"

#include "Pci.h"

/***************************************************************************/
/* Video Memory Mapping section                                            */
/***************************************************************************/

/*
 * Map an I/O region given its address (host POV)
 */
void *
pmax_iomap(unsigned long base, unsigned long len)
{
	int fd;
	void *rv;

	if ((fd = open("/dev/iomem", O_RDWR)) < 0)
	{
		ErrorF("pmax_iomap: failed to open /dev/iomem (%s)\n",
		       strerror(errno));
		return(MAP_FAILED);
	}
	
	rv = (void *)mmap((caddr_t)0, len, PROT_READ|PROT_WRITE,
			  MAP_SHARED, fd, (off_t)base);
	
	close(fd);
	return(rv);
}

Bool
xf86LinearVidMem()
{
	return TRUE;
}

extern void * pmax_iomap(unsigned long, unsigned long);

pointer
xf86MapVidMem(int ScreenNum, int Region, pointer Base, unsigned long Size)
{
	ErrorF("%s: Not supported on this OS. Drivers should use xf86MapPciMem() instead\n",
	       "xf86MapVidMem");
	FatalError("%s: Cannot map [s=%x,a=%x]\n", "xf86MapVidMem", Size, Base);
}


pointer
xf86MapPciMem(int ScreenNum, int Flags, PCITAG Tag, pointer Base,
		unsigned long Size)
{
	pointer hostbase = pciBusAddrToHostAddr(Tag, Base);
	pointer base;

	base = (pointer) pmax_iomap((unsigned long)hostbase, Size);
	if (base == MAP_FAILED)	{
		xf86Msg(X_WARNING,
			"xf86MapPciMem: Could not mmap PCI memory "
			"[base=0x%x,hostbase=0x%x,size=%x] (%s)\n",
			Base, hostbase, Size, strerror(errno));
	}
	return((pointer)base);
}


/* ARGSUSED */
void
xf86UnMapVidMem(int ScreenNum, pointer Base, unsigned long Size)
{
	munmap(Base, Size);
}

/*
 * Read BIOS via mmap()ing /dev/iomem.
 */
/*ARGSUSED*/
int
xf86ReadBIOS(unsigned long Base, unsigned long Offset, unsigned char *Buf, int Len)
{
	ErrorF("%s: Not supported on this OS. Drivers should use xf86ReadPciBIOS() instead\n",
	       "xf86ReadBIOS");
	FatalError("%s: Cannot read BIOS [base=0x%x,offset=0x%x,size=%d]\n", "xf86ReadBIOS", Base, Offset, Len);
}

int
xf86ReadPciBIOS(unsigned long Base, unsigned long Offset, PCITAG Tag,
		unsigned char *Buf, int Len)
{
	pointer hostbase = pciBusAddrToHostAddr(Tag, (void *)Base);
	char   *base;
	int	psize;
	int	mlen;

	psize = xf86getpagesize();
	Offset += Base & (psize - 1);
	Base &= ~(psize - 1);
	mlen = (Offset + Len + psize - 1) & ~(psize - 1);
	base = pmax_iomap((unsigned long)hostbase, mlen);
	if (base == MAP_FAILED)	{
		xf86Msg(X_WARNING, "xf86ReadPciBIOS: Could not mmap PCI memory"
			" [base=0x%x,hostbase=0x%x,size=%x] (%s)\n",
			Base, hostbase, mlen, strerror(errno));
		return(0);
	}
	
	(void)memcpy(Buf, base + Offset, Len);
	(void)munmap(base, mlen);
	return(Len);
}

/***************************************************************************/
/* Interrupt Handling section                                              */
/***************************************************************************/

#include <sys/ipl.h>

#ifndef PL_HI
#define PL_HI PL8
#endif

#ifndef PL_0
#define PL_0 PL0
#endif

static void *spl_map_addr = NULL;

void
pmax_init_splmap(void)
{
     spl_map_addr = spl_map(0);
     if (!spl_map_addr) {
	  xf86Msg(X_WARNING,
	 	"pmax_init_splmap: spl_map() failed. "
		"Cannot bind to IPL register\n");
	  xf86ErrorF("\tInterrupts cannot be disabled/enabled !!!\n");
     }
}


Bool
xf86DisableInterrupts()
{
	if (spl_map_addr) {
		(void)spl_request(PL_HI,spl_map_addr);
		return(TRUE);
	}
	
	return(FALSE);
}

void xf86EnableInterrupts()
{
	if (spl_map_addr) {
		(void)spl_request(PL_0, spl_map_addr);
	}
}

