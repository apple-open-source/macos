/* $Xorg: xdmcp.c,v 1.4 2001/02/09 02:05:41 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/xdm/xdmcp.c,v 3.26 2004/01/07 04:28:06 dawes Exp $ */

/*
 * xdm - display manager daemon
 * Author:  Keith Packard, MIT X Consortium
 *
 * xdmcp.c - Support for XDMCP
 */

# include "dm.h"
# include "dm_auth.h"
# include "dm_error.h"

#ifdef XDMCP

# include	<X11/X.h>
# include	<X11/Xfuncs.h>
# include	<sys/types.h>
# include	<ctype.h>

# include	"dm_socket.h"

#ifndef X_NO_SYS_UN
#ifndef Lynx
#include	<sys/un.h>
#else
#include	<un.h>
#endif
#endif
#include	<netdb.h>
#include	<arpa/inet.h>

#include <time.h>
#define Time_t time_t

#define getString(name,len)	((name = malloc (len + 1)) ? 1 : 0)

/*
 * misc externs
 */
extern volatile int Rescan;
extern int sourceAddress, ChildReady;

/*
 * Forward reference
 */
static void broadcast_respond (struct sockaddr *from, int fromlen, int length, int fd);
static void forward_respond (struct sockaddr *from, int fromlen, int length, int fd);
static void manage (struct sockaddr *from, int fromlen, int length, int fd);
static void query_respond (struct sockaddr *from, int fromlen, int length, int fd);
static void request_respond (struct sockaddr *from, int fromlen, int length, int fd);
static void send_accept (struct sockaddr *to, int tolen, CARD32 sessionID, ARRAY8Ptr authenticationName, ARRAY8Ptr authenticationData, ARRAY8Ptr authorizationName, ARRAY8Ptr authorizationData, int fd);
static void send_alive (struct sockaddr *from, int fromlen, int length, int fd);
static void send_decline (struct sockaddr *to, int tolen, ARRAY8Ptr authenticationName, ARRAY8Ptr authenticationData, ARRAY8Ptr status, int fd);
static void send_failed (struct sockaddr *from, int fromlen, char *name, CARD32 sessionID, char *reason, int fd);
static void send_refuse (struct sockaddr *from, int fromlen, CARD32 sessionID, int fd);
static void send_unwilling (struct sockaddr *from, int fromlen, ARRAY8Ptr authenticationName, ARRAY8Ptr status, int fd);
static void send_willing (struct sockaddr *from, int fromlen, ARRAY8Ptr authenticationName, ARRAY8Ptr status, int fd);

#ifdef STREAMSCONN
int	xdmcpFd = -1;
#endif
int	chooserFd = -1;
#if defined(IPv6) && defined(AF_INET6)
int	chooserFd6 = -1;
#endif

FD_TYPE	WellKnownSocketsMask;
int	WellKnownSocketsMax;

#define pS(s)	((s) ? ((char *) (s)) : "empty string")

void
DestroyWellKnownSockets (void)
{
#ifdef STREAMSCONN
    if (xdmcpFd != -1)
    {
	close (xdmcpFd);
	FD_CLR(xdmcpFd, &WellKnownSocketsMask);
	xdmcpFd = -1;
    }
#endif
    if (chooserFd != -1)
    {
	close (chooserFd);
	FD_CLR(chooserFd, &WellKnownSocketsMask);
	chooserFd = -1;
    }
#if defined(IPv6) && defined(AF_INET6)
    if (chooserFd6 != -1)
    {
	close (chooserFd6);
	FD_CLR(chooserFd6, &WellKnownSocketsMask);
	chooserFd6 = -1;
    }
#endif
    CloseListenSockets();
}

static int
FD_ANYSET(fd_set *fds)
{
    int i;
    char *mask = (char *) fds;

    for (i = 0 ; i < sizeof(fds); i++) {
	if (mask[i])
	    return TRUE;
    }
    return FALSE;
}

int
AnyWellKnownSockets (void)
{
    return 
#ifdef STREAMS_CONN
      xdmcpFd != -1 ||
#endif
#if defined(IPv6) && defined(AF_INET6)
      chooserFd6 != -1 ||
#endif
      chooserFd != -1 || FD_ANYSET(&WellKnownSocketsMask);
}

static XdmcpBuffer	buffer;

/*ARGSUSED*/
static void
sendForward (
    CARD16	connectionType,
    ARRAY8Ptr	address,
    char	*closure)
{
#ifdef AF_INET
    struct sockaddr_in	    in_addr;
#endif
#if defined(IPv6) && defined(AF_INET6)
    struct sockaddr_in6	    in6_addr;
#endif
#ifdef AF_DECnet
#endif
    struct sockaddr	    *addr;
    int			    addrlen;

    switch (connectionType)
    {
#ifdef AF_INET
    case FamilyInternet:
	addr = (struct sockaddr *) &in_addr;
	bzero ((char *) &in_addr, sizeof (in_addr));
#ifdef BSD44SOCKETS
	in_addr.sin_len = sizeof(in_addr);
#endif
	in_addr.sin_family = AF_INET;
	in_addr.sin_port = htons ((short) XDM_UDP_PORT);
	if (address->length != 4)
	    return;
	memmove( (char *) &in_addr.sin_addr, address->data, address->length);
	addrlen = sizeof (struct sockaddr_in);
	break;
#endif
#if defined(IPv6) && defined(AF_INET6)
    case FamilyInternet6:
	addr = (struct sockaddr *) &in6_addr;
	bzero ((char *) &in6_addr, sizeof (in6_addr));
#ifdef SIN6_LEN
	in6_addr.sin6_len = sizeof(in6_addr);
#endif
	in6_addr.sin6_family = AF_INET6;
	in6_addr.sin6_port = htons ((short) XDM_UDP_PORT);
	if (address->length != 16)
	    return;
	memmove( (char *) &in6_addr.sin6_addr, address->data, address->length);
	addrlen = sizeof (struct sockaddr_in6);
	break;
#endif
#ifdef AF_DECnet
    case FamilyDECnet:
#endif
    default:
	return;
    }
    XdmcpFlush (*((int *) closure), &buffer, (XdmcpNetaddr) addr, addrlen);
    return;
}

