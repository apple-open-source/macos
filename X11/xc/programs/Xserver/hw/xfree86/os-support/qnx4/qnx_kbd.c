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
 * SEBASTIEN MARINEAU BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF 
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
 * SOFTWARE.
 * 
 * Except as contained in this notice, the name of Sebastien Marineau shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from Sebastien Marineau.
 *
 * $XFree86: xc/programs/Xserver/hw/xfree86/os-support/qnx4/qnx_kbd.c,v 1.1 1999/12/27 00:45:47 robin Exp $
 */

/* This module contains the qnx-specific functions to access the keyboard
 * and the console.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <i86.h>
#include <sys/mman.h>
#include <sys/dev.h>
#include <errno.h>

#include <X.h>
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"


int QNX_kbd_fd = -1;
pid_t QNX_kbd_proxy = -1;

int
xf86KbdOn()
{

	if(QNX_kbd_fd == -1) 
		QNX_kbd_fd = open("/dev/kbd", O_RDWR);
	if(QNX_kbd_proxy == -1)
		QNX_kbd_proxy = qnx_proxy_attach(0, 0, 0, -1);
	if (QNX_kbd_fd == -1) {
		FatalError("xf86KbdOn: Could not open keyboard, errno = %d\n", errno);
		} 
	if (QNX_kbd_proxy == -1) {
		FatalError("xf86KbdOn: Could not create kbd proxy, errno = %d\n", errno);
		} 
	if(xf86Verbose) 
		ErrorF("xf86KbdOn: fd = %d, proxy = %d\n", QNX_kbd_fd, QNX_kbd_proxy);
	if (dev_arm(QNX_kbd_fd, QNX_kbd_proxy, _DEV_EVENT_RXRDY) == -1)
		FatalError("xf86KbdOn: could not arm kbd proxy, errno %d\n", errno);	
	return(-1); /* We don't want to select on kbd handle... */


}

int
xf86KbdOff()
{
	int fd;

	ErrorF("xf86KbdOff:\n ");
	fd = QNX_kbd_fd;
	close(QNX_kbd_fd);
	QNX_kbd_fd = -1;
	return(-1);
}

void xf86KbdEvents()
{
	unsigned char rBuf[64];
	int nBytes, i;

	if ((nBytes = dev_read( QNX_kbd_fd, (char *)rBuf, sizeof(rBuf),
	     0, 0, 0, 0, NULL)) > 0) {
		for (i = 0; i < nBytes; i++)
			xf86PostKbdEvent(rBuf[i]);
		/* Re-arm proxy */
		dev_arm(QNX_kbd_fd, QNX_kbd_proxy, _DEV_EVENT_RXRDY);
		
		}
}

