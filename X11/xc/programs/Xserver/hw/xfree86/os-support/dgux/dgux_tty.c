/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/dgux/dgux_tty.c,v 1.2 1999/01/26 10:40:38 dawes Exp $ */
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
/* BSD (POSIX) Flavor tty for ix86 DG/ux R4.20MU03 */

#define NEED_EVENTS
#include "X.h"
#include "Xproto.h"
#include "inputstr.h"
#include "scrnintstr.h"

#include "xf86Procs.h"
#include "xf86_OSlib.h"
#include "xf86_Config.h"

static Bool not_a_tty = FALSE;

void xf86SetMouseSpeed(mouse, old, new, cflag)
MouseDevPtr mouse;
int old;
int new;
unsigned cflag;
{
	struct termios tty;
	char *c;

	if (not_a_tty)
		return;

	if (tcgetattr(mouse->mseFd, &tty) < 0)
	{
		not_a_tty = TRUE;
		ErrorF("Warning: %s unable to get status of mouse fd (%s)\n",
		       mouse->mseDevice, strerror(errno));
		return;
	}

	/* this will query the initial baudrate only once */
	if (mouse->oldBaudRate < 0) { 
	   switch (cfgetispeed(&tty)) 
	      {
	      case B9600: 
		 mouse->oldBaudRate = 9600;
		 break;
	      case B4800: 
		 mouse->oldBaudRate = 4800;
		 break;
	      case B2400: 
		 mouse->oldBaudRate = 2400;
		 break;
	      case B1200: 
	      default:
		 mouse->oldBaudRate = 1200;
		 break;
	      }
	}

	tty.c_iflag = IGNBRK | IGNPAR;
	tty.c_oflag = 0;
	tty.c_lflag = 0;
	tty.c_cflag = (tcflag_t)cflag;
	tty.c_cc[VTIME] = 0;
	tty.c_cc[VMIN] = 1;

	switch (old)
	{
	case 9600:
		cfsetispeed(&tty, B9600);
		cfsetospeed(&tty, B9600);
		break;
	case 4800:
		cfsetispeed(&tty, B4800);
		cfsetospeed(&tty, B4800);
		break;
	case 2400:
		cfsetispeed(&tty, B2400);
		cfsetospeed(&tty, B2400);
		break;
	case 1200:
	default:
		cfsetispeed(&tty, B1200);
		cfsetospeed(&tty, B1200);
	}

	if (tcsetattr(mouse->mseFd, TCSADRAIN, &tty) < 0)
	{
		if (xf86Info.allowMouseOpenFail) {
			ErrorF("Unable to set status of mouse fd (%s) - Continuing...\n",
			       strerror(errno));
			return;
		}
		xf86FatalError("Unable to set status of mouse fd (%s)\n",
			       strerror(errno));
	}

	switch (new)
	{
	case 9600:
		c = "*q";
		cfsetispeed(&tty, B9600);
		cfsetospeed(&tty, B9600);
		break;
	case 4800:
		c = "*p";
		cfsetispeed(&tty, B4800);
		cfsetospeed(&tty, B4800);
		break;
	case 2400:
		c = "*o";
		cfsetispeed(&tty, B2400);
		cfsetospeed(&tty, B2400);
		break;
	case 1200:
	default:
		c = "*n";
		cfsetispeed(&tty, B1200);
		cfsetospeed(&tty, B1200);
	}

	if (mouse->mseType == P_LOGIMAN || mouse->mseType == P_LOGI)
	{
		if (write(mouse->mseFd, c, 2) != 2)
		{
			if (xf86AllowMouseOpenFail) {
				ErrorF("Unable to write to mouse fd (%s) - Continuing...\n",
				       strerror(errno));
				return;
			}
			xf86FatalError("Unable to write to mouse fd (%s)\n",
				       strerror(errno));
		}
	}
	usleep(100000);

	if (tcsetattr(mouse->mseFd, TCSADRAIN, &tty) < 0)
	{
		if (xf86AllowMouseOpenFail) {
			ErrorF("Unable to set status of mouse fd (%s) - Continuing...\n",
			       strerror(errno));
			return;
		}
		xf86FatalError("Unable to set status of mouse fd (%s)\n",
			       strerror(errno));
	}
}

/* ADDED FOR X 3.3.2.3 */
int
xf86FlushInput(fd)
int fd;
{
        return tcflush(fd, TCIFLUSH);
}

