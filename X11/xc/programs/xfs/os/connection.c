/* $Xorg: connection.c,v 1.5 2001/02/09 02:05:44 xorgcvs Exp $ */
/*
 * handles connections
 */
/*
 
Copyright 1990, 1991, 1998  The Open Group

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
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Network Computing Devices, or Digital
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 *
 * NETWORK COMPUTING DEVICES, AND DIGITAL DISCLAIM ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL NETWORK COMPUTING DEVICES,
 * OR DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 */
/*
 * Copyright 1990, 1991 Network Computing Devices;
 * Portions Copyright 1987 by Digital Equipment Corporation 
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Network Computing Devices, or Digital
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 *
 * NETWORK COMPUTING DEVICES, AND DIGITAL DISCLAIM ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL NETWORK COMPUTING DEVICES,
 * OR DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 */
/* $XFree86: xc/programs/xfs/os/connection.c,v 3.25 2002/10/15 01:45:03 dawes Exp $ */

#include	<X11/Xtrans.h>
#include	<stdlib.h>
#include	"misc.h"
#include	<stdio.h>
#include	<errno.h>
#include	<X11/Xos.h>
#ifndef Lynx
#include	<sys/param.h>
#include	<sys/socket.h>
#ifndef __UNIXOS2__
#include	<sys/uio.h>
#endif
#else
#include	<socket.h>
#include	<uio.h>
#endif
#include	<signal.h>

#include	"FS.h"
#include	"FSproto.h"
#include	"clientstr.h"
#include	"X11/Xpoll.h"
#include	"osdep.h"
#include	"globals.h"
#include	"osstruct.h"
#include	"servermd.h"
#include	"dispatch.h"
#include	"fsevents.h"

#ifdef __UNIXOS2__
#define _NFILE OPEN_MAX
#define select(n,r,w,x,t) os2PseudoSelect(n,r,w,x,t)
#endif


int         ListenPort = DEFAULT_FS_PORT;   /* port to listen on */
int         lastfdesc;

fd_set      WellKnownConnections;
fd_set      AllSockets;
fd_set      AllClients;
fd_set      LastSelectMask;
fd_set      ClientsWithInput;
fd_set      ClientsWriteBlocked;
fd_set      OutputPending;
long        OutputBufferSize = BUFSIZE;

Bool        NewOutputPending;
Bool        AnyClientsWriteBlocked;

int         ConnectionTranslation[MAXSOCKS];

XtransConnInfo 	*ListenTransConns = NULL;
int	       	*ListenTransFds = NULL;
int		ListenTransCount;


static void error_conn_max(XtransConnInfo trans_conn);
static void close_fd(OsCommPtr oc);


static XtransConnInfo
lookup_trans_conn (int fd)
{
    if (ListenTransFds)
    {
	int i;
	for (i = 0; i < ListenTransCount; i++)
	    if (ListenTransFds[i] == fd)
		return ListenTransConns[i];
    }

    return (NULL);
}

void
StopListening(void)
{
    int i;

    for (i = 0; i < ListenTransCount; i++)
    {
	FD_CLR (ListenTransFds[i], &AllSockets);
	_FontTransCloseForCloning (ListenTransConns[i]);
    }

    free ((char *) ListenTransFds);
    free ((char *) ListenTransConns);

    ListenTransFds = NULL;
    ListenTransConns = NULL;
}

/*
 * creates the sockets for listening to clients
 *
 * only called when server first started
 */
