/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * 	selfcheck.c
 *      Copyright (c) 1999 Sun Microsystems Inc.
 *      All Rights Reserved.
 */

/*
 * Portions Copyright 2007-2009 Apple Inc.
 */

#pragma ident	"@(#)selfcheck.c	1.3	05/06/08 SMI"

#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

#include <strings.h>
#include <stdio.h>
#include <netdb.h>

#include "autofs.h"
#include "automount.h"

static int self_check_af(char *, struct ifaddrs *, int);

int
self_check(hostname)
	char *hostname;
{
	int res;
	struct ifaddrs *ifaddrs;

	/*
	 * Check whether the host has a name that begins with ".";
	 * if so, it's not a valid host name, and we return ENOENT.
	 * That way, we avoid doing host name lookups for various
	 * dot-file names, e.g. ".localized" and ".DS_Store".
	 *
	 * We want to avoid this so that we don't pass the name on to
	 * getipnodebyname() and thus on to mDNSResponder, which will
	 * whine about being handed a name beginning with ".".  (This
	 * filtering *belongs* in lower-level routines such as
	 * getipnodebyname(), gethostbyname(), etc. or routines
	 * they call, so that we return errors for host names beginning
	 * with "." before sending them to anybody else, so that even
	 * "ping .localized" won't provoke mDNSResponder to whine, but
	 * it's not being done there, alas, so we have to do it ourselves.)
	 */
	if (hostname[0] == '.') {
		/*
		 * This cannot possibly be any host, much less us.
		 */
		return (0);
	}

	if (getifaddrs(&ifaddrs) == -1) {
		syslog(LOG_ERR, "getifaddrs failed:  %s\n",
		    strerror(errno));
		return (0);
	}
	res = self_check_af(hostname, ifaddrs, AF_INET6) ||
	    self_check_af(hostname, ifaddrs, AF_INET);
	freeifaddrs(ifaddrs);
	return (res);
}

static int
self_check_af(char *hostname, struct ifaddrs *ifaddrs, int family)
{
	struct hostent *hostinfo;
	int error_num;
	char **hostptr;
	struct ifaddrs *ifaddr;
	struct sockaddr *addr;
	struct sockaddr_in *addr_in;
	struct sockaddr_in6 *addr_in6;

	if ((hostinfo = getipnodebyname(hostname, family, AI_DEFAULT,
	    &error_num)) == NULL) {
		if (error_num == TRY_AGAIN)
			syslog(LOG_DEBUG,
			    "self_check: unknown host: %s (try again later)\n",
			    hostname);
		else
			syslog(LOG_DEBUG,
			    "self_check: unknown host: %s\n", hostname);

		return (0);
	}

	for (hostptr = hostinfo->h_addr_list; *hostptr; hostptr++) {
		for (ifaddr = ifaddrs; ifaddr != NULL; ifaddr = ifaddr->ifa_next) {
			addr = ifaddr->ifa_addr;
			if (addr->sa_family != hostinfo->h_addrtype)
				continue;
			switch (addr->sa_family) {

			case AF_INET:
				addr_in = (struct sockaddr_in *)addr;
				if (memcmp(*hostptr, &addr_in->sin_addr,
				    hostinfo->h_length) == 0) {
					freehostent(hostinfo);
					return (1);
				}
				break;

			case AF_INET6:
				addr_in6 = (struct sockaddr_in6 *)addr;
				if (memcmp(*hostptr, &addr_in6->sin6_addr,
				    hostinfo->h_length) == 0) {
					freehostent(hostinfo);
					return (1);
				}
				break;

			default:
				break;
			}
		}
	}

	freehostent(hostinfo);
	return (0);
}
