/* $Xorg: socket.c,v 1.4 2001/02/09 02:05:40 xorgcvs Exp $ */
/*

Copyright 1988, 1998  The Open Group
Copyright 2002 Sun Microsystems, Inc.  All rights reserved.

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
IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of a copyright holder shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the copyright holder.

*/
/* $XFree86: xc/programs/xdm/socket.c,v 3.14 2003/11/25 22:21:08 herrb Exp $ */

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
#include <arpa/inet.h>

extern int	chooserFd;

extern FD_TYPE	WellKnownSocketsMask;
extern int	WellKnownSocketsMax;

void
CreateWellKnownSockets (void)
{
    char *name = localHostname ();
    registerHostname (name, strlen (name));

#if defined(IPv6) && defined(AF_INET6)
    chooserFd = socket (AF_INET6, SOCK_STREAM, 0);
#else
    chooserFd = socket (AF_INET, SOCK_STREAM, 0);
#endif
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
#if defined(IPv6) && defined(AF_INET6)
    struct sockaddr_storage in_addr;
#else
    struct sockaddr_in	in_addr;
#endif
    int			len;
    int			retval = 0;

    len = sizeof in_addr;
    if (chooserFd < 0) 
	return -1;	/* TODO check other listening sockets */
    if (getsockname (chooserFd, (struct sockaddr *)&in_addr, (void *)&len) < 0)
	return -1;
#if defined(IPv6) && defined(AF_INET6)
    if (((struct sockaddr *)&in_addr)->sa_family == AF_INET6)
	Debug ("Chooser socket port: %d (IPv6)\n", 
	  ntohs(((struct sockaddr_in6 *) &in_addr)->sin6_port));
    else
#endif
	Debug ("Chooser socket port: %d\n", 
	  ntohs(((struct sockaddr_in *) &in_addr)->sin_port));
    if (*lenp < len)  
	retval = -2;
    else
	memmove( addr, (char *) &in_addr, len);
    *lenp = len;

    return retval;
}

static int
CreateListeningSocket (struct sockaddr *sock_addr, int salen)
{
    int fd;
    const char *addrstring = "unknown";
#if defined(IPv6) && defined(AF_INET6)
    char addrbuf[INET6_ADDRSTRLEN];
#endif

    if (request_port == 0)
	    return -1;

    if (debugLevel > 0) {
#if defined(IPv6) && defined(AF_INET6)
	void *ipaddr;
	if (sock_addr->sa_family == AF_INET6) {
	    ipaddr = & ((struct sockaddr_in6 *) sock_addr)->sin6_addr;
	} else {
	    ipaddr = & ((struct sockaddr_in *) sock_addr)->sin_addr;
	}
	addrstring =
	  inet_ntop(sock_addr->sa_family, ipaddr, addrbuf, sizeof(addrbuf));

#else
	addrstring = inet_ntoa(((struct sockaddr_in *) sock_addr)->sin_addr);
#endif

	Debug ("creating socket to listen on port %d of address %s\n", 
	  request_port,addrstring);
    }

    fd = socket (sock_addr->sa_family, SOCK_DGRAM, 0);

    if (fd == -1) {
	LogError ("XDMCP socket creation failed, errno %d\n", errno);
	return fd;
    }
    RegisterCloseOnFork (fd);

    if (bind (fd, sock_addr, salen) == -1)
    {
	LogError ("error %d binding socket address %d\n", errno, request_port);
	close (fd);
	fd = -1;
	return fd;
    }
    if (fd > WellKnownSocketsMax)
	WellKnownSocketsMax = fd;
    FD_SET (fd, &WellKnownSocketsMask);
    return fd;
}

struct socklist {
    struct socklist *	next;
    struct socklist *	mcastgroups;
    struct sockaddr *	addr;
    int			salen;
    int			addrlen;
    int			fd;
    int			ref;	/* referenced bit - see UpdateListenSockets */
};

static struct socklist *listensocks;

static void
DestroyListeningSocket (struct socklist *s)
{
    if (s->fd >= 0) {
	FD_CLR (s->fd, &WellKnownSocketsMask);
	close(s->fd);
	s->fd = -1;
    }
    if (s->addr) {
	free(s->addr);
	s->addr = NULL;
    }
    if (s->mcastgroups) {
	struct socklist *g, *n;

	for (g = s->mcastgroups; g != NULL; g = n) {
	    n = g->next;
	    if (g->addr)
		free(g->addr);
	    free(g);
	}
	s->mcastgroups = NULL;
    }
}