void
CreateSockets(int old_listen_count, OldListenRec *old_listen)
{
    int	i;

    FD_ZERO(&AllSockets);
    FD_ZERO(&AllClients);
    FD_ZERO(&LastSelectMask);
    FD_ZERO(&ClientsWithInput);
    FD_ZERO(&WellKnownConnections);

    for (i = 0; i < MAXSOCKS; i++)
	ConnectionTranslation[i] = 0;

#ifdef XNO_SYSCONF	/* should only be on FreeBSD 1.x and NetBSD 0.x */
#undef _SC_OPEN_MAX
#endif
#ifdef _SC_OPEN_MAX
    lastfdesc = sysconf(_SC_OPEN_MAX) - 1;
#else
#if defined(hpux) || defined(__UNIXOS2__)
    lastfdesc = _NFILE - 1;
#else
    lastfdesc = getdtablesize() - 1;
#endif				/* hpux */
#endif

    if (lastfdesc > MAXSOCKS) {
	lastfdesc = MAXSOCKS;
    }

    if (old_listen_count > 0) {

	/*
	 * The font server cloned itself.  Re-use previously opened
	 * transports for listening.
	 */

	ListenTransConns = (XtransConnInfo *) malloc (
	    old_listen_count * sizeof (XtransConnInfo));

	ListenTransFds = (int *) malloc (old_listen_count * sizeof (int));

	ListenTransCount = 0;

	for (i = 0; i < old_listen_count; i++)
	{
	    char portnum[10];

	    if (old_listen[i].portnum != ListenPort)
		continue;		/* this should never happen */
	    else
		sprintf (portnum, "%d", old_listen[i].portnum);

	    if ((ListenTransConns[ListenTransCount] =
		_FontTransReopenCOTSServer (old_listen[i].trans_id,
		old_listen[i].fd, portnum)) != NULL)
	    {
		ListenTransFds[ListenTransCount] = old_listen[i].fd;
		FD_SET (old_listen[i].fd, &WellKnownConnections);

		NoticeF("reusing existing file descriptor %d\n",
		    old_listen[i].fd);

		ListenTransCount++;
	    }
	}
    } else {
	char port[20];
	int partial;

	sprintf (port, "%d", ListenPort);

	if ((_FontTransMakeAllCOTSServerListeners (port, &partial,
	    &ListenTransCount, &ListenTransConns) >= 0) &&
	    (ListenTransCount >= 1))
	{
	    ListenTransFds = (int *) malloc (ListenTransCount * sizeof (int));

	    for (i = 0; i < ListenTransCount; i++)
	    {
		int fd = _FontTransGetConnectionNumber (ListenTransConns[i]);

		ListenTransFds[i] = fd;
		FD_SET (fd, &WellKnownConnections);
	    }
	}
    }

    if (! XFD_ANYSET(&WellKnownConnections))
	FatalError("cannot establish any listening sockets\n");

    /* set up all the signal handlers */
    signal(SIGPIPE, SIG_IGN);
    signal(SIGHUP, AutoResetServer);
    signal(SIGINT, GiveUp);
    signal(SIGTERM, GiveUp);
    signal(SIGUSR1, ServerReconfig);
    signal(SIGUSR2, ServerCacheFlush);
    signal(SIGCHLD, CleanupChild);

    XFD_COPYSET (&WellKnownConnections, &AllSockets);
}

/*
 * called when server cycles
 */
void
ResetSockets(void)
{
}

void
CloseSockets(void)
{
    int i;

    for (i = 0; i < ListenTransCount; i++)
	_FontTransClose (ListenTransConns[i]);
}

/*
 * accepts new connections
 */
