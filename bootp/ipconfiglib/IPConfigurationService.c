/*
 * Copyright (c) 2011-2020 Apple Inc. All rights reserved.
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
 * IPConfigurationService.c
 * - API to communicate with IPConfiguration to instantiate services
 */
/* 
 * Modification History
 *
 * April 14, 2011 	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <CoreFoundation/CFRuntime.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCDynamicStorePrivate.h>
#include <pthread.h>
#include <mach/mach_error.h>
#include <mach/mach_port.h>
#include <libkern/OSAtomic.h>
#include "ipconfig_types.h"
#include "ipconfig_ext.h"
#include "symbol_scope.h"
#include "cfutil.h"
#include "ipconfig.h"
#include "IPConfigurationLog.h"
#include "IPConfigurationServiceInternal.h"
#include "IPConfigurationService.h"

const CFStringRef 
kIPConfigurationServiceOptionEnableDAD = _kIPConfigurationServiceOptionEnableDAD;

const CFStringRef
kIPConfigurationServiceOptionEnableCLAT46 = _kIPConfigurationServiceOptionEnableCLAT46;

const CFStringRef
kIPConfigurationServiceOptionMTU = _kIPConfigurationServiceOptionMTU;

const CFStringRef
kIPConfigurationServiceOptionPerformNUD = _kIPConfigurationServiceOptionPerformNUD;

const CFStringRef
kIPConfigurationServiceOptionIPv6Entity = _kIPConfigurationServiceOptionIPv6Entity;

const CFStringRef
kIPConfigurationServiceOptionIPv6LinkLocalAddress = CFSTR("IPv6LinkLocalAddress");

const CFStringRef
kIPConfigurationServiceOptionAPNName = _kIPConfigurationServiceOptionAPNName;

const CFStringRef
kIPConfigurationServiceOptionIPv4Entity = _kIPConfigurationServiceOptionIPv4Entity;

#ifndef kSCPropNetIPv6LinkLocalAddress
STATIC const CFStringRef kSCPropNetIPv6LinkLocalAddress = CFSTR("LinkLocalAddress");
#define kSCPropNetIPv6LinkLocalAddress	kSCPropNetIPv6LinkLocalAddress
#endif /* kSCPropNetIPv6LinkLocalAddress */


/**
 ** ObjectWrapper
 **/

typedef struct {
    const void *	obj;
    int32_t		retain_count;
} ObjectWrapper, * ObjectWrapperRef;

STATIC const void *
ObjectWrapperRetain(const void * info)
{
    ObjectWrapperRef 	wrapper = (ObjectWrapperRef)info;

    (void)OSAtomicIncrement32(&wrapper->retain_count);
#ifdef DEBUG
    printf("wrapper retain (%d)\n", new_val);
#endif
    return (info);
}

STATIC ObjectWrapperRef
ObjectWrapperAlloc(const void * obj)
{
    ObjectWrapperRef	wrapper;

    wrapper = (ObjectWrapperRef)malloc(sizeof(*wrapper));
    wrapper->obj = obj;
    wrapper->retain_count = 1;
    return (wrapper);
}

STATIC void
ObjectWrapperRelease(const void * info)
{
    int32_t		new_val;
    ObjectWrapperRef 	wrapper = (ObjectWrapperRef)info;

    new_val = OSAtomicDecrement32(&wrapper->retain_count);
#ifdef DEBUG
    printf("wrapper release (%d)\n", new_val);
#endif
    if (new_val == 0) {
#ifdef DEBUG
	printf("wrapper free\n");
#endif
	free(wrapper);
    }
    else if (new_val < 0) {
	IPConfigLogFL(LOG_NOTICE,
		      "IPConfigurationService: retain count already zero");
	abort();
    }
    return;
}

/**
 ** IPConfigurationService
 **/
struct __IPConfigurationService {
    CFRuntimeBase		cf_base;

    InterfaceName		ifname;
    mach_port_t			server;
    SCDynamicStoreRef		store;
    dispatch_queue_t		queue;
    ServiceID			service_id;
    CFStringRef			store_key;
    ObjectWrapperRef		wrapper;
    CFDictionaryRef		config_dict;
    Boolean			is_ipv6;
    Boolean			need_reconnect;
};

/**
 ** CF object glue code
 **/
STATIC CFStringRef	__IPConfigurationServiceCopyDebugDesc(CFTypeRef cf);
STATIC void		__IPConfigurationServiceDeallocate(CFTypeRef cf);

STATIC CFTypeID __kIPConfigurationServiceTypeID = _kCFRuntimeNotATypeID;

