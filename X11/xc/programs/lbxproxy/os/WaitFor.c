/* $Xorg: WaitFor.c,v 1.4 2001/02/09 02:05:33 xorgcvs Exp $ */
/***********************************************************

Copyright 1987, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.


Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/
/* $XFree86: xc/programs/lbxproxy/os/WaitFor.c,v 1.9 2001/12/14 20:00:57 dawes Exp $ */

/*****************************************************************
 * OS Dependent input routines:
 *
 *  WaitForSomething
 *  TimerForce, TimerSet, TimerCheck, TimerFree
 *
 *****************************************************************/

#include "Xos.h"			/* for strings, fcntl, time */

#include <errno.h>
#include <stdio.h>
#include "misc.h"
#include "util.h"
#include <sys/param.h>

#include <X11/Xpoll.h>
#include "osdep.h"
#include "os.h"
#include "pm.h"

int
mffs(fd_mask mask)
{
    int i;

    if (!mask) return 0;

    i = 1;
    while (!(mask & 1))
    {
	i++;
	mask >>= 1;
    }
    return i;
}

/*****************
 * WaitForSomething:
 *     Make the server suspend until there is
 *	1. data from clients or
 *	2. clients that have buffered replies/events are ready
 *
 *     For more info on ClientsWithInput, see ReadRequestFromClient().
 *     pClientsReady is an array to store ready client->index values into.
 *****************/

int
WaitForSomething(pClientsReady, poll)
    int *pClientsReady;
    Bool poll;
{
    int i;
    struct timeval zerowt = {0, 0};
    struct timeval *wt;
    fd_set clientsReadable;
    fd_set clientsWritable;
    int curclient;
    int selecterr;
    int nready;

    FD_ZERO(&clientsReadable);

    /* We need a while loop here to handle 
       crashed connections and the screen saver timeout */
    while (1)
    {
	/* deal with any blocked jobs */
	if (workQueue)
	    ProcessWorkQueue();

	if (XFD_ANYSET(&ClientsWithInput))
	{
	    XFD_COPYSET(&ClientsWithInput, &clientsReadable);
	    break;
	}
	if (poll)
	    wt = &zerowt;
	else
	    wt = NULL;
	XFD_COPYSET(&AllSockets, &LastSelectMask);
	if (NewOutputPending)
	    FlushAllOutput();
	/* keep this check close to select() call to minimize race */
	if (dispatchException)
	    i = -1;
	else if (AnyClientsWriteBlocked)
	{
	    XFD_COPYSET(&ClientsWriteBlocked, &clientsWritable);
	    i = Select (MaxClients, &LastSelectMask, &clientsWritable, NULL, wt);
	}
	else
	    i = Select (MaxClients, &LastSelectMask, NULL, NULL, wt);
	selecterr = errno;

	if (poll && i == 0)
	    return 0;
	else if (i <= 0) /* An error or timeout occurred */
	{

	    if (dispatchException)
		return 0;
	    FD_ZERO(&clientsWritable);
	    if (i < 0) {
		if (selecterr == EBADF)    /* Some client disconnected */
		{
		    CheckConnections ();
		    if (! XFD_ANYSET (&AllClients))
			return 0;
		}
		else if (selecterr != EINTR)
		    ErrorF("WaitForSomething(): select: errno=%d\n",
			selecterr);
	    }
	}
	else
	{
	    fd_set tmp_set;
	    if (AnyClientsWriteBlocked && XFD_ANYSET (&clientsWritable))
	    {
		NewOutputPending = TRUE;
		XFD_ORSET(&OutputPending, &clientsWritable, &OutputPending);
		XFD_UNSET(&ClientsWriteBlocked, &clientsWritable);
		if (! XFD_ANYSET(&ClientsWriteBlocked))
		    AnyClientsWriteBlocked = FALSE;
	    }

	    XFD_ANDSET(&clientsReadable, &LastSelectMask, &AllClients); 
	    XFD_ANDSET(&tmp_set, &LastSelectMask, &WellKnownConnections);
	    if (XFD_ANYSET(&tmp_set))
		QueueWorkProc(EstablishNewConnections, NULL,
			      (pointer)&LastSelectMask);
	    if (proxy_manager_fd >= 0 &&
		FD_ISSET(proxy_manager_fd, &LastSelectMask))
		HandleProxyManagerConnection ();
		
	    if (XFD_ANYSET (&clientsReadable))
		break;
	}
    }

    nready = 0;
    if (XFD_ANYSET(&clientsReadable))
    {
	for (i=0; i<howmany(XFD_SETSIZE, NFDBITS); i++)
	{
	    while (clientsReadable.fds_bits[i])
	    {
                int	client_index; 

		curclient = ffs (clientsReadable.fds_bits[i]) - 1;
		client_index = ConnectionTranslation[curclient + (i * (sizeof(fd_mask)*8))];
		{
		    pClientsReady[nready++] = client_index;
		}
		clientsReadable.fds_bits[i] &= ~(((fd_mask)1L) << curclient);
	    }
	}	
    }
    return nready;
}
