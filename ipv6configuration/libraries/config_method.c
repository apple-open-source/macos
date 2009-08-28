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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>

#include "configthreads_common.h"
#include "ip6config_utils.h"
#include "globals.h"
#include "config_method.h"
#include "interfaces.h"


static ip6config_func_t *
lookup_func(ip6config_method_t method)
{
    switch (method) {
	case ip6config_method_automatic_e:
	case ip6config_method_rtadv_e: {
	    return rtadv_thread;
	    break;
	}
	case ip6config_method_manual_e: {
	    return manual_thread;
	    break;
	}
	case ip6config_method_6to4_e: {
	    return stf_thread;
	    break;
	}
	case ip6config_method_linklocal_e: {
	    return linklocal_thread;
	    break;
	}
	default: {
	    break;
	}
    }
    return (NULL);
}

__private_extern__ ip6config_status_t
config_method_start(Service_t * service_p, ip6config_method_t method,
		    ip6config_method_data_t * data)
{
    start_event_data_t	start_data;
    ip6config_func_t *	func;
    interface_t * 	if_p = service_interface(service_p);

    /*	This is checking to make sure that we can perform some basic functions
     *	over the given interface. For everything but 6to4 we need to be able
     *	to multicast, and we need to filter out loopback except on manual configs.
     */

    if (!(if_flags(if_p) & IFF_MULTICAST)) {
	switch (method) {
	    case ip6config_method_6to4_e: {
		break;
	    }
	    default: {
		if (if_ift_type(if_p) != IFT_L2VLAN) {
		    return (ip6config_status_invalid_operation_e);
		}
	    }
	}
    }
    if (if_flags(if_p) & IFF_LOOPBACK) {
	switch (method) {
	    case ip6config_method_automatic_e:
	    case ip6config_method_rtadv_e:
	    case ip6config_method_6to4_e: {
		my_log(LOG_ERR, "CONFIG_METHOD_START %s: invalid config method for loopback",
		       if_name(if_p));
		return (ip6config_status_invalid_operation_e);
		break;
	    }
	    default:
		break;
	}
    }

    func = lookup_func(method);
    if (func == NULL) {
	return (ip6config_status_operation_not_supported_e);
    }
    start_data.config.data = data;
    return (*func)(service_p, IFEventID_start_e, &start_data);
}

__private_extern__ ip6config_status_t
config_method_state_change(Service_t * service_p, ip6_addrinfo_list_t * ip6_addrs)
{
    ip6config_func_t *	func;
    ip6config_status_t	status;

    func = lookup_func(service_p->method);
    if (func == NULL) {
	return (ip6config_status_operation_not_supported_e);
    }

    status = (*func)(service_p, IFEventID_state_change_e, ip6_addrs);
    return (status);
}

__private_extern__ ip6config_status_t
config_method_change(Service_t * service_p, ip6config_method_t method,
		     ip6config_method_data_t * data,
		     boolean_t * needs_stop)
{
    change_event_data_t	change_data;
    ip6config_func_t *	func;
    ip6config_status_t	status;

    *needs_stop = FALSE;

    func = lookup_func(method);
    if (func == NULL) {
	return (ip6config_status_operation_not_supported_e);
    }
    change_data.config.data = data;
    change_data.needs_stop = FALSE;
    status = (*func)(service_p, IFEventID_change_e, &change_data);
    *needs_stop = change_data.needs_stop;
    return (status);
}

__private_extern__ ip6config_status_t
config_method_ipv4_primary_change(Service_t * service_p, ip6config_method_t method,
		    ip6config_method_data_t * data)
{
    start_event_data_t	start_data;
    ip6config_func_t *	func;

    func = lookup_func(method);
    if (func == NULL) {
	return (ip6config_status_operation_not_supported_e);
    }
    start_data.config.data = data;
    return (*func)(service_p, IFEventID_ipv4_primary_change_e, &start_data);
}

static ip6config_status_t
config_method_event(Service_t * service_p, IFEventID_t event)
{
    ip6config_status_t	status = ip6config_status_success_e;
    ip6config_func_t *	func;
    ip6config_method_t	method = service_p->method;

    func = lookup_func(method);
    if (func == NULL) {
	SCLog(TRUE, LOG_INFO,
	      CFSTR("config_method_event(%s): lookup_func(%d) failed"),
	      IFEventID_names(event), method);
	status = ip6config_status_internal_error_e;
	goto done;
    }

    (*func)(service_p, event, NULL);

 done:
    return (status);

}

__private_extern__ ip6config_status_t
config_method_stop(Service_t * service_p)
{
    return (config_method_event(service_p, IFEventID_stop_e));
}