STATIC const CFRuntimeClass __IPConfigurationServiceClass = {
    0,						/* version */
    "IPConfigurationService",			/* className */
    NULL,					/* init */
    NULL,					/* copy */
    __IPConfigurationServiceDeallocate,		/* deallocate */
    NULL,					/* equal */
    NULL,					/* hash */
    NULL,					/* copyFormattingDesc */
    __IPConfigurationServiceCopyDebugDesc	/* copyDebugDesc */
};

STATIC CFStringRef
__IPConfigurationServiceCopyDebugDesc(CFTypeRef cf)
{
    CFAllocatorRef		allocator = CFGetAllocator(cf);
    CFMutableStringRef		result;
    IPConfigurationServiceRef	service = (IPConfigurationServiceRef)cf;

    result = CFStringCreateMutable(allocator, 0);
    CFStringAppendFormat(result, NULL,
			 CFSTR("<IPConfigurationService %p [%p]> {"),
			 cf, allocator);
    CFStringAppendFormat(result, NULL, CFSTR("ifname = %s, serviceID = %s"),
			 service->ifname, service->service_id);
    CFStringAppend(result, CFSTR("}"));
    return (result);
}


STATIC void
__IPConfigurationServiceDeallocate(CFTypeRef cf)
{
    IPConfigurationServiceRef 	service = (IPConfigurationServiceRef)cf;

    if (service->wrapper != NULL) {
	if (service->queue != NULL) {
	    /* ensure disconnect code won't run anymore */
	    dispatch_sync(service->queue,
			  ^{
			      service->wrapper->obj = NULL;
			  });
	}
	ObjectWrapperRelease(service->wrapper);
	service->wrapper = NULL;
    }
    if (service->store != NULL) {
	SCDynamicStoreSetDispatchQueue(service->store, NULL);
	my_CFRelease(&service->store);
    }
    if (service->queue != NULL) {
	dispatch_release(service->queue);
	service->queue = NULL;
    }
    if (service->server != MACH_PORT_NULL) {
	kern_return_t		kret;
	ipconfig_status_t	status;

	kret = ipconfig_remove_service_on_interface(service->server,
						    service->ifname,
						    service->service_id,
						    &status);
	if (kret != KERN_SUCCESS) {
	    IPConfigLogFL(LOG_NOTICE,
			  "ipconfig_remove_service_on_interface(%s %s) "
			  "failed, %s",
			  service->ifname, service->service_id,
			  mach_error_string(kret));
	}
	else if (status != ipconfig_status_success_e) {
	    IPConfigLogFL(LOG_NOTICE,
			  "ipconfig_remove_service_on_interface(%s %s)"
			  " failed: %s",
			  service->ifname, service->service_id,
			  ipconfig_status_string(status));
	}
	mach_port_deallocate(mach_task_self(), service->server);
	service->server = MACH_PORT_NULL;
    }
    my_CFRelease(&service->config_dict);
    my_CFRelease(&service->store_key);

    return;
}

STATIC void
__IPConfigurationServiceInitialize(void)
{
    /* initialize runtime */
    __kIPConfigurationServiceTypeID 
	= _CFRuntimeRegisterClass(&__IPConfigurationServiceClass);
    return;
}

STATIC void
__IPConfigurationServiceRegisterClass(void)
{
    STATIC pthread_once_t	initialized = PTHREAD_ONCE_INIT;

    pthread_once(&initialized, __IPConfigurationServiceInitialize);
    return;
}

STATIC IPConfigurationServiceRef
__IPConfigurationServiceAllocate(CFAllocatorRef allocator)
{
    IPConfigurationServiceRef	service;
    int				size;

    __IPConfigurationServiceRegisterClass();

    size = sizeof(*service) - sizeof(CFRuntimeBase);
    service = (IPConfigurationServiceRef)
	_CFRuntimeCreateInstance(allocator,
				 __kIPConfigurationServiceTypeID, size, NULL);
    bzero(((void *)service) + sizeof(CFRuntimeBase), size);
    return (service);
}

/**
 ** Utility functions
 **/
typedef struct {
    CFDictionaryRef 		ipv6_entity;
    CFBooleanRef		perform_nud;
    CFStringRef			ll_addr;
    CFBooleanRef		enable_dad;
    CFBooleanRef		enable_clat46;
} IPv6Config, * IPv6ConfigRef;

typedef struct {
    CFDictionaryRef 		ipv4_entity;
} IPv4Config, * IPv4ConfigRef;

