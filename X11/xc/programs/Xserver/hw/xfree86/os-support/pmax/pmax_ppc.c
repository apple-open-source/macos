/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/pmax/pmax_ppc.c,v 1.4 2000/07/31 23:25:18 tsi Exp $ */
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
 */

#include <sys/types.h>
#include <errno.h>

#include <sys/prosrfs.h>
#include <sys/cpu.h>

#include "compiler.h"

#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"

volatile unsigned char *ioBase = MAP_FAILED;  /* Also referenced by compiler.h */
unsigned long ioSize = 0;

#undef outb
#undef outw
#undef outl
#undef inb
#undef inw
#undef inl

void
outb(unsigned int a, unsigned char b)
{
	if (ioBase == MAP_FAILED) {
		ErrorF("outb(0x%04X, 0x%02X) fails.  Uninitialized ioBase\n", a, b);
		return;
	}
	
	*((volatile unsigned char *)(ioBase + a)) = b; eieio();
}	

void
outw(unsigned int a, unsigned short w) 
{
	if (ioBase == MAP_FAILED) {
		ErrorF("outw(0x%04X, 0x%04X) fails.  Unitialized ioBase\n", a, w);
		return;
	}
	
	stw_brx(w,ioBase,a); eieio();
}

void
outl(unsigned int a, unsigned int l) 
{
	if (ioBase == MAP_FAILED) {
		ErrorF("outl(0x%04X, 0x%08X) fails.  Unitialized ioBase\n", a, l);
		return;
	}
	
	stl_brx(l,ioBase,a); eieio();
}

unsigned char
inb(unsigned int a) 
{
	unsigned char b;

	if (ioBase == MAP_FAILED) {
		FatalError("%s(0x%04X) fails.  Unitialized ioBase\n", "inb", a);
		/*NOTREACHED*/
	}
	
	b = *((volatile unsigned char *)(ioBase + a));
	
	return(b);
}

unsigned short
inw(unsigned int a) 
{
	unsigned short w;

	if (ioBase == MAP_FAILED) {
		FatalError("%s(0x%04X) fails.  Unitialized ioBase\n", "inw", a);
		/*NOTREACHED*/
	}
	
	w = ldw_brx(ioBase,a);
	return(w);
}

unsigned int
inl(unsigned int a) 
{
	unsigned int l;
	
	if (ioBase == MAP_FAILED) {
		FatalError("%s(0x%04X) fails.  Unitialized ioBase\n", "inl", a);
		/*NOTREACHED*/
	}
	
	l = ldl_brx(ioBase, a);
	return(l);
}

#ifdef PPCIO_DEBUG

void
debug_outb(unsigned int a, unsigned char b, int line, char *file)
{
	if (xf86Verbose > 3)
		ErrorF("outb(0x%04X, 0x%02X) at line %d, file \"%s\"\n", a, b, line, file);
	
	outb(a,b);
}	

void
debug_outw(unsigned int a, unsigned short w, int line, char *file) 
{
	if (xf86Verbose > 3)
		ErrorF("outw(0x%04X, 0x%04X) at line %d, file \"%s\"\n", a, w, line, file);
	
	outw(a,w);
}

void
debug_outl(unsigned int a, unsigned int l, int line, char *file)
{
	if (xf86Verbose > 3)
		ErrorF("outl(0x%04X, 0x%08X) at line %d, file \"%s\"\n", a, l, line, file);

	outl(a,l);
}


unsigned char
debug_inb(unsigned int a, int line, char *file)
{
	unsigned char b;
	
	if (xf86Verbose > 4)
		ErrorF("Calling %s(0x%04x) at line %d, file \"%s\" ...\n", "inb", a, line, file);

	b = inb(a);
	
	if (xf86Verbose > 3)
		ErrorF("... %s(0x%04X) returns 0x%02X\n", "inb", a, b);

	return(b);
}

unsigned short
debug_inw(unsigned int a, int line, char *file) 
{
	unsigned short w;
	
	if (xf86Verbose > 4)
		ErrorF("Calling %s(0x%04x) at line %d, file \"%s\" ...\n", "inw", a, line, file);

	w = inw(a);
	
	if (xf86Verbose > 3)
		ErrorF("... %s(0x%04X) returns 0x%04X\n", "inw", a, w);

	return(w);
}

unsigned int
debug_inl(unsigned int a, int line, char *file) 
{
	unsigned int l;
	
	if (xf86Verbose > 4)
		ErrorF("Calling %s(0x%04x) at line %d, file \"%s\" ...\n", "inl", a, line, file);

	l = inl(a);
	
	if (xf86Verbose > 3)
		ErrorF("... %s(0x%04X) returns 0x%08X\n", "inl", a, l);
	
	return(l);
}

#endif /* PPCIO_DEBUG */

/*
 * This is neccessary on the PPC 604 (and 604e) because they have
 * separate I and D caches and the caches must be manually synchronized
 * when applying relocation to the instruction portion of loaded modules.
 */
#define LINESIZE        32
#define CACHE_LINE(a)   (((unsigned long)a) & ~(LINESIZE-1))

void
ppc_flush_icache(char *addr)
{
	/* Flush D-cache to memory */
	__inst_dcbf (addr, 0);
	__inst_dcbf (addr, LINESIZE);
	__inst_sync ();
	
	/* Invalidate I-cache */
	__inst_icbi (addr, 0);
	__inst_icbi (addr, LINESIZE);
	__inst_sync ();
	__inst_isync ();
}