static void
ClientAddress (
    struct sockaddr *from,
    ARRAY8Ptr	    addr,	/* return */
    ARRAY8Ptr	    port,	/* return */
    CARD16	    *type)	/* return */
{
    int length, family;
    char *data;

    data = NetaddrPort((XdmcpNetaddr) from, &length);
    XdmcpAllocARRAY8 (port, length);
    memmove( port->data, data, length);
    port->length = length;

    family = ConvertAddr((XdmcpNetaddr) from, &length, &data);
    XdmcpAllocARRAY8 (addr, length);
    memmove( addr->data, data, length);
    addr->length = length;

    *type = family;
}

static void
all_query_respond (
    struct sockaddr	*from,
    int			fromlen,
    ARRAYofARRAY8Ptr	authenticationNames,
    xdmOpCode		type,
    int			fd)
{
    ARRAY8Ptr	authenticationName;
    ARRAY8	status;
    ARRAY8	addr;
    CARD16	connectionType;
    int		family;
    int		length;
    const char	*addrstring;
#if defined(IPv6) && defined(AF_INET6) 
    char	addrbuf[INET6_ADDRSTRLEN] = "";
#endif

    family = ConvertAddr((XdmcpNetaddr) from, &length, (char **)&(addr.data));
    addr.length = length;	/* convert int to short */
    if (debugLevel > 0) {
#if defined(IPv6) && defined(AF_INET6) 
	void *ipaddr;
	if (family == AF_INET6) {
	    ipaddr = & ((struct sockaddr_in6 *) from)->sin6_addr;
	} else {
	    ipaddr = & ((struct sockaddr_in *) from)->sin_addr;
	}
	addrstring = inet_ntop(family, ipaddr, addrbuf, sizeof(addrbuf));
#else
	addrstring = inet_ntoa(((struct sockaddr_in *)from)->sin_addr);
#endif
	Debug("all_query_respond: conntype=%d, addr=%s, len=%d\n",
	    family, addrstring, addr.length);
    }
    if (family < 0)
	return;
    connectionType = family;

    if (type == INDIRECT_QUERY)
	RememberIndirectClient (&addr, connectionType);
    else
	ForgetIndirectClient (&addr, connectionType);

    authenticationName = ChooseAuthentication (authenticationNames);
    if (Willing (&addr, connectionType, authenticationName, &status, type))
	send_willing (from, fromlen, authenticationName, &status, fd);
    else
	if (type == QUERY)
	    send_unwilling (from, fromlen, authenticationName, &status, fd);
    XdmcpDisposeARRAY8 (&status);
}

static void
indirect_respond (
    struct sockaddr *from,
    int		    fromlen,
    int		    length,
    int		    fd)
{
    ARRAYofARRAY8   queryAuthenticationNames;
    ARRAY8	    clientAddress;
    ARRAY8	    clientPort;
    CARD16	    connectionType;
    int		    expectedLen;
    int		    i;
    XdmcpHeader	    header;
    int		    localHostAsWell;
    
    Debug ("Indirect respond %d\n", length);
    if (!XdmcpReadARRAYofARRAY8 (&buffer, &queryAuthenticationNames))
	return;
    expectedLen = 1;
    for (i = 0; i < (int)queryAuthenticationNames.length; i++)
	expectedLen += 2 + queryAuthenticationNames.data[i].length;
    if (length == expectedLen)
    {
	ClientAddress (from, &clientAddress, &clientPort, &connectionType);
	/*
	 * set up the forward query packet
	 */
    	header.version = XDM_PROTOCOL_VERSION;
    	header.opcode = (CARD16) FORWARD_QUERY;
    	header.length = 0;
    	header.length += 2 + clientAddress.length;
    	header.length += 2 + clientPort.length;
    	header.length += 1;
    	for (i = 0; i < (int)queryAuthenticationNames.length; i++)
	    header.length += 2 + queryAuthenticationNames.data[i].length;
    	XdmcpWriteHeader (&buffer, &header);
    	XdmcpWriteARRAY8 (&buffer, &clientAddress);
    	XdmcpWriteARRAY8 (&buffer, &clientPort);
    	XdmcpWriteARRAYofARRAY8 (&buffer, &queryAuthenticationNames);

	localHostAsWell = ForEachMatchingIndirectHost (&clientAddress, connectionType, sendForward, (char *) &fd);
	
	XdmcpDisposeARRAY8 (&clientAddress);
	XdmcpDisposeARRAY8 (&clientPort);
	if (localHostAsWell)
	    all_query_respond (from, fromlen, &queryAuthenticationNames,
			   INDIRECT_QUERY, fd);
    }
    else
    {
	Debug ("Indirect length error got %d expect %d\n", length, expectedLen);
    }
    XdmcpDisposeARRAYofARRAY8 (&queryAuthenticationNames);
}

void
ProcessRequestSocket (int fd)
{
    XdmcpHeader		header;
#if defined(IPv6) && defined(AF_INET6)
    struct sockaddr_storage	addr;
#else
    struct sockaddr	addr;
#endif
    int			addrlen = sizeof addr;

    Debug ("ProcessRequestSocket\n");
    bzero ((char *) &addr, sizeof (addr));
    if (!XdmcpFill (fd, &buffer, (XdmcpNetaddr) &addr, &addrlen)) {
	Debug ("XdmcpFill failed\n");
	return;
    }
    if (!XdmcpReadHeader (&buffer, &header)) {
	Debug ("XdmcpReadHeader failed\n");
	return;
    }
    if (header.version != XDM_PROTOCOL_VERSION) {
	Debug ("XDMCP header version read was %d, expected %d\n",
	       header.version, XDM_PROTOCOL_VERSION);
	return;
    }
    Debug ("header: %d %d %d\n", header.version, header.opcode, header.length);
    switch (header.opcode)
    {
    case BROADCAST_QUERY:
	broadcast_respond ((struct sockaddr *) &addr, addrlen, header.length, fd);
	break;
    case QUERY:
	query_respond ((struct sockaddr *) &addr, addrlen, header.length, fd);
	break;
    case INDIRECT_QUERY:
	indirect_respond ((struct sockaddr *) &addr, addrlen, header.length, fd);
	break;
    case FORWARD_QUERY:
	forward_respond ((struct sockaddr *) &addr, addrlen, header.length, fd);
	break;
    case REQUEST:
	request_respond ((struct sockaddr *) &addr, addrlen, header.length, fd);
	break;
    case MANAGE:
	manage ((struct sockaddr *) &addr, addrlen, header.length, fd);
	break;
    case KEEPALIVE:
	send_alive ((struct sockaddr *) &addr, addrlen, header.length, fd);
	break;
    }
}