typedef struct {
    CFBooleanRef 		no_publish;
    CFNumberRef 		mtu;
    CFStringRef			apn_name;
    Boolean			is_ipv6;
    union {
	IPv4Config		v4;
	IPv6Config		v6;
    };
} ConfigParams, * ConfigParamsRef;

STATIC Boolean
plist_validate_array(CFDictionaryRef plist,
		     CFStringRef prop,
		     CFIndex count,
		     CFArrayRef * ret_array)
{
    CFArrayRef		array;
    Boolean		is_valid = FALSE;

    *ret_array = NULL;
    array = CFDictionaryGetValue(plist, prop);
    if (array != NULL) {
	CFIndex		array_count;

	if (isA_CFArray(array) == NULL) {
	    IPConfigLog(LOG_NOTICE, "%@ invalid", prop);
	    goto done;
	}
	array_count = CFArrayGetCount(array);
	if (count != array_count) {
	    IPConfigLog(LOG_NOTICE, "%@ array size %ld != %ld",
			prop, array_count, count);
	    goto done;
	}
    }
    is_valid = TRUE;
    *ret_array = array;

 done:
    return (is_valid);
}

STATIC Boolean
plist_validate_bool(CFDictionaryRef plist,
		    CFStringRef prop,
		    CFBooleanRef * ret_bool)
{
    CFBooleanRef	bool_val;
    Boolean		is_valid = FALSE;

    *ret_bool = NULL;
    bool_val = CFDictionaryGetValue(plist, prop);
    if (bool_val != NULL) {
	if (isA_CFBoolean(bool_val) == NULL) {
	    IPConfigLog(LOG_NOTICE, "%@' invalid", prop);
	    goto done;
	}
    }
    is_valid = TRUE;
    *ret_bool = bool_val;

 done:
    return (is_valid);
}

STATIC CFDictionaryRef
createIPv6Entity(IPv6ConfigRef v6)
{
    CFDictionaryRef	dict;

    if (v6->ipv6_entity != NULL) {
	if (v6->ll_addr != NULL
	    && !CFDictionaryContainsKey(v6->ipv6_entity,
					kSCPropNetIPv6LinkLocalAddress)) {
	    CFMutableDictionaryRef	temp;

	    temp = CFDictionaryCreateMutableCopy(NULL, 0,
						 v6->ipv6_entity);
	    CFDictionarySetValue(temp,
				 kSCPropNetIPv6LinkLocalAddress,
				 v6->ll_addr);
	    dict = temp;
	}
	else {
	    dict = CFRetain(v6->ipv6_entity);
	}
    }
    else {
	int		count;
	const void *	keys[2];
	const void *	values[2];

	count = 0;
	keys[count] = kSCPropNetIPv6ConfigMethod;
	values[count] = kSCValNetIPv6ConfigMethodAutomatic;
	count++;

	/* add the IPv6 link-local address if specified */
	if (v6->ll_addr != NULL) {
	    keys[count] = kSCPropNetIPv6LinkLocalAddress;
	    values[count] = v6->ll_addr;
	    count++;
	}
	/* create the default IPv6 dictionary */
	dict = CFDictionaryCreate(NULL, keys, values, count,
				  &kCFTypeDictionaryKeyCallBacks,
				  &kCFTypeDictionaryValueCallBacks);
    }
    return (dict);
}

STATIC CFDictionaryRef
createIPv4Entity(IPv4ConfigRef v4)
{
    CFDictionaryRef	dict;

    if (v4->ipv4_entity != NULL) {
	dict = CFRetain(v4->ipv4_entity);
    }
    else {
	/* create the default IPv4 dictionary */
	dict = CFDictionaryCreate(NULL,
				  (const void * *)&kSCPropNetIPv4ConfigMethod,
				  (const void * *)&kSCValNetIPv4ConfigMethodDHCP,
				  1,
				  &kCFTypeDictionaryKeyCallBacks,
				  &kCFTypeDictionaryValueCallBacks);
    }
    return (dict);
}