static struct socklist*
FindInList(struct socklist *list, ARRAY8Ptr addr)
{
    struct socklist *s;

    for (s = list; s != NULL; s = s->next) {
	if (s->addrlen == addr->length) {
	    char *addrdata;

	    switch (s->addr->sa_family) {
	    case AF_INET:
		addrdata = (char *)
		  &(((struct sockaddr_in *)s->addr)->sin_addr.s_addr);
		break;
#if defined(IPv6) && defined(AF_INET6)
	    case AF_INET6:
		addrdata = (char *)
		  &(((struct sockaddr_in6 *)s->addr)->sin6_addr.s6_addr);
		break;
#endif
	    default:
		/* Unrecognized address family */
		continue;
	    }	
	    if (memcmp(addrdata, addr->data, addr->length) == 0) {
		return s;
	    }
	}
    }
    return NULL;
}

static struct socklist *
CreateSocklistEntry(ARRAY8Ptr addr)
{
    struct socklist *s = malloc (sizeof(struct socklist));
    if (s == NULL) 
	LogOutOfMem("CreateSocklistEntry");

    bzero(s, sizeof(struct socklist));

    if (addr->length == 4) /* IPv4 */ 
    {
	struct sockaddr_in *sin;
	sin = malloc (sizeof(struct sockaddr_in));
	if (sin == NULL) 
	    LogOutOfMem("CreateSocklistEntry");
	s->addr = (struct sockaddr *) sin;

	bzero (sin, sizeof (struct sockaddr_in));
#ifdef BSD44SOCKETS
	sin->sin_len = sizeof(struct sockaddr_in);
#endif
	s->salen = sizeof(struct sockaddr_in);
	s->addrlen = sizeof(struct in_addr);
	sin->sin_family = AF_INET;
	sin->sin_port = htons ((short) request_port);
	memcpy(&sin->sin_addr, addr->data, addr->length);
    }
#if defined(IPv6) && defined(AF_INET6)
    else if (addr->length == 16) /* IPv6 */
    {
	struct sockaddr_in6 *sin6;
	sin6 = malloc (sizeof(struct sockaddr_in6));
	if (sin6 == NULL) 
	    LogOutOfMem("CreateSocklistEntry");
	s->addr = (struct sockaddr *) sin6;

	bzero (sin6, sizeof (struct sockaddr_in6));
#ifdef SIN6_LEN
	sin6->sin6_len = sizeof(struct sockaddr_in6);
#endif
	s->salen = sizeof(struct sockaddr_in6);
	s->addrlen = sizeof(struct in6_addr);
	sin6->sin6_family = AF_INET6;
	sin6->sin6_port = htons ((short) request_port);
	memcpy(&sin6->sin6_addr, addr->data, addr->length);
    } 
#endif
    else {
	/* Unknown address type */
	free(s);
	s = NULL;
    }
    return s;
}

static void 
UpdateListener(ARRAY8Ptr addr, void **closure)
{
    struct socklist *s;
    ARRAY8 tmpaddr;

    *closure = NULL;

    if (addr == NULL || addr->length == 0) {
#if defined(IPv6) && defined(AF_INET6)
	struct in6_addr in6 = in6addr_any;
	tmpaddr.length = sizeof(in6);
	tmpaddr.data = (CARD8Ptr) &in6;
#else
	struct in_addr in;
	in.s_addr = htonl (INADDR_ANY);
	tmpaddr.length = sizeof(in);
	tmpaddr.data = (CARD8Ptr) &in;
#endif
	addr = &tmpaddr;
    }
    
    s = FindInList(listensocks, addr);

    if (s) {
	*closure = (void *) s;
	s->ref = 1;
	return;
    }
    
    s = CreateSocklistEntry(addr);

    if (s == NULL)
	return;

    s->fd = CreateListeningSocket(s->addr, s->salen);
    if (s->fd < 0) {
	free(s->addr);
	free(s);
	return;
    }
    s->ref = 1;
    s->next = listensocks;
    listensocks = s;
    *closure = (void *) s;
}

#define JOIN_MCAST_GROUP 0
#define LEAVE_MCAST_GROUP 1

