/* (c) Copyright 1998 by Sebastien Marineau
 *			<sebastien@qnx.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a 
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, 
 * and/or sell copies of the Software, and to permit persons to whom the 
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL 
 * SEBASTIEN MARINEAU BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF 
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
 * SOFTWARE.
 * 
 * Except as contained in this notice, the name of Sebastien Marineau shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from Sebastien Marineau.
 *
 * $XFree86: xc/programs/Xserver/hw/xfree86/os-support/nto/nto_video.c,v 1.4 2003/03/14 13:46:07 tsi Exp $
 */

/* This module contains the NTO-specific functions to deal with video 
 * framebuffer access and interrupts.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/neutrino.h>

#include <X.h>
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"


/* These are the interrupt enabling/disabling functions. 
 */

void xf86EnableInterrupts()
{
	InterruptEnable();
}

Bool xf86DisableInterrupts()
{
	InterruptDisable();
	return 0;
}

/* These are the routines to map/unmap video memory... */

Bool xf86LinearVidMem()
{
	/* Yes we can... */
	return(TRUE);
}

/* This is our shmem "Physical" handle */
int NTO_PhMem_fd = -1;


/* Map a chunk of physical video memory, using mmap */
pointer
xf86MapVidMem(int ScreenNum, int Flags, unsigned long Base, unsigned long Size)
{
    int fd;
    pointer base;

    if(NTO_PhMem_fd < 0) {
        if ((fd = open("/dev/mem", O_RDWR, 0777)) < 0) {
            FatalError("xf86MapVidMem: Failed to open /dev/mem\n");
        }
        NTO_PhMem_fd = fd;
    }

    base = mmap((caddr_t)0, Size,
	     (Flags & VIDMEM_READONLY) ? PROT_READ : (PROT_READ | PROT_WRITE),
             MAP_SHARED, NTO_PhMem_fd, (off_t)Base);

	ErrorF("MapVidMem: addr %08x size %08x addr %08x\n", Base, Size, base);
    if ((long)base == -1) {
        FatalError("xf86MapVidMem: Failed to mmap video memory\n");
    }

    return base;
}

/* ARGSUSED */
void
xf86UnMapVidMem(int ScreenNum, pointer Base, unsigned long Size)
{

	ErrorF("xf86UnmapVidMem: base %x size %x\n", Base, Size);
	munmap((caddr_t) Base, Size);

}

/* Finally, this function allows us to read the video BIOS content */

int
xf86ReadBIOS(Base, Offset, Buf, Len)
unsigned long Base, Offset;
unsigned char *Buf;
int Len;
{
    unsigned char * VirtBase;

	ErrorF("xf86ReadBIOS: base %x offset %x len %x\n", Base, Offset, Len);

    if (NTO_PhMem_fd == -1) {
        if ((NTO_PhMem_fd = open("/dev/mem", O_RDWR, 0777)) < 0) {
            FatalError("xf86ReadBIOS: cannot open Physical memory\n");
        }
    }

    /* Use mmap to map BIOS region. Note the restrictions on
     * mmap alignement of offset variable (which must be on a page
     * boundary).
     */
    VirtBase = (unsigned char *) mmap(0, (size_t)((Offset & 0x7fff) + Len), PROT_READ,
            MAP_SHARED, NTO_PhMem_fd, (off_t) (Base + (Offset & 0xffff8000)));
    if((long)VirtBase == -1) {
        FatalError( "xf86ReadBIOS: Could not mmap BIOS memory space, errno=%i\n", errno);
    }

    /* So now we have our mapping to the BIOS region */

    /* Do a sanity check on what we have just mapped */
    if (((off_t)((off_t)Offset & 0x7FFF) != (off_t)0) &&
            (VirtBase[0] != 0x55) &&
            (VirtBase[1] != 0xaa)) {
        ErrorF( "xf86ReadBIOS: BIOS sanity check failed, addr=%x\n", 
        	(int)Base + Offset);
        munmap(VirtBase, (Offset & 0x7fff) + Len);
        return -1;
     }

	/* Things look good: copy BIOS data */
    memcpy(Buf, VirtBase + (Offset & 0x7fff), Len);
    munmap(VirtBase, (Offset & 0x7fff) + Len);

    return Len;
}

void
xf86MapReadSideEffects(int ScreenNum, int Flags, pointer base,
                        unsigned long Size)
{
                return;
}

Bool
xf86CheckMTRR(int s)
{
	return FALSE;
}

