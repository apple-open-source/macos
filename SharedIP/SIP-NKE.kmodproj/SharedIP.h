/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Shared IP Address support
 * Designed for Blue Box/Classic use, to permit
 *  sharing of IP addresses assigned to specific network
 *  interfaces
 *
 * Justin C. Walker, 991107
 */

#ifndef _NET_SHARED_IP_H_
#define _NET_SHARED_IP_H_

/*
 * PF_NDRV/Raw-level options to reserve, release, or lookup ports
 * Usage: setsockopt(s, 0, SO_NKE, &so_nke, sizeof (struct so_nke));
 * The value '0' is the "raw" protocol number for the PF_NDRV family,
 *  since the "raw" protocol is the only one supported.
 * 's' must be a socket of family PF_NDRV.
 */
#define SO_PROTO_REGISTER	0x10A0
#define SO_PORT_RESERVE		0x10A1
#define SO_PORT_RELEASE		0x10A2
#define SO_PORT_LOOKUP		0x10A3	/* XXX - Currently not supported */

/*
 * Theory of operation:
 * Use of these socket options is privileged.
 *
 * These operations are intended to let a user app share IP addresses
 *  with the Mac OS X kernel.  The assumption is that (a) the app
 *  has a full IP stack; and (b) it's using "raw" (AF_NDRV) access to
 *  network devices.
 * This should be viewed as a special version of the 'bind()' system
 *  call.
 * SO_PORT_RESERVE:
 *  a zero port requests that the kernel assign a port, which is
 *  returned; a non-zero port gets reserved, unless it's already in
 *  use (modulo 'laddr' and the use of SO_REUSEADDR and SO_REUSEPORT).
 *  If 'laddr' is INADDR_ANY, the port is reserved for all local IP
 *  addresses; if not, it is reserved for just that IP address (which
 *  must be valid).
 *  If the foreign address/port values are known, they can be specified
 *   as an optimization aid.  Otherwise, these values must be zero.
 * SO_PORT_RELEASE:
 *  essentially a 'close', except that the app doesn't have a socket
 *  to close for this port.  Assumed that the app is handling all the
 *  state machinations in the case of TCP.
 * SO_PORT_LOOKUP:
 *  not currently planned for support; here for completeness.
 *
 * The 'cookie' argument is a potential optimization; it's an opaque
 *  value that the app passes in during reservation, and which the
 *  kernel will pass back to the app, as a 'control' buffer (recvmsg()),
 *  to assist the app in "fast" matching, to avoid duplication of lookup
 *  effort between app and kernel.
 * The 'proto' argument specifies whether the reserved port is UDP
 *  or TCP.
 * The 'flags' argument is defined to permit possible additional
 *  information to be passed to the kernel or to the app (e.g., 
 *  SO_REUSEADDR/SO_REUSEPORT), if needed)
 */

struct sopt_shared_port_param {
    struct in_addr	laddr;	/* Local IP address for psuedo bind op */
    struct in_addr	faddr;	/* Foreign IP address if known */
    unsigned int	lport;	/* Local port for psuedo bind op */
    unsigned int	fport;	/* Foreign port if known */
    unsigned int	proto;	/* IPPROTO_XXX (UDP, TCP) */
    unsigned int	flags;	/* Modifiers */
    unsigned int	cookie;	/* Cookie to return to user with */
                                /* incoming frames (in control component) */
};

struct sopt_proto_register {
    unsigned int reg_flags;	/* See below */
    struct sockaddr *reg_sa;	/* Address to filter on */
};

/*
 * Theory of operation:
 * For each supported protocol (IP and AppleTalk only), register interest.
 * PF_NDRV processing won't recognize these, but they will be intercepted
 *  by the NKE.
 * RCV_ALL: all incoming traffic is passed to the user process
 * RCV_FILT: incoming traffic that matches the filter is passed
 * RCV_SHARED: special mode that requires cooperation between user process
 *  and NKE.  For IP, the "match" is made based on ports, which the user
 *  process must register (see above).
 * ENABLE: turn on the use of the filter (match against known filters)
 * DISABLE: turn off the use of the filter (match against known filters)
 * General note on matching:
 *  typically, the protocol will deal with ARP and protocol packets, with
 *  addressing involving unicast, multicast, and broadcast.  Each protocol
 *  module can decide for itself how to handle the different classes of
 *  traffic.  AppleTalk will pass multi/broadcast traffic to both
 *  stacks (assuming they're both on).  IP could do the same, but might
 *  do further discrimination on multicast packets by matching on 
 *  destination address and port.
 */
#define SIP_PROTO_ATALK		0x01 /* Register for AppleTalk traffic  */
#define SIP_PROTO_IPv4		0x02 /* Register for IPv4 traffic */
#define SIP_PROTO_RCV_ALL	0x04 /* Don't filter incoming traffic */
#define SIP_PROTO_RCV_FILT	0x08 /* Only pass matching traffic */
#define SIP_PROTO_RCV_SHARED	0x10 /* See above */
#define SIP_PROTO_ENABLE	0x20 /* Enable "filter" */
#define SIP_PROTO_DISABLE	0x40 /* Disable "filter" */

#endif /* _NET_SHARED_IP_H_ */