STATIC CFDictionaryRef
config_dict_create(CFStringRef serviceID, ConfigParamsRef params)
{
#define N_KEYS_VALUES	9
    int			count;
    CFDictionaryRef	config_dict;
    CFDictionaryRef 	proto_dict;
    CFStringRef 	proto_key;
    const void *	keys[N_KEYS_VALUES];
    CFDictionaryRef	options;
    const void *	values[N_KEYS_VALUES];

    /* 0 */
    /* monitor pid */    
    count = 0;
    keys[count] = _kIPConfigurationServiceOptionMonitorPID;
    values[count] = kCFBooleanTrue;
    count++;

    /* 1 */
    /* no publish */
    if (params->no_publish == NULL) {
	params->no_publish = kCFBooleanTrue;
    }
    keys[count] = _kIPConfigurationServiceOptionNoPublish;
    values[count] = params->no_publish;
    count++;

    /* 2 */
    /* mtu */
    if (params->mtu != NULL) {
	keys[count] = kIPConfigurationServiceOptionMTU;
	values[count] = params->mtu;
	count++;
    }

    /* 3 */
    /* serviceID */
    keys[count] = _kIPConfigurationServiceOptionServiceID;
    values[count] = serviceID;
    count++;

    /* 4 */
    /* clear state */
    keys[count] = _kIPConfigurationServiceOptionClearState;
    values[count] = kCFBooleanTrue;
    count++;

    /* 5 */
    /* APN name */
    if (params->apn_name != NULL) {
	keys[count] = kIPConfigurationServiceOptionAPNName;
	values[count] = params->apn_name;
	count++;
    }
    if (params->is_ipv6) {
	/* 6 */
	/* perform NUD */
	if (params->v6.perform_nud != NULL) {
	    keys[count] = kIPConfigurationServiceOptionPerformNUD;
	    values[count] = params->v6.perform_nud;
	    count++;
	}

	/* 7 */
	/* enable DAD */
	if (params->v6.enable_dad != NULL) {
	    keys[count] = kIPConfigurationServiceOptionEnableDAD;
	    values[count] = params->v6.enable_dad;
	    count++;
	}

	/* 8 */
	/* enable CLAT46 */
	if (params->v6.enable_clat46 != NULL) {
	    keys[count] = kIPConfigurationServiceOptionEnableCLAT46;
	    values[count] = params->v6.enable_clat46;
	    count++;
	}
    }
    options
	= CFDictionaryCreate(NULL, keys, values, count,
			     &kCFTypeDictionaryKeyCallBacks,
			     &kCFTypeDictionaryValueCallBacks);
    if (params->is_ipv6) {
	proto_dict = createIPv6Entity(&params->v6);
	proto_key = kSCEntNetIPv6;
    }
    else {
	proto_dict = createIPv4Entity(&params->v4);
	proto_key = kSCEntNetIPv4;
    }

    count = 0;
    keys[count] = kIPConfigurationServiceOptions;
    values[count] = options;
    count++;

    keys[count] = proto_key;
    values[count] = proto_dict;
    count++;

    config_dict
	= CFDictionaryCreate(NULL, keys, values, count,
			     &kCFTypeDictionaryKeyCallBacks,
			     &kCFTypeDictionaryValueCallBacks);
    CFRelease(options);
    CFRelease(proto_dict);
    return (config_dict);
}

STATIC Boolean
ipv4_config_is_valid(CFDictionaryRef config)
{
    CFIndex		config_count;
    CFStringRef		config_method;
    Boolean		is_valid = FALSE;

    config_method = CFDictionaryGetValue(config, kSCPropNetIPv4ConfigMethod);
    if (isA_CFString(config_method) == NULL) {
	goto done;
    }
    config_count = 1;
    if (CFEqual(config_method, kSCValNetIPv4ConfigMethodManual)) {
	CFArrayRef	addresses;
	CFIndex		count;
	CFArrayRef	dest_addresses;
	CFStringRef	router;
	CFArrayRef	subnet_masks;
	Boolean		valid;

	/* Addresses is required */
	addresses = CFDictionaryGetValue(config, kSCPropNetIPv4Addresses);
	if (isA_CFArray(addresses) == NULL) {
	    IPConfigLog(LOG_NOTICE, "%@ missing/invalid",
			kSCPropNetIPv4Addresses);
	    goto done;
	}
	count = CFArrayGetCount(addresses);
	if (count == 0) {
	    IPConfigLog(LOG_NOTICE, "%@ empty array",
			kSCPropNetIPv4Addresses);
	    goto done;
	}
	config_count++;
	valid = plist_validate_array(config, kSCPropNetIPv4SubnetMasks, count,
				     &subnet_masks);
	if (!valid) {
	    goto done;
	}
	valid = plist_validate_array(config, kSCPropNetIPv4DestAddresses, count,
				     &dest_addresses);
	if (!valid) {
	    goto done;
	}
	if (subnet_masks != NULL) {
	    config_count++;
	}
	if (dest_addresses != NULL) {
	    config_count++;
	}
	router = CFDictionaryGetValue(config, kSCPropNetIPv4Router);
	if (router != NULL) {
	    if (isA_CFString(router) == NULL) {
		IPConfigLog(LOG_NOTICE, "%@ invalid",
			    kSCPropNetIPv4Router);
		goto done;
	    }
	    config_count++;
	}
    }
    else if (CFEqual(config_method, kSCValNetIPv4ConfigMethodDHCP)) {
	CFStringRef	clientID;

	clientID = CFDictionaryGetValue(config, kSCPropNetIPv4DHCPClientID);
	if (clientID != NULL) {
	    if (isA_CFString(clientID) == NULL) {
		IPConfigLog(LOG_NOTICE,
			    "invalid %@",
			    kSCPropNetIPv4DHCPClientID);
		goto done;
	    }
	    config_count++;
	}
    }
    else if (CFEqual(config_method, kSCValNetIPv4ConfigMethodLinkLocal)) {
	/* no other properties required/allowed */
    }
    else {
	goto done;
    }
    if (config_count != CFDictionaryGetCount(config)) {
	IPConfigLog(LOG_NOTICE,
		    "IPv4 entity %@ contains extra properties",
		    config);
	goto done;
    }
    is_valid = TRUE;

 done:
    return (is_valid);
}

