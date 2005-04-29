/* $Xorg: sunLyKbd.c,v 1.3 2000/08/17 19:48:37 cpqbld Exp $ */
/*
 * This is sunKbd.c modified for LynxOS
 * Copyright 1996 by Thomas Mueller
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Thomas Mueller not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Thomas Mueller makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THOMAS MUELLER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THOMAS MUELLER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */
/* $XFree86: xc/programs/Xserver/hw/sunLynx/sunLyKbd.c,v 3.7 2003/11/17 22:20:37 dawes Exp $ */

/*-
 * Copyright 1987 by the Regents of the University of California
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies.  The University of California
 * makes no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

/************************************************************
Copyright 1987 by Sun Microsystems, Inc. Mountain View, CA.

                    All Rights Reserved

Permission  to  use,  copy,  modify,  and  distribute   this
software  and  its documentation for any purpose and without
fee is hereby granted, provided that the above copyright no-
tice  appear  in all copies and that both that copyright no-
tice and this permission notice appear in  supporting  docu-
mentation,  and  that the names of Sun or The Open Group
not be used in advertising or publicity pertaining to 
distribution  of  the software  without specific prior 
written permission. Sun and The Open Group make no 
representations about the suitability of this software for 
any purpose. It is provided "as is" without any express or 
implied warranty.

SUN DISCLAIMS ALL WARRANTIES WITH REGARD TO  THIS  SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FIT-
NESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL SUN BE  LI-
ABLE  FOR  ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,  DATA  OR
PROFITS,  WHETHER  IN  AN  ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION  WITH
THE USE OR PERFORMANCE OF THIS SOFTWARE.

********************************************************/

#define NEED_EVENTS
#include "sun.h"
#include "keysym.h"
#include "Sunkeysym.h"
#include "osdep.h"

#include "Xpoll.h"

#ifdef XKB
#include <X11/extensions/XKB.h>
#include <X11/extensions/XKBstr.h>
#include <X11/extensions/XKBsrv.h>
#endif

#define SUN_LED_MASK	0x0f
#define MIN_KEYCODE	7	/* necessary to avoid the mouse buttons */
#define MAX_KEYCODE	255	/* limited by the protocol */
#ifndef KB_SUN4 
#define KB_SUN4		4
#endif

#define AUTOREPEAT_INITIATE	400
#define AUTOREPEAT_DELAY	50

#define tvminus(tv, tv1, tv2)   /* tv = tv1 - tv2 */ \
		if ((tv1).tv_usec < (tv2).tv_usec) { \
		    (tv1).tv_usec += 1000000; \
		    (tv1).tv_sec -= 1; \
		} \
		(tv).tv_usec = (tv1).tv_usec - (tv2).tv_usec; \
		(tv).tv_sec = (tv1).tv_sec - (tv2).tv_sec;

#define tvplus(tv, tv1, tv2)    /* tv = tv1 + tv2 */ \
		(tv).tv_sec = (tv1).tv_sec + (tv2).tv_sec; \
		(tv).tv_usec = (tv1).tv_usec + (tv2).tv_usec; \
		if ((tv).tv_usec > 1000000) { \
		    (tv).tv_usec -= 1000000; \
		    (tv).tv_sec += 1; \
		}

extern KeySymsRec sunKeySyms[];
extern SunModmapRec* sunModMaps[];

long	  	  sunAutoRepeatInitiate = 1000 * AUTOREPEAT_INITIATE;
long	  	  sunAutoRepeatDelay = 1000 * AUTOREPEAT_DELAY;

static int		autoRepeatKeyDown = 0;
static int		autoRepeatReady;
static int		autoRepeatFirst;
static struct timeval	autoRepeatLastKeyDownTv;
static struct timeval	autoRepeatDeltaTv;

void sunKbdWait()
{
    static struct timeval lastChngKbdTransTv;
    struct timeval tv;
    struct timeval lastChngKbdDeltaTv;
    unsigned int lastChngKbdDelta;

    X_GETTIMEOFDAY(&tv);
    if (!lastChngKbdTransTv.tv_sec)
	lastChngKbdTransTv = tv;
    tvminus(lastChngKbdDeltaTv, tv, lastChngKbdTransTv);
    lastChngKbdDelta = TVTOMILLI(lastChngKbdDeltaTv);
    if (lastChngKbdDelta < 750) {
	unsigned wait;
	/*
         * We need to guarantee at least 750 milliseconds between
	 * calls to KIOCTRANS. YUCK!
	 */
	wait = (750L - lastChngKbdDelta) * 1000L;
        usleep (wait);
        X_GETTIMEOFDAY(&tv);
    }
    lastChngKbdTransTv = tv;
}

