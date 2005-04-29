/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/os2/os2_kbdEv.c,v 3.17 2004/02/14 00:07:01 dawes Exp $ */
/*
 * (c) Copyright 1994,1996,1999 by Holger Veit
 *			<Holger.Veit@gmd.de>
 * Modified 1996 Sebastien Marineau <marineau@genie.uottawa.ca>
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
 * HOLGER VEIT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF 
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
 * SOFTWARE.
 * 
 * Except as contained in this notice, the name of Holger Veit shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from Holger Veit.
 *
 */
/* $XConsortium: os2_kbdEv.c /main/10 1996/10/27 11:48:48 kaleb $ */

#define I_NEED_OS2_H
#define NEED_EVENTS
#include "X.h"
#include "Xproto.h"
#include "misc.h"
#include "inputstr.h"
#include "scrnintstr.h"

#define INCL_KBD
#define INCL_DOSMONITORS
#define INCL_WINSWITCHLIST
#define INCL_DOSQUEUES
#undef RT_FONT	/* must discard this */
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"
#include "atKeynames.h"

/* Attention! these lines copied from ../../common/xf86Events.c */
#define XE_POINTER 1
#define XE_KEYBOARD 2

#ifdef XKB
extern Bool noXkbExtension;
#endif

#ifdef XTESTEXT1

#define	XTestSERVER_SIDE
#include "xtestext1.h"
extern short xtest_mousex;
extern short xtest_mousey;
extern int on_steal_input; 
extern Bool XTestStealKeyData();
extern void XTestStealMotionData();

#ifdef XINPUT
#define ENQUEUE(ev, code, direction, dev_type) \
 (ev)->u.u.detail = (code); \
 (ev)->u.u.type = (direction); \
 if (!on_steal_input || \
 XTestStealKeyData((ev)->u.u.detail, (ev)->u.u.type, dev_type, \
 xtest_mousex, xtest_mousey)) \
 xf86eqEnqueue((ev))
#else
#define ENQUEUE(ev, code, direction, dev_type) \
 (ev)->u.u.detail = (code); \
 (ev)->u.u.type = (direction); \
 if (!on_steal_input || \
 XTestStealKeyData((ev)->u.u.detail, (ev)->u.u.type, dev_type, \
			xtest_mousex, xtest_mousey)) \
 mieqEnqueue((ev))
#endif

#define MOVEPOINTER(dx, dy, time) \
 if (on_steal_input) \
 XTestStealMotionData(dx, dy, XE_POINTER, xtest_mousex, xtest_mousey); \
 miPointerDeltaCursor (dx, dy, time)

#else /* ! XTESTEXT1 */

#ifdef XINPUT
#define ENQUEUE(ev, code, direction, dev_type) \
 (ev)->u.u.detail = (code); \
 (ev)->u.u.type = (direction); \
 xf86eqEnqueue((ev))
#else
#define ENQUEUE(ev, code, direction, dev_type) \
 (ev)->u.u.detail = (code); \
 (ev)->u.u.type = (direction); \
 mieqEnqueue((ev))
#endif
#define MOVEPOINTER(dx, dy, time) \
 miPointerDeltaCursor (dx, dy, time)

#endif
/* end of include */

HQUEUE hKbdQueue;
HEV hKbdSem;
int last_status;
int lastStatus;
int lastShiftState;
extern BOOL SwitchedToWPS;

void os2PostKbdEvent();

int os2KbdQueueQuery()
{
	ULONG numElements,postCount;
 
	(void)DosQueryQueue(hKbdQueue,&numElements);
	if (numElements!=0) return 0; /* We have something in queue */

	DosResetEventSem(hKbdSem,&postCount);
	return 1;
}