STATIC Boolean
ipv6_config_is_valid(CFDictionaryRef config)
{
    CFIndex		config_count;
    CFStringRef		config_method;
    Boolean		is_valid = FALSE;

    config_method = CFDictionaryGetValue(config, kSCPropNetIPv6ConfigMethod);
    if (isA_CFString(config_method) == NULL) {
	goto done;
    }
    config_count = 1;
    if (CFEqual(config_method, kSCValNetIPv6ConfigMethodManual)) {
	CFArrayRef	addresses;
	CFIndex		count;
	CFArrayRef	prefix_lengths;
	CFStringRef	router;

	/* must contain Addresses, PrefixLength */
	addresses = CFDictionaryGetValue(config, kSCPropNetIPv6Addresses);
	prefix_lengths = CFDictionaryGetValue(config, 
					      kSCPropNetIPv6PrefixLength);
	if (isA_CFArray(addresses) == NULL
	    || isA_CFArray(prefix_lengths) == NULL) {
	    IPConfigLog(LOG_NOTICE,
			"IPv6 entity contains invalid %@ or %@",
			kSCPropNetIPv6Addresses,
			kSCPropNetIPv6PrefixLength);
	    goto done;
	}
	count = CFArrayGetCount(addresses);
	if (count == 0) {
	    IPConfigLog(LOG_NOTICE,
			"IPv6 entity contains empty %@",
			kSCPropNetIPv6Addresses);
	    goto done;
	}
	if (count != CFArrayGetCount(prefix_lengths)) {
	    IPConfigLog(LOG_NOTICE,
			"IPv6 %@ and %@ are different sizes",
			kSCPropNetIPv6Addresses,
			kSCPropNetIPv6PrefixLength);
	    goto done;
	}
	config_count += 2;
	router = CFDictionaryGetValue(config, kSCPropNetIPv6Router);
	if (router != NULL) {
	    if (isA_CFString(router) == NULL) {
		IPConfigLog(LOG_NOTICE, "%@ invalid",
			    kSCPropNetIPv6Router);
		goto done;
	    }
	    config_count++;
	}
    }
    else if (CFEqual(config_method, kSCValNetIPv6ConfigMethodAutomatic)
	     || CFEqual(config_method, kSCValNetIPv6ConfigMethodLinkLocal)) {
	/* no other properties required/allowed */
    }
    else {
	goto done;
    }
    if (CFDictionaryGetCount(config) != config_count) {
	IPConfigLog(LOG_NOTICE,
		    "IPv6 entity %@ contains extra properties",
		    config);
	goto done;
    }
    is_valid = TRUE;

 done:
    return (is_valid);
}

