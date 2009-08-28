
#ifndef _CONFIGTHREADS_TYPES_H
#define _CONFIGTHREADS_TYPES_H

/*
 * Copyright (c) 2003-2008 Apple Inc. All rights reserved.
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

/* configthreads_types.h:
 *		config status and method types for the various configuration methods
 */

#include <netinet/in.h>

typedef enum {
    ip6config_status_success_e = 0,
    ip6config_status_invalid_parameter_e = 1,
    ip6config_status_invalid_operation_e = 2,
    ip6config_status_allocation_failed_e = 3,
    ip6config_status_internal_error_e = 4,
    ip6config_status_operation_not_supported_e = 5,
    ip6config_status_address_in_use_e = 6,
    ip6config_status_media_inactive_e = 7,
    ip6config_status_no_rtadv_response_e = 8,
    ip6config_status_last_e
} ip6config_status_t;

static __inline__ const char *
ip6config_status_string(ip6config_status_t status)
{
    static const char * str[] = {
	"operation succeded",
	"invalid parameter",
	"invalid operation",
	"allocation failed",
	"internal error",
	"operation not supported",
	"address in use",
	"media inactive",
	"no router advertisement response"
    };
    if (status < 0 || status >= ip6config_status_last_e)
	return ("<unknown>");
    return (str[status]);
}

typedef enum {
    ip6config_method_none_e = 0,
    ip6config_method_automatic_e = 1,
    ip6config_method_rtadv_e = 2,
    ip6config_method_manual_e = 3,
    ip6config_method_6to4_e = 4,
    ip6config_method_linklocal_e = 5,
    ip6config_method_last_e
} ip6config_method_t;

static __inline__ const char *
ip6config_method_string(ip6config_method_t m)
{
    static const char * str[] = {
		"NONE",
		"AUTOMATIC",
		"RTADV",
		"MANUAL",
		"6TO4",
		"LINKLOCAL"
    };
    if (m < 0 || m >= ip6config_method_last_e)
	return ("<unknown>");
    return (str[m]);
}

typedef enum {
    relay_address_type_none_e = 0,
    relay_address_type_ipv6_e = 1,
    relay_address_type_ipv4_e = 2,
    relay_address_type_dns_e = 3,
    relay_address_type_last_e
} relay_address_type_t;

static __inline__ const char *
relay_address_type_string(relay_address_type_t m)
{
    static const char * str[] = {
	"NONE",
	"IPV6",
	"IPV4",
	"DNS"
    };
    if (m < 0 || m >= relay_address_type_last_e)
	return ("<unknown>");
    return (str[m]);
}

typedef struct {
    relay_address_type_t	addr_type;
    union {
	struct in6_addr	ip6_relay_addr;
	struct in_addr	ip4_relay_addr;
	char *		dns_relay_addr;
    } relay_address_u;
} relay_address_t;

typedef struct {
    unsigned char	n_ip4;		/* number of addresses in list */
    relay_address_t	relay_address;	/* 6to4 relay */
    struct in_addr *	ip4_addrs_list;	/* ip4 addresses from which ip6 address will be derived */
} stf_method_data_t;

typedef struct {
    unsigned char	n_ip6;		/* number of addresses in list */
    stf_method_data_t	stf_data;
    struct {
	struct in6_addr	addr;
	int		prefixLen;
	int		flags;
    } ip6[0];
} ip6config_method_data_t;

typedef struct {
	struct in6_addr	addr;
	struct in6_addr	prefixmask;
	int		prefixlen;

	int		flags;
} ip6_addrinfo_t;

typedef struct {
    int			n_addrs;
    ip6_addrinfo_t	*addr_list;
} ip6_addrinfo_list_t;

#endif _CONFIGTHREADS_TYPES_H
