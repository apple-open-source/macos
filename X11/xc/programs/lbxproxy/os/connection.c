/* $Xorg: connection.c,v 1.5 2001/02/09 02:05:33 xorgcvs Exp $ */
/***********************************************************

Copyright 1987, 1989, 1998  The Open Group

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


Copyright 1987, 1989 by Digital Equipment Corporation, Maynard, Massachusetts.

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

 *****************************************************************
 *  Stuff to create connections --- OS dependent
 *
 *      EstablishNewConnections, CreateWellKnownSockets, 
 *      CloseDownConnection, CheckConnections, AddEnabledDevice,
 *	RemoveEnabledDevice, OnlyListToOneClient,
 *      ListenToAllClients,
 *
 *      (WaitForSomething is in its own file)
 *
 *      In this implementation, a client socket table is not kept.
 *      Instead, what would be the index into the table is just the
 *      file descriptor of the socket.  This won't work for if the
 *      socket ids aren't small nums (0 - 2^8)
 *
 *****************************************************************/
/* $XFree86: xc/programs/lbxproxy/os/connection.c,v 1.19 2003/11/17 22:20:49 dawes Exp $ */

#include "misc.h"
#include <X11/Xtrans.h>
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>			/* atoi */
#include <errno.h>
#include <signal.h>
#include <setjmp.h>

#include <sys/socket.h>

#ifdef hpux
#include <sys/utsname.h>
#include <sys/ioctl.h>
#endif

#ifdef AIXV3
#include <sys/ioctl.h>
#endif

#ifdef QNX4
#include <sys/stat.h>
#endif

#if defined (TCPCONN) || defined(STREAMSCONN)
# include <netinet/in.h>
# if !defined(hpux)
#  ifdef apollo
#   ifndef NO_TCP_H
#    include <netinet/tcp.h>
#   endif
#  else
#   include <netinet/tcp.h>
#  endif
# endif
#endif

#include <sys/uio.h>
#include <X11/Xpoll.h>
#include "osdep.h"
#include "os.h"
#include "lbx.h"
#include "util.h"
#include "pm.h"
#include "wire.h"

#ifdef X_NOT_POSIX
#define Pid_t int
#else
#define Pid_t pid_t
#endif

#ifdef DNETCONN
#include <netdnet/dn.h>
#endif /* DNETCONN */

int lastfdesc;			/* maximum file descriptor */

fd_set WellKnownConnections;	/* Listener mask */
fd_set AllSockets;		/* select on this */
fd_set AllClients;		/* available clients */
fd_set LastSelectMask;		/* mask returned from last select call */
fd_set ClientsWithInput;	/* clients with FULL requests in buffer */
fd_set ClientsWriteBlocked;	/* clients who cannot receive output */
fd_set OutputPending;		/* clients with reply/event data ready to go */
int MaxClients = 0;
Bool NewOutputPending;		/* not yet attempted to write some new output */
Bool AnyClientsWriteBlocked;	/* true if some client blocked on write */

Bool RunFromSmartParent;	/* send SIGUSR1 to parent process */
Bool PartialNetwork;		/* continue even if unable to bind all addrs */
static int ParentProcess;

static Bool debug_conns = FALSE;

static fd_set IgnoredClientsWithInput;
static fd_set GrabImperviousClients;
static fd_set SavedAllClients;
static fd_set SavedAllSockets;
static fd_set SavedClientsWithInput;

static int auditTrailLevel = 1;

int GrabInProgress = 0;
int *ConnectionTranslation = NULL;
int *ConnectionOutputTranslation = NULL;

static XtransConnInfo  *ListenTransConns = NULL;
static int             *ListenTransFds = NULL;
static int             ListenTransCount = 0;

unsigned long raw_stream_out;	/* out to server, in from client */
unsigned long raw_stream_in;	/* in from server, out to client */
extern unsigned long  stream_out_plain;
extern unsigned long  stream_in_plain;

static void ErrorConnMax(
    XtransConnInfo /* trans_conn */
);


static Bool
PickNewListenDisplay (displayP)
    char **displayP;
{
    static char newDisplay[16];
    sprintf (newDisplay, "%d", atoi (*displayP) + 1);
    *displayP = newDisplay;

    if (atoi (*displayP) > 65535)
	return (FALSE);

    return (TRUE);
}

static XtransConnInfo
lookup_trans_conn (fd)
    int fd;
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


/* Set MaxClients and lastfdesc, and allocate ConnectionTranslation */