void xf86KbdEvents()
{
	KBDKEYINFO keybuf;
	ULONG numElements;
	REQUESTDATA requestData;
	ULONG dataLength, postCount;
	PVOID dummy;
	BYTE elemPriority;
	int scan, down;
	static int last;
	USHORT ModState;
	int i;

	while(DosReadQueue(hKbdQueue,
			 &requestData,&dataLength,&dummy,
			 0L,1L,&elemPriority,hKbdSem) == 0) {

		/* xf86Msg(X_INFO,
		 "Got queue element. data=%d, scancode =%d,up=%d, ddflag %d\n",
		 requestData.ulData,
		 (requestData.ulData&0x7F00)>>8,
		 requestData.ulData&0x8000,
		 requestData.ulData>>16);*/

		scan=(requestData.ulData&0x7F00)>>8;

		/* the separate cursor keys return 0xe0/scan */
		if ((requestData.ulData & 0x3F0000)==0x20000) scan=0;
		if (requestData.ulData & 0x800000) {
			switch (scan) {

/* BUG ALERT: IBM has in its keyboard driver a 122 key keyboard, which
 * uses the "server generated scancodes" from atKeynames.h as real scan codes.
 * We wait until some poor guy with such a keyboard will break the whole
 * card house though...
 */
			case KEY_KP_7: scan = KEY_Home; break;	
			case KEY_KP_8: scan = KEY_Up; break;	
			case KEY_KP_9: scan = KEY_PgUp; break;	
			case KEY_KP_4: scan = KEY_Left; break;	
			case KEY_KP_5: scan = KEY_Begin; break;	
			case KEY_KP_6: scan = KEY_Right; break;	
			case KEY_KP_1: scan = KEY_End; break;	
			case KEY_KP_2: scan = KEY_Down; break;	
			case KEY_KP_3: scan = KEY_PgDown; break;	
			case KEY_KP_0: scan = KEY_Insert; break;	
			case KEY_KP_Decimal: scan = KEY_Delete; break; 
			case KEY_Enter: scan = KEY_KP_Enter; break;	
			case KEY_LCtrl: scan = KEY_RCtrl; break;	
			case KEY_KP_Multiply: scan = KEY_Print; break; 
			case KEY_Slash: scan = KEY_KP_Divide; break; 
			case KEY_Alt: scan = KEY_AltLang; break;	
			case KEY_ScrollLock: scan = KEY_Break; break; 
			case 0x5b: scan = KEY_LMeta; break;
			case 0x5c: scan = KEY_RMeta; break;
			case 0x5d: scan = KEY_Menu; break;
			default:
				/* virtual shifts: ignore */
			scan = 0; break;
			}
		}
	
		down = (requestData.ulData&0x8000) ? FALSE : TRUE;
		if (scan!=0) os2PostKbdEvent(scan, down);
	}
	(void)DosResetEventSem(hKbdSem,&postCount);
}

/*
 * xf86PostKbdEvent --
 *	Translate the raw hardware KbdEvent into an XEvent, and tell DIX
 *	about it. Scancode preprocessing and so on is done ...
 *
 * OS/2 specific xf86PostKbdEvent(key) has been moved from common/xf86Events.c
 * as some things differ, and I didnït want to scatter this routine with
 * ifdefs further (hv).
 */

