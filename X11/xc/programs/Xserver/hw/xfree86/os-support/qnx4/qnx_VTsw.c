/*
 * (c) Copyright 1998 by Sebastien Marineau
 *                      <sebastien@qnx.com>
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
 * Except as contained in this notice, the name of Sebastien Marineau shall not
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from Sebastien Marineau.
 *
 * $XFree86: xc/programs/Xserver/hw/xfree86/os-support/qnx4/qnx_VTsw.c,v 1.1 1999/12/27 00:45:47 robin Exp $
 */

/* This module contains the code to use _select_receive to handle
 * messages from the Mouse and Input driver. These cannot be select'ed on.
 */

/* This module contains the functions which are used to do 
 * VT switching to a text console and back... Experimental.
 */
#include "X.h"
#include "input.h"
#include "scrnintstr.h"

#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"

#include <sys/console.h>

int QNX_console_exist[10];
Bool QNX_vtswitch_pending = FALSE;
int QNX_con_toactivate = -1;
extern struct _console_ctrl *QNX_con_ctrl;
extern unsigned QNX_con_mask, QNX_con_bits;
extern pid_t QNX_console_proxy;
extern int QNX_our_console;

/* This gets called to determine if a VT switch has been requested */
Bool xf86VTSwitchPending()
{
        return(xf86Info.vtRequestsPending ? TRUE : FALSE);
}

/* This is called to do OS-specific stuff when we switch away from
 * our console.
 */
Bool xf86VTSwitchAway()
{
	int ret;
	unsigned event, bits;

	ErrorF("Called VT switch away!\n");

	/* First check wether we are trying to switch to our console... */
	if (xf86Info.vtRequestsPending == QNX_our_console) {
		xf86Info.vtRequestsPending = FALSE;
		return (FALSE);
		}

	/* Reenable console switching */
	QNX_con_bits &= ~QNX_con_mask;
        bits = console_ctrl(QNX_con_ctrl, -1, QNX_con_bits, QNX_con_mask);
        QNX_con_mask = 0;

	/* And activate the new console. Check if it is valid first... */
	ret = console_active(QNX_con_ctrl, xf86Info.vtRequestsPending);
	ErrorF("xf86VTSwitchAway: Made console %d active, ret %d\n", 
		xf86Info.vtRequestsPending, ret);
	xf86Info.vtRequestsPending = FALSE;

	if (ret == -1) {
	        QNX_con_mask = CONSOLE_INVISIBLE | CONSOLE_NOSWITCH;
	        QNX_con_bits = console_ctrl (QNX_con_ctrl, 
			QNX_our_console, QNX_con_mask, QNX_con_mask);
		return (FALSE);
		}
	/* Arm the console with the proxy so we know when we come back */
      	console_state(QNX_con_ctrl, QNX_our_console, 0L, _CON_EVENT_ACTIVE); 
	event = _CON_EVENT_ACTIVE;
        console_arm (QNX_con_ctrl, QNX_our_console, QNX_console_proxy, event);

        return(TRUE);
}

/* And this is called when we are switching back to the server */
Bool xf86VTSwitchTo()
{
	unsigned bits, mask;

	ErrorF("Called VT switch to the server!\n");
        QNX_con_mask = CONSOLE_INVISIBLE | CONSOLE_NOSWITCH;
        QNX_con_bits = console_ctrl (QNX_con_ctrl, QNX_our_console, QNX_con_mask, QNX_con_mask);
	xf86Info.vtRequestsPending = FALSE;
        return(TRUE);
}