void
InitConnectionLimits()
{
    lastfdesc = -1;

#ifndef __UNIXOS2__

#if !defined(XNO_SYSCONF) && defined(_SC_OPEN_MAX)
    lastfdesc = sysconf(_SC_OPEN_MAX) - 1;
#endif

#ifdef HAS_GETDTABLESIZE
    if (lastfdesc < 0)
	lastfdesc = getdtablesize() - 1;
#endif

#ifdef _NFILE
    if (lastfdesc < 0)
	lastfdesc = _NFILE - 1;
#endif

#else /* __UNIXOS2__ */
    lastfdesc = 255;
#endif

    /* This is the fallback */
    if (lastfdesc < 0)
	lastfdesc = MAXSOCKS;

    if (lastfdesc > MAXSELECT)
	lastfdesc = MAXSELECT;

    if (lastfdesc > 2 * MAXCLIENTS)
    {
	lastfdesc = 2 * MAXCLIENTS;
	if (debug_conns)
	    ErrorF( "REACHED MAXIMUM CLIENTS LIMIT %d\n", MAXCLIENTS);
    }
    MaxClients = lastfdesc;

    ConnectionTranslation = (int *)xalloc((lastfdesc + 1) * sizeof(int));
    ConnectionOutputTranslation = (int *)xalloc((lastfdesc + 1) * sizeof(int));
    if (ConnectionTranslation == NULL || ConnectionOutputTranslation == NULL)
	FatalError("failed to allocate ConnectionTranslation\n");
}
    
/*
 * Create the socket(s) that clients will used for one server.
 */
void
CreateServerSockets(fds)
    int		fds[];
{
    int			done = 0;
    int			partial;
    int			tmp_trans_count;
    XtransConnInfo     *tmp_trans_conns;

    while (!done)
    {

	if ((_LBXPROXYTransMakeAllCOTSServerListeners (display, &partial,
		    &tmp_trans_count, &tmp_trans_conns) >= 0) &&
	    (tmp_trans_count >= 1))
	{
	    if (!PartialNetwork && partial)
	    {
		(void) fprintf (stderr, "Failed to establish all listening sockets for port number '%s'\n", display);
		if (PickNewListenDisplay (&display))
		    continue;
	    }
	    else
	    {
		int i, num_fds;

		/*
		 * Create or Re-create the ListenTransFds array
		 */
		if (!ListenTransFds) {
		    ListenTransFds = (int *) malloc (tmp_trans_count *
						     sizeof (int));
		}
		else {
		    ListenTransFds = (int *) 
			realloc (ListenTransFds,
				(ListenTransCount + tmp_trans_count) *
				    sizeof (int));
		}
		if (!ListenTransFds)
		    FatalError ("Failed to create ListenTransFds");

		/*
		 * Create or Re-create the ListenTransConns array
		 */
		if (!ListenTransConns)
		    ListenTransConns = tmp_trans_conns;
		else {

		    ListenTransConns = (XtransConnInfo *)
			realloc (ListenTransConns,
				(ListenTransCount + tmp_trans_count) *
				    sizeof (XtransConnInfo));
		    if (!ListenTransConns)
			FatalError ("Failed to create ListenTransFds");

		    /*
		     * Add in the new trans' for this server
		     */
		    for (i = 0; i < tmp_trans_count; i++)
			ListenTransConns[ListenTransCount + i] = 
				tmp_trans_conns[i];
		}

		for (i = 0, num_fds = 0; i < tmp_trans_count; i++, num_fds++)
		{
		    int fd;

                    fd = _LBXPROXYTransGetConnectionNumber (
			    ListenTransConns[i + ListenTransCount]);
                    
                    fds[i] = fd;
		    ListenTransFds[i + ListenTransCount] = fd;
		    FD_SET (fd, &WellKnownConnections);
		}

		ListenTransCount += tmp_trans_count;

	    }
	} else {
	    if (PickNewListenDisplay (&display))
		continue;
	}
        done = 1;
    }

    if (!XFD_ANYSET (&WellKnownConnections))
        FatalError ("Cannot establish any listening sockets");

    /*
     * If the Proxy Manager isn't being used and the the default port 
     * number isn't available, need to advertise the port number that 
     * clients will use to connect to this server.
     */
    if (!proxyMngr && strcmp (display, "63")) 
	(void) fprintf (stderr, "Using port number '%s'\n", display);
}

