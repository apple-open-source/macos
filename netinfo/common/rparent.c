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

/*
 * nilib2a ± more NetInfo library routines.
 * Written by Marc Majka
 *
 * Copyright 1994,  NeXT Computer Inc.
 *
 * Lennart moved ni2_rparent() here to avoid having it be pulled into any
 * client that wants to make use of the other ni2 routines.  This will in
 * turn avoid having to pull in nibind_getregister_1().  95-06-08
 */

#include <NetInfo/nilib2.h>

typedef struct ni_private {
	int naddrs;		/* number of addresses */
	struct in_addr *addrs;	/* addresses of servers - network byte order */
	int whichwrite;	/* which one of the above is the master */
	ni_name *tags;		/* tags of servers */
	int pid;		/* pid, to detect forks */
	int tsock;		/* tcp socket */
	int tport;		/* tcp local port name - host byte order */
	CLIENT *tc;		/* tcp client */
	long tv_sec;		/* timeout for this call */
	long rtv_sec;		/* read timeout - 0 if default */
	long wtv_sec;		/* write timeout - 0 if default */
	int abort;		/* abort on timeout? */
	int needwrite;		/* need to lock writes? */
	int uid;		/* user id */
	ni_name passwd;		/* password */
} ni_private;

#define NIP(ni) ((ni_private *)(ni))

/* ask a NetInfo server for the address and tag of it's parent */
ni_status ni2_rparent(void *domain, struct sockaddr_in *addr, char **tag)
{
	CLIENT *c;
	ni_rparent_res *rpres;

	if (domain == NULL) return NI_INVALIDDOMAIN;
	if (addr == NULL) return NI_NONAME;
	if (tag == NULL) return NI_NONAME;

	c = NIP(domain)->tc;
	if (c == NULL) return NI_SYSTEMERR;

	/* send the NI_RPARENT */
	rpres = _ni_rparent_2((void *)0, c);

	if (rpres == NULL) return NI_SYSTEMERR;
	if (rpres->status != NI_OK) return rpres->status;

	/* copy the results into the caller's parameters */
	addr->sin_addr.s_addr = htonl(rpres->ni_rparent_res_u.binding.addr);
	*tag = malloc(strlen(rpres->ni_rparent_res_u.binding.tag) + 1);
	strcpy(*tag, rpres->ni_rparent_res_u.binding.tag);

	return NI_OK;
}