void
WaitForSomething (void)
{
    FD_TYPE	reads;
    int	nready;

    Debug ("WaitForSomething\n");
    if (AnyWellKnownSockets () && !ChildReady) {
	reads = WellKnownSocketsMask;
	nready = select (WellKnownSocketsMax + 1, &reads, 0, 0, 0);
	Debug ("select returns %d.  Rescan: %d  ChildReady: %d\n",
		nready, Rescan, ChildReady);
	if (nready > 0)
	{
#ifdef STREAMSCONN
	    if (xdmcpFd >= 0 && FD_ISSET (xdmcpFd, &reads))
		ProcessRequestSocket (xdmcpFd);
#endif
	    if (chooserFd >= 0 && FD_ISSET (chooserFd, &reads))
	    {
#ifdef ISC
	        if (!ChildReady) {
	           WaitForSomething ();
                } else
#endif
		ProcessChooserSocket (chooserFd);
		FD_CLR(chooserFd, &reads);
	    }
#if defined(IPv6) && defined(AF_INET6)
	    if (chooserFd6 >= 0 && FD_ISSET (chooserFd6, &reads))
	    {
#ifdef ISC
	        if (!ChildReady) {
	           WaitForSomething ();
                } else
#endif
		ProcessChooserSocket (chooserFd6);
		FD_CLR(chooserFd6, &reads);
	    }
#endif 
	    ProcessListenSockets(&reads);
	}
	if (ChildReady)
	{
	    WaitForChild ();
	}
    } else
	WaitForChild ();
}

/*
 * respond to a request on the UDP socket.
 */

static ARRAY8	Hostname;

void
registerHostname (
    char    *name,
    int	    namelen)
{
    int	i;

    if (!XdmcpReallocARRAY8 (&Hostname, namelen))
	return;
    for (i = 0; i < namelen; i++)
	Hostname.data[i] = name[i];
}

static void
direct_query_respond (
    struct sockaddr *from,
    int		    fromlen,
    int		    length,
    xdmOpCode	    type,
    int		    fd)
{
    ARRAYofARRAY8   queryAuthenticationNames;
    int		    expectedLen;
    int		    i;
    
    if (!XdmcpReadARRAYofARRAY8 (&buffer, &queryAuthenticationNames))
	return;
    expectedLen = 1;
    for (i = 0; i < (int)queryAuthenticationNames.length; i++)
	expectedLen += 2 + queryAuthenticationNames.data[i].length;
    if (length == expectedLen)
	all_query_respond (from, fromlen, &queryAuthenticationNames, type, fd);
    XdmcpDisposeARRAYofARRAY8 (&queryAuthenticationNames);
}

static void
query_respond (
    struct sockaddr *from,
    int		    fromlen,
    int		    length,
    int		    fd)
{
    Debug ("Query respond %d\n", length);
    direct_query_respond (from, fromlen, length, QUERY, fd);
}

static void
broadcast_respond (
    struct sockaddr *from,
    int		    fromlen,
    int		    length,
    int		    fd)
{
    direct_query_respond (from, fromlen, length, BROADCAST_QUERY, fd);
}

/* computes an X display name */

static char *
NetworkAddressToName(
    CARD16	connectionType,
    ARRAY8Ptr   connectionAddress,
    struct sockaddr   *originalAddress,
    CARD16	displayNumber)
{
    switch (connectionType)
    {
    case FamilyInternet:
#if defined(IPv6) && defined(AF_INET6)
    case FamilyInternet6:
	{
	    CARD8		*data;
	    struct hostent	*hostent;
	    char 		*hostname = NULL;
	    char		*name;
	    char		*localhost;
	    int			 multiHomed = 0;
	    struct addrinfo	 hints, *ai = NULL, *nai;
	    int 		 type;

	    if (connectionType == FamilyInternet6)
		type = AF_INET6;
	    else
		type = AF_INET;

	    data = connectionAddress->data;
	    hostent = gethostbyaddr ((char *)data,
				     connectionAddress->length, type);
	    if (hostent) {
		if (sourceAddress) {
		    bzero(&hints, sizeof(hints));
		    hints.ai_flags = AI_CANONNAME;
		    if (getaddrinfo(hostent->h_name, NULL, &hints, &ai) == 0) {
			hostname = ai->ai_canonname;
			for (nai = ai->ai_next; nai!=NULL; nai=nai->ai_next) {
			    if ((ai->ai_protocol == nai->ai_protocol) &&
				(ai->ai_addrlen == nai->ai_addrlen) &&
			        (memcmp(ai->ai_addr,nai->ai_addr,
					ai->ai_addrlen) != 0) ) {
				multiHomed = 1;
			    }
			}
		    }
		} else {
		    hostname = hostent->h_name;
		}
	    }

	    localhost = localHostname ();

	    /* 
	     * protect against bogus host names 
	     */
	    if (hostname && hostname[0] && (hostname[0] != '.') 
			&& !multiHomed)
	    {
		if (!strcmp (localhost, hostname))
		{
		    if (!getString (name, 10))
			return 0;
		    sprintf (name, ":%d", displayNumber);
		}
		else
		{
		    if (removeDomainname)
		    {
		    	char    *localDot, *remoteDot;
    
			/* check for a common domain name.  This
			 * could reduce names by recognising common
			 * super-domain names as well, but I don't think
			 * this is as useful, and will confuse more
			 * people
 			 */
		    	if ((localDot = strchr(localhost, '.')) &&
		            (remoteDot = strchr(hostname, '.')))
			{
			    /* smash the name in place; it won't
			     * be needed later.
			     */
			    if (!strcmp (localDot+1, remoteDot+1))
				*remoteDot = '\0';
			}
		    }

		    if (!getString (name, strlen (hostname) + 10))
			return 0;
		    sprintf (name, "%s:%d", hostname, displayNumber);
		}
	    }
	    else
	    {
		if (!getString (name, INET6_ADDRSTRLEN + 10))
		    return 0;
		if (multiHomed) {
		    if (connectionType == FamilyInternet) {
			data = (CARD8 *) 
			  &((struct sockaddr_in *)originalAddress)->
			  sin_addr;
		    } else {
			data = (CARD8 *) 
			  &((struct sockaddr_in6 *)originalAddress)->sin6_addr;
		    }
		}
		if (inet_ntop(type, data, name, INET6_ADDRSTRLEN) == NULL) {
		    free(name);
		    return 0;
		} 
		sprintf(name + strlen(name), ":%d", displayNumber);
	    }
	    if (ai)
		freeaddrinfo(ai);
	    return name;
	}
#else /* IPv6 */
	{
	    CARD8		*data;
	    struct hostent	*hostent;
	    char		*name;
	    char		*localhost;
	    int			 multiHomed = 0;

	    data = connectionAddress->data;
	    hostent = gethostbyaddr ((char *)data,
				     connectionAddress->length, AF_INET);
	    if (sourceAddress && hostent) {
		hostent = gethostbyname(hostent->h_name);
		if (hostent)
			multiHomed = hostent->h_addr_list[1] != NULL;
	    }

	    localhost = localHostname ();

	    /* 
	     * protect against bogus host names 
	     */
	    if (hostent && hostent->h_name && hostent->h_name[0]
			&& (hostent->h_name[0] != '.') 
			&& !multiHomed)
	    {
		if (!strcmp (localhost, hostent->h_name))
		{
		    if (!getString (name, 10))
			return 0;
		    sprintf (name, ":%d", displayNumber);
		}
		else
		{
		    if (removeDomainname)
		    {
		    	char    *localDot, *remoteDot;
    
			/* check for a common domain name.  This
			 * could reduce names by recognising common
			 * super-domain names as well, but I don't think
			 * this is as useful, and will confuse more
			 * people
 			 */
		    	if ((localDot = strchr(localhost, '.')) &&
		            (remoteDot = strchr(hostent->h_name, '.')))
			{
			    /* smash the name in place; it won't
			     * be needed later.
			     */
			    if (!strcmp (localDot+1, remoteDot+1))
				*remoteDot = '\0';
			}
		    }

		    if (!getString (name, strlen (hostent->h_name) + 10))
			return 0;
		    sprintf (name, "%s:%d", hostent->h_name, displayNumber);
		}
	    }
	    else
	    {
		if (!getString (name, 25))
		    return 0;
		if (multiHomed)
		    data = (CARD8 *) &((struct sockaddr_in *)originalAddress)->
				sin_addr.s_addr;
		sprintf(name, "%d.%d.%d.%d:%d",
			data[0], data[1], data[2], data[3], displayNumber);
	    }
	    return name;
	}
#endif /* IPv6 */
#ifdef DNET
    case FamilyDECnet:
	return NULL;
#endif /* DNET */
    default:
	return NULL;
    }
}