void
CloseServerSockets()
{
    int i;

    for (i = 0; i < ListenTransCount; i++)
	_LBXPROXYTransClose (ListenTransConns[i]);
}

/*****************
 * CreateWellKnownSockets
 *    Initialize the global connection file descriptor arrays
 *    but don't create the listen sockets until a server is
 *    requested.
 *****************/
void
CreateWellKnownSockets()
{
    int		i;

    FD_ZERO(&AllSockets);
    FD_ZERO(&AllClients);
    FD_ZERO(&LastSelectMask);
    FD_ZERO(&ClientsWithInput);

    for (i=0; i<MaxClients; i++) ConnectionTranslation[i] = 0;
    for (i=0; i<MaxClients; i++) ConnectionOutputTranslation[i] = 0;

    FD_ZERO(&WellKnownConnections);

    OsSignal (SIGPIPE, SIG_IGN);
    OsSignal (SIGHUP, AutoResetServer);
    OsSignal (SIGINT, GiveUp);
    OsSignal (SIGTERM, GiveUp);
    /*
     * Magic:  If SIGUSR1 was set to SIG_IGN when
     * the server started, assume that either
     *
     *  a- The parent process is ignoring SIGUSR1
     *
     * or
     *
     *  b- The parent process is expecting a SIGUSR1
     *     when the server is ready to accept connections
     *
     * In the first case, the signal will be harmless,
     * in the second case, the signal will be quite
     * useful
     */
    if (OsSignal (SIGUSR1, SIG_IGN) == SIG_IGN)
	RunFromSmartParent = TRUE;
    ParentProcess = getppid ();
    if (RunFromSmartParent) {
	if (ParentProcess > 0) {
	    kill (ParentProcess, SIGUSR1);
	}
    }
}

void
ListenToProxyManager ()
{
    if (proxy_manager_fd >= 0)
	FD_SET(proxy_manager_fd, &AllSockets);
}

void
ListenWellKnownSockets ()
{
    XFD_ORSET (&AllSockets, &AllSockets, &WellKnownConnections);
}

void
AvailableClientInput (client)
    ClientPtr	client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;

    if (FD_ISSET(oc->fd, &AllSockets))
	FD_SET(oc->fd, &ClientsWithInput);
}

static int
ClientRead (fd, buf, len)
    int fd;
    char *buf;
    int len;
{
    int n;
    n = read(fd, buf, len);
    if (n > 0)
	raw_stream_out += n;
    return n;
}

static int
ClientWritev(fd, iov, iovcnt)
    int fd;
    struct iovec *iov;
    int iovcnt;
{
    int n;
    n = writev(fd, iov, iovcnt);
    if (n > 0)
	raw_stream_in += n;
    return n;
}

static int
ServerRead (fd, buf, len)
    int fd;
    char *buf;
    int len;
{
    int n;
    n = read(fd, buf, len);
    if (n > 0)
	stream_in_plain += n;
    return n;
}

static int
ServerWritev(fd, iov, iovcnt)
    int fd;
    struct iovec *iov;
    int iovcnt;
{
    int n;
    n = writev(fd, iov, iovcnt);
    if (n > 0)
	stream_out_plain += n;
    return n;
}

ClientPtr
AllocNewConnection (fd, connect_fd, to_server, trans_conn)
    int	    		fd;
    int     		connect_fd;
    Bool    		to_server;
    XtransConnInfo	trans_conn;
{
    OsCommPtr	oc;
    ClientPtr	client;

    if (fd >= lastfdesc)
	return NullClient;
    oc = (OsCommPtr)xalloc(sizeof(OsCommRec));
    if (!oc)
	return NullClient;
    oc->fd = fd;
    oc->trans_conn = trans_conn;
    oc->input = (ConnectionInputPtr)NULL;
    oc->output = (ConnectionOutputPtr)NULL;
    if (to_server) {
	oc->Read = ServerRead;
	oc->Writev = ServerWritev;
	oc->Close = CloseServer;
    } else {
	oc->Read = ClientRead;
	oc->Writev = ClientWritev;
	oc->Close = CloseDownFileDescriptor;
    }
    oc->flushClient = StandardFlushClient;
    oc->ofirst = (ConnectionOutputPtr) NULL;
    if (!(client = NextAvailableClient((pointer)oc, connect_fd)))
    {
	xfree (oc);
	return NullClient;
    }
    if (!ConnectionTranslation[fd])
    {
	ConnectionTranslation[fd] = client->index;
	ConnectionOutputTranslation[fd] = client->index;
	if (GrabInProgress)
	{
	    FD_SET(fd, &SavedAllClients);
	    FD_SET(fd, &SavedAllSockets);
	}
	else
	{
	    FD_SET(fd, &AllClients);
	    FD_SET(fd, &AllSockets);
	}
    }
    client->public.readRequest = StandardReadRequestFromClient;
    client->public.writeToClient = StandardWriteToClient;
    client->public.requestLength = StandardRequestLength;
    return client;
}