static void
ChangeMcastMembership(struct socklist *s, struct socklist *g, int op)
{
    int sockopt;

    switch (s->addr->sa_family) 
    {
        case AF_INET:
        {
	    struct ip_mreq mreq;
	    memcpy(&mreq.imr_multiaddr, 
	      &((struct sockaddr_in *) g->addr)->sin_addr, 
	      sizeof(struct in_addr));
	    memcpy(&mreq.imr_interface,
	      &((struct sockaddr_in *) s->addr)->sin_addr, 
	      sizeof(struct in_addr));
	    if (op == JOIN_MCAST_GROUP) {
		sockopt = IP_ADD_MEMBERSHIP;
	    } else {
		sockopt = IP_DROP_MEMBERSHIP;
	    }
	    if (setsockopt(s->fd, IPPROTO_IP, sockopt,
	      &mreq, sizeof(mreq)) < 0) {
		LogError ("XDMCP socket multicast %s to %s failed, errno %d\n",
		  (op == JOIN_MCAST_GROUP) ? "join" : "drop",
		  inet_ntoa(((struct sockaddr_in *) g->addr)->sin_addr),
		  errno);
		return;
	    } else if (debugLevel > 0) {
		Debug ("XDMCP socket multicast %s to %s succeeded\n", 
		  (op == JOIN_MCAST_GROUP) ? "join" : "drop",
		  inet_ntoa(((struct sockaddr_in *) g->addr)->sin_addr));
	    }
	}
#if defined(IPv6) && defined(AF_INET6)
#ifndef IPV6_JOIN_GROUP
#define IPV6_JOIN_GROUP IPV6_ADD_MEMBERSHIP 
#endif
#ifndef IPV6_LEAVE_GROUP
#define IPV6_LEAVE_GROUP IPV6_DROP_MEMBERSHIP
#endif
	case AF_INET6:
	{
	    struct ipv6_mreq mreq6;
	    memcpy(&mreq6.ipv6mr_multiaddr, 
	      &((struct sockaddr_in6 *) g->addr)->sin6_addr, 
	      sizeof(struct in6_addr));
	    mreq6.ipv6mr_interface = 0;  /* TODO: fix this */
	    if (op == JOIN_MCAST_GROUP) {
		sockopt = IPV6_JOIN_GROUP;
	    } else {
		sockopt = IPV6_LEAVE_GROUP;
	    }
	    if (setsockopt(s->fd, IPPROTO_IPV6, sockopt,
	      &mreq6, sizeof(mreq6)) < 0) {
		int saveerr = errno;
		char addrbuf[INET6_ADDRSTRLEN];

		inet_ntop(s->addr->sa_family, 
		  &((struct sockaddr_in6 *) g->addr)->sin6_addr,
		  addrbuf, sizeof(addrbuf));

		LogError ("XDMCP socket multicast %s to %s failed, errno %d\n",
		  (op == JOIN_MCAST_GROUP) ? "join" : "drop", addrbuf,
		  saveerr);
		return;
	    } else if (debugLevel > 0) {
		char addrbuf[INET6_ADDRSTRLEN];

		inet_ntop(s->addr->sa_family, 
		  &((struct sockaddr_in6 *) g->addr)->sin6_addr,
		  addrbuf, sizeof(addrbuf));

		Debug ("XDMCP socket multicast %s to %s succeeded\n", 
		  (op == JOIN_MCAST_GROUP) ? "join" : "drop", addrbuf);
	    }
	}
#endif
    }
}

static void 
UpdateMcastGroup(ARRAY8Ptr addr, void **closure)
{
    struct socklist *s = (struct socklist *) *closure;
    struct socklist *g;

    if (s == NULL) 
	    return;

    g = FindInList(s->mcastgroups, addr);

    if (g) { /* Already in the group, mark & continue */
	g->ref = 1;
	return;
    }

    /* Need to join the group */
    g = CreateSocklistEntry(addr);
    if (g == NULL)
	return;

    ChangeMcastMembership(s, g, JOIN_MCAST_GROUP);
}

/* Open or close listening sockets to match the current settings read in
   from the access database. */
void UpdateListenSockets (void)
{
    struct socklist *s, *g, **ls, **lg, *ns, *ng;
    void *tmpPtr = NULL;

    /* Clear Ref bits - any not marked by UpdateCallback will be closed */
    for (s = listensocks; s != NULL; s = s->next) {
	s->ref = 0;
	for (g = s->mcastgroups; g != NULL ; g = g->next) {
	    g->ref = 0;
	}
    }
    ForEachListenAddr(UpdateListener, UpdateMcastGroup, &tmpPtr);
    for (s = listensocks, ls = &listensocks; s != NULL; s = ns) {
	ns = s->next;
	if (s->ref == 0) {
	    DestroyListeningSocket(s);
	    *ls = s->next;
	    free(s);
	} else {
	    ls = &(s->next);
	    for (lg = &s->mcastgroups, g = *lg; g != NULL ; g = ng) {
		ng = g->next;
		if (g->ref == 0) {
		    ChangeMcastMembership(s,g,LEAVE_MCAST_GROUP);
		    *lg = g->next;
		    free(g);
		} else {
		    lg = &(g->next);
		}
	    }
	}
    }    
}

/* Close all additional listening sockets beyond the basic chooserFd and
   remove them from the WellKnownSocketsMask. */
void CloseListenSockets (void)
{
    struct socklist *s, *n;

    for (s = listensocks; s != NULL; s = n) {
	n = s->next;
	DestroyListeningSocket(s);
	free(s);
    }
    listensocks = NULL;
}

/* For each listening socket identified in readmask, process the incoming
   XDMCP request */
void ProcessListenSockets (fd_set *readmask)
{
    struct socklist *s;

    for (s = listensocks; s != NULL; s = s->next) {
	if (FD_ISSET(s->fd, readmask))
	    ProcessRequestSocket(s->fd);
    }    
}

#endif /* !STREAMSCONN */
#endif /* XDMCP */
