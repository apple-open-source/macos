/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/dgux/bios_DGmmap.c,v 1.4 2000/11/19 16:38:06 tsi Exp $ */
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
#include "input.h"
#include "scrnintstr.h"

#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"

/*
 * Read the BIOS via mmap() to the device /dev/mem.
 */
int xf86ReadBIOS(Base, Offset, Buf, Len)
unsigned long Base;
unsigned long Offset;
unsigned char *Buf;
int Len;
{
	int fd;
	unsigned char *ptr;
	int psize;
	int mlen;

	if ((fd = open(DEV_MEM, O_RDONLY)) < 0)
	{
		ErrorF("xf86ReadBios: Failed to open %s (%s)\n", DEV_MEM,
		       strerror(errno));
		return(-1);
	}
	psize = xf86getpagesize();
	Offset += Base & (psize - 1);
	Base &= ~(psize - 1);
	mlen = (Offset + Len + psize - 1) & ~(psize - 1);
	ptr = (unsigned char *)mmap((caddr_t)0, mlen, PROT_READ, MAP_SHARED,
				    fd, (off_t)Base);
	if ((int)ptr == -1)
	{
		ErrorF("xf86ReadBios: %s mmap failed\n", DEV_MEM);
		close(fd);
		return(-1);
	}
	(void)memcpy(Buf, (void *)(ptr + Offset), Len);
	(void)munmap((caddr_t)ptr, mlen);
	(void)close(fd);
	return(Len);
}