static void SwapLKeys(keysyms)
    KeySymsRec* keysyms;
{
    unsigned int i;
    KeySym k;

    for (i = 2; i < keysyms->maxKeyCode * keysyms->mapWidth; i++)
	if (keysyms->map[i] == XK_L1 ||
	    keysyms->map[i] == XK_L2 ||
	    keysyms->map[i] == XK_L3 ||
	    keysyms->map[i] == XK_L4 ||
	    keysyms->map[i] == XK_L5 ||
	    keysyms->map[i] == XK_L6 ||
	    keysyms->map[i] == XK_L7 ||
	    keysyms->map[i] == XK_L8 ||
	    keysyms->map[i] == XK_L9 ||
	    keysyms->map[i] == XK_L10) {
	    /* yes, I could have done a clever two line swap! */
	    k = keysyms->map[i - 2];
	    keysyms->map[i - 2] = keysyms->map[i];
	    keysyms->map[i] = k;
	}
}

static void SetLights (ctrl, fd)
    KeybdCtrl*	ctrl;
    int fd;
{
#ifdef KIOCSLED
    static unsigned char led_tab[16] = {
	0,
	LED_NUM_LOCK,
	LED_SCROLL_LOCK,
	LED_SCROLL_LOCK | LED_NUM_LOCK,
	LED_COMPOSE,
	LED_COMPOSE | LED_NUM_LOCK,
	LED_COMPOSE | LED_SCROLL_LOCK,
	LED_COMPOSE | LED_SCROLL_LOCK | LED_NUM_LOCK,
	LED_CAPS_LOCK,
	LED_CAPS_LOCK | LED_NUM_LOCK,
	LED_CAPS_LOCK | LED_SCROLL_LOCK,
	LED_CAPS_LOCK | LED_SCROLL_LOCK | LED_NUM_LOCK,
	LED_CAPS_LOCK | LED_COMPOSE,
	LED_CAPS_LOCK | LED_COMPOSE | LED_NUM_LOCK,
	LED_CAPS_LOCK | LED_COMPOSE | LED_SCROLL_LOCK,
	LED_CAPS_LOCK | LED_COMPOSE | LED_SCROLL_LOCK | LED_NUM_LOCK
    };
    if (ioctl (fd, KIOCSLED, (caddr_t)&led_tab[ctrl->leds & 0x0f]) == -1)
#if defined(PATCHED_CONSOLE)
	Error("Failed to set keyboard lights");
#else
        ; /* silly driver bug always returns error */
#endif
#endif
}


static void ModLight (device, on, led)
    DeviceIntPtr device;
    Bool	on;
    int		led;
{
    KeybdCtrl*	ctrl = &device->kbdfeed->ctrl;
    sunKbdPrivPtr pPriv = (sunKbdPrivPtr) device->public.devicePrivate;

    if(on) {
	ctrl->leds |= led;
	pPriv->leds |= led;
    } else {
	ctrl->leds &= ~led;
	pPriv->leds &= ~led;
    }
    SetLights (ctrl, pPriv->fd);
}

/*-
 *-----------------------------------------------------------------------
 * sunBell --
 *	Ring the terminal/keyboard bell
 *
 * Results:
 *	Ring the keyboard bell for an amount of time proportional to
 *	"loudness."
 *
 * Side Effects:
 *	None, really...
 *
 *-----------------------------------------------------------------------
 */

static void bell (
    int fd,
    int duration)
{
#if defined(PATCHED_CONSOLE)
    int		    kbdCmd;   	    /* Command to give keyboard */

    kbdCmd = KBD_CMD_BELL;
    if (ioctl (fd, KIOCCMD, &kbdCmd) == -1) {
 	Error("Failed to activate bell");
	return;
    }
    if (duration) usleep (duration);
    kbdCmd = KBD_CMD_NOBELL;
    if (ioctl (fd, KIOCCMD, &kbdCmd) == -1)
	Error ("Failed to deactivate bell");
#endif
}

static void sunBell (
    int		    percent,
    DeviceIntPtr    device,
    pointer	    ctrl,
    int		    unused)
{
    KeybdCtrl*      kctrl = (KeybdCtrl*) ctrl;
    sunKbdPrivPtr   pPriv = (sunKbdPrivPtr) device->public.devicePrivate;
 
    if (percent == 0 || kctrl->bell == 0)
 	return;

    bell (pPriv->fd, kctrl->bell_duration * 1000);
}

static void sunEnqueueEvent (xE)
    xEvent* xE;
{
    sigset_t holdmask;

    (void) sigaddset (&holdmask, SIGIO);
    (void) sigprocmask (SIG_BLOCK, &holdmask, (sigset_t*)NULL);
    mieqEnqueue (xE);
    (void) sigprocmask (SIG_UNBLOCK, &holdmask, (sigset_t*)NULL);
}


#define XLED_NUM_LOCK    0x1
#define XLED_COMPOSE     0x4
#define XLED_SCROLL_LOCK 0x2
#define XLED_CAPS_LOCK   0x8

