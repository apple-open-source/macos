
#ifndef _S_IPCONFIG_TYPES_H
#define _S_IPCONFIG_TYPES_H

/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

#include <sys/types.h>
#include <netinet/in.h>

#define IPCONFIG_IF_ANY		""
#define MAX_IF_NAMELEN 		32
#define MAX_INLINE_DATA 	2048

typedef char if_name_t[MAX_IF_NAMELEN];
typedef char inline_data_t[MAX_INLINE_DATA];
typedef char *ooline_data_t;
typedef u_int32_t ip_address_t;

typedef enum {
    ipconfig_status_success_e = 0,
    ipconfig_status_permission_denied_e = 1,
    ipconfig_status_interface_does_not_exist_e = 2,
    ipconfig_status_invalid_parameter_e = 3,
    ipconfig_status_invalid_operation_e = 4,
    ipconfig_status_allocation_failed_e = 5,
    ipconfig_status_internal_error_e = 6,
    ipconfig_status_operation_not_supported_e = 7,
    ipconfig_status_address_in_use_e = 8,
    ipconfig_status_no_server_e = 9,
    ipconfig_status_server_not_responding_e = 10,
    ipconfig_status_lease_terminated_e = 11,
    ipconfig_status_media_inactive_e = 12,
    ipconfig_status_server_error_e = 13,
    ipconfig_status_no_such_service_e = 14,
    ipconfig_status_duplicate_service_e = 15,
    ipconfig_status_address_timed_out_e = 16,
    ipconfig_status_last_e,
} ipconfig_status_t;

static __inline__ const char *
ipconfig_status_string(ipconfig_status_t status)
{
    static const char * str[] = {
	"operation succeded",
	"permission denied",
	"interface doesn't exist",
	"invalid parameter",
	"invalid operation",
	"allocation failed",
	"internal error",
	"operation not supported",
	"address in use",
	"no server",
	"server not responding",
	"lease terminated",
	"media inactive",
	"server error",
	"no such service",
	"duplicate service",
	"address timed out",
    };
    if (status < 0 || status >= ipconfig_status_last_e)
	return ("<unknown>");
    return (str[status]);
}
    
typedef enum {
    ipconfig_method_none_e = 0,
    ipconfig_method_manual_e = 1,
    ipconfig_method_bootp_e = 2,
    ipconfig_method_dhcp_e = 3,
    ipconfig_method_inform_e = 4,
    ipconfig_method_linklocal_e = 5,
    ipconfig_method_failover_e = 6,
    ipconfig_method_last_e,
} ipconfig_method_t;

static __inline__ const char *
ipconfig_method_string(ipconfig_method_t m)
{
    static const char * str[] = {
	"NONE",
	"MANUAL",
	"BOOTP",
	"DHCP",
	"INFORM",
	"LINKLOCAL",
	"FAILOVER"
    };
    if (m < 0 || m >= ipconfig_method_last_e)
	return ("<unknown>");
    return (str[m]);
}

static __inline__ boolean_t
ipconfig_method_is_dynamic(ipconfig_method_t method)
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

typedef struct {
    struct in_addr		addr;
    struct in_addr		mask;
} ip_addr_mask_t;

enum {
    ipconfig_method_data_flags_ignore_link_status_e = 0x1
};

/*
 * Type: ipconfig_method_data_t
 * Purpose:
 *   Communicate the configuration data from ipconfig tool to IPConfiguration.
 *   Also used internally by IPConfiguration to communicate the config to
 *   the various methods.
 * Note:
 *   This structure is not very flexible, and should probably change to have
 *   a specific data structure for each configuration method.
 */
typedef struct {
    uint8_t		n_ip;
    uint8_t		n_dhcp_client_id;
    uint8_t		reserved_0;
    uint8_t		flags;
    union {
	struct in_addr	manual_router;
	uint32_t	failover_timeout;
    } u;
    ip_addr_mask_t	ip[0];
    /* 
    char		dhcp_client_id[0];
    */
} ipconfig_method_data_t;
#define IPCONFIG_METHOD_DATA_MIN_SIZE	(sizeof(ipconfig_method_data_t) + sizeof(ip_addr_mask_t))
#endif _S_IPCONFIG_TYPES_H
