/*
 * Copyright © 2003 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <xfs-config.h>

#define FONT_t 1
#define TRANS_REOPEN 1
#define TRANS_SERVER 1

#include <X11/Xtrans/transport.c>

#ifdef XFS_INETD
/* xfs special handling for listen socket passed from inetd */
#include "misc.h"

OldListenRec *
TRANS(GetInetdListenInfo) (int fd)
{
    char *port = "0";
    XtransConnInfo inetdCI;
    OldListenRec *old_listen;
    int portnum;

    /* Create a XtransConnInfo struct for this connection */
    inetdCI = TRANS(ReopenCOTSServer)(TRANS_SOCKET_TCP_INDEX, fd, port);

    /* Fill in correct address/portnum */
    TRANS(SocketINETGetAddr)(inetdCI);
#ifdef AF_INET6
    if ( ((struct sockaddr *)(inetdCI->addr))->sa_family == AF_INET6 )
	portnum = ntohs(((struct sockaddr_in6 *)(inetdCI->addr))->sin6_port);
    else
#endif
	portnum = ntohs(((struct sockaddr_in *)(inetdCI->addr))->sin_port);
    inetdCI->port = xalloc(6); /* Base 10 integer <= 65535 + trailing NUL */
    snprintf(inetdCI->port, 6, "%d", portnum);

    /* Do the socket setup that xtrans normally takes care of in
     * TRANS(SocketOpen) && TRANS(SocketCreateListener)
     */
    {
	/*
	 * turn off TCP coalescence for INET sockets
	 */

	int tmp = 1;
	setsockopt (fd, IPPROTO_TCP, TCP_NODELAY,
		    (char *) &tmp, sizeof (int));
    }
#ifdef SO_DONTLINGER
    setsockopt (fd, SOL_SOCKET, SO_DONTLINGER, NULL, 0);
#else
# ifdef SO_LINGER
    {
	static int linger[2] = { 0, 0 };
	setsockopt (fd, SOL_SOCKET, SO_LINGER,
		    (char *) linger, sizeof (linger));
    }
# endif
#endif

    if (listen (fd, BACKLOG) < 0)
    {
	FatalError("listen() failed on inetd socket: %s\n",
		   strerror(errno));
    }

    /* Pass the inetd socket back through the connection setup code
     * the same way as a cloned listening port
     */
    old_listen =  malloc (sizeof (OldListenRec));
    if (old_listen != NULL) {
	TRANS(GetReopenInfo)(inetdCI, &(old_listen->trans_id),
			     &(old_listen->fd), &port);

	old_listen->portnum = portnum;
    }

    return old_listen;
}
#endif /* XFS_INETD */