void
SwitchConnectionFuncs (client, Read, Writev)
    ClientPtr	client;
    int		(*Read)();
    int		(*Writev)();
{
    OsCommPtr	oc = (OsCommPtr) client->osPrivate;

    oc->Read = Read;
    oc->Writev = Writev;
}

void
StartOutputCompression(client, CompressOn, CompressOff)
    ClientPtr	client;
    void	(*CompressOn)();
    void	(*CompressOff)();
{
    OsCommPtr	oc = (OsCommPtr) client->osPrivate;

    oc->compressOn = CompressOn;
    oc->compressOff = CompressOff;
    oc->flushClient = LbxFlushClient;
}

/*****************
 * EstablishNewConnections
 *    If anyone is waiting on listened sockets, accept them.
 *    Returns a mask with indices of new clients.  Updates AllClients
 *    and AllSockets.
 *****************/

/*ARGSUSED*/
Bool
EstablishNewConnections(clientUnused, closure)
    ClientPtr clientUnused;
    pointer closure;
{
    fd_set readyconnections;      /* set of listeners that are ready */
    int curconn;                  /* fd of listener that's ready */
    register int newconn;         /* fd of new client */
    register ClientPtr client;
    fd_set tmask;
    int i;

    XFD_ANDSET(&tmask, (fd_set*)closure, &WellKnownConnections);
    XFD_COPYSET(&tmask, &readyconnections);
    if (!XFD_ANYSET(&readyconnections))
	return TRUE;

    for (i = 0; i < howmany(XFD_SETSIZE, NFDBITS); i++)
    {
      while (readyconnections.fds_bits[i])
      {
	XtransConnInfo trans_conn, new_trans_conn;
	int status;

	curconn = ffs (readyconnections.fds_bits[i]) - 1;
	readyconnections.fds_bits[i] &= ~(1L << curconn);
	curconn += (i * (sizeof(fd_mask)*8));

	if ((trans_conn = lookup_trans_conn (curconn)) == NULL)
	    continue;

	if ((new_trans_conn = _LBXPROXYTransAccept (trans_conn, &status)) == NULL)
	    continue;

	newconn = _LBXPROXYTransGetConnectionNumber (new_trans_conn);

	_LBXPROXYTransSetOption(new_trans_conn, TRANS_NONBLOCKING, 1);

	client = AllocNewConnection (newconn, curconn, FALSE, new_trans_conn);
	if (!client)
	{
	    ErrorConnMax(new_trans_conn);
	    _LBXPROXYTransClose(new_trans_conn);
	    return FALSE;
	}
      }
    }

    return TRUE;
}

#define NOROOM "Maximum number of clients reached"

/************
 *   ErrorConnMax
 *     Fail a connection due to lack of client or file descriptor space
 ************/

static void
ErrorConnMax(trans_conn)
XtransConnInfo trans_conn;
{
    register int fd = _LBXPROXYTransGetConnectionNumber (trans_conn);
    xConnSetupPrefix csp;
    char pad[3];
    struct iovec iov[3];
    char byteOrder = 0;
    int whichbyte = 1;
    struct timeval waittime;
    fd_set mask;

    /* if these seems like a lot of trouble to go to, it probably is */
    waittime.tv_sec = BOTIMEOUT / MILLI_PER_SECOND;
    waittime.tv_usec = (BOTIMEOUT % MILLI_PER_SECOND) *
		       (1000000 / MILLI_PER_SECOND);
    FD_ZERO(&mask);
    FD_SET(fd, &mask);
    (void)Select(fd + 1, &mask, NULL, NULL, &waittime);
    /* try to read the byte-order of the connection */
    (void)_LBXPROXYTransRead(trans_conn, &byteOrder, 1);
    if ((byteOrder == 'l') || (byteOrder == 'B'))
    {
	csp.success = xFalse;
	csp.lengthReason = sizeof(NOROOM) - 1;
	csp.length = (sizeof(NOROOM) + 2) >> 2;
	csp.majorVersion = X_PROTOCOL;
	csp.minorVersion = X_PROTOCOL_REVISION;
	if (((*(char *) &whichbyte) && (byteOrder == 'B')) ||
	    (!(*(char *) &whichbyte) && (byteOrder == 'l')))
	{
	    swaps(&csp.majorVersion, whichbyte);
	    swaps(&csp.minorVersion, whichbyte);
	    swaps(&csp.length, whichbyte);
	}
	iov[0].iov_len = sz_xConnSetupPrefix;
	iov[0].iov_base = (char *) &csp;
	iov[1].iov_len = csp.lengthReason;
	iov[1].iov_base = NOROOM;
	iov[2].iov_len = (4 - (csp.lengthReason & 3)) & 3;
	iov[2].iov_base = pad;
	(void)_LBXPROXYTransWritev(trans_conn, iov, 3);
    }
}