static KeyCode LookupKeyCode (keysym, keysymsrec)
    KeySym keysym;
    KeySymsPtr keysymsrec;
{
    KeyCode i;
    int ii, index = 0;

    for (i = keysymsrec->minKeyCode; i < keysymsrec->maxKeyCode; i++)
	for (ii = 0; ii < keysymsrec->mapWidth; ii++)
	    if (keysymsrec->map[index++] == keysym)
		return i;
}

static void pseudoKey(device, down, keycode)
    DeviceIntPtr device;
    Bool down;
    KeyCode keycode;
{
    int bit;
    CARD8 modifiers;
    CARD16 mask;
    BYTE* kptr;

    kptr = &device->key->down[keycode >> 3];
    bit = 1 << (keycode & 7);
    modifiers = device->key->modifierMap[keycode];
    if (down) {
	/* fool dix into thinking this key is now "down" */
	int i;
	*kptr |= bit;
	device->key->prev_state = device->key->state;
	for (i = 0, mask = 1; modifiers; i++, mask <<= 1)
	    if (mask & modifiers) {
		device->key->modifierKeyCount[i]++;
		device->key->state += mask;
		modifiers &= ~mask;
	    }
    } else {
	/* fool dix into thinking this key is now "up" */
	if (*kptr & bit) {
	    int i;
	    *kptr &= ~bit;
	    device->key->prev_state = device->key->state;
	    for (i = 0, mask = 1; modifiers; i++, mask <<= 1)
		if (mask & modifiers) {
		    if (--device->key->modifierKeyCount[i] <= 0) {
			device->key->state &= ~mask;
			device->key->modifierKeyCount[i] = 0;
		    }
		    modifiers &= ~mask;
		}
	}
    }
}

static void DoLEDs(device, ctrl, pPriv)
    DeviceIntPtr    device;	    /* Keyboard to alter */
    KeybdCtrl* ctrl;
    sunKbdPrivPtr pPriv; 
{
#ifdef XKB
    if (noXkbExtension) {
#endif
    if ((ctrl->leds & XLED_CAPS_LOCK) && !(pPriv->leds & XLED_CAPS_LOCK))
	    pseudoKey(device, TRUE,
		LookupKeyCode(XK_Caps_Lock, &device->key->curKeySyms));

    if (!(ctrl->leds & XLED_CAPS_LOCK) && (pPriv->leds & XLED_CAPS_LOCK))
	    pseudoKey(device, FALSE,
		LookupKeyCode(XK_Caps_Lock, &device->key->curKeySyms));

    if ((ctrl->leds & XLED_NUM_LOCK) && !(pPriv->leds & XLED_NUM_LOCK))
	    pseudoKey(device, TRUE,
		LookupKeyCode(XK_Num_Lock, &device->key->curKeySyms));

    if (!(ctrl->leds & XLED_NUM_LOCK) && (pPriv->leds & XLED_NUM_LOCK))
	    pseudoKey(device, FALSE,
		LookupKeyCode(XK_Num_Lock, &device->key->curKeySyms));

    if ((ctrl->leds & XLED_SCROLL_LOCK) && !(pPriv->leds & XLED_SCROLL_LOCK))
	    pseudoKey(device, TRUE,
		LookupKeyCode(XK_Scroll_Lock, &device->key->curKeySyms));

    if (!(ctrl->leds & XLED_SCROLL_LOCK) && (pPriv->leds & XLED_SCROLL_LOCK))
	    pseudoKey(device, FALSE,
		LookupKeyCode(XK_Scroll_Lock, &device->key->curKeySyms));

    if ((ctrl->leds & XLED_COMPOSE) && !(pPriv->leds & XLED_COMPOSE))
	    pseudoKey(device, TRUE,
		LookupKeyCode(SunXK_Compose, &device->key->curKeySyms));

    if (!(ctrl->leds & XLED_COMPOSE) && (pPriv->leds & XLED_COMPOSE))
	    pseudoKey(device, FALSE,
		LookupKeyCode(SunXK_Compose, &device->key->curKeySyms));
#ifdef XKB
    }
#endif
    pPriv->leds = ctrl->leds & 0x0f;
    SetLights (ctrl, pPriv->fd);
}

/*-
 *-----------------------------------------------------------------------
 * sunKbdCtrl --
 *	Alter some of the keyboard control parameters
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Some...
 *
 *-----------------------------------------------------------------------
 */

static void sunKbdCtrl (
    DeviceIntPtr    device,
    KeybdCtrl*	    ctrl)
{
    sunKbdPrivPtr pPriv = (sunKbdPrivPtr) device->public.devicePrivate;

    if (pPriv->fd < 0) return;

    if (ctrl->click != pPriv->click) {
    	int kbdClickCmd;

	pPriv->click = ctrl->click;
#if defined(PATCHED_CONSOLE)
	kbdClickCmd = pPriv->click ? KBD_CMD_CLICK : KBD_CMD_NOCLICK;
    	if (ioctl (pPriv->fd, KIOCCMD, &kbdClickCmd) == -1)
 	    Error("Failed to set keyclick");
#endif
    }
    if (pPriv->type == KB_SUN4 && pPriv->leds != ctrl->leds & 0x0f)
	DoLEDs(device, ctrl, pPriv);
}