STATIC Boolean
init_ipv6_params(CFDictionaryRef options, IPv6ConfigRef v6)
{
    struct in6_addr	ip;
    CFDictionaryRef	ip_dict;
    CFStringRef		ipv6_ll;
    Boolean		is_valid = FALSE;

    if (!plist_validate_bool(options,
			     kIPConfigurationServiceOptionPerformNUD,
			     &v6->perform_nud)) {
	goto done;
    }
    if (!plist_validate_bool(options,
			     kIPConfigurationServiceOptionEnableDAD,
			     &v6->enable_dad)) {
	goto done;
    }
    if (!plist_validate_bool(options,
			     kIPConfigurationServiceOptionEnableCLAT46,
			     &v6->enable_clat46)) {
	goto done;
    }
    ip_dict = CFDictionaryGetValue(options,
				   kIPConfigurationServiceOptionIPv6Entity);
    if (ip_dict != NULL) {
	if (!ipv6_config_is_valid(ip_dict)) {
	    IPConfigLog(LOG_NOTICE, "invalid '%@' option",
			kIPConfigurationServiceOptionIPv6Entity);
	    goto done;
	}
	v6->ipv6_entity = ip_dict;
    }
    ipv6_ll
	= CFDictionaryGetValue(options,
			       kIPConfigurationServiceOptionIPv6LinkLocalAddress);
    if (ipv6_ll != NULL) {
	if (my_CFStringToIPv6Address(ipv6_ll, &ip) == FALSE
	    || IN6_IS_ADDR_LINKLOCAL(&ip) == FALSE) {
	    IPConfigLogFL(LOG_NOTICE, "invalid '%@' option",
			  kIPConfigurationServiceOptionIPv6LinkLocalAddress);
	    goto done;
	}
	v6->ll_addr = ipv6_ll;
    }
    is_valid = TRUE;

 done:
    return (is_valid);
}

STATIC Boolean
init_ipv4_params(CFDictionaryRef options, IPv4ConfigRef v4)
{
    CFDictionaryRef	ip_dict;
    Boolean		is_valid = FALSE;

    ip_dict = CFDictionaryGetValue(options,
				   kIPConfigurationServiceOptionIPv4Entity);
    if (ip_dict != NULL) {
	if (!ipv4_config_is_valid(ip_dict)) {
	    IPConfigLog(LOG_NOTICE, "invalid '%@' option",
			kIPConfigurationServiceOptionIPv4Entity);
	    goto done;
	}
	v4->ipv4_entity = ip_dict;
    }
    is_valid = TRUE;

 done:
    return (is_valid);
}

STATIC ipconfig_status_t
create_service(IPConfigurationServiceRef service, mach_port_t server)
{
    CFDataRef			config_data;
    kern_return_t		kret;
    ipconfig_status_t		status = ipconfig_status_success_e;
    boolean_t			tried_to_delete = FALSE;
    void *			xml_data_ptr = NULL;
    int				xml_data_len = 0;

    config_data
	= CFPropertyListCreateData(NULL,
				   service->config_dict,
				   kCFPropertyListBinaryFormat_v1_0,
				   0,
				   NULL);
    xml_data_ptr = (void *)CFDataGetBytePtr(config_data);
    xml_data_len = (int)CFDataGetLength(config_data);
    while (1) {
	kret = ipconfig_add_service(server, service->ifname,
				    xml_data_ptr, xml_data_len,
				    service->service_id,
				    &status);
	if (kret != KERN_SUCCESS) {
	    IPConfigLogFL(LOG_NOTICE,
			  "ipconfig_add_service(%s) failed, %s",
			  service->ifname, mach_error_string(kret));
	    break;
	}
	if (status == ipconfig_status_success_e) {
	    break;
	}
	if (status != ipconfig_status_duplicate_service_e
	    || tried_to_delete) {
	    IPConfigLogFL(LOG_NOTICE,
			  "ipconfig_add_service(%s) failed: %s",
			  service->ifname, ipconfig_status_string(status));
	    break;
	}
	tried_to_delete = TRUE;
	(void)ipconfig_remove_service(server, service->ifname,
				      xml_data_ptr, xml_data_len,
				      &status);
    }
    CFRelease(config_data);
    return (status);
}

STATIC void
remove_clear_state(IPConfigurationServiceRef service)
{
    CFDictionaryRef	options;

    options = CFDictionaryGetValue(service->config_dict,
				   kIPConfigurationServiceOptions);
    if (options != NULL
	&& CFDictionaryContainsKey(options,
				   _kIPConfigurationServiceOptionClearState)) {
	CFMutableDictionaryRef	config_dict;
	CFMutableDictionaryRef	new_options;

	new_options = CFDictionaryCreateMutableCopy(NULL, 0, options);
	CFDictionaryRemoveValue(new_options,
				_kIPConfigurationServiceOptionClearState);
	config_dict
	     = CFDictionaryCreateMutableCopy(NULL, 0, service->config_dict);
	CFDictionarySetValue(config_dict,
			     kIPConfigurationServiceOptions,
			     new_options);
	CFRelease(new_options);
	CFRelease(service->config_dict);
	service->config_dict = config_dict;
    }
    return;
}

