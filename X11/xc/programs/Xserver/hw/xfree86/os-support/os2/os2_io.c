/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/os2/os2_io.c,v 3.17 2003/02/17 15:11:58 dawes Exp $ */
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
/* $XConsortium: os2_io.c /main/9 1996/05/13 16:38:07 kaleb $ */

#define I_NEED_OS2_H
#include "X.h"
#include "Xpoll.h"
#include "compiler.h"
#include <time.h>

#define INCL_DOSPROCESS
#define INCL_KBD
#define INCL_MOU
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"

int os2MouseQueueQuery();
int os2KbdQueueQuery();
void os2RecoverFromPopup();
void os2CheckPopupPending();
extern BOOL os2PopupErrorPending;
int _select2 (int, fd_set *, fd_set *,fd_set *, struct timeval *);


/***************************************************************************/

void xf86SoundKbdBell(loudness, pitch, duration)
int loudness;
int pitch;
int duration;
{
	DosBeep((ULONG)pitch, (ULONG)duration);
}

void xf86SetKbdLeds(leds)
int leds;
{
	KBDINFO kinfo;
	APIRET rc;

	rc = KbdGetStatus(&kinfo,(HKBD)xf86Info.consoleFd);
	if (!rc) {
		kinfo.fsMask = 0x10;
		kinfo.fsState &= ~0x70;
		kinfo.fsState |= (leds&0x70);
		KbdSetStatus(&kinfo,(HKBD)xf86Info.consoleFd);
	}
}

int xf86GetKbdLeds()
{
	KBDINFO kinfo;
	APIRET rc;

	rc = KbdGetStatus(&kinfo,(HKBD)xf86Info.consoleFd);
	return rc ? 0 : kinfo.fsState & 0x70;
}

#if NeedFunctionPrototypes
void xf86SetKbdRepeat(char rad)
#else
void xf86SetKbdRepeat(rad)
char rad;
#endif
{
	/*notyet*/
}

void xf86KbdInit()
{
	/*none required*/
}


USHORT OrigKbdState;
USHORT OrigKbdInterim;

typedef struct {
    USHORT state;
    UCHAR makeCode;
    UCHAR breakCode;
    USHORT keyID;
} HOTKEYPARAM;


int xf86KbdOn()
{
	KBDINFO info;
	APIRET rc;
	int i,k;
	ULONG len;


	KbdGetStatus(&info,(HKBD)xf86Info.consoleFd);
	OrigKbdState=info.fsMask;
	OrigKbdInterim=info.fsInterim;
	info.fsMask &= ~0x09;
	info.fsMask |= 0x136;
	info.fsInterim &= ~0x20;
	KbdSetStatus(&info,(HKBD)xf86Info.consoleFd);
	return -1;
}

int xf86KbdOff()
{
	ULONG len;
	APIRET rc;
	KBDINFO info;

	info.fsMask=OrigKbdState;
	info.fsInterim=OrigKbdInterim;
	KbdSetStatus(&info,(HKBD)xf86Info.consoleFd);
	return -1;
}

#if 0 /*OBSOLETE*/
void xf86MouseInit(mouse)
MouseDevPtr mouse;
{
	HMOU fd;
	APIRET rc;
	USHORT nbut;

	if (serverGeneration == 1) {
		rc = MouOpen((PSZ)NULL,(PHMOU)&fd);
		if (rc != 0)
			FatalError("Cannot open mouse, rc=%d\n", rc);
			mouse->mseFd = fd;
	}
	
	/* flush mouse queue */
	MouFlushQue(fd);

	/* check buttons */
	rc = MouGetNumButtons(&nbut,fd);
	if (rc == 0)
		xf86Msg(X_INFO,"OsMouse has %d button(s).\n",nbut);
}
#endif

#if 0 /*OBSOLETE*/
int xf86MouseOn(mouse)
MouseDevPtr mouse;
{
#if 0
	HMOU fd;
	APIRET rc;
	USHORT nbut;
#endif
	xf86Msg (X_ERROR,
		"Calling MouseOn, a bad thing.... Must be some bug in the code!\n");

#if 0
	if (serverGeneration == 1) {
		rc = MouOpen((PSZ)NULL,(PHMOU)&fd);
		if (rc != 0)
			FatalError("Cannot open mouse, rc=%d\n", rc);
			mouse->mseFd = fd;
	}
	
	/* flush mouse queue */
	MouFlushQue(fd);

	/* check buttons */
	rc = MouGetNumButtons(&nbut,fd);
	if (rc == 0)
		xf86Msg(X_INFO,"OsMouse has %d button(s).\n",nbut);

	return (mouse->mseFd);
#endif
}
#endif

#if 0 /*OBSOLETE*/
/* This table is a bit irritating, because these mouse types are infact
 * defined in the OS/2 kernel, but I want to force the user to put
 * "OsMouse" in the config file, and not worry about the particular mouse
 * type that is connected.
 */
Bool xf86SupportedMouseTypes[] =
{
	FALSE,	/* Microsoft */
	FALSE,	/* MouseSystems */
	FALSE,	/* MMSeries */
	FALSE,	/* Logitech */
	FALSE,	/* BusMouse */
	FALSE,	/* MouseMan */
	FALSE,	/* PS/2 */
	FALSE,	/* Hitachi Tablet */
};

int xf86NumMouseTypes = sizeof(xf86SupportedMouseTypes) /
			sizeof(xf86SupportedMouseTypes[0]);
#endif

#include "xf86OSKbd.h"

Bool
xf86OSKbdPreInit(InputInfoPtr pInfo)
{
    return FALSE;
}