/*-
 *-----------------------------------------------------------------------
 * sunInitKbdNames --
 *	Handle the XKB initialization
 *
 * Results:
 *	None.
 *
 * Comments: 
 *     This function needs considerable work, in conjunctions with
 *     the need to add geometry descriptions of Sun Keyboards.
 *     It would also be nice to have #defines for all the keyboard
 *     layouts so that we don't have to have these hard-coded
 *     numbers.
 *
 *-----------------------------------------------------------------------
 */
#ifdef XKB
static void sunInitKbdNames (
    XkbComponentNamesRec* names,
    sunKbdPrivPtr pKbd)
{
#ifndef XKBBUFSIZE
#define XKBBUFSIZE 64
#endif
    static char keycodesbuf[XKBBUFSIZE];
    static char geometrybuf[XKBBUFSIZE];
    static char  symbolsbuf[XKBBUFSIZE];

    names->keymap = NULL;
    names->compat = "compat/complete";
    names->types  = "types/complete";
    names->keycodes = keycodesbuf;
    names->geometry = geometrybuf;
    names->symbols = symbolsbuf;
    (void) strcpy (keycodesbuf, "keycodes/");
    (void) strcpy (geometrybuf, "geometry/");
    (void) strcpy (symbolsbuf, "symbols/");

    /* keycodes & geometry */
    switch (pKbd->type) {
    case KB_SUN2:
	(void) strcat (names->keycodes, "sun(type2)");
	(void) strcat (names->geometry, "sun(type2)");
	(void) strcat (names->symbols, "us(sun2)");
	break;
    case KB_SUN3:
	(void) strcat (names->keycodes, "sun(type3)");
	(void) strcat (names->geometry, "sun(type3)");
	(void) strcat (names->symbols, "us(sun3)");
	break;
    case KB_SUN4:
	if (pKbd->layout == 19) {
	    (void) strcat (names->keycodes, "sun(US101A)");
	    (void) strcat (names->geometry, "pc101-NG"); /* XXX */
	    (void) strcat (names->symbols, "us(pc101)");
	} else if (pKbd->layout < 33) {
	    (void) strcat (names->keycodes, "sun(type4)");
	    (void) strcat (names->geometry, "sun(type4)");
	    if (sunSwapLkeys)
		(void) strcat (names->symbols, "sun/us(sun4ol)");
	    else
		(void) strcat (names->symbols, "sun/us(sun4)");
	} else {
	    (void) strcat (names->keycodes, "sun(type5)");
	    if (pKbd->layout == 34 || pKbd->layout == 81)
		(void) strcat (names->geometry, "sun(type5unix)");
	    else
		(void) strcat (names->geometry, "sun(type5)");
	    if (sunSwapLkeys)
		(void) strcat (names->symbols, "sun/us(sun5ol)");
	    else
		(void) strcat (names->symbols, "sun/us(sun5)");
	}
	break;
    default:
	names->keycodes = names->geometry = NULL;
	break;
    }

    /* extra symbols */
    if (pKbd->type == KB_SUN4) {
	switch (pKbd->layout) {
	case  0: case  1: case 33: case 34: case 80: case 81: 
	    break;
	case  3:
	    (void) strcat (names->symbols, "+ca"); break;
	case  4: case 36: case 83: 
	    (void) strcat (names->symbols, "+dk"); break;
	case  5: case 37: case 84: 
	    (void) strcat (names->symbols, "+de"); break;
	case  6: case 38: case 85: 
	    (void) strcat (names->symbols, "+it"); break;
	case  8: case 40: case 87: 
	    (void) strcat (names->symbols, "+no"); break;
	case  9: case 41: case 88: 
	    (void) strcat (names->symbols, "+pt"); break;
	case 10: case 42: case 89: 
	    (void) strcat (names->symbols, "+es"); break;
	case 11: case 43: case 90: 
	    (void) strcat (names->symbols, "+se"); break;
	case 12: case 44: case 91: 
	    (void) strcat (names->symbols, "+fr_CH"); break;
	case 13: case 45: case 92: 
	    (void) strcat (names->symbols, "+de_CH"); break;
	case 14: case 46: case 93: 
	    (void) strcat (names->symbols, "+gb"); break; /* s/b en_UK */
	case 52:
	    (void) strcat (names->symbols, "+pl"); break;
	case 53:
	    (void) strcat (names->symbols, "+cs"); break;
	case 54:
	    (void) strcat (names->symbols, "+ru"); break;
#if 0
	/* don't have symbols defined for these yet, let them default */
	case  2:
	    (void) strcat (names->symbols, "+fr_BE"); break;
	case  7: case 39: case 86: 
	    (void) strcat (names->symbols, "+nl"); break;
	case 50: case 97:
	    (void) strcat (names->symbols, "+fr_CA"); break;
	case 16: case 47: case 94: 
	    (void) strcat (names->symbols, "+ko"); break;
	case 17: case 48: case 95: 
	    (void) strcat (names->symbols, "+tw"); break;
	case 32: case 49: case 96: 
	    (void) strcat (names->symbols, "+jp"); break;
	case 51:
	    (void) strcat (names->symbols, "+hu"); break;
#endif
	/* 
	 * by setting the symbols to NULL XKB will use the symbols in
	 * the "default" keymap.
	 */
	default: 
	    names->symbols = NULL; return; break;
	}
    }
}
#endif /* XKB */

