/*
 * (c) Copyright 1998 by Sebastien Marineau
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
 * SEBASTIEN MARINEAU  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF 
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
 * SOFTWARE.
 * 
 * Except as contained in this notice, the name of Sebastien Marineau shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from Sebastien Marineau.
 *
 * $XFree86: xc/programs/Xserver/hw/xfree86/os-support/qnx4/qnx_io.c,v 1.3 2003/02/17 15:11:59 dawes Exp $
 */

/* This module contains the qnx-specific functions to access the keyboard
 * and the console.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <i86.h>
#include <sys/mman.h>
#include <sys/qioctl.h>
#include <errno.h>

#include <X.h>
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"


void
xf86SoundKbdBell(loudness, pitch, duration)
int loudness, pitch, duration;
{

/* Come back and fix! */
ErrorF("xf86SoundKbdBell: to implement\n");
}

void
xf86SetKbdLeds(leds)
int leds;
{
unsigned long bits[2], oldbits;

	bits[0] = leds;
	bits[1] = LED_CAP | LED_NUM | LED_SCR;

	if (qnx_ioctl (xf86Info.consoleFd, QCTL_DEV_CTL, &bits, 8, &oldbits, 4) == -1) 
		ErrorF("xf86SetKbdLeds: error setting kbd leds, errno %d\n", errno);

}

int 
xf86GetKbdLeds()
{
unsigned long bits[2], oldbits = 0;

	bits[0] = bits[1] = 0;
	if (qnx_ioctl(xf86Info.consoleFd, QCTL_DEV_CTL, &bits, 8, &oldbits, 4) == -1)
		ErrorF("xf86SetKbdLeds: error getting kbd leds, errno %d\n", errno);

	return(oldbits);
}

/* This is a no-op for now */
void
xf86SetKbdRepeat(rad)
char rad;
{
	return;
}


/* This is a no-op for now */
void
xf86KbdInit()
{

	return;
}

#include "xf86OSKbd.h"

Bool
xf86OSKbdPreInit(InputInfoPtr pInfo)
{
    return FALSE;
}