/************
 *   CloseDownFileDescriptor:
 *     Remove this file descriptor and it's I/O buffers, etc.
 ************/

void
CloseDownFileDescriptor(client)
    ClientPtr	client;
{
    register OsCommPtr oc = (OsCommPtr) client->osPrivate;
    int connection = oc->fd;

    if (oc->trans_conn) {
        _LBXPROXYTransDisconnect(oc->trans_conn);
        _LBXPROXYTransClose(oc->trans_conn);
    }

    ConnectionTranslation[connection] = 0;
    ConnectionOutputTranslation[connection] = 0;

    FD_CLR(connection, &AllSockets);
    FD_CLR(connection, &AllClients);
    FD_CLR(connection, &ClientsWithInput);
    FD_CLR(connection, &GrabImperviousClients);
    if (GrabInProgress)
    {
	FD_CLR(connection, &SavedAllSockets);
	FD_CLR(connection, &SavedAllClients);
	FD_CLR(connection, &SavedClientsWithInput);
    }
    FD_CLR(connection, &ClientsWriteBlocked);
    if (!XFD_ANYSET(&ClientsWriteBlocked))
    	AnyClientsWriteBlocked = FALSE;
    FD_CLR(connection, &OutputPending);
}

/*****************
 * CheckConections
 *    Some connection has died, go find which one and shut it down 
 *    The file descriptor has been closed, but is still in AllClients.
 *    If would truly be wonderful if select() would put the bogus
 *    file descriptors in the exception mask, but nooooo.  So we have
 *    to check each and every socket individually.
 *****************/

void
CheckConnections()
{
    fd_mask		mask;
    fd_set		tmask; 
    register int	curclient, curoff;
    int			i;
    struct timeval	notime;
    int r;

    notime.tv_sec = 0;
    notime.tv_usec = 0;

    for (i=0; i<howmany(XFD_SETSIZE, NFDBITS); i++)
    {
	mask = AllClients.fds_bits[i];
        while (mask)
    	{
	    curoff = ffs (mask) - 1;
 	    curclient = curoff + (i * (sizeof(fd_mask)*8));
            FD_ZERO(&tmask);
            FD_SET(curclient, &tmask);
            r = Select (curclient + 1, &tmask, NULL, NULL, &notime);
            if (r < 0)
		CloseDownClient(clients[ConnectionTranslation[curclient]]);
	    mask &= ~(1L << curoff);
	}
    }	
}


/*****************
 * CloseDownConnection
 *    Delete client from AllClients and free resources 
 *****************/

void
CloseDownConnection(client)
    ClientPtr client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;

    if (oc->output && oc->output->count)
	FlushClient(client, oc, (char *)NULL, 0);
    ConnectionTranslation[oc->fd] = 0;
    (*oc->Close) (client);
    FreeOsBuffers(oc);
    xfree(oc);
    client->osPrivate = (pointer)NULL;
    if (auditTrailLevel > 1)
	AuditF("client %d disconnected\n", client->index);
}

/*****************
 * OnlyListenToOneClient:
 *    Only accept requests from  one client.  Continue to handle new
 *    connections, but don't take any protocol requests from the new
 *    ones.  Note that if GrabInProgress is set, EstablishNewConnections
 *    needs to put new clients into SavedAllSockets and SavedAllClients.
 *    Note also that there is no timeout for this in the protocol.
 *    This routine is "undone" by ListenToAllClients()
 *****************/