/*-
 *-----------------------------------------------------------------------
 * sunKbdProc --
 *	Handle the initialization, etc. of a keyboard.
 *
 * Results:
 *	None.
 *
 *-----------------------------------------------------------------------
 */

int sunKbdProc (
    DeviceIntPtr  device,
    int	    	  what)
{
    static int once;
    static struct termio kbdtty;
    struct termio tty;
    int i;
    DevicePtr pKeyboard = (DevicePtr) device;
    sunKbdPrivPtr pPriv;
    KeybdCtrl*	ctrl = &device->kbdfeed->ctrl;
    extern int XkbDfltRepeatDelay, XkbDfltRepeatInterval;

    static CARD8 *workingModMap = NULL;
    static KeySymsRec *workingKeySyms;

    switch (what) {
    case DEVICE_INIT:
	if (pKeyboard != LookupKeyboardDevice()) {
	    ErrorF ("Cannot open non-system keyboard\n");
	    return (!Success);
	}
	    
	if (!workingKeySyms) {
	    workingKeySyms = &sunKeySyms[sunKbdPriv.type];

	    if (sunKbdPriv.type == KB_SUN4 && sunSwapLkeys)
		SwapLKeys(workingKeySyms);

	    if (workingKeySyms->minKeyCode < MIN_KEYCODE) {
		workingKeySyms->minKeyCode += MIN_KEYCODE;
		workingKeySyms->maxKeyCode += MIN_KEYCODE;
	    }
	    if (workingKeySyms->maxKeyCode > MAX_KEYCODE)
		workingKeySyms->maxKeyCode = MAX_KEYCODE;
	}

	if (!workingModMap) {
	    workingModMap=(CARD8 *)xalloc(MAP_LENGTH);
	    (void) memset(workingModMap, 0, MAP_LENGTH);
	    for(i=0; sunModMaps[sunKbdPriv.type][i].key != 0; i++)
		workingModMap[sunModMaps[sunKbdPriv.type][i].key + MIN_KEYCODE] = 
		sunModMaps[sunKbdPriv.type][i].modifiers;
	}

	(void) memset ((void *) defaultKeyboardControl.autoRepeats,
			~0, sizeof defaultKeyboardControl.autoRepeats);

#ifdef XKB
	if (noXkbExtension) {
	    sunAutoRepeatInitiate = XkbDfltRepeatDelay * 1000;
	    sunAutoRepeatDelay = XkbDfltRepeatInterval * 1000;
#endif
	autoRepeatKeyDown = 0;
#ifdef XKB
	}
#endif
	pKeyboard->devicePrivate = (pointer)&sunKbdPriv;
	pKeyboard->on = FALSE;

#ifdef XKB
	if (noXkbExtension) {
#endif
	InitKeyboardDeviceStruct(pKeyboard, 
				 workingKeySyms, workingModMap,
				 sunBell, sunKbdCtrl);
#ifdef XKB
	} else {
	    XkbComponentNamesRec names;
	    sunInitKbdNames (&names, &sunKbdPriv);
	    XkbInitKeyboardDeviceStruct((DeviceIntPtr) pKeyboard, &names,
					workingKeySyms, workingModMap,
					sunBell, sunKbdCtrl);
	}
#endif
	break;

    case DEVICE_ON:
	pPriv = (sunKbdPrivPtr)pKeyboard->devicePrivate;
	/*
	 * Set the keyboard into "direct" mode and turn on
	 * event translation.
	 */
	if (sunChangeKbdTranslation(pPriv->fd, TRUE) == -1)
	    FatalError("Can't set keyboard translation\n");
	/*
	 * for LynxOS, save current termio setting and
	 * set the keyboard into raw mode
	 */
	if (!once)
	{
	    ioctl(pPriv->fd, TCGETA, &kbdtty);
	    once = 1;
	}
	tty = kbdtty;
	tty.c_iflag = (IGNPAR | IGNBRK) & (~PARMRK) & (~ISTRIP);
	tty.c_oflag = 0;
	tty.c_cflag = CREAD | CS8;
	tty.c_lflag = 0;
	tty.c_cc[VTIME] = 0;
	tty.c_cc[VMIN] = 0;
	if (ioctl(pPriv->fd, TCSETAW, &tty) < 0)
		perror("ioctl TCSETAW");
	AddEnabledDevice(pPriv->fd);
	pKeyboard->on = TRUE;
	break;

    case DEVICE_CLOSE:
    case DEVICE_OFF:
	pPriv = (sunKbdPrivPtr)pKeyboard->devicePrivate;
	if (pPriv->type == KB_SUN4) {
	    /* dumb bug in Sun's keyboard! Turn off LEDS before resetting */
	    pPriv->leds = 0;
	    ctrl->leds = 0;
	    SetLights(ctrl, pPriv->fd);
	}
	/*
	 * Restore original keyboard directness and translation.
	 */
	if (sunChangeKbdTranslation(pPriv->fd,FALSE) == -1)
	    FatalError("Can't reset keyboard translation\n");
	/* restore saved termio setting */
	if (ioctl(pPriv->fd, TCSETAW, &kbdtty) < 0)
		perror("ioctl TCSETAW");
	pKeyboard->on = FALSE;
	RemoveEnabledDevice(pPriv->fd);
	break;
    default:
	FatalError("Unknown keyboard operation\n");
    }
    return Success;
}

