#ifndef _S_INTERFACES_H
#define _S_INTERFACES_H
/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
 * interfaces.h
 * - get the list of inet interfaces in the system
 */

/*
 * Modification History
 * 02/23/98	Dieter Siegmund (dieter@apple.com)
 * - initial version
 */

#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <net/if_arp.h>
#include <netinet/if_ether.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <mach/boolean.h>
#include <sys/param.h>
#include <stdint.h>


#include "dynarray.h"

#define INDEX_BAD	((int)(-1))

/*
 * Type: interface_t
 * Purpose:
 *   Enclose IP and link-level information for a particular
 *   interface.
 */
typedef struct {
    struct in_addr	addr;
    struct in_addr	mask;
    struct in_addr	netaddr;
    struct in_addr	broadcast;
} inet_addrinfo_t;

#define MAX_LINK_ADDR_LEN	16
typedef struct {
    uint8_t		addr[MAX_LINK_ADDR_LEN];
    uint16_t		index;
    uint8_t		alen;
    uint8_t		type;
} link_addr_t;

typedef struct {
    char 		name[IFNAMSIZ]; /* eg. en0 */
    uint16_t		flags;
    uint8_t		type;	/* e.g. IFT_ETHER */
    uint8_t		is_wireless;
    dynarray_t		inet;
    link_addr_t		link_address;
    uint32_t		user_defined;
} interface_t;

typedef struct {
    interface_t *	list;
    int			count;
    int			size;
} interface_list_t;

/*
 * Functions: ifl_*
 * Purpose:
 *   Interface list routines.
 */
interface_list_t * 	ifl_init();
interface_t * 		ifl_first_broadcast_inet(interface_list_t * intface);
interface_t *		ifl_find_name(interface_list_t * intface, 
				      const char * name);
interface_t *		ifl_find_link(interface_list_t * intface,
				      int link_index);
interface_t *		ifl_find_ip(interface_list_t * intface,
				    struct in_addr iaddr);
interface_t *		ifl_find_subnet(interface_list_t * intface, 
					struct in_addr iaddr);
void			ifl_free(interface_list_t * * list_p);
int			ifl_count(interface_list_t * list_p);
interface_t *		ifl_at_index(interface_list_t * list_p, int i);
int			ifl_index(interface_list_t * list_p, 
				  interface_t * if_p);

/*
 * Functions: if_*
 * Purpose:
 *   Interface-specific routines.
 */
interface_t *		if_dup(interface_t * if_p); /* dup an entry */
void			if_free(interface_t * * if_p_p); /* free dup'd entry */
const char *		if_name(interface_t * if_p);
uint16_t		if_flags(interface_t * if_p);
void			if_setflags(interface_t * if_p, uint16_t flags);

int			if_inet_count(interface_t * if_p);
int			if_inet_find_ip(interface_t * if_p, 
					struct in_addr iaddr);
boolean_t		if_inet_addr_add(interface_t * if_p, 
					 inet_addrinfo_t * info);
boolean_t		if_inet_addr_remove(interface_t * if_p, 
					    struct in_addr iaddr);
struct in_addr		if_inet_addr(interface_t * if_p);
struct in_addr		if_inet_netmask(interface_t * if_p);
struct in_addr		if_inet_netaddr(interface_t * if_p);
struct in_addr		if_inet_broadcast(interface_t * if_p);
boolean_t		if_inet_valid(interface_t * if_p);
inet_addrinfo_t *	if_inet_addr_at(interface_t * if_p, int i);

int			if_ift_type(interface_t * if_p);

int			if_link_type(interface_t * if_p);
int			if_link_dhcptype(interface_t * if_p);
int			if_link_arptype(interface_t * if_p);
void *			if_link_address(interface_t * if_p);
int			if_link_length(interface_t * if_p);
void			if_link_update(interface_t * if_p);
void			if_link_copy(interface_t * dest, 
				     const interface_t * source);
boolean_t		if_is_wireless(interface_t * if_p);
int			if_link_index(interface_t * if_p);

static __inline__ int
dl_to_arp_hwtype(int dltype)
{
    int type;

    switch (dltype) {
    case IFT_ETHER:
	type = ARPHRD_ETHER;
	break;
    case IFT_IEEE1394:
	type = ARPHRD_IEEE1394;
	break;
    default:
	type = -1;
	break;
    }
    return (type);
}
#endif _S_INTERFACES_H