/*ARGSUSED*/
static void
forward_respond (
    struct sockaddr	*from,
    int			fromlen,
    int			length,
    int			fd)
{
    ARRAY8	    clientAddress;
    ARRAY8	    clientPort;
    ARRAYofARRAY8   authenticationNames;
    struct sockaddr *client;
    int		    clientlen;
    int		    expectedLen;
    int		    i;
    
    Debug ("Forward respond %d\n", length);
    clientAddress.length = 0;
    clientAddress.data = 0;
    clientPort.length = 0;
    clientPort.data = 0;
    authenticationNames.length = 0;
    authenticationNames.data = 0;
    if (XdmcpReadARRAY8 (&buffer, &clientAddress) &&
	XdmcpReadARRAY8 (&buffer, &clientPort) &&
	XdmcpReadARRAYofARRAY8 (&buffer, &authenticationNames))
    {
	expectedLen = 0;
	expectedLen += 2 + clientAddress.length;
	expectedLen += 2 + clientPort.length;
	expectedLen += 1;	    /* authenticationNames */
	for (i = 0; i < (int)authenticationNames.length; i++)
	    expectedLen += 2 + authenticationNames.data[i].length;
	if (length == expectedLen)
	{
	    int	j;

	    j = 0;
	    for (i = 0; i < (int)clientPort.length; i++)
		j = j * 256 + clientPort.data[i];
	    Debug ("Forward client address (port %d)", j);
	    for (i = 0; i < (int)clientAddress.length; i++)
		Debug (" %d", clientAddress.data[i]);
	    Debug ("\n");
    	    switch (from->sa_family)
    	    {
#ifdef AF_INET
	    case AF_INET:
		{
		    struct sockaddr_in	in_addr;

		    if (clientAddress.length != 4 ||
		        clientPort.length != 2)
		    {
			goto badAddress;
		    }
		    bzero ((char *) &in_addr, sizeof (in_addr));
#ifdef BSD44SOCKETS
		    in_addr.sin_len = sizeof(in_addr);
#endif
		    in_addr.sin_family = AF_INET;
		    memmove( &in_addr.sin_addr, clientAddress.data, 4);
		    memmove( (char *) &in_addr.sin_port, clientPort.data, 2);
		    client = (struct sockaddr *) &in_addr;
		    clientlen = sizeof (in_addr);
		    all_query_respond (client, clientlen, &authenticationNames,
			       FORWARD_QUERY, fd);
		}
		break;
#endif
#if defined(IPv6) && defined(AF_INET6)
	    case AF_INET6:
		{
		    struct sockaddr_in6	in6_addr;

		    if (clientAddress.length != 16 ||
		        clientPort.length != 2)
		    {
			goto badAddress;
		    }
		    bzero ((char *) &in6_addr, sizeof (in6_addr));
#ifdef SIN6_LEN
		    in6_addr.sin6_len = sizeof(in6_addr);
#endif
		    in6_addr.sin6_family = AF_INET6;
		    memmove(&in6_addr,clientAddress.data,clientAddress.length);
		    memmove((char *) &in6_addr.sin6_port, clientPort.data, 2);
		    client = (struct sockaddr *) &in6_addr;
		    clientlen = sizeof (in6_addr);
		    all_query_respond (client, clientlen, &authenticationNames,
			       FORWARD_QUERY, fd);
		}
		break;
#endif
#ifdef AF_UNIX
	    case AF_UNIX:
		{
		    struct sockaddr_un	un_addr;

		    if (clientAddress.length >= sizeof (un_addr.sun_path))
			goto badAddress;
		    bzero ((char *) &un_addr, sizeof (un_addr));
		    un_addr.sun_family = AF_UNIX;
		    memmove( un_addr.sun_path, clientAddress.data, clientAddress.length);
		    un_addr.sun_path[clientAddress.length] = '\0';
		    client = (struct sockaddr *) &un_addr;
#if defined(BSD44SOCKETS) && !defined(Lynx) && defined(UNIXCONN)
		    un_addr.sun_len = strlen(un_addr.sun_path);
		    clientlen = SUN_LEN(&un_addr);
#else
		    clientlen = sizeof (un_addr);
#endif
		    all_query_respond (client, clientlen, &authenticationNames,
			       FORWARD_QUERY, fd);
		}
		break;
#endif
#ifdef AF_CHAOS
	    case AF_CHAOS:
		goto badAddress;
#endif
#ifdef AF_DECnet
	    case AF_DECnet:
		goto badAddress;
#endif
    	    }
	}
	else
	{
	    Debug ("Forward length error got %d expect %d\n", length, expectedLen);
	}
    }
badAddress:
    XdmcpDisposeARRAY8 (&clientAddress);
    XdmcpDisposeARRAY8 (&clientPort);
    XdmcpDisposeARRAYofARRAY8 (&authenticationNames);
}