/*-
 *-----------------------------------------------------------------------
 * sunKbdGetEvents --
 *	Return the events waiting in the wings for the given keyboard.
 *
 * Results:
 *	A pointer to an array of Firm_events or (Firm_event *)0 if no events
 *	The number of events contained in the array.
 *	A boolean as to whether more events might be available.
 *
 * Side Effects:
 *	None.
 *-----------------------------------------------------------------------
 */

Firm_event* sunKbdGetEvents (
    int		fd,
    Bool	on,
    int*	pNumEvents,
    Bool*	pAgain)
{
    int	    	  nBytes;	    /* number of bytes of events available. */
    static Firm_event	evBuf[MAXEVENTS];   /* Buffer for Firm_events */

    char buf[64];

    if ((nBytes = read(fd, buf, sizeof(buf))) == -1) {
    	if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR)
    	{
    	    *pNumEvents = 0;
    	    if (errno == EINTR)
    	    	*pAgain = TRUE;
    	    else
	    	*pAgain = FALSE;
    	} else {
	    Error ("Reading keyboard");
	    FatalError ("Could not read the keyboard");
    	}
    } else {
    	int i;
	struct timeval now;

	if (on) {
	    X_GETTIMEOFDAY(&now);
	    *pNumEvents = nBytes;
	    *pAgain = (nBytes == sizeof(buf)); /* very unlikely... */
	    for (i = 0; i < nBytes; i++)
	    {
    		evBuf[i].id = buf[i] & 0x7f;
    		evBuf[i].value = (buf[i] & 0x80) ? VKEY_UP : VKEY_DOWN;
    		evBuf[i].time = now;
	    }
	} else {
	    *pNumEvents = 0;
	    *pAgain = FALSE;
	}
    }
    return evBuf;
}

/*-
 *-----------------------------------------------------------------------
 * sunKbdEnqueueEvent --
 *
 *-----------------------------------------------------------------------
 */
static xEvent	autoRepeatEvent;
static int	composeCount;

