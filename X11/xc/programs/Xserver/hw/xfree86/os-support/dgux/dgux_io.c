/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/dgux/dgux_io.c,v 1.4 2003/02/17 15:11:56 dawes Exp $ */
/*
 * INTEL DG/UX RELEASE 4.20 MU03
 * Copyright 1997 Takis Psarogiannakopoulos Cambridge,UK
 * <takis@dpmms.cam.ac.uk>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.
 * XCONSORTIUM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE.
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FITNESS, IN NO EVENT SHALL XCONSORTIUM BE LIABLE FOR
 * ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#define NEED_EVENTS
#include "X.h"
#include "Xproto.h"
#include "inputstr.h"
#include "scrnintstr.h"

#include "compiler.h"

#include "xf86Procs.h"
#include "xf86_OSlib.h"



void xf86SoundKbdBell(loudness, pitch, duration)
int loudness;
int pitch;
int duration;
{
	if (loudness && pitch)
	{

		/*
		 * We use KBD_TONE_HIGH to avoid putting the server
		 * to sleep
		 */
		ioctl(xf86Info.kbdFd, KBD_TONE_HIGH,
		      ((1193190 / pitch) & 0xffff) |
		      (((unsigned long)duration *
			loudness / 50) << 16));

	}
}



void xf86MouseInit(mouse)
MouseDevPtr mouse;
{
	return;
}



/* Added for DG/ux: only RDONLY will not crash the Xserver */
int xf86MouseOn(mouse)
MouseDevPtr mouse;
{
	if ((mouse->mseFd = open(mouse->mseDevice, O_RDONLY|O_NDELAY)) < 0)
	{
		if (xf86Info.allowMouseOpenFail) {
			ErrorF("Cannot open mouse (%s) - Continuing...\n",
				strerror(errno));
			return(-2);
		}
		FatalError("Cannot open mouse (%s)\n", strerror(errno));
	}

	xf86SetupMouse(mouse);

	/* Flush any pending input */
	ioctl(mouse->mseFd, TCFLSH, 0);
	return(mouse->mseFd);
}

#include "xf86OSKbd.h"

Bool
xf86OSKbdPreInit(InputInfoPtr pInfo)
{
    return FALSE;
}
