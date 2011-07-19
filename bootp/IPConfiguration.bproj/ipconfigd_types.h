#ifndef _S_IPCONFIGD_TYPES_H
#define _S_IPCONFIGD_TYPES_H

/*
 * Copyright (c) 2000-2009 Apple Inc. All rights reserved.
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

#include <mach/boolean.h>
#include <netinet/in.h>
#include "DHCPv6.h"
#include "DHCPv6Options.h"
#include "timer.h"
#include "dhcp_options.h"

#define IPV4_METHOD_BIT		0x100
#define IPV6_METHOD_BIT		0x200

typedef enum {
    ipconfig_method_none_e 	= 0x0,

    /* IPv4 */
    ipconfig_method_none_v4_e	= 0x100,
    ipconfig_method_manual_e 	= 0x101,
    ipconfig_method_bootp_e 	= 0x102,
    ipconfig_method_dhcp_e 	= 0x103,
    ipconfig_method_inform_e 	= 0x104,
    ipconfig_method_linklocal_e = 0x105,
    ipconfig_method_failover_e 	= 0x106,

    /* IPv6 */
    ipconfig_method_none_v6_e	= 0x200,
    ipconfig_method_manual_v6_e	= 0x201,
    ipconfig_method_automatic_v6_e = 0x202,
    ipconfig_method_rtadv_e 	= 0x203,
    ipconfig_method_stf_e 	= 0x204,
    ipconfig_method_linklocal_v6_e = 0x205,
} ipconfig_method_t;

static __inline__ const char *
ipconfig_method_string(ipconfig_method_t m)
{
    switch (m) {
    case ipconfig_method_none_e:
	return ("NONE");
    case ipconfig_method_none_v4_e:
	return ("NONE-V4");
    case ipconfig_method_none_v6_e:
	return ("NONE-V6");
    case ipconfig_method_manual_e:
	return ("MANUAL");
    case ipconfig_method_bootp_e:
	return ("BOOTP");
    case ipconfig_method_dhcp_e:
	return ("DHCP");
    case ipconfig_method_inform_e:
	return ("INFORM");
    case ipconfig_method_linklocal_e:
	return ("LINKLOCAL");
    case ipconfig_method_failover_e:
	return ("FAILOVER");
    case ipconfig_method_manual_v6_e:
	return ("MANUAL-V6");
    case ipconfig_method_automatic_v6_e:
	return ("AUTOMATIC-V6");
    case ipconfig_method_rtadv_e:
	return ("RTADV");
    case ipconfig_method_stf_e:
	return ("6TO4");
    case ipconfig_method_linklocal_v6_e:
	return ("LINKLOCAL-V6");
    default:
	break;
    }
    return ("<unknown>");
}

static __inline__ boolean_t
ipconfig_method_is_dhcp_or_bootp(ipconfig_method_t method)
{
    if (method == ipconfig_method_dhcp_e
	|| method == ipconfig_method_bootp_e) {
	return (TRUE);
    }
    return (FALSE);
}

static __inline__ boolean_t
ipconfig_method_is_manual(ipconfig_method_t method)
{
    if (method == ipconfig_method_manual_e
	|| method == ipconfig_method_failover_e
	|| method == ipconfig_method_inform_e) {
	return (TRUE);
    }
    return (FALSE);
}

static __inline__ boolean_t
ipconfig_method_is_v4(ipconfig_method_t method)
{
    return ((method & IPV4_METHOD_BIT) != 0);
}

static __inline__ boolean_t
ipconfig_method_is_v6(ipconfig_method_t method)
{
    return ((method & IPV6_METHOD_BIT) != 0);
}

typedef struct {
    struct in_addr	addr;
    struct in_addr	mask;
    struct in_addr	router;
    boolean_t		ignore_link_status;
    int32_t		failover_timeout;
} ipconfig_method_data_manual_t;

typedef struct {
    int			client_id_len;
    uint8_t		client_id[1];
} ipconfig_method_data_dhcp_t;

#define LINKLOCAL_ALLOCATE	TRUE
#define LINKLOCAL_NO_ALLOCATE	FALSE
typedef struct {
    boolean_t	allocate;
} ipconfig_method_data_linklocal_t;

typedef struct {
    struct in6_addr	addr;
    int			prefix_length;
} ipconfig_method_data_manual_v6_t;

typedef enum {
    address_type_none_e,
    address_type_ipv4_e,
    address_type_ipv6_e,
    address_type_dns_e
} address_type_t;

typedef struct {
    address_type_t	relay_addr_type;
    union {
	struct in_addr	v4;
	struct in6_addr	v6;
	char 		dns[1];
    } relay_addr;
} ipconfig_method_data_stf_t;

/*
 * Type: ipconfig_method_data_t
 * Purpose:
 *   Used internally by IPConfiguration to communicate the config to
 *   the various methods.
 */
typedef union {
    ipconfig_method_data_manual_t	manual;
    ipconfig_method_data_dhcp_t		dhcp;
    ipconfig_method_data_linklocal_t	linklocal;
    ipconfig_method_data_manual_v6_t	manual_v6;
    ipconfig_method_data_stf_t		stf;
} ipconfig_method_data_t;

/*
 * Types to hold DHCP/auto-configuration information
 */
typedef struct {
    const uint8_t *		pkt;
    int				pkt_size;
    dhcpol_t *			options;
    absolute_time_t		lease_start;
    absolute_time_t		lease_expiration;
} dhcp_info_t;

typedef struct {
    DHCPv6PacketRef		pkt;
    int				pkt_len;
    DHCPv6OptionListRef		options;
    struct in6_addr		addr;
    const struct in6_addr *	dns_servers;
    int				dns_servers_count;
} dhcpv6_info_t;

#endif /* _S_IPCONFIGD_TYPES_H */
