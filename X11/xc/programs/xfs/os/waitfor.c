/* $Xorg: waitfor.c,v 1.4 2001/02/09 02:05:45 xorgcvs Exp $ */
/*
 * waits for input
 */
/*
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
 * Copyright 1990, 1991 Network Computing Devices;
 * Portions Copyright 1987 by Digital Equipment Corporation 
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Network Computing Devices,
 * or Digital not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Network Computing Devices, or Digital
 * make no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * NETWORK COMPUTING DEVICES, AND DIGITAL DISCLAIM ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL NETWORK COMPUTING DEVICES, OR DIGITAL BE
 * LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $NCDXorg: @(#)waitfor.c,v 4.5 1991/06/24 11:59:20 lemke Exp $
 *
 */
/* $XFree86: xc/programs/xfs/os/waitfor.c,v 3.16 2002/05/31 18:46:12 dawes Exp $ */

#include	<X11/Xos.h>	/* strings, time, etc */

#include	<stdio.h>
#include	<errno.h>
#if !defined(Lynx)
#include	<sys/param.h>
#endif

#include	"clientstr.h"
#include	"globals.h"
#include	"X11/Xpoll.h"
#include	"osdep.h"
#include	"os.h"

#ifdef __UNIXOS2__
#define select(n,r,w,x,t) os2PseudoSelect(n,r,w,x,t)
#endif

extern WorkQueuePtr workQueue;


extern fd_set WellKnownConnections;
extern fd_set LastSelectMask;
extern fd_set WriteMask;
extern fd_set ClientsWithInput;
extern fd_set ClientsWriteBlocked;
extern fd_set AllSockets;
extern fd_set AllClients;
extern fd_set OutputPending;

extern Bool AnyClientsWriteBlocked;
extern Bool NewOutputPending;

extern int  ConnectionTranslation[];

long        LastReapTime;

/*
 * wait_for_something
 *
 * server suspends until
 * - data from clients
 * - new client connects
 * - room to write data to clients
 */

int
WaitForSomething(int *pClientsReady)
{
    struct timeval *wt,
                waittime;
    fd_set      clientsReadable;
    fd_set      clientsWriteable;
    long        curclient;
    int         selecterr;
    long        current_time = 0;
    long        timeout;
    int         nready,
                i;

    while (1) {
	/* handle the work Q */
	if (workQueue)
	    ProcessWorkQueue();

	if (XFD_ANYSET(&ClientsWithInput)) {
	    XFD_COPYSET(&ClientsWithInput, &clientsReadable);
	    break;
	}
	/*
	 * deal with KeepAlive timeouts.  if this seems to costly, SIGALRM
	 * could be used, but its more dangerous since some it could catch us
	 * at an inopportune moment (like inside un-reentrant malloc()).
	 */
	current_time = GetTimeInMillis();
	timeout = current_time - LastReapTime;
	if (timeout > ReapClientTime) {
	    ReapAnyOldClients();
	    LastReapTime = current_time;
	    timeout = ReapClientTime;
	}
	timeout = ReapClientTime - timeout;
	waittime.tv_sec = timeout / MILLI_PER_SECOND;
	waittime.tv_usec = (timeout % MILLI_PER_SECOND) *
	    (1000000 / MILLI_PER_SECOND);
	wt = &waittime;

	XFD_COPYSET(&AllSockets, &LastSelectMask);

	BlockHandler(&wt, (pointer) &LastSelectMask);
	if (NewOutputPending)
	    FlushAllOutput();

	if (AnyClientsWriteBlocked) {
	    XFD_COPYSET(&ClientsWriteBlocked, &clientsWriteable);
	    i = Select(MAXSOCKS, &LastSelectMask, &clientsWriteable, NULL, wt);
	} else {
	    i = Select(MAXSOCKS, &LastSelectMask, NULL, NULL, wt);
	}
	selecterr = errno;

	WakeupHandler(i, (unsigned long *) &LastSelectMask);
	if (i <= 0) {		/* error or timeout */
	    FD_ZERO(&clientsWriteable);
	    if (i < 0) {
		if (selecterr == EBADF) {	/* somebody disconnected */
		    CheckConnections();
		} else if (selecterr != EINTR) {
		    ErrorF("WaitForSomething: select(): errno %d\n", selecterr);
		} else {
		    /*
		     * must have been broken by a signal.  go deal with any
		     * exception flags
		     */
		    return 0;
		}
	    } else {		/* must have timed out */
		ReapAnyOldClients();
		LastReapTime = GetTimeInMillis();
	    }
	} else {
	    if (AnyClientsWriteBlocked && XFD_ANYSET(&clientsWriteable)) {
		NewOutputPending = TRUE;
		XFD_ORSET(&OutputPending, &clientsWriteable, &OutputPending);
		XFD_UNSET(&ClientsWriteBlocked, &clientsWriteable);
		if (!XFD_ANYSET(&ClientsWriteBlocked))
		    AnyClientsWriteBlocked = FALSE;
	    }
	    XFD_ANDSET(&clientsReadable, &LastSelectMask, &AllClients);
	    if (LastSelectMask.fds_bits[0] & WellKnownConnections.fds_bits[0])
		MakeNewConnections();
	    if (XFD_ANYSET(&clientsReadable))
		break;

	}
    }
    nready = 0;

    if (XFD_ANYSET(&clientsReadable)) {
	ClientPtr   client;
	int         conn;

	if (current_time)	/* may not have been set */
	    current_time = GetTimeInMillis();
	for (i = 0; i < howmany(XFD_SETSIZE, NFDBITS); i++) {
	    while (clientsReadable.fds_bits[i]) {
		curclient = ffs(clientsReadable.fds_bits[i]) - 1;
		conn = ConnectionTranslation[curclient + (i << 5)];
		clientsReadable.fds_bits[i] &= ~(((fd_mask)1L) << curclient);
		client = clients[conn];
		if (!client)
		    continue;
		pClientsReady[nready++] = conn;
		client->last_request_time = current_time;
		client->clientGone = CLIENT_ALIVE;
	    }
	}
    }
    return nready;
}

#if 0
/*
 * This is not always a macro
  */
int
ANYSET(long *src)
{
    int         i;

    for (i = 0; i < howmany(XFD_SETSIZE, NFDBITS); i++)
	if (src[i])
	    return (1);
    return (0);
}

#endif
