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
 * $XFree86: xc/programs/Xserver/hw/xfree86/os-support/nto/nto_io.c,v 1.5 2003/02/17 15:11:58 dawes Exp $
 */

/* This module contains the NTO-specific functions to access the keyboard
 * and the console.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <X.h>
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"

int NTO_kbd_fd = -1;
int NTO_con_fd = -1;

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

ErrorF("xf86SetKbdLeds: to implement\n");
}

int 
xf86GetKbdLeds()
{
	int leds;

	leds = 0;
	ErrorF("xf86GetKbdLeds: to implement\n");
	return(leds);
}

/* This is a no-op for now */
void
xf86SetKbdRepeat(rad)
char rad;
{
	return;
}


/* Need to open keyboard here ? */
void
xf86KbdInit()
{

	ErrorF("xf86KbdInit: to implement\n");
	return;
}


int xf86KbdOn()
{
    /* Here, we need to open /dev/kbd */
	NTO_kbd_fd = open("/dev/kbd", O_RDWR | O_NONBLOCK);
	ErrorF("Opened keyboard device, handle %d\n",NTO_kbd_fd);
	return(NTO_kbd_fd);
}

int xf86KbdOff()
{
	close(NTO_kbd_fd);
	NTO_kbd_fd = -1;
	return (-1);
}

#include "xf86OSKbd.h"

Bool
xf86OSKbdPreInit(InputInfoPtr pInfo)
{
    return FALSE;
}