STATIC void
store_reconnect(SCDynamicStoreRef store, void * info)
{
    IPConfigurationServiceRef	service;
    ObjectWrapperRef		wrapper = (ObjectWrapperRef)info;

    service = (IPConfigurationServiceRef)(wrapper->obj);
    if (service == NULL) {
	/* service has been deallocated */
	return;
    }

    /* not ready yet */
    if (service->server == MACH_PORT_NULL) {
	service->need_reconnect = TRUE;
	return;
    }

    /* on re-connect, make sure we don't clear the interface state */
    remove_clear_state(service);
    IPConfigLog(LOG_NOTICE,
		"IPConfigurationService: re-establishing service over %s",
		service->ifname);
    (void)create_service(service, service->server);
    return;
}

STATIC void
store_handle_changes(SCDynamicStoreRef session, CFArrayRef changes, void * arg)
{
    /* not used */
    return;
}

STATIC Boolean
store_init(IPConfigurationServiceRef service, dispatch_queue_t queue)
{
    SCDynamicStoreContext	context = {
	.version = 0,
	.info = NULL,
	.retain = ObjectWrapperRetain,
	.release = ObjectWrapperRelease,
	.copyDescription = NULL
    };
    SCDynamicStoreRef		store;
    ObjectWrapperRef			wrapper;

    wrapper = ObjectWrapperAlloc(service);
    context.info = wrapper;
    store = SCDynamicStoreCreate(NULL,
				 CFSTR("IPConfigurationService"),
				 store_handle_changes,
				 &context);
    if (store == NULL) {
	IPConfigLogFL(LOG_NOTICE,
		      "SCDynamicStoreCreate failed");
    }
    else if (!SCDynamicStoreSetDisconnectCallBack(store,
						  store_reconnect)) {
	IPConfigLogFL(LOG_NOTICE,
		      "SCDynamicStoreSetDisconnectCallBack failed");
    }
    else if (!SCDynamicStoreSetDispatchQueue(store, queue)) {
	IPConfigLogFL(LOG_NOTICE,
		      "SCDynamicStoreSetDispatchQueue failed");
    }
    else {
	service->store = store;
	service->wrapper = wrapper;
    }
    if (service->store == NULL) {
	if (wrapper != NULL) {
	    ObjectWrapperRelease(wrapper);
	}
	my_CFRelease(&store);
    }
    return (service->store != NULL);
}

STATIC void
IPConfigurationServiceSetServiceID(IPConfigurationServiceRef service,
				   CFStringRef serviceID,
				   Boolean is_ipv6,
				   Boolean no_publish)
{
    service->is_ipv6 = is_ipv6;
    if (no_publish) {
	service->store_key = IPConfigurationServiceKey(serviceID);
    }
    else {
	CFStringRef		entity;

	entity = is_ipv6 ? kSCEntNetIPv6 : kSCEntNetIPv4;
	service->store_key =
	    SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							kSCDynamicStoreDomainState,
							serviceID,
							entity);
    }
    ServiceIDInitWithCFString(service->service_id, serviceID);
    return;
}

/**
 ** IPConfigurationService APIs
 **/

CFTypeID
IPConfigurationServiceGetTypeID(void)
{
    __IPConfigurationServiceRegisterClass();
    return (__kIPConfigurationServiceTypeID);
}

STATIC void
init_log(void)
{
    static os_log_t handle;

    if (handle == NULL) {
	handle = os_log_create(kIPConfigurationLogSubsystem,
			       kIPConfigurationLogCategoryLibrary);
	IPConfigLogSetHandle(handle);
    }
    return;
}

