/*
 * Copied from os2_io.c which is
 *
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
/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/os2/os2_kbd.c,v 1.2 2003/11/03 05:36:33 tsi Exp $ */

#define I_NEED_OS2_H
#include "X.h"
#include "Xpoll.h"
#include "compiler.h"
#include <time.h>

#define INCL_DOSPROCESS
#define INCL_KBD
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"
#include "xf86Xinput.h"
#include "xf86OSKbd.h"



/***************************************************************************/

static void SoundKbdBell(loudness, pitch, duration)
int loudness;
int pitch;
int duration;
{
	DosBeep((ULONG)pitch, (ULONG)duration);
}

static void SetKbdLeds(pInfo, leds)
InputInfoPtr pInfo;
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

static int GetKbdLeds(pInfo)
InputInfoPtr pInfo;
{
	KBDINFO kinfo;
	APIRET rc;

	rc = KbdGetStatus(&kinfo,(HKBD)xf86Info.consoleFd);
	return rc ? 0 : kinfo.fsState & 0x70;
}

static void SetKbdRepeat(pInfo, rad)
InputInfoPtr pInfo;
char rad;
{
	/*notyet*/
}

static void KbdInit(pInfo)
InputInfoPtr pInfo;
{
	/*none required*/
	xf86Msg(X_INFO,"XKB module: Keyboard initialized\n");
}


static USHORT OrigKbdState;
static USHORT OrigKbdInterim;

typedef struct {
    USHORT state;
    UCHAR makeCode;
    UCHAR breakCode;
    USHORT keyID;
} HOTKEYPARAM;


static int KbdOn(pInfo)
InputInfoPtr pInfo;
{
	KBDINFO info;

	KbdGetStatus(&info,(HKBD)xf86Info.consoleFd);
	OrigKbdState=info.fsMask;
	OrigKbdInterim=info.fsInterim;
	info.fsMask &= ~0x09;
	info.fsMask |= 0x136;
	info.fsInterim &= ~0x20;
	KbdSetStatus(&info,(HKBD)xf86Info.consoleFd);
	return -1;
}

static int KbdOff(pInfo)
InputInfoPtr pInfo;
{
	KBDINFO info;

	info.fsMask=OrigKbdState;
	info.fsInterim=OrigKbdInterim;
	KbdSetStatus(&info,(HKBD)xf86Info.consoleFd);
	return -1;
}

Bool
xf86OSKbdPreInit(InputInfoPtr pInfo)
{
	KbdDevPtr pKbd = pInfo->private;

	pKbd->KbdInit		= KbdInit;
	pKbd->KbdOn		= KbdOn;
	pKbd->KbdOff		= KbdOff;
	pKbd->Bell		= SoundKbdBell;
	pKbd->SetLeds		= SetKbdLeds;
	pKbd->GetLeds		= GetKbdLeds;
	pKbd->SetKbdRepeat	= SetKbdRepeat;

	pKbd->vtSwitchSupported	= FALSE;

	/* not yet */
	return FALSE;
}