void
OnlyListenToOneClient(client)
    ClientPtr client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    int connection = oc->fd;

    if (! GrabInProgress)
    {
	XFD_COPYSET(&ClientsWithInput, &SavedClientsWithInput);
	XFD_ANDSET(&ClientsWithInput,
		       &ClientsWithInput, &GrabImperviousClients);
	if (FD_ISSET(connection, &SavedClientsWithInput))
	{
	    FD_CLR(connection, &SavedClientsWithInput);
	    FD_SET(connection, &ClientsWithInput);
	}
	XFD_UNSET(&SavedClientsWithInput, &GrabImperviousClients);
	FD_CLR(connection, &AllSockets);
	XFD_COPYSET(&AllSockets, &SavedAllSockets);
	FD_CLR(connection, &AllClients);
	XFD_COPYSET(&AllClients, &SavedAllClients);
	XFD_UNSET(&AllSockets, &AllClients);
	XFD_ANDSET(&AllClients, &AllClients, &GrabImperviousClients);
	FD_SET(connection, &AllClients);
	XFD_ORSET(&AllSockets, &AllSockets, &AllClients);
	GrabInProgress = client->index;
    }
}

/****************
 * ListenToAllClients:
 *    Undoes OnlyListentToOneClient()
 ****************/

void
ListenToAllClients()
{
    if (GrabInProgress)
    {
	XFD_ORSET(&AllSockets, &AllSockets, &SavedAllSockets);
	XFD_ORSET(&AllClients, &AllClients, &SavedAllClients);
	XFD_ORSET(&ClientsWithInput, &ClientsWithInput, &SavedClientsWithInput);
	GrabInProgress = 0;
    }	
}

/****************
 * IgnoreClient
 *    Removes one client from input masks.
 *    Must have cooresponding call to AttendClient.
 ****************/

void
IgnoreClient (client)
    ClientPtr	client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    int connection = oc->fd;

    if (!GrabInProgress || FD_ISSET (connection, &AllClients))
    {
    	if (FD_ISSET (connection, &ClientsWithInput))
	    FD_SET(connection, &IgnoredClientsWithInput);
    	else
	    FD_CLR(connection, &IgnoredClientsWithInput);
    	FD_CLR(connection, &ClientsWithInput);
    	FD_CLR(connection, &AllSockets);
    	FD_CLR(connection, &AllClients);
	FD_CLR(connection, &LastSelectMask);
    }
    else
    {
    	if (FD_ISSET (connection, &SavedClientsWithInput))
	    FD_SET(connection, &IgnoredClientsWithInput);
    	else
	    FD_CLR(connection, &IgnoredClientsWithInput);
	FD_CLR(connection, &SavedClientsWithInput);
	FD_CLR(connection, &SavedAllSockets);
	FD_CLR(connection, &SavedAllClients);
    }
    isItTimeToYield = TRUE;
}

/****************
 * AttendClient
 *    Adds one client back into the input masks.
 ****************/

void
AttendClient (client)
    ClientPtr	client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    int connection = oc->fd;

    if (!GrabInProgress || GrabInProgress == client->index ||
	FD_ISSET(connection, &GrabImperviousClients))
    {
    	FD_SET(connection, &AllClients);
    	FD_SET(connection, &AllSockets);
	FD_SET(connection, &LastSelectMask);
    	if (FD_ISSET (connection, &IgnoredClientsWithInput))
	    FD_SET(connection, &ClientsWithInput);
    }
    else
    {
	FD_SET(connection, &SavedAllClients);
	FD_SET(connection, &SavedAllSockets);
	if (FD_ISSET(connection, &IgnoredClientsWithInput))
	    FD_SET(connection, &SavedClientsWithInput);
    }
}

/* make client impervious to grabs; assume only executing client calls this */

void
MakeClientGrabImpervious(client)
    ClientPtr client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    int connection = oc->fd;

    FD_SET(connection, &GrabImperviousClients);
}

/* make client pervious to grabs; assume only executing client calls this */

void
MakeClientGrabPervious(client)
    ClientPtr client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    int connection = oc->fd;

    FD_CLR(connection, &GrabImperviousClients);
    if (GrabInProgress && (GrabInProgress != client->index))
    {
	if (FD_ISSET(connection, &ClientsWithInput))
	{
	    FD_SET(connection, &SavedClientsWithInput);
	    FD_CLR(connection, &ClientsWithInput);
	}
	FD_CLR(connection, &AllSockets);
	FD_CLR(connection, &AllClients);
	isItTimeToYield = TRUE;
    }
}
