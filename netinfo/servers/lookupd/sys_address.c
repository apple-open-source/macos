/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <NetInfo/config.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <libc.h>
#include <unistd.h>

unsigned long sys_address(void)
{
	struct ifconf ifc;
	struct ifreq *ifr;
	char buf[1024]; /* XXX */
	int offset, addrlen, extra, delta;
	int sock;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) return (htonl(INADDR_LOOPBACK));

	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;

	if (ioctl(sock, SIOCGIFCONF, (char *)&ifc) < 0)
	{
		close(sock);
		return (htonl(INADDR_LOOPBACK));
	}

	delta = sizeof(struct ifreq);
	addrlen = delta - IFNAMSIZ;
	extra = 0;

	offset = 0;

	while (offset <= ifc.ifc_len)
	{
		ifr = (struct ifreq *)(ifc.ifc_buf + offset);

#ifndef _NO_SOCKADDR_LENGTH_
		extra = ifr->ifr_addr.sa_len - addrlen;
		if (extra < 0) extra = 0;
#endif

		offset = offset + delta + extra;

		if (ifr->ifr_addr.sa_family != AF_INET) continue;
		if (ioctl(sock, SIOCGIFFLAGS, ifr) < 0) continue;

		if ((ifr->ifr_flags & IFF_UP) && (!(ifr->ifr_flags & IFF_LOOPBACK)))
		{
			close(sock);
			return (((struct sockaddr_in *)&(ifr->ifr_addr))->sin_addr.s_addr);
		}
		
	}

	close(sock);
	return (htonl(INADDR_LOOPBACK));
}