static Bool DoSpecialKeys(device, xE, fe)
    DeviceIntPtr  device;
    xEvent*       xE;
    Firm_event* fe;
{
    int	shift_index, map_index, bit;
    KeySym ksym;
    BYTE* kptr;
    sunKbdPrivPtr pPriv = (sunKbdPrivPtr)device->public.devicePrivate;
    BYTE keycode = xE->u.u.detail;
    CARD8 keyModifiers = device->key->modifierMap[keycode];

    /* look up the present idea of the keysym */
    shift_index = 0;
    if (device->key->state & ShiftMask) 
	shift_index ^= 1;
    if (device->key->state & LockMask) 
	shift_index ^= 1;
    map_index = (fe->id - 1) * device->key->curKeySyms.mapWidth;
    ksym = device->key->curKeySyms.map[shift_index + map_index];
    if (ksym == NoSymbol)
	ksym = device->key->curKeySyms.map[map_index];

    /*
     * Toggle functionality is hardcoded. This is achieved by always
     * discarding KeyReleases on these keys, and converting every other
     * KeyPress into a KeyRelease.
     */
    if (xE->u.u.type == KeyRelease 
	&& (ksym == XK_Num_Lock 
	|| ksym == XK_Scroll_Lock 
	|| ksym == SunXK_Compose
	|| (keyModifiers & LockMask))) 
	return TRUE;

    kptr = &device->key->down[keycode >> 3];
    bit = 1 << (keycode & 7);
    if ((*kptr & bit) &&
	(ksym == XK_Num_Lock || ksym == XK_Scroll_Lock ||
	ksym == SunXK_Compose || (keyModifiers & LockMask)))
	xE->u.u.type = KeyRelease;

    if (pPriv->type == KB_SUN4) {
	if (ksym == XK_Num_Lock) {
	    ModLight (device, xE->u.u.type == KeyPress, XLED_NUM_LOCK);
	} else if (ksym == XK_Scroll_Lock) {
	    ModLight (device, xE->u.u.type == KeyPress, XLED_SCROLL_LOCK);
	} else if (ksym == SunXK_Compose) {
	    ModLight (device, xE->u.u.type == KeyPress, XLED_COMPOSE);
	    if (xE->u.u.type == KeyPress) composeCount = 2;
	    else composeCount = 0;
	} else if (keyModifiers & LockMask) {
	    ModLight (device, xE->u.u.type == KeyPress, XLED_CAPS_LOCK);
	}
	if (xE->u.u.type == KeyRelease) {
	    if (composeCount > 0 && --composeCount == 0) {
		pseudoKey(device, FALSE,
		    LookupKeyCode(SunXK_Compose, &device->key->curKeySyms));
		ModLight (device, FALSE, XLED_COMPOSE);
	    }
	}
    }

    if ((xE->u.u.type == KeyPress) && (keyModifiers == 0)) {
	/* initialize new AutoRepeater event & mark AutoRepeater on */
	autoRepeatEvent = *xE;
	autoRepeatFirst = TRUE;
	autoRepeatKeyDown++;
	autoRepeatLastKeyDownTv = fe->time;
    }
    return FALSE;
}

void sunKbdEnqueueEvent (
    DeviceIntPtr  device,
    Firm_event	  *fe)
{
    xEvent		xE;
    BYTE		keycode;
    CARD8		keyModifiers;

    keycode = (fe->id & 0x7f) + MIN_KEYCODE;

    keyModifiers = device->key->modifierMap[keycode];
#ifdef XKB
    if (noXkbExtension) {
#endif
    if (autoRepeatKeyDown && (keyModifiers == 0) &&
	((fe->value == VKEY_DOWN) || (keycode == autoRepeatEvent.u.u.detail))) {
	/*
	 * Kill AutoRepeater on any real non-modifier key down, or auto key up
	 */
	autoRepeatKeyDown = 0;
    }
#ifdef XKB
    }
#endif
    xE.u.keyButtonPointer.time = TVTOMILLI(fe->time);
    xE.u.u.type = ((fe->value == VKEY_UP) ? KeyRelease : KeyPress);
    xE.u.u.detail = keycode;
#ifdef XKB
    if (noXkbExtension) {
#endif
    if (DoSpecialKeys(device, &xE, fe))
	return;
#ifdef XKB
    }
#endif /* ! XKB */
    mieqEnqueue (&xE);
}

void sunEnqueueAutoRepeat ()
{
    int	delta;
    int	i, mask;
    DeviceIntPtr device = (DeviceIntPtr)LookupKeyboardDevice();
    KeybdCtrl* ctrl = &device->kbdfeed->ctrl;
    sunKbdPrivPtr   pPriv = (sunKbdPrivPtr) device->public.devicePrivate;

    if (ctrl->autoRepeat != AutoRepeatModeOn) {
	autoRepeatKeyDown = 0;
	return;
    }
    i=(autoRepeatEvent.u.u.detail >> 3);
    mask=(1 << (autoRepeatEvent.u.u.detail & 7));
    if (!(ctrl->autoRepeats[i] & mask)) {
	autoRepeatKeyDown = 0;
	return;
    }

    /*
     * Generate auto repeat event.	XXX one for now.
     * Update time & pointer location of saved KeyPress event.
     */

    delta = TVTOMILLI(autoRepeatDeltaTv);
    autoRepeatFirst = FALSE;

    /*
     * Fake a key up event and a key down event
     * for the last key pressed.
     */
    autoRepeatEvent.u.keyButtonPointer.time += delta;
    autoRepeatEvent.u.u.type = KeyRelease;

    /*
     * hold off any more inputs while we get these safely queued up
     * further SIGIO are 
     */
    sunEnqueueEvent (&autoRepeatEvent);
    autoRepeatEvent.u.u.type = KeyPress;
    sunEnqueueEvent (&autoRepeatEvent);
    if (ctrl->click) bell (pPriv->fd, 0);

    /* Update time of last key down */
    tvplus(autoRepeatLastKeyDownTv, autoRepeatLastKeyDownTv, 
		    autoRepeatDeltaTv);
}

/*-
 *-----------------------------------------------------------------------
 * sunChangeKbdTranslation
 *	Makes operating system calls to set keyboard translation 
 *	and direction on or off.
 *
 * Results:
 *	-1 if failure, else 0.
 *
 * Side Effects:
 * 	Changes kernel management of keyboard.
 *
 *-----------------------------------------------------------------------
 */