__private_extern__ ip6config_status_t
config_method_media(Service_t * service_p)
{
    return (config_method_event(service_p, IFEventID_media_e));
}

static boolean_t
ip6config_method_from_cfstring(CFStringRef m, ip6config_method_t * method)
{
    if (CFEqual(m, kSCValNetIPv6ConfigMethodManual)) {
	*method = ip6config_method_manual_e;
    }
    else if (CFEqual(m, kSCValNetIPv6ConfigMethodAutomatic)
		|| CFEqual(m, kSCValNetIPv6ConfigMethodRouterAdvertisement)) {
	*method = ip6config_method_rtadv_e;
    }
    else if (CFEqual(m, kSCValNetIPv6ConfigMethod6to4)) {
	*method = ip6config_method_6to4_e;
    }
    else {
	return (FALSE);
    }
    return (TRUE);
}

static void
ip6config_get_ip4_addresses(CFArrayRef addresses, stf_method_data_t * stf_data)
{
    int	count;
    int	i;

    count = CFArrayGetCount(addresses);
    if (count == 0) {
	my_log(LOG_ERR, "ip6config: 6TO4 - ipv4 address array empty");
	stf_data->ip4_addrs_list = NULL;
	return;
    }

    stf_data->ip4_addrs_list = (struct in_addr *) calloc(count, sizeof(struct in_addr));
    if (stf_data->ip4_addrs_list == NULL) {
	my_log(LOG_ERR,
	       "ip6config: malloc stf_data->ip4_addrs_list failed");
	return;
    }

    stf_data->n_ip4 = count;

    for (i = 0; i < count; i++) {
	struct in_addr tmp_addr = { 0 };

	cfstring_to_numeric(AF_INET, CFArrayGetValueAtIndex(addresses, i), &tmp_addr);
	memcpy(&(stf_data->ip4_addrs_list[i]), &tmp_addr, sizeof(struct in_addr));
    }

    return;
}

__private_extern__ int
ip6config_get_6to4_address_data(SCDynamicStoreRef session,
	    ip6config_method_data_t * method_data)
{
    CFStringRef		ipv4_global_key = NULL;
    CFDictionaryRef	ipv4_global_dict = NULL;
    CFStringRef		primary_service = NULL;
    CFStringRef		service_key = NULL;
    CFDictionaryRef	ipv4_service_dict = NULL;
    CFArrayRef		addresses = NULL;
    int			error = -1;

    /* get State:/Network/Global/IPv4 */
    ipv4_global_key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
								 kSCDynamicStoreDomainState,
								 kSCEntNetIPv4);
    if (ipv4_global_key == NULL) {
	goto done;
    }

    ipv4_global_dict = my_SCDynamicStoreCopyValue(session, ipv4_global_key);
    if (ipv4_global_dict == NULL) {
	goto done;
    }

    /* get primary IPv4 service */
    primary_service = CFDictionaryGetValue(ipv4_global_dict, kSCDynamicStorePropNetPrimaryService);
    if (primary_service == NULL) {
	goto done;
    }

    /* get State:/Network/Service/<primary_service>/IPv4 */
    service_key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							      kSCDynamicStoreDomainState,
							      primary_service,
							      kSCEntNetIPv4);
    if (service_key == NULL) {
	goto done;
    }

    ipv4_service_dict = my_SCDynamicStoreCopyValue(session, service_key);
    if (ipv4_service_dict == NULL) {
	goto done;
    }

    addresses = CFDictionaryGetValue(ipv4_service_dict, kSCPropNetIPv4Addresses);
    if (addresses == NULL || isA_CFArray(addresses) == NULL) {
	goto done;
    }

    ip6config_get_ip4_addresses(addresses, &method_data->stf_data);
    if (method_data->stf_data.ip4_addrs_list != NULL) {
	error = 0;
    }

done:
    my_CFRelease(&ipv4_global_key);
    my_CFRelease(&ipv4_global_dict);
    my_CFRelease(&service_key);
    my_CFRelease(&ipv4_service_dict);

    return (error);
}

