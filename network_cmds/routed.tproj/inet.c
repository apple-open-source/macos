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
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgment:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)defs.h	8.1 (Berkeley) 6/5/93
 */


/*
 * Temporarily, copy these routines from the kernel,
 * as we need to know about subnets.
 */
#include "defs.h"

extern struct interface *ifnet;

/*
 * Formulate an Internet address from network + host.
 */
__private_extern__
struct in_addr
inet_makeaddr(net, host)
	u_long net, host;
{
	register struct interface *ifp;
	register u_long mask;
	u_long addr;

	if (IN_CLASSA(net))
		mask = IN_CLASSA_HOST;
	else if (IN_CLASSB(net))
		mask = IN_CLASSB_HOST;
	else
		mask = IN_CLASSC_HOST;
	for (ifp = ifnet; ifp; ifp = ifp->int_next)
		if ((ifp->int_netmask & net) == ifp->int_net) {
			mask = ~ifp->int_subnetmask;
			break;
		}
	addr = net | (host & mask);
	addr = htonl(addr);
	return (*(struct in_addr *)&addr);
}

/*
 * Return the network number from an internet address.
 */
__private_extern__ int
inet_netof(in)
	struct in_addr in;
{
	register u_long i = ntohl(in.s_addr);
	register u_long net;
	register struct interface *ifp;

	if (IN_CLASSA(i))
		net = i & IN_CLASSA_NET;
	else if (IN_CLASSB(i))
		net = i & IN_CLASSB_NET;
	else
		net = i & IN_CLASSC_NET;

	/*
	 * Check whether network is a subnet;
	 * if so, return subnet number.
	 */
	for (ifp = ifnet; ifp; ifp = ifp->int_next)
		if ((ifp->int_netmask & net) == ifp->int_net)
			return (i & ifp->int_subnetmask);
	return (net);
}

/*
 * Return the host portion of an internet address.
 */
int
inet_lnaof(in)
	struct in_addr in;
{
	register u_long i = ntohl(in.s_addr);
	register u_long net, host;
	register struct interface *ifp;

	if (IN_CLASSA(i)) {
		net = i & IN_CLASSA_NET;
		host = i & IN_CLASSA_HOST;
	} else if (IN_CLASSB(i)) {
		net = i & IN_CLASSB_NET;
		host = i & IN_CLASSB_HOST;
	} else {
		net = i & IN_CLASSC_NET;
		host = i & IN_CLASSC_HOST;
	}

	/*
	 * Check whether network is a subnet;
	 * if so, use the modified interpretation of `host'.
	 */
	for (ifp = ifnet; ifp; ifp = ifp->int_next)
		if ((ifp->int_netmask & net) == ifp->int_net)
			return (host &~ ifp->int_subnetmask);
	return ((int)host);
}

/*
 * Return the netmask pertaining to an internet address.
 */
int
inet_maskof(inaddr)
	u_long inaddr;
{
	register u_long i = ntohl(inaddr);
	register u_long mask;
	register struct interface *ifp;

	if (i == 0) {
		mask = 0;
	} else if (IN_CLASSA(i)) {
		mask = IN_CLASSA_NET;
	} else if (IN_CLASSB(i)) {
		mask = IN_CLASSB_NET;
	} else
		mask = IN_CLASSC_NET;

	/*
	 * Check whether network is a subnet;
	 * if so, use the modified interpretation of `host'.
	 */
	for (ifp = ifnet; ifp; ifp = ifp->int_next)
		if ((ifp->int_netmask & i) == ifp->int_net)
			mask = ifp->int_subnetmask;
	return ((int)htonl(mask));
}

/*
 * Return RTF_HOST if the address is
 * for an Internet host, RTF_SUBNET for a subnet,
 * 0 for a network.
 */
int
inet_rtflags(sin)
	struct sockaddr_in *sin;
{
	register u_long i = ntohl(sin->sin_addr.s_addr);
	register u_long net, host;
	register struct interface *ifp;

	if (IN_CLASSA(i)) {
		net = i & IN_CLASSA_NET;
		host = i & IN_CLASSA_HOST;
	} else if (IN_CLASSB(i)) {
		net = i & IN_CLASSB_NET;
		host = i & IN_CLASSB_HOST;
	} else {
		net = i & IN_CLASSC_NET;
		host = i & IN_CLASSC_HOST;
	}

	/*
	 * Check whether this network is subnetted;
	 * if so, check whether this is a subnet or a host.
	 */
	for (ifp = ifnet; ifp; ifp = ifp->int_next)
		if (net == ifp->int_net) {
			if (host &~ ifp->int_subnetmask)
				return (RTF_HOST);
			else if (ifp->int_subnetmask != ifp->int_netmask)
				return (RTF_SUBNET);
			else
				return (0);		/* network */
		}
	if (host == 0)
		return (0);	/* network */
	else
		return (RTF_HOST);
}

/*
 * Return true if a route to subnet/host of route rt should be sent to dst.
 * Send it only if dst is on the same logical network if not "internal",
 * otherwise only if the route is the "internal" route for the logical net.
 */
int
inet_sendroute(rt, dst)
	struct rt_entry *rt;
	struct sockaddr_in *dst;
{
	register u_long r =
	    ntohl(((struct sockaddr_in *)&rt->rt_dst)->sin_addr.s_addr);
	register u_long d = ntohl(dst->sin_addr.s_addr);

	if (IN_CLASSA(r)) {
		if ((r & IN_CLASSA_NET) == (d & IN_CLASSA_NET)) {
			if ((r & IN_CLASSA_HOST) == 0)
				return ((rt->rt_state & RTS_INTERNAL) == 0);
			return (1);
		}
		if (r & IN_CLASSA_HOST)
			return (0);
		return ((rt->rt_state & RTS_INTERNAL) != 0);
	} else if (IN_CLASSB(r)) {
		if ((r & IN_CLASSB_NET) == (d & IN_CLASSB_NET)) {
			if ((r & IN_CLASSB_HOST) == 0)
				return ((rt->rt_state & RTS_INTERNAL) == 0);
			return (1);
		}
		if (r & IN_CLASSB_HOST)
			return (0);
		return ((rt->rt_state & RTS_INTERNAL) != 0);
	} else {
		if ((r & IN_CLASSC_NET) == (d & IN_CLASSC_NET)) {
			if ((r & IN_CLASSC_HOST) == 0)
				return ((rt->rt_state & RTS_INTERNAL) == 0);
			return (1);
		}
		if (r & IN_CLASSC_HOST)
			return (0);
		return ((rt->rt_state & RTS_INTERNAL) != 0);
	}
}
