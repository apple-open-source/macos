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
 * $XFree86: xc/programs/Xserver/hw/xfree86/os-support/qnx4/qnx_init.c,v 1.1 1999/12/27 00:45:47 robin Exp $
 */

/* This module contains the qnx-specific functions used at server init.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <i86.h>
#include <sys/mman.h>
#include <sys/console.h>

#include <X.h>
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"

struct _console_ctrl *QNX_con_ctrl = NULL;
unsigned long QNX_con_mask = 0;
unsigned long QNX_con_bits = 0;
pid_t QNX_console_proxy = -1;
int QNX_our_console = -1;

void
xf86UseMsg()
{
	ErrorF("No QNX-specific usage options available at this time\n");
	return;
}

int
xf86ProcessArgument(argc, argv, i)
int argc;
char **argv;
int i;
{

	return 0;

}


void
xf86OpenConsole()
{
struct _console_info info;
unsigned event;
int default_console = FALSE;
char con_name[32];
int active;

	ErrorF("xf86OpenConsole\n");

	if(QNX_con_ctrl == NULL) {
	    /* First open a channel to default stdout */
	    xf86Info.consoleFd = fileno(stdout);
	    if((QNX_con_ctrl = console_open (fileno(stdout), O_RDWR)) == NULL){
		/* Hmmm. Didn't work. Try to open con1 as default */

 		if (( xf86Info.consoleFd = open("/dev/con1", O_RDWR)) < 0){
		    FatalError("xf86OpenConsole: could not open console driver\n");
		    return;
		    } 
		if((QNX_con_ctrl = 
		        console_open (xf86Info.consoleFd, O_RDWR)) == NULL){
		    FatalError("xf86OpenConsole: could not open console\n");
		    }
	    default_console = TRUE;
	    }
	    if(QNX_con_ctrl && console_info(QNX_con_ctrl, 0, &info) == 0 ) {
		    if( info.type != _CON_TYPE_STANDARD ) {
		    FatalError("xf86OpenConsole: console is not a standard text console\n");
		    return;
		    }
		}
	    else {	
		FatalError("xf86OpenConsole: Error querying console\n");
		return;
		}
	    }

	/* We have a console, and it is text. Keep going */
	/* Next, check if Photon has got the screen */
	if(qnx_name_locate(0, "/qnx/crt", 0, NULL) != -1) {
	    FatalError("xf86OpenConsole: Photon has already grabbed the display\n");
	    return;
	    }
	fclose (stdout);
	/* We have two cases here: either this is the first time through, 
	 * and QNX_our_console is not set yet, or we're coming here from
	 * a reset. In that case, make sure our console is now active
	 * before we go further...
	 */
	if (QNX_our_console < 0) {	
	    QNX_our_console = console_active(QNX_con_ctrl, -1);
	    }
	else {
	    console_arm(QNX_con_ctrl, QNX_our_console, -1, _CON_EVENT_ACTIVE);
	    while ((active = console_active(QNX_con_ctrl, -1)) !=
			QNX_our_console) {
		sleep(2);
		ErrorF("Waiting for our console to become active!\n");
		}
	    }
	QNX_con_mask = CONSOLE_INVISIBLE | CONSOLE_NOSWITCH;
	QNX_con_bits = console_ctrl (QNX_con_ctrl, QNX_our_console, 
		QNX_con_mask, QNX_con_mask);
	ErrorF("xf86OpenConsole: Locked console %d\n", QNX_our_console);  

	/* If we had the wrong console opened in the first place, reopen */
	if(default_console) {
		close(xf86Info.consoleFd);
		sprintf(con_name, "/dev/con%d", QNX_our_console);
		xf86Info.consoleFd = open(con_name, O_RDWR);
		ErrorF("xf86OpenConsole: reopened console %d\n", QNX_our_console);
		}

	/* Next create the proxy used to notify us of console events */
	if(QNX_console_proxy == -1){
		if((QNX_console_proxy = qnx_proxy_attach(0, 0, 0, -1)) == -1){
			ErrorF("xf86OpenConsole: Could not create proxy for VT switching\n");
			}
		}

	return;
}

void
xf86CloseConsole()
{
	unsigned bits;
	int font;

	ErrorF("xf86CloseConsole\n");
	if(QNX_con_ctrl == NULL) return;
	QNX_con_bits &= ~QNX_con_mask; /* To make sure */
	bits = console_ctrl(QNX_con_ctrl, 0, QNX_con_bits, QNX_con_mask);

	ErrorF("xf86CloseConsole: unlocked console\n");
	/* For now, dump malloc info as well */
#if 0
	malloc_dump(2);
#endif
	return;
}