static void
send_willing (
    struct sockaddr *from,
    int		    fromlen,
    ARRAY8Ptr	    authenticationName,
    ARRAY8Ptr	    status,
    int		    fd)
{
    XdmcpHeader	header;

    Debug ("Send willing %*.*s %*.*s\n", authenticationName->length,
					 authenticationName->length,
					 pS(authenticationName->data),
					 status->length,
					 status->length,
					 pS(status->data));
    header.version = XDM_PROTOCOL_VERSION;
    header.opcode = (CARD16) WILLING;
    header.length = 6 + authenticationName->length +
		    Hostname.length + status->length;
    XdmcpWriteHeader (&buffer, &header);
    XdmcpWriteARRAY8 (&buffer, authenticationName);
    XdmcpWriteARRAY8 (&buffer, &Hostname);
    XdmcpWriteARRAY8 (&buffer, status);
    XdmcpFlush (fd, &buffer, (XdmcpNetaddr) from, fromlen);
}

static void
send_unwilling (
    struct sockaddr *from,
    int		    fromlen,
    ARRAY8Ptr	    authenticationName,
    ARRAY8Ptr	    status,
    int		    fd)
{
    XdmcpHeader	header;

    Debug ("Send unwilling %*.*s %*.*s\n", authenticationName->length,
					 authenticationName->length,
					 pS(authenticationName->data),
					 status->length,
					 status->length,
					 pS(status->data));
    header.version = XDM_PROTOCOL_VERSION;
    header.opcode = (CARD16) UNWILLING;
    header.length = 4 + Hostname.length + status->length;
    XdmcpWriteHeader (&buffer, &header);
    XdmcpWriteARRAY8 (&buffer, &Hostname);
    XdmcpWriteARRAY8 (&buffer, status);
    XdmcpFlush (fd, &buffer, (XdmcpNetaddr) from, fromlen);
}

static unsigned long	globalSessionID;

#define NextSessionID()    (++globalSessionID)

void init_session_id(void)
{
    /* Set randomly so we are unlikely to reuse id's from a previous
     * incarnation so we don't say "Alive" to those displays.
     * Start with low digits 0 to make debugging easier.
     */
    globalSessionID = (time((Time_t)0)&0x7fff) * 16000;
}
    
static ARRAY8 outOfMemory = { (CARD16) 13, (CARD8Ptr) "Out of memory" };
static ARRAY8 noValidAddr = { (CARD16) 16, (CARD8Ptr) "No valid address" };
static ARRAY8 noValidAuth = { (CARD16) 22, (CARD8Ptr) "No valid authorization" };
static ARRAY8 noAuthentic = { (CARD16) 29, (CARD8Ptr) "XDM has no authentication key" };

