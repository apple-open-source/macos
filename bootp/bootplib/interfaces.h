/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
 * interfaces.h
 * - get the list of inet interfaces in the system
 */

/*
 * Modification History
 * 02/23/98	Dieter Siegmund (dieter@apple.com)
 * - initial version
 */

#import <sys/socket.h>
#import <net/if.h>
#import <netinet/in.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <netinet/udp.h>
#import <netinet/bootp.h>
#import <net/if_arp.h>
#import <netinet/if_ether.h>
#import <net/if_dl.h>
#import <net/if_types.h>
#import <mach/boolean.h>
#import <sys/param.h>


#import "dynarray.h"

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

typedef struct {
    char 		name[IFNAMSIZ + 1]; /* eg. en0 */
    short		flags;

    dynarray_t		inet;

    boolean_t		link_valid;
    struct sockaddr_dl	link;

    u_int32_t		user_defined;
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
interface_t *		if_dup(interface_t * intface);
char *			if_name(interface_t * if_p);
short			if_flags(interface_t * if_p);
void			if_setflags(interface_t * if_p, short flags);

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

int			if_link_arptype(interface_t * if_p);
void *			if_link_address(interface_t * if_p);
int			if_link_length(interface_t * if_p);

static __inline__ int
dl_to_arp_hwtype(int dltype)
{
    int type;

    switch (dltype) {
    case IFT_ETHER:
	type = ARPHRD_ETHER;
	break;
    default:
	type = -1;
	break;
    }
    return (type);
}
