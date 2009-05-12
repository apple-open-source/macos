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

	if (getifaddrs(&ifaddrs) == -1) {
		syslog(LOG_DEBUG, "getifaddrs failed:  %s\n",
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
			if (memcmp(*hostptr, addr->sa_data, hostinfo->h_length) == 0) {
				freehostent(hostinfo);
				return (1);
			}
		}
	}

	freehostent(hostinfo);
	return (0);
}
