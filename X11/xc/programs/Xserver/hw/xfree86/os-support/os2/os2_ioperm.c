/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/os2/os2_ioperm.c,v 3.6 1999/04/29 09:13:49 dawes Exp $ */
/*
 * Copyright 1993 by David Wexelblat <dwex@goblin.org>
 * Modified 1996 by Sebastien Marineau <marineau@genie.uottawa.ca>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of David Wexelblat not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  David Wexelblat makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * DAVID WEXELBLAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL DAVID WEXELBLAT BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */
/* $XConsortium: os2_ioperm.c /main/4 1996/04/18 16:50:01 kaleb $ */



#define I_NEED_OS2_H
#define INCL_32
#define INCL_DOS
#define INCL_DOSFILEMGR
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"

/*
 * To access I/O ports under OS/2, we use the xf86sup.sys driver.
 * For the moment, we use a function which basically grants IO priviledge
 * to the whole server. NOTE: Once the server is running, we should 
 * change this to use inline IO functions through the callgate returned by 
 * the fastio$ driver.
 */

int ioEnabled=FALSE;
ULONG action;
char *ioDrvPath = "/dev/fastio$";
USHORT callgate[3]={0,0,0};


void xf86EnableIO()
{

HFILE hfd;
	ULONG dlen;
	APIRET rc;

	/* no need to call multiple times */
	if (ioEnabled) return;
	
	if (DosOpen((PSZ)ioDrvPath, (PHFILE)&hfd, (PULONG)&action,
	   (ULONG)0, FILE_SYSTEM, FILE_OPEN,
	   OPEN_SHARE_DENYNONE|OPEN_FLAGS_NOINHERIT|OPEN_ACCESS_READONLY,
	   (ULONG)0) != 0) {
		xf86Msg(X_ERROR,"Error opening fastio$ driver...\n");
		xf86Msg(X_ERROR,"Please install xf86sup.sys in config.sys!\n");
		exit(42);
	}
	callgate[0] = callgate[1] = 0;

/* Get callgate from driver for fast io to ports and other stuff */

	rc = DosDevIOCtl(hfd, (ULONG)0x76, (ULONG)0x64,
		NULL, 0, NULL,
		(ULONG*)&callgate[2], sizeof(USHORT), &dlen);
	if (rc) {
		xf86Msg(X_ERROR,
			"EnableIOPorts failed, rc=%d, dlen=%d; emergency exit\n",
			rc,dlen);
		DosClose(hfd);
		exit(42);
	}

/* Calling callgate with function 13 sets IOPL for the program */

	asm volatile ("movl $13,%%ebx;.byte 0xff,0x1d;.long _callgate"
			: /*no outputs */ 
			: /*no inputs */
			: "eax","ebx","ecx","edx","cc");

	ioEnabled = TRUE;
        DosClose(hfd);
	return;
}

void xf86DisableIO()
{
HFILE hfd;
	ULONG dlen;
	APIRET rc;

	/* no need to call multiple times */
	if (!ioEnabled) return;
	
	if (DosOpen((PSZ)ioDrvPath, (PHFILE)&hfd, (PULONG)&action,
	   (ULONG)0, FILE_SYSTEM, FILE_OPEN,
	   OPEN_SHARE_DENYNONE|OPEN_FLAGS_NOINHERIT|OPEN_ACCESS_READONLY,
	   (ULONG)0) != 0) {
		xf86Msg(X_ERROR,"Error opening fastio$ driver...\n");
		xf86Msg(X_ERROR,"Please install xf86sup.sys in config.sys!\n");
		return;
	}
	callgate[0] = callgate[1] = 0;

	rc = DosDevIOCtl(hfd, (ULONG)0x76, (ULONG)0x64,
		NULL, 0, NULL,
		(ULONG*)&callgate[2], sizeof(USHORT), &dlen);
	if (rc) {
		xf86Msg(X_ERROR,"DisableIOPorts failed, rc=%d, dlen=%d\n",
			rc,dlen);
		DosClose(hfd);
		return;
	}

/* Function 14 of callgate brings program back to ring 3 */

	asm volatile ("movl $14,%%ebx;.byte 0xff,0x1d;.long _callgate"
			: /*no outputs */ 
			: /*no inputs */
			: "eax","ebx","ecx","edx","cc");
	ioEnabled=FALSE;
        DosClose(hfd);
	return;

}