IPConfigurationServiceRef
IPConfigurationServiceCreate(CFStringRef interface_name, 
			     CFDictionaryRef options)
{
    kern_return_t		kret;
    Boolean			no_publish = TRUE;
    ConfigParams		params;
    mach_port_t			server = MACH_PORT_NULL;
    IPConfigurationServiceRef	ret_service = NULL;
    IPConfigurationServiceRef	service;
    CFStringRef			serviceID = NULL;
    ipconfig_status_t		status = ipconfig_status_success_e;

    init_log();
    kret = ipconfig_server_port(&server);
    if (kret != BOOTSTRAP_SUCCESS) {
	IPConfigLogFL(LOG_NOTICE,
		      "ipconfig_server_port, %s",
		      mach_error_string(kret));
	return (NULL);
    }

    bzero(&params, sizeof(params));

    /* create the configuration, encapsulated as XML plist data */
    if (options != NULL) {
	/* common options */
	params.no_publish
	    = CFDictionaryGetValue(options,
				   _kIPConfigurationServiceOptionNoPublish);
	if (params.no_publish != NULL) {
	    if (isA_CFBoolean(params.no_publish) == NULL) {
		IPConfigLog(LOG_NOTICE, "invalid '%@' option",
			    _kIPConfigurationServiceOptionNoPublish);
		goto done;
	    }
	    no_publish = CFBooleanGetValue(params.no_publish);
	}
	params.mtu = CFDictionaryGetValue(options,
					  kIPConfigurationServiceOptionMTU);
	if (params.mtu != NULL && isA_CFNumber(params.mtu) == NULL) {
	    IPConfigLog(LOG_NOTICE, "invalid '%@' option",
			kIPConfigurationServiceOptionMTU);
	    goto done;
	}
	params.apn_name
	    = CFDictionaryGetValue(options,
				   kIPConfigurationServiceOptionAPNName);
	if (params.apn_name != NULL
	    && isA_CFString(params.apn_name) == NULL) {
	    IPConfigLogFL(LOG_NOTICE, "invalid '%@' option",
			  kIPConfigurationServiceOptionAPNName);
	    goto done;
	}

	/* IPv4 */
	if (CFDictionaryContainsKey(options,
				    kIPConfigurationServiceOptionIPv4Entity)) {
	    if (!init_ipv4_params(options, &params.v4)) {
		/* invalid configuration */
		goto done;
	    }
	}
	else {
	    params.is_ipv6 = TRUE;
	    if (!init_ipv6_params(options, &params.v6)) {
		/* invalid configuration */
		goto done;
	    }
	}
    }
    /* allocate/return an IPConfigurationServiceRef */
    service = __IPConfigurationServiceAllocate(NULL);
    if (service == NULL) {
	goto done;

    }

    /* remember the interface name */
    InterfaceNameInitWithCFString(service->ifname, interface_name);

    serviceID = my_CFUUIDStringCreate(NULL);
    service->config_dict = config_dict_create(serviceID, &params);


    /* monitor for configd restart */
    service->queue = dispatch_queue_create("IPConfigurationService", NULL);
    if (service->queue == NULL) {
	IPConfigLogFL(LOG_NOTICE,
		      "dispatch_queue_create failed");
	goto done;
    }
    if (!store_init(service, service->queue)) {
	goto done;
    }

    /* create the service in IPConfiguration */
    status = create_service(service, server);
    if (status != ipconfig_status_success_e) {
	goto done;
    }
    IPConfigurationServiceSetServiceID(service, serviceID,
				       params.is_ipv6, no_publish);
    dispatch_sync(service->queue,
		  ^{
		      service->server = server;
		      if (service->need_reconnect) {
			  (void)create_service(service, server);
		      }
		  });
    server = MACH_PORT_NULL;
    ret_service = service;
    service = NULL;

 done:
    if (server != MACH_PORT_NULL) {
	mach_port_deallocate(mach_task_self(), server);
    }
    my_CFRelease(&serviceID);
    my_CFRelease(&service);
    return (ret_service);
}

/*
 * Function: IPConfigurationServiceGetNotificationKey
 *
 * Purpose:
 *   Return the SCDynamicStoreKeyRef used to monitor the service using
 *   SCDynamicStoreSetNotificationKeys().
 *
 * Parameters:
 *   service			: the service to monitor
 */
CFStringRef
IPConfigurationServiceGetNotificationKey(IPConfigurationServiceRef service)
{
    return (service->store_key);
}

CFDictionaryRef
IPConfigurationServiceCopyInformation(IPConfigurationServiceRef service)
{
    CFDictionaryRef		info;

    info = SCDynamicStoreCopyValue(service->store, service->store_key);
    if (info != NULL
	&& isA_CFDictionary(info) == NULL) {
	my_CFRelease(&info);
    }
    return (info);
}

void
IPConfigurationServiceRefreshConfiguration(IPConfigurationServiceRef service)
{
    kern_return_t		kret;
    ipconfig_status_t		status;

    kret = ipconfig_refresh_service(service->server,
				    service->ifname,
				    service->service_id,
				    &status);
    if (kret != KERN_SUCCESS) {
	IPConfigLogFL(LOG_NOTICE,
		      "ipconfig_refresh_service(%s %s) failed, %s",
		      service->ifname, service->service_id,
		      mach_error_string(kret));
    }
    else if (status != ipconfig_status_success_e) {
	IPConfigLogFL(LOG_NOTICE,
		      "ipconfig_refresh_service(%s %s) failed: %s",
		      service->ifname, service->service_id,
		      ipconfig_status_string(status));
    }
    return;
}