static void
request_respond (
    struct sockaddr *from,
    int		    fromlen,
    int		    length,
    int		    fd)
{
    CARD16	    displayNumber;
    ARRAY16	    connectionTypes;
    ARRAYofARRAY8   connectionAddresses;
    ARRAY8	    authenticationName;
    ARRAY8	    authenticationData;
    ARRAYofARRAY8   authorizationNames;
    ARRAY8	    manufacturerDisplayID;
    ARRAY8Ptr	    reason = 0;
    int		    expectlen;
    int		    i, j;
    struct protoDisplay  *pdpy;
    ARRAY8	    authorizationName, authorizationData;
    ARRAY8Ptr	    connectionAddress;

    Debug ("Request respond %d\n", length);
    connectionTypes.data = 0;
    connectionAddresses.data = 0;
    authenticationName.data = 0;
    authenticationData.data = 0;
    authorizationNames.data = 0;
    authorizationName.length = 0;
    authorizationData.length = 0;
    manufacturerDisplayID.data = 0;
    if (XdmcpReadCARD16 (&buffer, &displayNumber) &&
	XdmcpReadARRAY16 (&buffer, &connectionTypes) &&
	XdmcpReadARRAYofARRAY8 (&buffer, &connectionAddresses) &&
	XdmcpReadARRAY8 (&buffer, &authenticationName) &&
	XdmcpReadARRAY8 (&buffer, &authenticationData) &&
	XdmcpReadARRAYofARRAY8 (&buffer, &authorizationNames) &&
	XdmcpReadARRAY8 (&buffer, &manufacturerDisplayID))
    {
	expectlen = 0;
	expectlen += 2;				    /* displayNumber */
	expectlen += 1 + 2*connectionTypes.length;  /* connectionTypes */
	expectlen += 1;				    /* connectionAddresses */
	for (i = 0; i < (int)connectionAddresses.length; i++)
	    expectlen += 2 + connectionAddresses.data[i].length;
	expectlen += 2 + authenticationName.length; /* authenticationName */
	expectlen += 2 + authenticationData.length; /* authenticationData */
	expectlen += 1;				    /* authoriationNames */
	for (i = 0; i < (int)authorizationNames.length; i++)
	    expectlen += 2 + authorizationNames.data[i].length;
	expectlen += 2 + manufacturerDisplayID.length;	/* displayID */
	if (expectlen != length)
	{
	    Debug ("Request length error got %d expect %d\n", length, expectlen);
	    goto abort;
	}
	if (connectionTypes.length == 0 ||
	    connectionAddresses.length != connectionTypes.length)
	{
	    reason = &noValidAddr;
	    pdpy = 0;
	    goto decline;
	}
	pdpy = FindProtoDisplay ((XdmcpNetaddr) from, fromlen, displayNumber);
	if (!pdpy) {

	    /* Check this Display against the Manager's policy */
	    reason = Accept (from, fromlen, displayNumber);
	    if (reason)
		goto decline;

	    /* Check the Display's stream services against Manager's policy */
	    i = SelectConnectionTypeIndex (&connectionTypes,
					   &connectionAddresses);
	    if (i < 0) {
		reason = &noValidAddr;
		goto decline;
	    }
	
	    /* The Manager considers this a new session */
	    connectionAddress = &connectionAddresses.data[i];
	    pdpy = NewProtoDisplay ((XdmcpNetaddr) from, fromlen, displayNumber,
				    connectionTypes.data[i], connectionAddress,
				    NextSessionID());
	    Debug ("NewProtoDisplay %p\n", pdpy);
	    if (!pdpy) {
		reason = &outOfMemory;
		goto decline;
	    }
	}
	if (authorizationNames.length == 0)
	    j = 0;
	else
	    j = SelectAuthorizationTypeIndex (&authenticationName,
					      &authorizationNames);
	if (j < 0)
	{
	    reason = &noValidAuth;
	    goto decline;
	}
	if (!CheckAuthentication (pdpy,
				  &manufacturerDisplayID,
				  &authenticationName,
				  &authenticationData))
	{
	    reason = &noAuthentic;
	    goto decline;
	}
	if (j < (int)authorizationNames.length)
	{
	    Xauth   *auth;
	    SetProtoDisplayAuthorization (pdpy,
		(unsigned short) authorizationNames.data[j].length,
		(char *) authorizationNames.data[j].data);
	    auth = pdpy->xdmcpAuthorization;
	    if (!auth)
		auth = pdpy->fileAuthorization;
	    if (auth)
	    {
		authorizationName.length = auth->name_length;
		authorizationName.data = (CARD8Ptr) auth->name;
		authorizationData.length = auth->data_length;
		authorizationData.data = (CARD8Ptr) auth->data;
	    }
	}
	if (pdpy)
	{
	    send_accept (from, fromlen, pdpy->sessionID,
				        &authenticationName,
					&authenticationData,
					&authorizationName,
					&authorizationData, fd);
	}
	else
	{
decline:    ;
	    send_decline (from, fromlen, &authenticationName,
				 &authenticationData,
				 reason, fd);
            if (pdpy)
		DisposeProtoDisplay (pdpy);
	}
    }
abort:
    XdmcpDisposeARRAY16 (&connectionTypes);
    XdmcpDisposeARRAYofARRAY8 (&connectionAddresses);
    XdmcpDisposeARRAY8 (&authenticationName);
    XdmcpDisposeARRAY8 (&authenticationData);
    XdmcpDisposeARRAYofARRAY8 (&authorizationNames);
    XdmcpDisposeARRAY8 (&manufacturerDisplayID);
}

static void
send_accept (
    struct sockaddr *to,
    int		    tolen,
    CARD32	    sessionID,
    ARRAY8Ptr	    authenticationName,
    ARRAY8Ptr	    authenticationData,
    ARRAY8Ptr	    authorizationName,
    ARRAY8Ptr	    authorizationData,
    int		    fd)
{
    XdmcpHeader	header;

    Debug ("Accept Session ID %ld\n", (long) sessionID);
    header.version = XDM_PROTOCOL_VERSION;
    header.opcode = (CARD16) ACCEPT;
    header.length = 4;			    /* session ID */
    header.length += 2 + authenticationName->length;
    header.length += 2 + authenticationData->length;
    header.length += 2 + authorizationName->length;
    header.length += 2 + authorizationData->length;
    XdmcpWriteHeader (&buffer, &header);
    XdmcpWriteCARD32 (&buffer, sessionID);
    XdmcpWriteARRAY8 (&buffer, authenticationName);
    XdmcpWriteARRAY8 (&buffer, authenticationData);
    XdmcpWriteARRAY8 (&buffer, authorizationName);
    XdmcpWriteARRAY8 (&buffer, authorizationData);
    XdmcpFlush (fd, &buffer, (XdmcpNetaddr) to, tolen);
}
   
static void
send_decline (
    struct sockaddr *to,
    int		    tolen,
    ARRAY8Ptr	    authenticationName,
    ARRAY8Ptr	    authenticationData,
    ARRAY8Ptr	    status,
    int	       	    fd)
{
    XdmcpHeader	header;

    Debug ("Decline %*.*s\n", status->length, status->length, pS(status->data));
    header.version = XDM_PROTOCOL_VERSION;
    header.opcode = (CARD16) DECLINE;
    header.length = 0;
    header.length += 2 + status->length;
    header.length += 2 + authenticationName->length;
    header.length += 2 + authenticationData->length;
    XdmcpWriteHeader (&buffer, &header);
    XdmcpWriteARRAY8 (&buffer, status);
    XdmcpWriteARRAY8 (&buffer, authenticationName);
    XdmcpWriteARRAY8 (&buffer, authenticationData);
    XdmcpFlush (fd, &buffer, (XdmcpNetaddr) to, tolen);
}

