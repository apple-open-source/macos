/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/os2/os2_video.c,v 3.15 2002/05/31 18:46:02 dawes Exp $ */
/*
 * (c) Copyright 1994,1999 by Holger Veit
 *			<Holger.Veit@gmd.de>
 * Modified 1996 by Sebastien Marineau <marineau@genie.uottawa.ca>
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
 * HOLGER VEIT  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF 
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
 * SOFTWARE.
 * 
 * Except as contained in this notice, the name of Holger Veit shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from Holger Veit.
 *
 */
/* $XConsortium: os2_video.c /main/8 1996/10/27 11:49:02 kaleb $ */

#define I_NEED_OS2_H
#include "X.h"
#include "input.h"
#include "scrnintstr.h"

#define INCL_DOSFILEMGR
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"
#include "xf86OSpriv.h"

#include "compiler.h"

/***************************************************************************/
/* Video Memory Mapping helper functions                                   */
/***************************************************************************/

/* This section uses the xf86sup.sys driver developed for xfree86.
 * The driver allows mapping of physical memory
 * You must install it with a line DEVICE=path\xf86sup.sys in config.sys.
 */

static HFILE mapdev = -1;
static ULONG stored_virt_addr;
static char* mappath = "\\DEV\\PMAP$";
static HFILE open_mmap() 
{
	APIRET rc;
	ULONG action;

	if (mapdev != -1)
		return mapdev;

	rc = DosOpen((PSZ)mappath, (PHFILE)&mapdev, (PULONG)&action,
	   (ULONG)0, FILE_SYSTEM, FILE_OPEN,
	   OPEN_SHARE_DENYNONE|OPEN_FLAGS_NOINHERIT|OPEN_ACCESS_READONLY,
	   (ULONG)0);
	if (rc!=0)
		mapdev = -1;
	return mapdev;
}

static void close_mmap()
{
	if (mapdev != -1)
		DosClose(mapdev);
	mapdev = -1;
}

/* this structure is used as a parameter packet for the direct access
 * ioctl of pmap$
 */

/* Changed here for structure of driver PMAP$ */

typedef struct{
	ULONG addr;
	ULONG size;
} DIOParPkt;

/* This is the data packet for the mapping function */

typedef struct {
	ULONG addr;
	USHORT sel;
} DIODtaPkt;

/***************************************************************************/
/* Video Memory Mapping section                                            */
/***************************************************************************/

/* ARGSUSED */
static pointer
mapVidMem(int ScreenNum, unsigned long Base, unsigned long Size, int flags)
{
	DIOParPkt	par;
	ULONG		plen;
	DIODtaPkt	dta;
	ULONG		dlen;
	static BOOL	ErrRedir = FALSE;
	APIRET		rc;

	par.addr	= (ULONG)Base;
	par.size	= (ULONG)Size;
	plen 		= sizeof(par);
	dlen		= sizeof(dta);

	open_mmap();
	if (mapdev == -1)
		FatalError("mapVidMem: install DEVICE=path\\XF86SUP.SYS!");

	if ((rc=DosDevIOCtl(mapdev, (ULONG)0x76, (ULONG)0x44,
	      (PVOID)&par, (ULONG)plen, (PULONG)&plen,
	      (PVOID)&dta, (ULONG)dlen, (PULONG)&dlen)) == 0) {
		xf86Msg(X_INFO,"mapVidMem succeeded: (ScreenNum= %d, Base= 0x%x, Size= 0x%x,paddr=0x%x)\n",
		ScreenNum, Base, Size, dta.addr);
		if (dlen==sizeof(dta)) {
			return (pointer)dta.addr;
		}
		/*else fail*/
	}

	/* fail */
	FatalError("mapVidMem FAILED!!: rc = %d (ScreenNum= %d, Base= 0x%x, Size= 0x%x return len %d)\n",
		rc, ScreenNum, Base, Size,dlen);
	return (pointer)0;
}

/* ARGSUSED */
static void
unmapVidMem(int ScreenNum, pointer Base, unsigned long Size)
{
	DIOParPkt	par;
	ULONG		plen,vmaddr;

/* We need here the VIRTADDR for unmapping, not the physical address      */
/* This should be taken care of either here by keeping track of allocated */
/* pointers, but this is also already done in the driver... Thus it would */
/* be a waste to do this tracking twice. Can this be changed when the fn. */
/* is called? This would require tracking this function in all servers,   */
/* and changing it appropriately to call this with the virtual adress	  */
/* If the above mapping function is only called once, then we can store   */
/* the virtual adress and use it here.... 				  */
	
	par.addr	= (ULONG)Base;
	par.size	= 0xffffffff; /* This is the virtual address parameter. Set this to ignore */
	plen 		= sizeof(par);

	if (mapdev != -1)
	    DosDevIOCtl(mapdev, (ULONG)0x76, (ULONG)0x46,
	      (PVOID)&par, (ULONG)plen, (PULONG)&plen,
	      &vmaddr, sizeof(ULONG), &plen);
        xf86Msg(X_INFO,"unmapVidMem: Unmap phys memory at base %x, virtual address %x\n",Base,vmaddr);

/* Now if more than one region has been allocated and we close the driver,
 * the other pointers will immediately become invalid. We avoid closing
 * driver for now, but this should be fixed for server exit
 */
 
	/* close_mmap(); */
}

/***************************************************************************/
/* Interrupt Handling section                                              */
/***************************************************************************/

Bool xf86DisableInterrupts()
{
	/* allow interrupt disabling but check for side-effects. 
	 * Not a good policy on OS/2...
	 */
        asm ("cli");
	return TRUE;
}

void xf86EnableInterrupts()
{
	/*Reenable*/
        asm ("sti");
}

/***************************************************************************/
/* Initialize video memory                                                 */
/***************************************************************************/

void
xf86OSInitVidMem(VidMemInfoPtr pVidMem)
{
        pVidMem->linearSupported = TRUE;
        pVidMem->mapMem = mapVidMem;
        pVidMem->unmapMem = unmapVidMem;
#if 0
        pVidMem->mapMemSparse = 0;
        pVidMem->unmapMemSparse = 0;
#endif
        pVidMem->setWC = 0; /* no MTRR support */
        pVidMem->undoWC = 0;
        pVidMem->initialised = TRUE;
}