int sunChangeKbdTranslation(
    int fd,
    Bool makeTranslated)
{   
    int 	tmp;
    sigset_t	hold_mask, old_mask;
    int		toread;
    char	junk[8192];

    (void) sigfillset(&hold_mask);
    (void) sigprocmask(SIG_BLOCK, &hold_mask, &old_mask);
    if (makeTranslated) {
	if (ioctl (fd, TIO_ENSCANMODE, &tmp) == -1) {
	    Error ("Setting keyboard translation TIO_ENSCANMODE");
	    ErrorF ("sunChangeKbdTranslation: kbdFd=%d\n", fd);
	    return -1;
	}
    } else {
	if (ioctl (fd, TIO_DISSCANMODE, &tmp) == -1) {
	    Error ("Setting keyboard translation TIO_DISSCANMODE");
	    ErrorF ("sunChangeKbdTranslation: kbdFd=%d\n", fd);
	}
    }
    if (ioctl (fd, FIONREAD, &toread) != -1 && toread > 0) {
	while (toread) {
	    tmp = toread;
	    if (toread > sizeof (junk))
		tmp = sizeof (junk);
	    (void) read (fd, junk, tmp);
	    toread -= tmp;
	}
    }
    (void) sigprocmask(SIG_SETMASK, &old_mask, (sigset_t *)NULL);
    return 0;
}

/*ARGSUSED*/
Bool LegalModifier(key, pDev)
    unsigned int key;
    DevicePtr	pDev;
{
    return TRUE;
}

/*ARGSUSED*/
void sunBlockHandler(nscreen, pbdata, pptv, pReadmask)
    int nscreen;
    pointer pbdata;
    struct timeval **pptv;
    pointer pReadmask;
{
    KeybdCtrl* ctrl = &((DeviceIntPtr)LookupKeyboardDevice())->kbdfeed->ctrl;
    static struct timeval artv = { 0, 0 };	/* autorepeat timeval */

    if (!autoRepeatKeyDown)
	return;

    if (ctrl->autoRepeat != AutoRepeatModeOn)
	return;

    if (autoRepeatFirst == TRUE)
	artv.tv_usec = sunAutoRepeatInitiate;
    else
	artv.tv_usec = sunAutoRepeatDelay;
    *pptv = &artv;

}

/*ARGSUSED*/
void sunWakeupHandler(nscreen, pbdata, err, pReadmask)
    int nscreen;
    pointer pbdata;
    unsigned long err;
    pointer pReadmask;
{
    KeybdCtrl* ctrl = &((DeviceIntPtr)LookupKeyboardDevice())->kbdfeed->ctrl;
    struct timeval tv;

    /* this works around a weird behaviour on LynxOS 2.4.0:
     * usually we have no problems using true SIGIO driven mouse input
     * as it is used on the other UN*X Suns. On LynxOS we have a
     * strange behaviour upon the very first server startup after a 
     * reboot. We won't get SIGIOs from the mouse device. The mouse
     * will only move if we get SIGIOs from the keyboard.
     * The solution (for now) is to use an additional WakeupHandler and
     * poll the mouse file descriptor.
     */
    struct fd_set devicesWithInput;
    struct fd_set device;
    extern struct fd_set EnabledDevices;

    XFD_ANDSET(&devicesWithInput, ((struct fd_set *) pReadmask), &EnabledDevices);

    FD_ZERO(&device);
    FD_SET(sunPtrPriv.fd, &device);
    XFD_ANDSET(&device, &device, &devicesWithInput);
    if (XFD_ANYSET(&device)) {
        sigset_t newsigmask;

        (void) sigemptyset (&newsigmask);
        (void) sigaddset (&newsigmask, SIGIO);
        (void) sigprocmask (SIG_BLOCK, &newsigmask, (sigset_t *)NULL);
        sunEnqueueMseEvents();
        (void) sigprocmask (SIG_UNBLOCK, &newsigmask, (sigset_t *)NULL);
    }

#ifdef XKB
    if (!noXkbExtension)
      return;
#endif

    if (ctrl->autoRepeat != AutoRepeatModeOn)
	return;

    if (autoRepeatKeyDown) {
	X_GETTIMEOFDAY(&tv);
	tvminus(autoRepeatDeltaTv, tv, autoRepeatLastKeyDownTv);
	if (autoRepeatDeltaTv.tv_sec > 0 ||
			(!autoRepeatFirst && autoRepeatDeltaTv.tv_usec >
				sunAutoRepeatDelay) ||
			(autoRepeatDeltaTv.tv_usec >
				sunAutoRepeatInitiate))
		autoRepeatReady++;
    }
    
    if (autoRepeatReady)
    {
	sunEnqueueAutoRepeat ();
	autoRepeatReady = 0;
    }
}
