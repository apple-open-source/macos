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
	"INFORM"
    };
    if (m < 0 || m >= ipconfig_method_last_e)
	return ("<unknown>");
    return (str[m]);
}

typedef struct {
    unsigned char	n_ip;
    unsigned char	n_dhcp_client_id;
    unsigned char	reserved_0;
    unsigned char	reserved_1;
    unsigned long	reserved_2;
    struct {
	struct in_addr	addr;
	struct in_addr	mask;
    } ip[0];
    /* 
    char		dhcp_client_id[0];
    */
} ipconfig_method_data_t;
