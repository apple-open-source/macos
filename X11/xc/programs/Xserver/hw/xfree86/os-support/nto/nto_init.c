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
 * $XFree86: xc/programs/Xserver/hw/xfree86/os-support/nto/nto_init.c,v 1.3 2001/11/16 16:47:56 dawes Exp $
 */

/* This module contains the NTO-specific functions used at server init.
 */
#include <sys/neutrino.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/dcmd_chr.h>

#include <X.h>
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86_OSlib.h"

extern int NTO_con_fd;

void
xf86UseMsg()
{
	ErrorF("No NTO-specific usage options available at this time\n");
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


/* Right now, OpenConsole() does not do much; later, we may want to handle
 * console switching and so on....
 */

void
xf86OpenConsole()
{
	unsigned flags;


	ThreadCtl(_NTO_TCTL_IO, 0);

	if((NTO_con_fd = open("/dev/con1", O_RDWR)) == -1) {
		ErrorF("Unable to open console\n");
		return;
	}

    /* Make the console invisible to prevent devc-con from touching hardware */
	flags = _CONCTL_INVISIBLE | _CONCTL_INVISIBLE_CHG;
	devctl(NTO_con_fd, DCMD_CHR_SERCTL, &flags, sizeof flags, 0);
	
	return;
}

void
xf86CloseConsole()
{
	unsigned flags;

    /* Make console visible again */
	flags = _CONCTL_INVISIBLE_CHG;
	devctl(NTO_con_fd, DCMD_CHR_SERCTL, &flags, sizeof flags, 0);
	close(NTO_con_fd);

	return;
}