static int
ip6config_6to4_relay_is_numeric_address(CFStringRef address,
	    relay_address_t * relay_address)
{
    struct in6_addr	tmp6_addr;
    struct in_addr	tmp4_addr;
    int			err;

    if ((err = cfstring_to_numeric(AF_INET6, address, &tmp6_addr)) == 0) {
	relay_address->addr_type = relay_address_type_ipv6_e;
	memcpy(&relay_address->relay_address_u.ip6_relay_addr,
		&tmp6_addr, sizeof(struct in6_addr));
    }
    else {
	my_log(LOG_DEBUG, "ip6config_6to4_relay_is_numeric_address: NOT IP6 ADDRESS");
	if ((err = cfstring_to_numeric(AF_INET, address, &tmp4_addr)) == 0) {
	    relay_address->addr_type = relay_address_type_ipv4_e;
	    relay_address->relay_address_u.ip4_relay_addr = tmp4_addr;
	}
	else {
	    my_log(LOG_DEBUG, "ip6config_6to4_relay_is_numeric_address: NOT IP4 ADDRESS");
	}
    }

    return (err);
}

static int
ip6config_get_6to4_relay_address(CFStringRef address,
		relay_address_t * relay_address)
{
    if (isA_CFString(address) == NULL) {
	my_log(LOG_DEBUG, "ip6config_get_6to4_relay_address: address not CFString");
	return (-1);
    }

    if (ip6config_6to4_relay_is_numeric_address(address, relay_address) != 0) {
	int	len;
	char	buf[256];

	len = cfstring_to_cstring(address, buf, sizeof(buf));

	if (len == 0) {
	    my_log(LOG_DEBUG,
		   "ip6config_get_6to4_relay_address: length of address is 0");
	    return (-1);
	}

	relay_address->relay_address_u.dns_relay_addr = malloc(len + 1);
	if (relay_address->relay_address_u.dns_relay_addr == NULL) {
	    my_log(LOG_DEBUG, "ip6config_get_6to4_relay_address: malloc relay failed");
	    return (-1);
	}

	relay_address->addr_type = relay_address_type_dns_e;
	memcpy(relay_address->relay_address_u.dns_relay_addr, buf, (len + 1));
    }

    return (0);
}

#define DEFAULT_PREFIX_LENGTH 64
/* from RFC3068 */
#define DEFAULT_6TO4_RELAY_ADDRESS_INIT \
	{{{ 0x20, 0x02, 0xc0, 0x58, 0x63, 0x01, 0x00, 0x00, \
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }}}

__private_extern__ ip6config_method_data_t*
ip6config_method_data_from_dict(CFDictionaryRef dict,
			       ip6config_method_t * method)
{
    CFStringRef			config_method;
    int				count = 0;
    int				i;
    CFArrayRef			addresses = NULL;
    CFArrayRef			prefixLens = NULL;
    CFStringRef			relay_address = NULL;
    ip6config_method_data_t *	method_data = NULL;
    int				method_data_len = 0;

    config_method = CFDictionaryGetValue(dict, kSCPropNetIPv6ConfigMethod);
    if (config_method == NULL || ip6config_method_from_cfstring(config_method, method) == FALSE) {
	my_log(LOG_ERR, "ip6config: configuration method is missing/invalid");
	goto error;
    }

    if (*method == ip6config_method_6to4_e) {
	method_data_len = sizeof(*method_data) +
			  (count * sizeof(method_data->ip6[0]));
	method_data = (ip6config_method_data_t *) malloc(method_data_len);
	if (method_data == NULL) {
	    my_log(LOG_ERR,
		   "ip6config: malloc method_data failed");
	    goto error;
	}

	bzero(method_data, method_data_len);
	method_data->n_ip6 = count;

	relay_address = CFDictionaryGetValue(dict, kSCPropNet6to4Relay);
	if (relay_address == NULL) {
	    /* by default, we use the 6to4 anycast address */
	    struct in6_addr	tmp6_addr = DEFAULT_6TO4_RELAY_ADDRESS_INIT;

	    method_data->stf_data.relay_address.addr_type = relay_address_type_ipv6_e;
	    memcpy(&method_data->stf_data.relay_address.relay_address_u.ip6_relay_addr,
		   &tmp6_addr, sizeof(struct in6_addr));
	}
	else if (ip6config_get_6to4_relay_address(relay_address,
						  &method_data->stf_data.relay_address) != 0) {
	    my_log(LOG_INFO, "ip6config: bad 6to4 relay address");
	    goto error;
	}
    }
    else {
	addresses = CFDictionaryGetValue(dict, kSCPropNetIPv6Addresses);
	prefixLens = CFDictionaryGetValue(dict, kSCPropNetIPv6PrefixLength);

	/* All must be same size */
	if (addresses) {
	    if (isA_CFArray(addresses)) {
		count = CFArrayGetCount(addresses);
		if (count == 0) {
		    my_log(LOG_ERR, "ip6configd: address array empty");
		    goto error;
		}
	    }
	    if (prefixLens) {
		if (isA_CFArray(prefixLens)) {
		    if (count != CFArrayGetCount(prefixLens)) {
			my_log(LOG_ERR,
			       "ip6configd: address/prefix arrays not same size");
			goto error;
		    }
		}
		else {
		    prefixLens = NULL;
		}
	    }
	}

	if (*method == ip6config_method_manual_e && addresses == NULL) {
	    my_log(LOG_ERR,
		   "ip6configd: manual method requires an address");
	    goto error;
	}
	method_data_len = sizeof(*method_data) +
			  (count * sizeof(method_data->ip6[0]));
	method_data = (ip6config_method_data_t *) malloc(method_data_len);
	if (method_data == NULL) {
	    my_log(LOG_ERR,
		   "ip6config: malloc method_data failed");
	    goto error;
	}

	bzero(method_data, method_data_len);
	method_data->n_ip6 = count;

	for (i = 0; i < count; i++) {
	    struct in6_addr tmp_addr = IN6ADDR_ANY_INIT;

	    cfstring_to_numeric(AF_INET6, CFArrayGetValueAtIndex(addresses, i), &tmp_addr);
	    memcpy(&(method_data->ip6[i].addr), &tmp_addr, sizeof(struct in6_addr));

	    if (prefixLens) {
		CFNumberRef	val = isA_CFNumber(CFArrayGetValueAtIndex(prefixLens, i));

		if (val) {
		    CFNumberGetValue(val, kCFNumberIntType, &(method_data->ip6[i].prefixLen));
		}
		else {
		    my_log(LOG_ERR, "ip6config: error getting prefixlen");
		    goto error;
		}
	    }
	    else {
		/* we use the default if none was specified - this is a
		 * workaround for Radar 3227716.
		 */
		method_data->ip6[i].prefixLen = DEFAULT_PREFIX_LENGTH;
	    }
	}
    }

    return (method_data);

 error:
    if (method_data)
	free(method_data);
    return (NULL);
}

