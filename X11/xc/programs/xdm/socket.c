/* $Xorg: socket.c,v 1.4 2001/02/09 02:05:40 xorgcvs Exp $ */
/*

Copyright 1988, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/* $XFree86: xc/programs/xdm/socket.c,v 3.10 2001/12/14 20:01:24 dawes Exp $ */

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 *
 * socket.c - Support for BSD sockets
 */

#include "dm.h"
#include "dm_error.h"

#ifdef XDMCP
#ifndef STREAMSCONN

#include <errno.h>
#include "dm_socket.h"

#ifndef X_NO_SYS_UN
#ifndef Lynx
#include <sys/un.h>
#else
#include <un.h>
#endif
#endif
#include <netdb.h>


extern int	xdmcpFd;
extern int	chooserFd;

extern FD_TYPE	WellKnownSocketsMask;
extern int	WellKnownSocketsMax;

void
CreateWellKnownSockets (void)
{
    struct sockaddr_in	sock_addr;
    char *name;

    if (request_port == 0)
	    return;
    Debug ("creating socket %d\n", request_port);
    xdmcpFd = socket (AF_INET, SOCK_DGRAM, 0);
    if (xdmcpFd == -1) {
	LogError ("XDMCP socket creation failed, errno %d\n", errno);
	return;
    }
    name = localHostname ();
    registerHostname (name, strlen (name));
    RegisterCloseOnFork (xdmcpFd);
    /* zero out the entire structure; this avoids 4.4 incompatibilities */
    bzero ((char *) &sock_addr, sizeof (sock_addr));
#ifdef BSD44SOCKETS
    sock_addr.sin_len = sizeof(sock_addr);
#endif
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons ((short) request_port);
    sock_addr.sin_addr.s_addr = htonl (INADDR_ANY);
    if (bind (xdmcpFd, (struct sockaddr *)&sock_addr, sizeof (sock_addr)) == -1)
    {
	LogError ("error %d binding socket address %d\n", errno, request_port);
	close (xdmcpFd);
	xdmcpFd = -1;
	return;
    }
    WellKnownSocketsMax = xdmcpFd;
    FD_SET (xdmcpFd, &WellKnownSocketsMask);

    chooserFd = socket (AF_INET, SOCK_STREAM, 0);
    Debug ("Created chooser socket %d\n", chooserFd);
    if (chooserFd == -1)
    {
	LogError ("chooser socket creation failed, errno %d\n", errno);
	return;
    }
    listen (chooserFd, 5);
    if (chooserFd > WellKnownSocketsMax)
	WellKnownSocketsMax = chooserFd;
    FD_SET (chooserFd, &WellKnownSocketsMask);
}

int
GetChooserAddr (
    char	*addr,
    int		*lenp)
{
    struct sockaddr_in	in_addr;
    int			len;

    len = sizeof in_addr;
    if (getsockname (chooserFd, (struct sockaddr *)&in_addr, (void *)&len) < 0)
	return -1;
    Debug ("Chooser socket port: %d\n", ntohs(in_addr.sin_port));
    memmove( addr, (char *) &in_addr, len);
    *lenp = len;

    return 0;
}

#endif /* !STREAMSCONN */
#endif /* XDMCP */
