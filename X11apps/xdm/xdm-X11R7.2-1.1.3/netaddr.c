/* $Xorg: netaddr.c,v 1.4 2001/02/09 02:05:40 xorgcvs Exp $ */
/*

Copyright 1991, 1998  The Open Group

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
/* $XFree86: xc/programs/xdm/netaddr.c,v 3.7 2003/07/09 15:27:39 tsi Exp $ */

/*
 * xdm - X display manager
 *
 * netaddr.c - Interpretation of XdmcpNetaddr object.
 */

#include "dm.h"
#include "dm_error.h"

#include <X11/X.h>		/* FamilyInternet, etc. */

#ifdef XDMCP

#include "dm_socket.h"

#ifdef UNIXCONN
#ifndef X_NO_SYS_UN
#ifndef Lynx
#include <sys/un.h>		/* struct sockaddr_un */
#else
#include <un.h>			/* struct sockaddr_un */
#endif
#endif
#endif
#ifdef DNETCONN
#include <netdnet/dn.h>		/* struct sockaddr_dn */
#endif

/* given an XdmcpNetaddr, returns the socket protocol family used,
   e.g., AF_INET */

int NetaddrFamily(XdmcpNetaddr netaddrp)
{
#ifdef STREAMSCONN
    short family = *(short *)netaddrp;
    return family;
#else
    return ((struct sockaddr *)netaddrp)->sa_family;
#endif
}


/* given an XdmcpNetaddr, returns a pointer to the TCP/UDP port used
   and sets *lenp to the length of the address
   or 0 if not using TCP or UDP. */

char * NetaddrPort(XdmcpNetaddr netaddrp, int *lenp)
{
#ifdef STREAMSCONN
    *lenp = 2;
    return netaddrp+2;
#else
    switch (NetaddrFamily(netaddrp))
    {
    case AF_INET:
	*lenp = 2;
	return (char *)&(((struct sockaddr_in *)netaddrp)->sin_port);
#if defined(IPv6) && defined(AF_INET6)
    case AF_INET6:
	*lenp = 2;
	return (char *)&(((struct sockaddr_in6 *)netaddrp)->sin6_port);
#endif
    default:
	*lenp = 0;
	return NULL;
    }
#endif
}


/* given an XdmcpNetaddr, returns a pointer to the network address
   and sets *lenp to the length of the address */

char * NetaddrAddress(XdmcpNetaddr netaddrp, int *lenp)
{
#ifdef STREAMSCONN
    *lenp = 4;
    return netaddrp+4;
#else
    switch (NetaddrFamily(netaddrp)) {
#ifdef UNIXCONN
    case AF_UNIX:
	*lenp = strlen(((struct sockaddr_un *)netaddrp)->sun_path);
        return (char *) (((struct sockaddr_un *)netaddrp)->sun_path);
#endif
#ifdef TCPCONN
    case AF_INET:
        *lenp = sizeof (struct in_addr);
        return (char *) &(((struct sockaddr_in *)netaddrp)->sin_addr);
#if defined(IPv6) && defined(AF_INET6)
    case AF_INET6:
    {
	struct in6_addr *a = &(((struct sockaddr_in6 *)netaddrp)->sin6_addr);
	if (IN6_IS_ADDR_V4MAPPED(a)) {
	    *lenp = sizeof (struct in_addr);
	    return ((char *) &(a->s6_addr))+12;
	} else {
	    *lenp = sizeof (struct in6_addr);
	    return (char *) &(a->s6_addr);
	}
    }
#endif
#endif
#ifdef DNETCONN
    case AF_DECnet:
        *lenp = sizeof (struct dn_naddr);
        return (char *) &(((struct sockaddr_dn *)netaddrp)->sdn_add);
#endif
#ifdef AF_CHAOS
    case AF_CHAOS:
#endif
    default:
	*lenp = 0;
	return NULL;
    }
#endif /* STREAMSCONN else */
}


/* given an XdmcpNetaddr, sets *addr to the network address used and
   sets *len to the number of bytes in addr.
   Returns the X protocol family used, e.g., FamilyInternet */

int ConvertAddr (XdmcpNetaddr saddr, int *len, char **addr)
{
    int retval;

    if ((len == NULL) || (saddr == NULL))
        return -1;
    *addr = NetaddrAddress(saddr, len);
#ifdef STREAMSCONN
    /* kludge */
    if (NetaddrFamily(saddr) == 2)
	retval = FamilyInternet;
#else
    switch (NetaddrFamily(saddr))
    {
#ifdef AF_UNSPEC
      case AF_UNSPEC:
	retval = FamilyLocal;
	break;
#endif
#ifdef AF_UNIX
#ifndef hpux
      case AF_UNIX:
        retval = FamilyLocal;
	break;
#endif
#endif
#ifdef TCPCONN
      case AF_INET:
        retval = FamilyInternet;
	break;
#if defined(IPv6) && defined(AF_INET6)
      case AF_INET6:
	if (*len == sizeof(struct in_addr)) 
	    retval = FamilyInternet;
	else  
	    retval = FamilyInternet6;
	break;
#endif
#endif
#ifdef DNETCONN
      case AF_DECnet:
        retval = FamilyDECnet;
	break;
#endif
#ifdef AF_CHAOS
    case AF_CHAOS:
	retval = FamilyChaos;
	break;
#endif
      default:
	retval = -1;
        break;
    }
#endif /* STREAMSCONN else */
    Debug ("ConvertAddr returning %d for family %d\n", retval,
	   NetaddrFamily(saddr));
    return retval;
}

int
addressEqual (XdmcpNetaddr a1, int len1, XdmcpNetaddr a2, int len2)
{
    int partlen1, partlen2;
    char *part1, *part2;

    if (len1 != len2)
    {
	return FALSE;
    }
    if (NetaddrFamily(a1) != NetaddrFamily(a2))
    {
	return FALSE;
    }
    part1 = NetaddrPort(a1, &partlen1);
    part2 = NetaddrPort(a2, &partlen2);
    if (partlen1 != partlen2 || memcmp(part1, part2, partlen1) != 0)
    {
	return FALSE;
    }
    part1 = NetaddrAddress(a1, &partlen1);
    part2 = NetaddrAddress(a2, &partlen2);
    if (partlen1 != partlen2 || memcmp(part1, part2, partlen1) != 0)
    {
	return FALSE;
    }
    return TRUE;
}

#ifdef DEBUG
/*ARGSUSED*/
void
PrintSockAddr (struct sockaddr *a, int len)
{
    unsigned char    *t, *p;

    Debug ("family %d, ", a->sa_family);
    switch (a->sa_family) {
#ifdef AF_INET
    case AF_INET:

	p = (unsigned char *) &((struct sockaddr_in *) a)->sin_port;
	t = (unsigned char *) &((struct sockaddr_in *) a)->sin_addr;

	Debug ("port %d, host %d.%d.%d.%d\n",
		(p[0] << 8) + p[1], t[0], t[1], t[2], t[3]);
	break;
#endif
#if defined(IPv6) && defined(AF_INET6)
    case AF_INET6:
    {
	char astr[INET6_ADDRSTRLEN] = "";
	
	inet_ntop(a->sa_family, &((struct sockaddr_in6 *) a)->sin6_addr,
	  astr, sizeof(astr));
	p = (unsigned char *) &((struct sockaddr_in6 *) a)->sin6_port;
		
	Debug ("port %d, host %s\n", (p[0] << 8) + p[1], astr);
	break;
    }
#endif
    }
}
#endif

#endif /* XDMCP */
