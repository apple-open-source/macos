/*
 * Copyright (c) 2000-2014 Apple Inc. All rights reserved.
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

#ifndef _S_IPCONFIG_TYPES_H
#define _S_IPCONFIG_TYPES_H

#include <sys/types.h>
#include <netinet/in.h>

#ifdef mig_external
#undef mig_external
#endif
#define mig_external __private_extern__

#define IPCONFIG_IF_ANY		""
#define MAX_IF_NAMELEN 		32
#define MAX_INLINE_DATA 	2048

typedef char if_name_t[MAX_IF_NAMELEN];
typedef char inline_data_t[MAX_INLINE_DATA];
typedef uint8_t * xmlData_t;
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
    ipconfig_status_not_found_e = 17,
    ipconfig_status_resource_unavailable_e = 18,
    ipconfig_status_network_changed_e = 19,
    ipconfig_status_last_e,
} ipconfig_status_t;

static __inline__ const char *
ipconfig_status_string(ipconfig_status_t status)
{
    static const char * str[] = {
	"success",
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
	"not found",
	"resource unavailable",
	"network changed"
    };
    if (status >= ipconfig_status_last_e)
	return ("<unknown>");
    return (str[status]);
}
    
#endif /* _S_IPCONFIG_TYPES_H */