static void
manage (
    struct sockaddr *from,
    int		    fromlen,
    int		    length,
    int		    fd)
{
    CARD32		sessionID;
    CARD16		displayNumber;
    ARRAY8		displayClass;
    int			expectlen;
    struct protoDisplay	*pdpy;
    struct display	*d;
    char		*name = NULL;
    char		*class = NULL;
    XdmcpNetaddr	from_save;
    ARRAY8		clientAddress, clientPort;
    CARD16		connectionType;

    Debug ("Manage %d\n", length);
    displayClass.data = 0;
    displayClass.length = 0;
    if (XdmcpReadCARD32 (&buffer, &sessionID) &&
	XdmcpReadCARD16 (&buffer, &displayNumber) &&
	XdmcpReadARRAY8 (&buffer, &displayClass))
    {
	expectlen = 4 +				/* session ID */
		    2 +				/* displayNumber */
		    2 + displayClass.length;	/* displayClass */
	if (expectlen != length)
	{
	    Debug ("Manage length error got %d expect %d\n", length, expectlen);
	    goto abort;
	}
	pdpy = FindProtoDisplay ((XdmcpNetaddr) from, fromlen, displayNumber);
	Debug ("Manage Session ID %ld, pdpy %p\n", (long) sessionID, pdpy);
	if (!pdpy || pdpy->sessionID != sessionID)
	{
	    /*
	     * We may have already started a session for this display
	     * but it hasn't seen the response in the form of an
	     * XOpenDisplay() yet. So check if it is in the list of active
	     * displays, and if so check that the session id's match.
	     * If all this is true, then we have a duplicate request that
	     * can be ignored.
	     */
	    if (!pdpy 
		&& (d = FindDisplayByAddress((XdmcpNetaddr) from, fromlen, displayNumber))
		&& d->sessionID == sessionID) {
		     Debug("manage: got duplicate pkt, ignoring\n");
		     goto abort;
	    }
	    Debug ("Session ID %ld refused\n", (long) sessionID);
	    if (pdpy)
		Debug ("Existing Session ID %ld\n", (long) pdpy->sessionID);
	    send_refuse (from, fromlen, sessionID, fd);
	}
	else
	{
	    name = NetworkAddressToName (pdpy->connectionType,
					 &pdpy->connectionAddress,
					 from,
					 pdpy->displayNumber);
	    Debug ("Computed display name: %s\n", name);
	    if (!name)
	    {
		send_failed (from, fromlen, "(no name)", sessionID, 
		  "out of memory", fd);
		goto abort;
	    }
	    d = FindDisplayByName (name);
	    if (d)
	    {
		Debug ("Terminating active session for %s\n", d->name);
		StopDisplay (d);
	    }
	    class = malloc (displayClass.length + 1);
	    if (!class)
	    {
		send_failed (from, fromlen, name, sessionID, 
		  "out of memory", fd);
		goto abort;
	    }
	    if (displayClass.length)
	    {
		memmove( class, displayClass.data, displayClass.length);
		class[displayClass.length] = '\0';
	    }
	    else
	    {
		free ((char *) class);
		class = (char *) NULL;
	    }
	    from_save = (XdmcpNetaddr) malloc (fromlen);
	    if (!from_save)
	    {
		send_failed (from, fromlen, name, sessionID,
		  "out of memory", fd);
		goto abort;
	    }
	    memmove( from_save, from, fromlen);
	    d = NewDisplay (name, class);
	    if (!d)
	    {
		free ((char *) from_save);
		send_failed (from, fromlen, name, sessionID, 
		  "out of memory", fd);
		goto abort;
	    }
	    d->displayType.location = Foreign;
	    d->displayType.lifetime = Transient;
	    d->displayType.origin = FromXDMCP;
	    d->sessionID = pdpy->sessionID;
	    d->from = from_save;
	    d->fromlen = fromlen;
	    d->displayNumber = pdpy->displayNumber;
	    ClientAddress (from, &clientAddress, &clientPort, &connectionType);
	    d->useChooser = 0;
	    d->xdmcpFd = fd;
	    if (IsIndirectClient (&clientAddress, connectionType))
	    {
		Debug ("IsIndirectClient\n");
		ForgetIndirectClient (&clientAddress, connectionType);
		if (UseChooser (&clientAddress, connectionType))
		{
		    d->useChooser = 1;
		    Debug ("Use chooser for %s\n", d->name);
		}
	    }
	    d->clientAddr = clientAddress;
	    d->connectionType = connectionType;
	    XdmcpDisposeARRAY8 (&clientPort);
	    if (pdpy->fileAuthorization)
	    {
		d->authorizations = (Xauth **) malloc (sizeof (Xauth *));
		if (!d->authorizations)
		{
		    free ((char *) from_save);
		    free ((char *) d);
		    send_failed (from, fromlen, name, sessionID,
		      "out of memory", fd);
		    goto abort;
		}
		d->authorizations[0] = pdpy->fileAuthorization;
		d->authNum = 1;
		pdpy->fileAuthorization = 0;
	    }
	    DisposeProtoDisplay (pdpy);
	    Debug ("Starting display %s,%s\n", d->name, d->class);
	    StartDisplay (d);
	}
    }
abort:
    XdmcpDisposeARRAY8 (&displayClass);
    if (name) free ((char*) name);
    if (class) free ((char*) class);
}

void
SendFailed (
    struct display  *d,
    char	    *reason)
{
    Debug ("Display start failed, sending Failed\n");
    send_failed ((struct sockaddr *)(d->from), d->fromlen, d->name, 
      d->sessionID, reason, d->xdmcpFd);
}

static void
send_failed (
    struct sockaddr *from,
    int		    fromlen,
    char	    *name,
    CARD32	    sessionID,
    char	    *reason,
    int		    fd)
{
    static char	buf[256];
    XdmcpHeader	header;
    ARRAY8	status;

    sprintf (buf, "Session %ld failed for display %.100s: %.100s",
	     (long) sessionID, name, reason);
    Debug ("Send failed %ld %s\n", (long) sessionID, buf);
    status.length = strlen (buf);
    status.data = (CARD8Ptr) buf;
    header.version = XDM_PROTOCOL_VERSION;
    header.opcode = (CARD16) FAILED;
    header.length = 6 + status.length;
    XdmcpWriteHeader (&buffer, &header);
    XdmcpWriteCARD32 (&buffer, sessionID);
    XdmcpWriteARRAY8 (&buffer, &status);
    XdmcpFlush (fd, &buffer, (XdmcpNetaddr) from, fromlen);
}

static void
send_refuse (
    struct sockaddr *from,
    int		    fromlen,
    CARD32	    sessionID,
    int		    fd)
{
    XdmcpHeader	header;

    Debug ("Send refuse %ld\n", (long) sessionID);
    header.version = XDM_PROTOCOL_VERSION;
    header.opcode = (CARD16) REFUSE;
    header.length = 4;
    XdmcpWriteHeader (&buffer, &header);
    XdmcpWriteCARD32 (&buffer, sessionID);
    XdmcpFlush (fd, &buffer, (XdmcpNetaddr) from, fromlen);
}

