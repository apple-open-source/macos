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
 * Fundamental constants relating to firewire.
 *
 */

#ifndef _NET_FIREWIRE_H_
#define _NET_FIREWIRE_H_
#include <sys/appleapiopts.h>

#define FIREWIRE_MTU	4096

/*
 * The number of bytes in an firewire ethernet like address.
 */
#define	FIREWIRE_ADDR_LEN		8

/*
 * The number of bytes in the type field.
 */
#define	FIREWIRE_TYPE_LEN		2

/*
 * The length of the combined header.
 */
#define	FIREWIRE_HDR_LEN		(FIREWIRE_ADDR_LEN*2+FIREWIRE_TYPE_LEN)

/*
 * The minimum packet length.
 */
#define	FIREWIRE_MIN_LEN		64

/*
 * The maximum packet length.
 */
#define	FIREWIRE_MAX_LEN		4096

/*
 * A macro to validate a length with
 */
#define	FIREWIRE_IS_VALID_LEN(foo)	\
	((foo) >= FIREWIRE_MIN_LEN && (foo) <= FIREWIRE_MAX_LEN)

/*
 * Structure for firewire header
 */
struct	firewire_header {
	u_char	fw_dhost[FIREWIRE_ADDR_LEN];
	u_char	fw_shost[FIREWIRE_ADDR_LEN];
	u_short	fw_type;
};

#define	FWTYPE_IP		0x0800	/* IP protocol */
#define FWTYPE_ARP		0x0806	/* Addr. resolution protocol */
#define FWTYPE_IPV6		0x86dd	/* IPv6 */

#define	FIREWIREMTU	(FIREWIRE_MAX_LEN-FIREWIRE_HDR_LEN)

int firewire_attach_inet(ifnet_t ifp, protocol_family_t protocol_family);
int firewire_attach_inet6(ifnet_t ifp, __unused protocol_family_t protocol_family);

int firewire_ifattach(ifnet_t ifp);
void firewire_ifdetach(ifnet_t ifp);

#endif /* !_NET_FIREWIRE_H_ */
