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
 * HOLGER VEIT  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF 
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
 * SOFTWARE.
 * 
 * Except as contained in this notice, the name of Sebastien Marineau shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from Sebastien Marineau.
 *
 * $XFree86: xc/programs/Xserver/hw/xfree86/os-support/qnx4/qnx_select.c,v 1.1 1999/12/27 00:45:48 robin Exp $
 */

/* This module contains the code to use _select_receive to handle 
 * messages from the Mouse and Input driver. These cannot be select'ed on.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#define FD_SETSIZE 256
#include <sys/select.h>
#include <sys/kernel.h>

#include <X.h>
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"

extern pid_t QNX_mouse_proxy;
extern Bool QNX_mouse_event;
extern pid_t QNX_console_proxy;

pid_t _select_receive ( pid_t proxy)
{

	pid_t pid;
	char msg[16];

	pid = Receive(0, msg, sizeof(msg));
/* ErrorF("Received message from pid %d %d. Mouse pid %d\n", pid, proxy, 
	QNX_mouse_proxy);
*/
	if (pid == QNX_mouse_proxy) return (-1);
	if (pid == QNX_console_proxy) {
		ErrorF("VT swicth requested by proxy to select()\n");
		xf86Info.vtRequestsPending = TRUE;
		return(-1);
		}

	/* For now; check exact semantics */
	return (proxy);
}


