/* $XFree86: xc/programs/Xserver/hw/xfree86/os-support/dgux/dgux_kbd.c,v 1.1 1998/12/13 07:37:46 dawes Exp $ */
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

/* *Contents*

  1  xf86KbdSetLeds    
  2  xf86KbdGetLeds  
  3  xf86SetKbdRepeat
  4  xf86KbdInit()
  5  xf86KbdOn()
  6  xf86KbdOff()

*/


#define NEED_EVENTS
#include "X.h"
#include "Xproto.h"
#include "inputstr.h"
#include "scrnintstr.h"

#include "compiler.h"

#include "xf86Procs.h"
#include "xf86_OSlib.h"


static struct termios kbdtty;


/* ADDED FOR  INTEL DGUX */
void xf86SetKbdLeds(leds)
int leds;
{
        ioctl(xf86Info.kbdFd, KBD_SET_LED, leds);
}



/* ADDED FOR  INTEL DGUX */
int xf86GetKbdLeds()
{
        int leds;

        ioctl(xf86Info.kbdFd, KBD_GET_STATE, &leds);
        return(leds);
}

/* ADDED FOR INTEL DGUX */
#if NeedFunctionPrototypes
void xf86SetKbdRepeat(char rad)
#else
void xf86SetKbdRepeat(rad)
char rad;
#endif
{
        return;
}




/* ADDED FOR INTEL DGUX */

void xf86KbdInit()
{
        tcgetattr(xf86Info.kbdFd, &kbdtty);
}





/* ADDED FOR INTEL DGUX */

int xf86KbdOn()
{
        struct termios nTty;

        nTty = kbdtty;
        nTty.c_iflag = IGNPAR | IGNBRK;
        nTty.c_oflag = 0;
        nTty.c_cflag = CREAD | CS8;
        nTty.c_lflag = 0;
        nTty.c_cc[VTIME] = 0;
        nTty.c_cc[VMIN] = 1;
        cfsetispeed(&nTty, 9600);
        cfsetospeed(&nTty, 9600);
        tcsetattr(xf86Info.kbdFd, TCSANOW, &nTty);
        return(xf86Info.kbdFd);
}





/* Intel DG/ux */
int xf86KbdOff()
{
        tcsetattr(xf86Info.kbdFd, TCSANOW, &kbdtty);
        return(xf86Info.kbdFd);
}

