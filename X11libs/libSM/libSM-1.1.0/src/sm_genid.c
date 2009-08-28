/*

Copyright 1993, 1998  The Open Group
Copyright 2002 Sun Microsystems, Inc.

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

*/

/*
 * Author: Ralph Mor, X Consortium
 */

#ifdef WIN32
#define _WILLWINSOCK_
#endif
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <X11/SM/SMlib.h>
#include "SMlibint.h"
#ifdef XTHREADS
#include <X11/Xthreads.h>
#endif
#include <stdio.h>

#include <time.h>
#define Time_t time_t

#ifndef WIN32

#if defined(TCPCONN) || defined(STREAMSCONN)
#ifndef Lynx
#include <sys/socket.h>
#else
#include <socket.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#define XOS_USE_NO_LOCKING
#define X_INCLUDE_NETDB_H
#include <X11/Xos_r.h>
#endif

#else /* WIN32 */

#include <X11/Xwinsock.h>
#include <X11/Xw32defs.h>
#define X_INCLUDE_NETDB_H
#define XOS_USE_MTSAFE_NETDBAPI
#include <X11/Xos_r.h>

#endif /* WIN32 */

#ifdef MNX_TCPCONN
#include <net/gen/netdb.h>

#define TCPCONN
#endif

#if defined(HAVE_LIBUUID)
#include <uuid/uuid.h>
#endif


char *
SmsGenerateClientID (smsConn)
    SmsConn smsConn;
{
#if defined(HAVE_LIBUUID)
    char *id;
    char temp[256];
    uuid_t uuid;

    uuid_generate(uuid);

    temp[0] = '2';
    temp[1] = '\0';
    uuid_unparse_lower(uuid, &temp[1]);

    if ((id = malloc (strlen (temp) + 1)) != NULL)
	strcpy (id, temp);

    return id;
#else
#if defined(TCPCONN) || defined(STREAMSCONN)
    static const char hex[] = "0123456789abcdef";
    char hostname[256];
    char address[64], *addr_ptr = address;
    char temp[256];
    char *id;
    static int sequence = 0;

    if (gethostname (hostname, sizeof (hostname)))
	return (NULL);

    {
    char* inet_addr;
    char temp[4], *ptr1, *ptr2;
    unsigned char decimal[4];
    int i, len;
    struct in_addr *haddr = NULL;
#if defined(IPv6) && defined(AF_INET6)
    struct addrinfo *ai, *first_ai;
    if (getaddrinfo(hostname,NULL,NULL,&ai) != 0)
	return NULL;

    for (first_ai = ai; ai != NULL; ai = ai->ai_next) {
	if ( (ai->ai_family == AF_INET) || (ai->ai_family == AF_INET6) ) 
	    break;
    }
    if (ai == NULL) {
	freeaddrinfo(first_ai);
	return NULL;
    } 

    if (ai->ai_family == AF_INET6) {
	unsigned char *cp = (unsigned char *) &((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr.s6_addr;
	
	*addr_ptr++ = '6';	/* IPv6 address code */

	for (i = 0 ; i < 16 ; i++) {
            *addr_ptr++ = hex[cp[i] >> 4];
            *addr_ptr++ = hex[cp[i] & 0x0f];
	}

        *addr_ptr++ = '\0';

    } else { /* Fall through to IPv4 address handling */
	haddr = &((struct sockaddr_in *)ai->ai_addr)->sin_addr;
#else
#ifdef XTHREADS_NEEDS_BYNAMEPARAMS
    _Xgethostbynameparams hparams;
#endif
    struct hostent *hostp;

    if ((hostp = _XGethostbyname (hostname,hparams)) != NULL)
	haddr = (struct in_addr *)(hostp->h_addr);
    else
	return NULL;
#endif

    inet_addr = inet_ntoa (*haddr);
    for (i = 0, ptr1 = inet_addr; i < 3; i++)
    {
	ptr2 = strchr (ptr1, '.');
	len = ptr2 - ptr1;
	if (!ptr2 || len > 3) {
#if defined(IPv6) && defined(AF_INET6)
	    freeaddrinfo(first_ai);
#endif
	    return (NULL);
	}
	strncpy (temp, ptr1, len);
	temp[len] = '\0';
	decimal[i] = atoi (temp);
	ptr1 = ptr2 + 1;
    }

    decimal[3] = atoi (ptr1);

    *addr_ptr++ = '1';

    for (i = 0; i < 4; i++) {
        *addr_ptr++ = hex[decimal[i] >> 4];
        *addr_ptr++ = hex[decimal[i] & 0x0f];
    }

    *addr_ptr++ = '\0';

#if defined(IPv6) && defined(AF_INET6)
    }
    freeaddrinfo(first_ai);
#endif
    }

    sprintf (temp, "1%s%.13ld%.10ld%.4d", address, (long)time((Time_t*)0),
	     (long)getpid(), sequence);

    if (++sequence > 9999)
	sequence = 0;

    if ((id = malloc (strlen (temp) + 1)) != NULL)
	strcpy (id, temp);

    return (id);
#else
    return (NULL);
#endif
#endif
}
