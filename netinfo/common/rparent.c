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

ni_status ni2_rparent(void *domain, struct sockaddr_in *addr, char **tag)
{
	/* ask a NetInfo server for the address and tag of it's parent */
	/* there's no library routine to ask a server for it's parent */
	/* directly, so we have to jump through a couple of hoops */
	/* first ask nibindd for the tcp port of the server, then */
	/* make our own RCP connection to the server and send it */
	/* an NI_RPARENT message */

	ni_status ret;
	CLIENT *bindclnt, *niclnt;
	char child_tag[1024];
	nibind_getregister_res *grres;
	ni_rparent_res *rpres;
	int sock;
	struct sockaddr_in sin, child_addr;

	/* get current server's address and tag */
	ret = ni_addrtag(domain, &child_addr, (ni_name *)&child_tag);

	/* avoid crashing if bad info in child_addr */
	if (NI_OK != ret || 
	    INADDR_NONE == child_addr.sin_addr.s_addr ||
	    INADDR_ANY == child_addr.sin_addr.s_addr) {
	    /*
	     * If we got no address, there's a weird error in network
	     * interface configuration, and we should complain.
	     */
	    return(NI_CANTFINDADDRESS);
	}

	/* ask nibindd for the tcp port of the server */

	/* create an RPC client for NIBIND */
	child_addr.sin_port = 0; /* let portmapper find it */
	sock = RPC_ANYSOCK;
	bindclnt = clnttcp_create(&child_addr, NIBIND_PROG, NIBIND_VERS, &sock, 0, 0);
	if (bindclnt == NULL) return NI_SYSTEMERR;

	/* ask for the ports used by the server */
	grres = nibind_getregister_1((ni_name *)&child_tag, bindclnt);
	clnt_destroy(bindclnt);
	if (grres == NULL) return NI_SYSTEMERR;

	/* create a NI client */
	/* we need to use clnttcp_create, so we need to set up an address and a socket */
	/* here's the address */
	sin.sin_port=htons(grres->nibind_getregister_res_u.addrs.tcp_port);
	sin.sin_family = AF_INET;
	bcopy(&child_addr.sin_addr, &sin.sin_addr, sizeof(child_addr.sin_addr));
	sock = RPC_ANYSOCK;

	/* now make me a client */
	niclnt = clnttcp_create(&sin, NI_PROG, NI_VERS, &sock, 0, 0);
	if (niclnt == NULL) return NI_SYSTEMERR;

	/* send the NI_RPARENT */
	rpres = _ni_rparent_2((void *)0, niclnt);
	clnt_destroy(niclnt);
	if (rpres == NULL) return NI_SYSTEMERR;
	if (rpres->status != NI_OK) return rpres->status;

	/* copy the results into the caller's parameters */
	addr->sin_addr.s_addr = htonl(rpres->ni_rparent_res_u.binding.addr);
	*tag = malloc(strlen(rpres->ni_rparent_res_u.binding.tag) + 1);
	strcpy(*tag, rpres->ni_rparent_res_u.binding.tag);

	return NI_OK;
}
