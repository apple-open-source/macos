/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
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
 * The number of bytes in the trailing CRC field.
 */
#define	FIREWIRE_CRC_LEN		4

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
	u_char	ether_dhost[FIREWIRE_ADDR_LEN];
	u_char	ether_shost[FIREWIRE_ADDR_LEN];
	u_short	ether_type;
};


#define ether_addr_octet octet

#define	ETHERTYPE_PUP		0x0200	/* PUP protocol */
#define	ETHERTYPE_IP		0x0800	/* IP protocol */
#define ETHERTYPE_ARP		0x0806	/* Addr. resolution protocol */
#define ETHERTYPE_REVARP	0x8035	/* reverse Addr. resolution protocol */
#define	ETHERTYPE_VLAN		0x8100	/* IEEE 802.1Q VLAN tagging */
#define ETHERTYPE_IPV6		0x86dd	/* IPv6 */
#define	ETHERTYPE_LOOPBACK	0x9000	/* used to test interfaces */
/* XXX - add more useful types here */

/*
 * The ETHERTYPE_NTRAILER packet types starting at ETHERTYPE_TRAIL have
 * (type-ETHERTYPE_TRAIL)*512 bytes of data followed
 * by an ETHER type (as given above) and then the (variable-length) header.
 */
#define	ETHERTYPE_TRAIL		0x1000		/* Trailer packet */
#define	ETHERTYPE_NTRAILER	16

#define	FIREWIREMTU	(FIREWIRE_MAX_LEN-FIREWIRE_HDR_LEN)
#define	ETHERMIN	(ETHER_MIN_LEN-ETHER_HDR_LEN-ETHER_CRC_LEN)


int firewire_family_init();

int firewire_attach_inet (struct ifnet *ifp, u_long *dl_tag);
int firewire_attach_inet6 (struct ifnet *ifp, u_long *dl_tag);
int firewire_detach_inet(struct ifnet *ifp, u_long dl_tag);
int firewire_detach_inet6(struct ifnet *ifp, u_long dl_tag);

int firewire_ifattach(register struct ifnet *ifp);
void firewire_ifdetach(register struct ifnet *ifp);

#ifdef KERNEL
#ifdef __APPLE_API_PRIVATE
struct	ether_addr *ether_aton __P((char *));
#endif /* __APPLE_API_PRIVATE */
#endif

#ifndef KERNEL
#include <sys/cdefs.h>

/*
 * firewire address conversion/parsing routines.
 */
__BEGIN_DECLS

int	ether_hostton __P((char *, struct ether_addr *));
int	ether_line __P((char *, struct ether_addr *, char *));
char 	*ether_ntoa __P((struct ether_addr *));
int	ether_ntohost __P((char *, struct ether_addr *));
__END_DECLS
#endif /* !KERNEL */

#endif /* !_NET_FIREWIRE_H_ */
