/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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
 * DHCPv6PDService.h
 * - API to request a prefix using DHCPv6 Prefix Delegation
 */

#ifndef _DHCPV6PDSERVICE_H
#define _DHCPV6PDSERVICE_H

#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFString.h>
#include <netinet/in.h>
#include <stdint.h>
#include <dispatch/dispatch.h>

typedef struct __DHCPv6PDService * DHCPv6PDServiceRef;

CFTypeID
DHCPv6PDServiceGetTypeID(void);

/*
 * Type: DHCPv6PDServiceInfoRef
 *
 * Purpose:
 *   When non-NULL, conveys information about the delegated prefix.
 *   Used in combination with accessor functions  DHCPv6PDServiceInfoGet*().
 * Note:
 *   This is not a CF object.
 */
typedef const struct DHCPv6PDServiceInfo * DHCPv6PDServiceInfoRef;


/*
 * Function: DHCPv6PDServiceInfoGetPrefix
 *
 * Purpose:
 *   Copies the IPv6 prefix from the DHCPv6PDServiceInfoRef to the specified
 *   prefix address.
 *
 * Parameters:
 *   info		non-NULL DHCPv6PDServiceInfoRef
 *   prefix		non-NULL prefix address
 */
void
DHCPv6PDServiceInfoGetPrefix(DHCPv6PDServiceInfoRef info,
			     struct in6_addr * prefix);

/*
 * Function: DHCPv6PDServiceInfoGetPrefixLength
 *
 * Purpose:
 *   Returns the prefix length from the DHCPv6PDServiceInfoRef.
 *
 * Parameters:
 *   info		non-NULL DHCPv6PDServiceInfoRef
 */
uint8_t
DHCPv6PDServiceInfoGetPrefixLength(DHCPv6PDServiceInfoRef info);

/*
 * Function: DHCPv6PDServiceInfoGetPrefixValidLifetime
 *
 * Purpose:
 *   Returns the prefix's valid lifetime from the DHCPv6PDServiceInfoRef.
 *
 * Parameters:
 *   info		non-NULL DHCPv6PDServiceInfoRef
 */
uint32_t
DHCPv6PDServiceInfoGetPrefixValidLifetime(DHCPv6PDServiceInfoRef info);

/*
 * Function: DHCPv6PDServiceInfoGetPrefixPreferredLifetime
 *
 * Purpose:
 *   Returns the prefix's preferred lifetime from the DHCPv6PDServiceInfoRef.
 *
 * Parameters:
 *   info		non-NULL DHCPv6PDServiceInfoRef
 */
uint32_t
DHCPv6PDServiceInfoGetPrefixPreferredLifetime(DHCPv6PDServiceInfoRef info);

/*
 * Function: DHCPv6PDServiceInfoGetOptionData
 *
 * Purpose:
 *   Return the DHCPv6 option data for the specified `option_code`.
 *   from the DHDPv6ServiceInfoRef. The format of the returned data
 *   is a CFArray[CFData].
 *
 *   Returns NULL if that option is not present.
 *
 * Parameters:
 *   info		non-NULL DHCPv6PDServiceInfoRef
 *   option_code	DHCPv6 option code
 *
 * Returns:
 *   Non-NULL CFArray[CFData] if present, NULL otherwise.
 */
CFArrayRef /* of CFDataRef */
DHCPv6PDServiceInfoGetOptionData(DHCPv6PDServiceInfoRef info,
				 uint16_t option_code);

/*
 * Block: DHCPv6PDServiceHandler
 *
 * Purpose:
 *   Called to provide updates on the prefix request.
 *   Use the accessor functions DHCPv6PDServiceInfoGet*() to retrieve
 *   information from the non-NULL `info` parameter.
 *
 * Parameters:
 *   valid		Indicates whether service is still valid.
 *			See Notes below.
 *   service_info	Pointer to service information, NULL if that
 *			information is not currently available.
 *			Note that `service_info` is not a CF object.
 *   info		For future use, always NULL.
 *
 * Notes:
 * - If `valid` is TRUE, and `info` is non-NULL, a prefix has been
 *   allocated or maintained.
 * - If `valid` is TRUE, but `info` is NULL, the link status could have
 *   become inactive, or the DHCPv6 server stopped responding before the
 *   lease could be extended.
 * - If `valid` is FALSE, `info` will also be NULL.
 *   This can happen if the initial request could not be completed
 *   (e.g. non-existent interface, duplicate service), or if the service
 *   becomes invalid at some later point (e.g. the interface detached).
 *   In this case, the service is no longer useful and should be released.
 */
typedef void
(^DHCPv6PDServiceHandler)(Boolean valid, DHCPv6PDServiceInfoRef service_info,
			  CFDictionaryRef info);


/*
 * Function: DHCPv6PDServiceCreate
 *
 * Purpose:
 *   Allocate a DHCPv6PDServiceRef object to acquire an IPv6 prefix.
 *   You should call DHCPv6PDServiceSetHandler() to arrange to receive
 *   the prefix information, and subsequently call DHCPv6PDServiceResume()
 *   to initiate the request.
 *
 *      DHCPv6PDServiceRef	dhcp;
 *	DHCPv6PDServiceHandler  handler;
 *
 *	dhcp = DHCPv6PDServiceCreate(...);
 *      handler = ^( ... ) { ... };
 *	DHCPv6PDServiceSetHandler(dhcp, queue, handler);
 *      DHCPv6PDServiceResume(dhcp);
 *
 * Parameters:
 *   interface_name	Name of interface to perform prefix delegation.
 *   prefix		Desired prefix if one is known, NULL otherwise.
 *   prefix_length	Desired prefix length, 0 if no specific length required.
 *   options		For future use. Must be NULL.
 *
 * Returns:
 *   A non-NULL CF object if the object was successfully allocated, NULL
 *   if an error occurred.
 */
DHCPv6PDServiceRef
DHCPv6PDServiceCreate(CFStringRef interface_name,
		      const struct in6_addr * prefix,
		      uint8_t prefix_length,
		      CFDictionaryRef options);
/*
 * Function: DHCPv6PDServiceSetQueueAndHandler
 *
 * Purpose:
 *   Set the dispatch queue and callback to be invoked when prefix results
 *   are ready, or change. See Notes below.
 *
 * Parameters:
 *   service		Object returned by DHCPv6PDServiceCreate().
 *   queue		Dispatch queue to schedule the handler on.
 *			If NULL, handler will be scheduled on the default
 *			global queue.
 *   handler		Callback to call when prefix results are available,
 *			or an error occurs.
 * Notes:
 *   You must adhere to the expected calling sequence described in
 *   DHCPv6PDServiceCreate() above.
 */
void
DHCPv6PDServiceSetQueueAndHandler(DHCPv6PDServiceRef dhcp,
				  dispatch_queue_t queue,
				  DHCPv6PDServiceHandler handler);

/*
 * Function: DHCPv6PDServiceResume
 *
 * Purpose:
 *   Attempts to schedule the DHCPv6 prefix delegation request.
 */
void
DHCPv6PDServiceResume(DHCPv6PDServiceRef service);

#endif /* _DHCPV6PDSERVICE_H */