static void
send_alive (
    struct sockaddr *from,
    int		    fromlen,
    int		    length,
    int		    fd)
{
    CARD32		sessionID;
    CARD16		displayNumber;
    struct display	*d;
    XdmcpHeader		header;
    CARD8		sendRunning;
    CARD32		sendSessionID;

    Debug ("Send alive\n");
    if (XdmcpReadCARD16 (&buffer, &displayNumber) &&
	XdmcpReadCARD32 (&buffer, &sessionID))
    {
	if (length == 6)
	{
	    d = FindDisplayBySessionID (sessionID);
	    if (!d) {
		d = FindDisplayByAddress ((XdmcpNetaddr) from, fromlen, displayNumber);
	    }
	    sendRunning = 0;
	    sendSessionID = 0;
	    if (d && d->status == running)
 	    {
		if (d->sessionID == sessionID)
		    sendRunning = 1;
		sendSessionID = d->sessionID;
	    }
	    header.version = XDM_PROTOCOL_VERSION;
	    header.opcode = (CARD16) ALIVE;
	    header.length = 5;
	    Debug ("alive: %d %ld\n", sendRunning, (long) sendSessionID);
	    XdmcpWriteHeader (&buffer, &header);
	    XdmcpWriteCARD8 (&buffer, sendRunning);
	    XdmcpWriteCARD32 (&buffer, sendSessionID);
	    XdmcpFlush (fd, &buffer, (XdmcpNetaddr) from, fromlen);
	}
    }
}

char *
NetworkAddressToHostname (
    CARD16	connectionType,
    ARRAY8Ptr   connectionAddress)
{
    char    *name = 0;

    switch (connectionType)
    {
    case FamilyInternet:
#if defined(IPv6) && defined(AF_INET6)
    case FamilyInternet6:
#endif
	{
	    struct hostent	*hostent = NULL;
#if defined(IPv6) && defined(AF_INET6)
	    char dotted[INET6_ADDRSTRLEN];
#else
	    char dotted[20];
#endif
	    char *local_name = "";
	    int af_type;

#if defined(IPv6) && defined(AF_INET6)
	    if (connectionType == FamilyInternet6)
		af_type = AF_INET6;
	    else
#endif
		af_type = AF_INET;

	    hostent = gethostbyaddr ((char *)connectionAddress->data,
				      connectionAddress->length, af_type);

	    if (hostent) {
		/* check for DNS spoofing */
#if defined(IPv6) && defined(AF_INET6)
		struct addrinfo	*ai = NULL, *nai;
		if (getaddrinfo(hostent->h_name, NULL, NULL, &ai) == 0) {
		    for (nai = ai; nai != NULL; nai = nai->ai_next) {
			if ((af_type == nai->ai_family) &&
			  (connectionAddress->length == nai->ai_addrlen) &&
			  (memcmp(connectionAddress->data,nai->ai_addr,
			    nai->ai_addrlen) != 0) ) {
			    break;
			}
		    }
		    if (ai == NULL) {
			inet_ntop(af_type, connectionAddress->data, 
			  dotted, sizeof(dotted));

			LogError("Possible DNS spoof attempt %s->%s.", dotted,
			  hostent->h_name);
			hostent = NULL;
		    }
		    freeaddrinfo(ai);
		} else {
		    hostent = NULL;
		}
#else
		char *s = strdup(hostent->h_name); /* fscking non-reentrancy of getXXX() */
		if ((hostent = gethostbyname(s))) {
			if (memcmp((char*)connectionAddress->data, hostent->h_addr,
			    hostent->h_length) != 0) {
				LogError("Possible DNS spoof attempt.");
				hostent = NULL; /* so it enters next if() */
			} else {
				local_name = hostent->h_name;
			}
		}
		free(s);
#endif
	    }

	    if (!hostent) {
		/* can't get name, so use emergency fallback */
#if defined(IPv6) && defined(AF_INET6)
		inet_ntop(af_type, connectionAddress->data, 
		  	  dotted, sizeof(dotted));
#else
		sprintf(dotted, "%d.%d.%d.%d",
			connectionAddress->data[0],
			connectionAddress->data[1],
			connectionAddress->data[2],
			connectionAddress->data[3]);
#endif
		local_name = dotted;
		LogError ("Cannot convert Internet address %s to host name\n",
			  dotted);
	    }
	    if (!getString (name, strlen (local_name)))
		break;
	    strcpy (name, local_name);
	    break;
	}
#ifdef DNET
    case FamilyDECnet:
	break;
#endif /* DNET */
    default:
	break;
    }
    return name;
}

#if 0
static int
HostnameToNetworkAddress (
char	    *name,
CARD16	    connectionType,
ARRAY8Ptr   connectionAddress)
{
    switch (connectionType)
    {
    case FamilyInternet:
	{
	    struct hostent	*hostent;

	    hostent = gethostbyname (name);
	    if (!hostent)
		return FALSE;
	    if (!XdmcpAllocARRAY8 (connectionAddress, hostent->h_length))
		return FALSE;
	    memmove( connectionAddress->data, hostent->h_addr, hostent->h_length);
	    return TRUE;
	}
#ifdef DNET
    case FamilyDECnet:
	return FALSE;
#endif
    }
    return FALSE;
}

/*
 * converts a display name into a network address, using
 * the same rules as XOpenDisplay (algorithm cribbed from there)
 */
static int
NameToNetworkAddress(
char	    *name,
CARD16Ptr   connectionTypep,
ARRAY8Ptr   connectionAddress,
CARD16Ptr   displayNumber)
{
    char    *colon, *display_number;
    char    hostname[1024];
    int	    dnet = FALSE;
    CARD16  number;
    CARD16  connectionType;

    colon = strchr(name, ':');
    if (!colon)
	return FALSE;
    if (colon != name)
    {
	if (colon - name > sizeof (hostname))
	    return FALSE;
	strncpy (hostname, name, colon - name);
	hostname[colon - name] = '\0';
    }
    else
    {
	strcpy (hostname, localHostname ());
    }
    if (colon[1] == ':')
    {
	dnet = TRUE;
	colon++;
    }
#ifndef DNETCONN
    if (dnet)
	return FALSE;
#endif
    display_number = colon + 1;
    while (*display_number && *display_number != '.')
    {
	if (!isascii (*display_number) || !isdigit(*display_number))
	    return FALSE;
    }
    if (display_number == colon + 1)
	return FALSE;
    number = atoi (colon + 1);
#ifdef DNETCONN
    if (dnet)
	connectionType = FamilyDECnet;
    else
#endif
	connectionType = FamilyInternet;
    if (!HostnameToNetworkAddress (hostname, connectionType, connectionAddress))
	return FALSE;
    *displayNumber = number;
    *connectionTypep = connectionType;
    return TRUE;
}
#endif

#endif /* XDMCP */
