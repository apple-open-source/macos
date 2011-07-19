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

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 *
 * streams.c - Support for STREAMS
 */

#include "dm.h"
#include "dm_error.h"

#ifdef XDMCP
# ifdef STREAMSCONN

#  include <fcntl.h>
#  include <tiuser.h>
#  include <netconfig.h>
#  include <netdir.h>

extern int	xdmcpFd;
extern int	chooserFd;

extern FD_TYPE	WellKnownSocketsMask;
extern int	WellKnownSocketsMax;

void
CreateWellKnownSockets (void)
{
    struct t_bind bind_addr;
    struct netconfig *nconf;
    struct nd_hostserv service;
    struct nd_addrlist *servaddrs;
    char *name, *localHostname();
    char bindbuf[15];
    int it;

    if (request_port == 0)
	return;
    Debug ("creating UDP stream %d\n", request_port);

    nconf = getnetconfigent("udp");
    if (!nconf) {
	t_error("getnetconfigent udp");
	return;
    }

    xdmcpFd = t_open(nconf->nc_device, O_RDWR, NULL);
    if (xdmcpFd == -1) {
	LogError ("XDMCP stream creation failed\n");
	t_error ("CreateWellKnownSockets(xdmcpFd): t_open failed");
	return;
    }
    name = localHostname ();
    registerHostname (name, strlen (name));
    RegisterCloseOnFork (xdmcpFd);

    service.h_host = HOST_SELF;
    snprintf(bindbuf, sizeof(bindbuf), "%d", request_port);
    service.h_serv = bindbuf;
    netdir_getbyname(nconf, &service, &servaddrs);
    freenetconfigent(nconf);

    bind_addr.qlen = 5;
    bind_addr.addr.buf = servaddrs->n_addrs[0].buf;
    bind_addr.addr.len = servaddrs->n_addrs[0].len;
    bind_addr.addr.maxlen = servaddrs->n_addrs[0].len;
    it = t_bind(xdmcpFd, &bind_addr, &bind_addr);
    netdir_free((char *)servaddrs, ND_ADDRLIST);
    if (it < 0)
    {
	LogError ("error binding STREAMS address %d\n", request_port);
	t_error("CreateWellKNowSocket(xdmcpFd): t_bind failed");
	t_close (xdmcpFd);
	xdmcpFd = -1;
	return;
    }
    WellKnownSocketsMax = xdmcpFd;
    FD_SET (xdmcpFd, &WellKnownSocketsMask);

    chooserFd = t_open ("/dev/tcp", O_RDWR, NULL);
    Debug ("Created chooser fd %d\n", chooserFd);
    if (chooserFd == -1)
    {
	LogError ("chooser stream creation failed\n");
	t_error("CreateWellKnowSockets(chooserFd): t_open failed");
	return;
    }
    bind_addr.qlen = 5;
    bind_addr.addr.len = 0;
    bind_addr.addr.maxlen = 0;
    if( t_bind( chooserFd, &bind_addr, NULL ) < 0 )
    {
        t_error("CreateWellKnowSockets(chooserFd): t_bind failed");
    }

    if (chooserFd > WellKnownSocketsMax)
	WellKnownSocketsMax = chooserFd;
    FD_SET (chooserFd, &WellKnownSocketsMask);
}

int
GetChooserAddr (char *addr,	/* return */
    int *lenp)			/* size of addr, returned as amt used */
{
    struct netbuf nbuf;
    int retval;

    nbuf.buf = addr;
    nbuf.len = *lenp;
    nbuf.maxlen = *lenp;
    retval = t_getname (chooserFd, &nbuf, LOCALNAME);
    if (retval < 0) {
	if (debugLevel > 0)
	    t_error("t_getname on chooser fd");
    }
    *lenp = nbuf.len;
    return retval;
}

/* TODO: Implement support for controlling which interfaces are listened on
   and for listening to multicast addresses.  See the sockets equivalent in
   sockets.c for details. */

void UpdateListenSockets (void)
{
    return;
}

void CloseListenSockets (void)
{
    return;
}

void ProcessListenSockets (fd_set *readmask)
{
    return;
}

# endif /* STREAMSCONN */
#endif /* XDMCP */
