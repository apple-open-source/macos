/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/shared/sysv_tty.c,v 3.12 1999/05/07 02:56:23 dawes Exp $ */
/*
 * Copyright 1990,91 by Thomas Roell, Dinkelscherben, Germany
 * Copyright 1993 by David Dawes <dawes@xfree86.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Thomas Roell and David Dawes 
 * not be used in advertising or publicity pertaining to distribution of 
 * the software without specific, written prior permission. Thomas Roell and
 * David Dawes makes no representations about the suitability of this 
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 *
 * THOMAS ROELL AND DAVID DAWES DISCLAIM ALL WARRANTIES WITH REGARD TO 
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND 
 * FITNESS, IN NO EVENT SHALL THOMAS ROELL OR DAVID DAWES BE LIABLE FOR 
 * ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER 
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF 
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */
/* $XConsortium: sysv_tty.c /main/7 1996/10/19 18:07:52 kaleb $ */

#include "X.h"

#include "compiler.h"

#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"

static Bool not_a_tty = FALSE;

void
xf86SetMouseSpeed(MouseDevPtr mouse, int old, int new, unsigned cflag)
{
	struct termio tty;
	char *c;

	if (not_a_tty)
		return;

	if (ioctl(mouse->mseFd, TCGETA, &tty) < 0)
	{
		not_a_tty = TRUE;
		xf86Msg(X_WARNING, "unable to get status of mouse fd (%s)\n",
		       strerror(errno));
		return;
	}

	/* this will query the initial baudrate only once */
	if (mouse->oldBaudRate < 0) { 
	   switch (tty.c_cflag & CBAUD) 
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
	tty.c_cflag = cflag;
	tty.c_line = 0;
	tty.c_cc[VTIME] = 0;
	tty.c_cc[VMIN] = 1;

	switch (old)
	{
	case 9600:
		tty.c_cflag |= B9600;
		break;
	case 4800:
		tty.c_cflag |= B4800;
		break;
	case 2400:
		tty.c_cflag |= B2400;
		break;
	case 1200:
	default:
		tty.c_cflag |= B1200;
	}

	if (ioctl(mouse->mseFd, TCSETAW, &tty) < 0)
	{
	    if (xf86Info.allowMouseOpenFail) {
		xf86Msg(X_WARNING,
		    "Unable to set status of mouse fd (%s) - Continuing...\n",
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
		tty.c_cflag |= B9600;
		break;
	case 4800:
		c = "*p";
		tty.c_cflag |= B4800;
		break;
	case 2400:
		c = "*o";
		tty.c_cflag |= B2400;
		break;
	case 1200:
	default:
		c = "*n";
		tty.c_cflag |= B1200;
	}

	if (mouse->mseType == P_LOGIMAN || mouse->mseType == P_LOGI)
	{
	    if (write(mouse->mseFd, c, 2) != 2)
	    {
		if (xf86AllowMouseOpenFail) {
		    xf86Msg(X_WARNING,
			"Unable to write to mouse fd (%s) - Continuing...\n",
			strerror(errno));
		    return;
		}
		xf86FatalError("Unable to write to mouse fd (%s)\n",
			       strerror(errno));
	    }
	}
	usleep(100000);

	if (ioctl(mouse->mseFd, TCSETAW, &tty) < 0)
	{
	    if (xf86AllowMouseOpenFail) {
		xf86Msg(X_WARNING,
		    "Unable to set status of mouse fd (%s) - Continuing...\n",
		    strerror(errno));
		return;
	    }
	    xf86FatalError("Unable to set status of mouse fd (%s)\n",
			   strerror(errno));
	}
#ifdef TCMOUSE
	ioctl(mouse->mseFd, TCMOUSE, 1);
#endif
}