void
MakeNewConnections(void)
{
    fd_mask     readyconnections;
    int         curconn;
    int         newconn;
    long        connect_time;
    int         i;
    ClientPtr   client;
    OsCommPtr   oc;
    fd_set	tmask;

    XFD_ANDSET (&tmask, &LastSelectMask, &WellKnownConnections);
    readyconnections = tmask.fds_bits[0];
    if (!readyconnections)
	return;
    connect_time = GetTimeInMillis();

    /* kill off stragglers */
    for (i = MINCLIENT; i < currentMaxClients; i++) {
	if ((client = clients[i]) != NullClient) {
	    oc = (OsCommPtr) client->osPrivate;
	    if ((oc && (oc->conn_time != 0) &&
		    (connect_time - oc->conn_time) >= TimeOutValue) ||
		     ((client->noClientException != FSSuccess) &&
		      (client->clientGone != CLIENT_GONE)))
		CloseDownClient(client);
	}
    }

    while (readyconnections) {
	XtransConnInfo trans_conn, new_trans_conn;
	int status;

	curconn = ffs(readyconnections) - 1;
	readyconnections &= ~(1 << curconn);

	if ((trans_conn = lookup_trans_conn (curconn)) == NULL)
	    continue;

	if ((new_trans_conn = _FontTransAccept (trans_conn, &status)) == NULL)
	    continue;

	newconn = _FontTransGetConnectionNumber (new_trans_conn);

	_FontTransSetOption(new_trans_conn, TRANS_NONBLOCKING, 1);

	oc = (OsCommPtr) fsalloc(sizeof(OsCommRec));
	if (!oc) {
	    fsfree(oc);
	    error_conn_max(new_trans_conn);
	    _FontTransClose(new_trans_conn);
	    continue;
	}
	FD_SET(newconn, &AllClients);
	FD_SET(newconn, &AllSockets);
	oc->fd = newconn;
	oc->trans_conn = new_trans_conn;
	oc->input = (ConnectionInputPtr) NULL;
	oc->output = (ConnectionOutputPtr) NULL;
	oc->conn_time = connect_time;

	if ((newconn < lastfdesc) &&
		(client = NextAvailableClient((pointer) oc))) {
	    ConnectionTranslation[newconn] = client->index;
	} else {
	    error_conn_max(new_trans_conn);
	    close_fd(oc);
	}
    }
}

#define	NOROOM	"maximum number of clients reached"

static void
error_conn_max(XtransConnInfo trans_conn)
{
    int fd = _FontTransGetConnectionNumber (trans_conn);
    fsConnSetup conn;
    char        pad[3];
    char        byteOrder = 0;
    int         whichbyte = 1;
    struct timeval waittime;
    fd_set      mask;


    waittime.tv_usec = BOTIMEOUT / MILLI_PER_SECOND;
    waittime.tv_usec = (BOTIMEOUT % MILLI_PER_SECOND) *
	(1000000 / MILLI_PER_SECOND);
    FD_ZERO(&mask);
    FD_SET(fd, &mask);
    (void) Select(fd + 1, &mask, NULL, NULL, &waittime);
    /* try to read the byteorder of the connection */
    (void) _FontTransRead(trans_conn, &byteOrder, 1);
    if ((byteOrder == 'l') || (byteOrder == 'B')) {
	int         num_alts;
	AlternateServerPtr altservers,
	            as;
	int         i,
	            altlen = 0;

	num_alts = ListAlternateServers(&altservers);
	conn.status = AuthDenied;
	conn.major_version = FS_PROTOCOL;
	conn.minor_version = FS_PROTOCOL_MINOR;
	conn.num_alternates = num_alts;
	for (i = 0, as = altservers; i < num_alts; i++, as++) {
	    altlen += (2 + as->namelen + 3) >> 2;
	}
	conn.alternate_len = altlen;
	/* blow off the auth info */
	conn.auth_index = 0;
	conn.auth_len = 0;

	if (((*(char *) &whichbyte) && (byteOrder == 'B')) ||
		(!(*(char *) &whichbyte) && (byteOrder == 'l'))) {
	    conn.status = lswaps(conn.status);
	    conn.major_version = lswaps(conn.major_version);
	    conn.minor_version = lswaps(conn.minor_version);
	    conn.alternate_len = lswaps(conn.alternate_len);
	}
	(void) _FontTransWrite(trans_conn,
	    (char *) &conn, SIZEOF(fsConnSetup));
	/* dump alternates */
	for (i = 0, as = altservers; i < num_alts; i++, as++) {
	    (void) _FontTransWrite(trans_conn,
		(char *) as, 2);  /* XXX */
	    (void) _FontTransWrite(trans_conn,
		(char *) as->name, as->namelen);
	    altlen = 2 + as->namelen;
	    /* pad it */
	    if (altlen & 3)
		(void) _FontTransWrite(trans_conn,
		(char *) pad, ((4 - (altlen & 3)) & 3));
	}
    }
}