__private_extern__ int
ip6config_address_data_from_state(CFDictionaryRef dict,
	    ip6_addrinfo_list_t * ip6_addrs)
{
    int		i, count;
    CFArrayRef	addresses = NULL;
    CFArrayRef	prefixLens = NULL;
    CFArrayRef	flags = NULL;

    addresses = CFDictionaryGetValue(dict, kSCPropNetIPv6Addresses);
    prefixLens = CFDictionaryGetValue(dict, kSCPropNetIPv6PrefixLength);
    flags = CFDictionaryGetValue(dict, kSCPropNetIPv6Flags);

    /* All must be same size */
    if (!addresses || !prefixLens || !flags) {
	my_log(LOG_DEBUG,
	    "ip6config_address_data_from_state: no addresses/prefixlens/flags in dictionary");
	return (-1);
    }

    count = CFArrayGetCount(addresses);
    if (count == 0) {
	my_log(LOG_ERR,
	    "ip6config_address_data_from_state: address array empty");
	return (-1);
    }
    if (count != CFArrayGetCount(prefixLens)) {
	my_log(LOG_ERR,
	    "ip6config_address_data_from_state: address/prefixlen arrays not same size");
	return (-1);
    }
    if (count != CFArrayGetCount(flags)) {
	my_log(LOG_ERR,
	    "ip6config_address_data_from_state: address/flags arrays not same size");
	return (-1);
    }

    ip6_addrs->addr_list = calloc(count, sizeof(*ip6_addrs->addr_list));
    if (ip6_addrs->addr_list == NULL) {
	my_log(LOG_DEBUG, "ip6config_address_data_from_state: error allocating addr_list");
	return (-1);
    }

    for (i = 0; i < count; i++) {
	CFNumberRef	val;
	struct in6_addr tmp_addr = IN6ADDR_ANY_INIT;

	cfstring_to_numeric(AF_INET6, CFArrayGetValueAtIndex(addresses, i), &tmp_addr);
	memcpy(&ip6_addrs->addr_list[i].addr, &tmp_addr, sizeof(struct in6_addr));

	val = isA_CFNumber(CFArrayGetValueAtIndex(prefixLens, i));
	if (val) {
	    CFNumberGetValue(val, kCFNumberIntType, &ip6_addrs->addr_list[i].prefixlen);
	}

	val = isA_CFNumber(CFArrayGetValueAtIndex(flags, i));
	if (val) {
	    CFNumberGetValue(val, kCFNumberIntType, &ip6_addrs->addr_list[i].flags);
	}
    }

    ip6_addrs->n_addrs = count;
    return (0);
}