void os2PostKbdEvent(unsigned scanCode, Bool down)
{
	KeyClassRec *keyc = ((DeviceIntPtr)xf86Info.pKeyboard)->key;
	Bool updateLeds = FALSE;
	Bool UsePrefix = FALSE;
	Bool Direction = FALSE;
	xEvent kevent;
	KeySym *keysym;
	int keycode;
	static int lockkeys = 0;

	/*
	 * and now get some special keysequences
	 */
	if ((ModifierDown(ControlMask | AltMask)) ||
	    (ModifierDown(ControlMask | AltLangMask))) {
		switch (scanCode) {
		case KEY_BackSpace:
			if (!xf86Info.dontZap) GiveUp(0);
			return;
		case KEY_KP_Minus: /* Keypad - */
			if (!xf86Info.dontZoom) {
				if (down)
					xf86ZoomViewport(xf86Info.currentScreen, -1);
				return;
	 		}
			break;
		case KEY_KP_Plus: /* Keypad + */
			if (!xf86Info.dontZoom) {
				if (down)
					xf86ZoomViewport(xf86Info.currentScreen, 1);
				return;
			 }
			 break;
		}
	}

	/* CTRL-ESC is std OS/2 hotkey for going back to PM and popping up
	 * window list... handled by keyboard driverand PM if you tell it. This is 
	 * what we have done, and thus should never detect this key combo */
	if (ModifierDown(ControlMask) && scanCode==KEY_Escape) {
		/* eat it */
		return;	
	} else if (ModifierDown(AltLangMask|AltMask) && scanCode==KEY_Escape) {
		/* same here */
		return;
	}

	/*
	 * Now map the scancodes to real X-keycodes ...
	 */
	keycode = scanCode + MIN_KEYCODE;
	keysym = (keyc->curKeySyms.map +
		  keyc->curKeySyms.mapWidth * 
		  (keycode - keyc->curKeySyms.minKeyCode));
#ifdef XKB
	if (noXkbExtension) {
#endif
	/* Filter autorepeated caps/num/scroll lock keycodes. */

#define CAPSFLAG 0x01
#define NUMFLAG 0x02
#define SCROLLFLAG 0x04
#define MODEFLAG 0x08
	if (down) {
		switch (keysym[0]) {
		case XK_Caps_Lock:
			if (lockkeys & CAPSFLAG)
				return;
			else
				lockkeys |= CAPSFLAG;
			break;
		case XK_Num_Lock:
			if (lockkeys & NUMFLAG)
				return;
			else
				lockkeys |= NUMFLAG;
			break;
		case XK_Scroll_Lock:
			if (lockkeys & SCROLLFLAG)
				return;
			else
				lockkeys |= SCROLLFLAG;
			break;
		}

		if (keysym[1] == XF86XK_ModeLock) {
			if (lockkeys & MODEFLAG)
				return;
			else
				lockkeys |= MODEFLAG;
		}
	} else {
		switch (keysym[0]) {
		case XK_Caps_Lock:
			lockkeys &= ~CAPSFLAG;
			break;
		case XK_Num_Lock:
			lockkeys &= ~NUMFLAG;
			break;
		case XK_Scroll_Lock:
			lockkeys &= ~SCROLLFLAG;
			break;
		}

		if (keysym[1] == XF86XK_ModeLock)
			lockkeys &= ~MODEFLAG;
	}

	/*
	 * LockKey special handling:
	 * ignore releases, toggle on & off on presses.
	 * Don't deal with the Caps_Lock keysym directly, 
	 * but check the lock modifier
	 */
#ifndef PC98
	if (keyc->modifierMap[keycode] & LockMask ||
	    keysym[0] == XK_Scroll_Lock ||
	    keysym[1] == XF86XK_ModeLock ||
	    keysym[0] == XK_Num_Lock) {
		Bool flag;

		if (!down) return;
		flag = !KeyPressed(keycode);
		if (!flag) down = !down;

		if (keyc->modifierMap[keycode] & LockMask)
			xf86Info.capsLock = flag;
		if (keysym[0] == XK_Num_Lock)
			xf86Info.numLock = flag;
		if (keysym[0] == XK_Scroll_Lock)
			xf86Info.scrollLock = flag;
		if (keysym[1] == XF86XK_ModeLock)
			xf86Info.modeSwitchLock = flag;
		updateLeds = TRUE;
	}
#endif /* not PC98 */	

	/* normal, non-keypad keys */
	if (scanCode < KEY_KP_7 || scanCode > KEY_KP_Decimal) {
		/* magic ALT_L key on AT84 keyboards for multilingual support */
		if (xf86Info.kbdType == KB_84 &&
		    ModifierDown(AltMask) &&
		    keysym[2] != NoSymbol) {
			UsePrefix = TRUE;
			Direction = TRUE;
		}
	}

#ifdef XKB /* Warning: got position wrong first time */
	}
#endif

	/* check for an autorepeat-event */
	if ((down && KeyPressed(keycode)) &&
	    (xf86Info.autoRepeat != AutoRepeatModeOn || keyc->modifierMap[keycode]))
		return;

	xf86Info.lastEventTime = 
		kevent.u.keyButtonPointer.time = 
		GetTimeInMillis();

	/*
	 * And now send these prefixes ...
	 * NOTE: There cannot be multiple Mode_Switch keys !!!!
	 */
	if (UsePrefix) {
		ENQUEUE(&kevent,
			keyc->modifierKeyMap[keyc->maxKeysPerModifier*7],
			Direction ? KeyPress : KeyRelease,
			XE_KEYBOARD);
		ENQUEUE(&kevent, 
			keycode, 
			down ? KeyPress : KeyRelease,
			XE_KEYBOARD);
		ENQUEUE(&kevent,
			keyc->modifierKeyMap[keyc->maxKeysPerModifier*7],
			Direction ? KeyRelease : KeyPress,
			XE_KEYBOARD);
	} else {
#ifdef XFreeDGA
		if (((ScrnInfoPtr)(xf86Info.currentScreen->devPrivates[xf86ScreenIndex].ptr))->directMode&XF86DGADirectKeyb) {
			XF86DirectVideoKeyEvent(&kevent, 
						keycode,
						down ? KeyPress : KeyRelease);
		} else
#endif
		{
			ENQUEUE(&kevent,
				keycode,
				down ? KeyPress : KeyRelease,
				XE_KEYBOARD);
		}
	}

	if (updateLeds) xf86KbdLeds();
}