static void
close_fd(OsCommPtr oc)
{
    int         fd = oc->fd;

    if (oc->trans_conn)
	_FontTransClose(oc->trans_conn);
    FreeOsBuffers(oc);
    FD_CLR(fd, &AllSockets);
    FD_CLR(fd, &AllClients);
    FD_CLR(fd, &ClientsWithInput);
    FD_CLR(fd, &ClientsWriteBlocked);
    if (!XFD_ANYSET(&ClientsWriteBlocked))
	AnyClientsWriteBlocked = FALSE;
    FD_CLR(fd, &OutputPending);
    fsfree(oc);
}

void
CheckConnections(void)
{
    fd_set      mask;
    fd_set      tmask;
    int         curclient;
    int         i;
    struct timeval notime;
    int         r;

    notime.tv_sec = 0;
    notime.tv_usec = 0;

    XFD_COPYSET(&AllClients, &mask);
    for (i = 0; i < howmany(XFD_SETSIZE, NFDBITS); i++) {
	while (mask.fds_bits[i]) {
	    curclient = ffs(mask.fds_bits[i]) - 1 + (i << 5);
	    FD_ZERO(&tmask);
	    FD_SET(curclient, &tmask);
	    r = Select(curclient + 1, &tmask, NULL, NULL, &notime);
	    if (r < 0)
		CloseDownClient(clients[ConnectionTranslation[curclient]]);
	    FD_CLR(curclient, &mask);
	}
    }
}

void
CloseDownConnection(ClientPtr client)
{
    OsCommPtr   oc = (OsCommPtr) client->osPrivate;

    if (oc->output && oc->output->count)
	FlushClient(client, oc, (char *) NULL, 0, 0);
    ConnectionTranslation[oc->fd] = 0;
    close_fd(oc);
    client->osPrivate = (pointer) NULL;
}


/****************
 * IgnoreClient
 *    Removes one client from input masks.
 *    Must have cooresponding call to AttendClient.
 ****************/

static fd_set IgnoredClientsWithInput;

void
IgnoreClient(ClientPtr client)
{
    OsCommPtr   oc = (OsCommPtr) client->osPrivate;
    int         connection = oc->fd;

    if (FD_ISSET(connection, &ClientsWithInput))
	FD_SET(connection, &IgnoredClientsWithInput);
    else
	FD_CLR(connection, &IgnoredClientsWithInput);
    FD_CLR(connection, &ClientsWithInput);
    FD_CLR(connection, &AllSockets);
    FD_CLR(connection, &AllClients);
    FD_CLR(connection, &LastSelectMask);
    isItTimeToYield = TRUE;
}

/****************
 * AttendClient
 *    Adds one client back into the input masks.
 ****************/

void
AttendClient(ClientPtr client)
{
    OsCommPtr   oc = (OsCommPtr) client->osPrivate;
    int         connection = oc->fd;

    FD_SET(connection, &AllClients);
    FD_SET(connection, &AllSockets);
    FD_SET(connection, &LastSelectMask);
    if (FD_ISSET(connection, &IgnoredClientsWithInput))
	FD_SET(connection, &ClientsWithInput);
}

/*
 * figure out which clients need to be toasted
 */
void
ReapAnyOldClients(void)
{
    int         i;
    long        cur_time = GetTimeInMillis();
    ClientPtr   client;

#ifdef DEBUG
    fprintf(stderr, "looking for clients to reap\n");
#endif

    for (i = MINCLIENT; i < currentMaxClients; i++) {
	client = clients[i];
	if (client) {
	    if ((cur_time - client->last_request_time) >= ReapClientTime) {
		if (client->clientGone == CLIENT_AGED) {
		    client->clientGone = CLIENT_TIMED_OUT;

#ifdef DEBUG
		    fprintf(stderr, "reaping client #%d\n", i);
#endif

		    CloseDownClient(client);
		} else {
		    client->clientGone = CLIENT_AGED;
		    SendKeepAliveEvent(client);
		}
	    }
	}
    }
}
