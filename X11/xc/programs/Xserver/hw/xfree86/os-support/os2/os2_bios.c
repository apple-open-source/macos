/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/os2/os2_bios.c,v 3.11 2002/05/31 18:46:01 dawes Exp $ */
/*
 * (c) Copyright 1994 by Holger Veit
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
/* $XConsortium: os2_bios.c /main/5 1996/10/27 11:48:45 kaleb $ */

#define I_NEED_OS2_H
#include "X.h"
#include "input.h"
#include "scrnintstr.h"

#define INCL_32
#define INCL_DOS
#define INCL_DOSFILEMGR
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"

/*
 * Read BIOS via xf86sup.SYS device driver
 */

static APIRET doioctl(HFILE fd,ULONG addr,ULONG len,unsigned char* dbuf)
{
	UCHAR	*dta;
	ULONG	plen,dlen;
	APIRET rc;

	struct {
		ULONG command;
		ULONG physaddr;
		USHORT numbytes;
	} par;

	/* prepare parameter and data packets for ioctl */
	par.command 	= 0;
	par.physaddr 	= addr;
	par.numbytes 	= dlen = len;
	plen 		= sizeof(par);

	/* issue call to get a readonly copy of BIOS ROM */
	rc = DosDevIOCtl(fd, (ULONG)0x76, (ULONG)0x64,
	   (PVOID)&par, (ULONG)plen, (PULONG)&plen,
	   (PVOID)dbuf, (ULONG)dlen, (PULONG)&dlen);

	return rc;
}

int xf86ReadBIOS(Base, Offset, Buf, Len)
unsigned long Base;
unsigned long Offset;
unsigned char *Buf;
int Len;
{
	HFILE	fd;
	int	i;
	ULONG	action;
	APIRET	rc;
	ULONG	Phys_address;
	UCHAR*	dta;
	int	off, chunksz,lensave;

	/* allocate dta */
	dta = (UCHAR*)xalloc(Len);

	Phys_address=Base+Offset;

	/* open the special device pmap$ (default with OS/2) */
	if (DosOpen((PSZ)"PMAP$", (PHFILE)&fd, (PULONG)&action,
	   (ULONG)0, FILE_SYSTEM, FILE_OPEN,
	   OPEN_SHARE_DENYNONE|OPEN_FLAGS_NOINHERIT|OPEN_ACCESS_READONLY,
	   (ULONG)0) != 0) {
		FatalError("xf86ReadBIOS: install DEVICE=path\\xf86sup.SYS!");
		return -1;
	}

 	/* copy 32K at a time */
 	off = 0;
 	lensave = Len;
 	while (Len > 0) {
 		chunksz = (Len > 32768) ? 32768 : Len;
 		Len -= chunksz;
 		rc = doioctl(fd,(ULONG)Phys_address,chunksz,dta+off);
 		if (rc != 0) {
 			FatalError("xf86ReadBIOS: BIOS map failed, addr=%lx, rc=%d\n",
 			Phys_address,rc);
 			xfree(dta);
 			DosClose(fd);
 			return -1;
 		}		
 		off += chunksz;
  	}

	/*
	 * Sanity check... No longer fatal, as some PS/1 and PS/2 fail here but still work.
	 * S. Marineau, 10/10/96
         */
#if 0
	if ((Phys_address & 0x7fff) != 0 && 
		(dta[0] != 0x55 || dta[1] != 0xaa)) {
		FatalError("BIOS sanity check failed, addr=%x\nPlease report if you encounter problems\n",
			Phys_address);
	}
#endif

	/* copy data to buffer */
 	memcpy(Buf, dta, lensave);
	xfree(dta);

	/* close device */
	DosClose(fd);

 	return(lensave);
}