#pragma pack(1)
struct KeyPacket {
	unsigned short mnflags;
	KBDKEYINFO cp;
	unsigned short ddflags;
};
#pragma pack()

/* The next function runs as a thread. It registers a monitor on the kbd
 * driver, and uses that to get keystrokes. This is because the standard
 * OS/2 keyboard driver does not send keyboard release events. A queue
 * is used to communicate with the main thread to send keystrokes */

void os2KbdMonitorThread(void* arg)
{
	struct KeyPacket packet;
	APIRET rc;
	USHORT length,print_flag;
	ULONG queueParam;
	HMONITOR hKbdMonitor;
	MONIN monInbuf;
	MONOUT monOutbuf;
	char queueName[128];

#if 0
	monInbuf=(MONIN *)_tmalloc(2*sizeof(MONIN));
	if (monInbuf==NULL) {
		xf86Msg(X_ERROR,
			"Could not allocate memory in kbd monitor thread!\n");
		exit(1);
	}
	monOutbuf=(MONOUT *) &monInbuf[1];
#endif

	monInbuf.cb=sizeof(MONIN);
	monOutbuf.cb=sizeof(MONOUT);

	rc = DosMonOpen("KBD$",&hKbdMonitor);
	xf86Msg(X_INFO,"Opened kbd monitor, rc=%d\n",rc);
 	rc = DosMonReg(hKbdMonitor,
		       (PBYTE)&monInbuf,(PBYTE)&monOutbuf,(USHORT)2,(USHORT)-1);
	xf86Msg(X_INFO,"Kbd monitor registered, rc=%d\n",rc);
	if (rc) {
		DosMonClose(hKbdMonitor);
		exit(1);
	}

	/* create a queue */
	sprintf(queueName,"\\QUEUES\\XF86KBD\\%d",getpid());
	rc = DosCreateQueue(&hKbdQueue,0L,queueName);
	xf86Msg(X_INFO,"Kbd Queue created, rc=%d\n",rc);
	(void)DosPurgeQueue(hKbdQueue);

	while (1) {
		length = sizeof(packet);
		rc = DosMonRead((PBYTE)&monInbuf,0,(PBYTE)&packet,&length);
		if (rc)	{
			xf86Msg(X_ERROR,
				"DosMonRead returned bad RC! rc=%d\n",rc);
			DosMonClose(hKbdMonitor);
			exit(1);
		}
		queueParam = packet.mnflags+(packet.ddflags<<16);
		if (packet.mnflags&0x7F00)
			DosWriteQueue(hKbdQueue,queueParam,0L,NULL,0L);
			/*xf86Msg(X_INFO,"Wrote a char to queue, rc=%d\n",rc); */
		print_flag = packet.ddflags & 0x1F;

		/*xf86Msg(X_INFO,"Kbd Monitor: Key press %d, scan code %d, ddflags %d\n",
			  packet.mnflags&0x8000,(packet.mnflags&0x7F00)>>8,packet.ddflags); 
		*/

		/* This line will swallow print-screen keypresses */
		if (print_flag == 0x13 || print_flag == 0x14 || 
		    print_flag == 0x15 || print_flag == 0x16)
			rc = 0;
		else
			rc = DosMonWrite((PBYTE)&monOutbuf,(PBYTE)&packet,length); 
		if (rc) {
			xf86Msg(X_ERROR,
				"DosMonWrite returned bad RC! rc=%d\n",rc);
			DosMonClose(hKbdMonitor);
			exit(1);
		}
	}

	DosCloseQueue(hKbdQueue);
	DosMonClose(hKbdMonitor);
}

void os2KbdBitBucketThread(void* arg)
{
	KBDKEYINFO key;
	while (1) {
		if (xf86Info.consoleFd != -1) {
			KbdCharIn(&key,1,xf86Info.consoleFd);
			usleep(100000);
		} else
			usleep(500000);
	}
}
