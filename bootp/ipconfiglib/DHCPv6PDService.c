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
 * DHCPv6PDService.c
 * - API to request a prefix using DHCPv6 Prefix Delegation
 */
/* 
 * Modification History
 *
 * October 12, 2022 	Dieter Siegmund (dieter@apple.com)
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
#include <arpa/inet.h>
#include "ipconfig_types.h"
#include "ipconfig_ext.h"
#include "symbol_scope.h"
#include "cfutil.h"
#include "ipconfig.h"
#include "IPConfigurationLog.h"
#include "IPConfigurationServiceInternal.h"
#include "IPConfigurationService.h"
#include "DHCPv6PDService.h"

/**
 ** DHCPv6PDService
 **/

/**
 ** CF object glue code
 **/
struct __DHCPv6PDService {
	CFRuntimeBase			cf_base;

	IPConfigurationServiceRef	service;
	SCDynamicStoreRef		store;
	ObjectWrapperRef		wrapper;
	dispatch_queue_t		queue;
	DHCPv6PDServiceHandler		handler;
	dispatch_queue_t		handler_queue;
};

STATIC CFStringRef	__DHCPv6PDServiceCopyDebugDesc(CFTypeRef cf);
STATIC void		__DHCPv6PDServiceDeallocate(CFTypeRef cf);

STATIC CFTypeID __kDHCPv6PDServiceTypeID = _kCFRuntimeNotATypeID;

STATIC const CFRuntimeClass __DHCPv6PDServiceClass = {
	0,					/* version */
	"DHCPv6PDService",			/* className */
	NULL,					/* init */
	NULL,					/* copy */
	__DHCPv6PDServiceDeallocate,		/* deallocate */
	NULL,					/* equal */
	NULL,					/* hash */
	NULL,					/* copyFormattingDesc */
	__DHCPv6PDServiceCopyDebugDesc		/* copyDebugDesc */
};

STATIC CFStringRef
__DHCPv6PDServiceCopyDebugDesc(CFTypeRef cf)
{
	CFAllocatorRef		allocator = CFGetAllocator(cf);
	CFMutableStringRef	result;
	DHCPv6PDServiceRef	dhcp = (DHCPv6PDServiceRef)cf;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL,
			     CFSTR("<DHCPv6PDService %p [%p]> {"),
			     cf, allocator);
	CFStringAppendFormat(result, NULL, CFSTR("service = %@"),
			     dhcp->service);
	CFStringAppend(result, CFSTR("}"));
	return (result);
}

STATIC void
__DHCPv6PDServiceDeallocate(CFTypeRef cf)
{
	DHCPv6PDServiceRef 	dhcp = (DHCPv6PDServiceRef)cf;

	if (dhcp->wrapper != NULL) {
		if (dhcp->queue != NULL) {
			/* ensure change callbacks won't run anymore */
			dispatch_sync(dhcp->queue, ^{
					ObjectWrapperClearObject(dhcp->wrapper);
				});
		}
		ObjectWrapperRelease(dhcp->wrapper);
		dhcp->wrapper = NULL;
	}

	if (dhcp->store != NULL) {
		SCDynamicStoreSetDispatchQueue(dhcp->store, NULL);
		my_CFRelease(&dhcp->store);
	}
	my_CFRelease(&dhcp->service);
	if (dhcp->handler != NULL) {
		Block_release(dhcp->handler);
		dhcp->handler = NULL;
	}
	if (dhcp->handler_queue != NULL) {
		dispatch_release(dhcp->handler_queue);
		dhcp->handler_queue = NULL;
	}
	if (dhcp->queue != NULL) {
		dispatch_release(dhcp->queue);
		dhcp->queue = NULL;
	}
	return;
}

STATIC void
__DHCPv6PDServiceInitialize(void)
{
	/* initialize runtime */
	__kDHCPv6PDServiceTypeID 
		= _CFRuntimeRegisterClass(&__DHCPv6PDServiceClass);
	return;
}

STATIC void
__DHCPv6PDServiceRegisterClass(void)
{
	STATIC pthread_once_t	initialized = PTHREAD_ONCE_INIT;

	pthread_once(&initialized, __DHCPv6PDServiceInitialize);
	return;
}

STATIC DHCPv6PDServiceRef
__DHCPv6PDServiceAllocate(CFAllocatorRef allocator)
{
	DHCPv6PDServiceRef	dhcp;
	int			size;

	__DHCPv6PDServiceRegisterClass();

	size = sizeof(*dhcp) - sizeof(CFRuntimeBase);
	dhcp = (DHCPv6PDServiceRef)
		_CFRuntimeCreateInstance(allocator,
					 __kDHCPv6PDServiceTypeID, size, NULL);
	bzero(((void *)dhcp) + sizeof(CFRuntimeBase), size);
	return (dhcp);
}

/*
 * Private Functions
 */

STATIC dispatch_queue_t
get_global_queue(void)
{
	return dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
}

typedef struct DHCPv6PrefixInfo {
	struct in6_addr 	prefix;
	uint8_t 		prefix_length;
	uint32_t 		valid_lifetime;
	uint32_t 		preferred_lifetime;
} DHCPv6PrefixInfo, *DHCPv6PrefixInfoRef;

struct DHCPv6PDServiceInfo {
	DHCPv6PrefixInfo	prefix_info;
	CFDictionaryRef		info_dict;
};

STATIC Boolean
DHCPv6PrefixInfoInitWithDict(struct DHCPv6PrefixInfo * prefix_info_p,
			     CFDictionaryRef dict)
{
	CFDictionaryRef		ipv6_dict;
	CFStringRef		ip_str;
	CFNumberRef		num;
	uint32_t		num_val;
	Boolean			valid = FALSE;

	bzero(prefix_info_p, sizeof(*prefix_info_p));
	ipv6_dict = CFDictionaryGetValue(dict, kSCEntNetIPv6);
	if (isA_CFDictionary(ipv6_dict) == NULL) {
		goto done;
	}

	/* prefix */
	ip_str = CFDictionaryGetValue(ipv6_dict,
				      kSCPropNetIPv6DelegatedPrefix);
	if (isA_CFString(ip_str) == NULL
	    || !my_CFStringToIPv6Address(ip_str,
					 &prefix_info_p->prefix)) {
		goto done;
	}

	/* prefix length */
	num = CFDictionaryGetValue(ipv6_dict,
				   kSCPropNetIPv6DelegatedPrefixLength);
	if (!my_CFTypeToNumber(num, &num_val)) {
		goto done;
	}
	prefix_info_p->prefix_length = (uint8_t)num_val;

	/* valid lifetime */
	num = CFDictionaryGetValue(ipv6_dict,
				   kSCPropNetIPv6DelegatedPrefixValidLifetime);
	if (!my_CFTypeToNumber(num, &num_val)) {
		goto done;
	}
	prefix_info_p->valid_lifetime = num_val;

	/* preferred lifetime */
	num = CFDictionaryGetValue(ipv6_dict,
				   kSCPropNetIPv6DelegatedPrefixPreferredLifetime);
	if (!my_CFTypeToNumber(num, &num_val)) {
		goto done;
	}
	prefix_info_p->preferred_lifetime = num_val;
	valid = TRUE;

 done:
	return (valid);
}

STATIC void
DHCPv6PDServiceStoreChange(SCDynamicStoreRef session,
			   CFArrayRef changes, void * info)
{
#pragma unused(session)
#pragma unused(changes)
	DHCPv6PDServiceRef	dhcp;
	DHCPv6PDServiceHandler	handler;
	CFDictionaryRef		info_dict;
	dispatch_queue_t	queue;
	dispatch_block_t	notify_block;
	Boolean			service_valid = TRUE;
	ObjectWrapperRef	wrapper = (ObjectWrapperRef)info;

	dhcp = (DHCPv6PDServiceRef)ObjectWrapperGetObject(wrapper);
	if (dhcp == NULL || dhcp->service == NULL) {
		/* service has been deallocated */
		return;
	}

	/* retrieve the DHCPv6 information and call the handler */
	info_dict = IPConfigurationServiceCopyInformation(dhcp->service);
	if (info_dict == NULL) {
		/* service_valid will be FALSE if the interface detached */
		service_valid = IPConfigurationServiceIsValid(dhcp->service);
	}
	queue = dhcp->handler_queue;
	if (queue == NULL) {
		queue = get_global_queue();
	}
	handler = dhcp->handler;
	notify_block = ^{
		struct DHCPv6PDServiceInfo	service_info;
		DHCPv6PDServiceInfoRef		service_info_p = NULL;

		if (info_dict != NULL) {
			DHCPv6PrefixInfoRef	prefix_info_p;

			bzero(&service_info, sizeof(service_info));
			prefix_info_p = &service_info.prefix_info;
			if (DHCPv6PrefixInfoInitWithDict(prefix_info_p,
							 info_dict)) {
				service_info.info_dict = info_dict;
				service_info_p = &service_info;
			}
		}
		(handler)(service_valid, service_info_p, NULL);
		if (info_dict != NULL) {
			CFRelease(info_dict);
		}
	};
	dispatch_async(queue, notify_block);
	return;
}

STATIC void
DHCPv6PDServiceInvalidate(DHCPv6PDServiceRef dhcp)
{
	my_CFRelease(&dhcp->service);
	if (dhcp->store != NULL) {
		SCDynamicStoreSetDispatchQueue(dhcp->store, NULL);
		SCDynamicStoreSetNotificationKeys(dhcp->store, NULL, NULL);
	}
}

STATIC void
DHCPv6PDServiceResumeSync(DHCPv6PDServiceRef dhcp)
{
	CFStringRef		key;
	CFArrayRef		keys;
	Boolean			ok = FALSE;

	/* schedule our service change callbacks */
	key = IPConfigurationServiceGetNotificationKey(dhcp->service);
	keys = CFArrayCreate(NULL, (const void * *)&key,
			     1, &kCFTypeArrayCallBacks);
	SCDynamicStoreSetNotificationKeys(dhcp->store, keys, NULL);
	CFRelease(keys);
	if (!SCDynamicStoreSetDispatchQueue(dhcp->store, dhcp->queue)) {
		IPConfigLog(LOG_NOTICE,
			    "%s: SCDynamicStoreSetDispatchQueue failed",
			    __func__);
		goto done;
	}
	/* start the service */
	if (!IPConfigurationServiceStart(dhcp->service)) {
		IPConfigLog(LOG_NOTICE,
			    "%s: IPConfigurationServiceStart failed",
			    __func__);
		goto done;
	}
	ok = TRUE;

 done:
	if (!ok) {
		DHCPv6PDServiceHandler	handler = dhcp->handler;
		dispatch_queue_t	queue = dhcp->handler_queue;

		/* invalidate the service, call the handler */
		DHCPv6PDServiceInvalidate(dhcp);
		if (queue == NULL) {
			queue = get_global_queue();
		}
		dispatch_async(queue, ^{
				(handler)(FALSE, NULL, NULL);
			});
	}
	return;
}

/*
 * API
 */
CFTypeID
DHCPv6PDServiceGetTypeID(void)
{
	__DHCPv6PDServiceRegisterClass();
	return (__kDHCPv6PDServiceTypeID);
}

DHCPv6PDServiceRef
DHCPv6PDServiceCreate(CFStringRef interface_name,
		      const struct in6_addr * prefix,
		      uint8_t prefix_length,
		      CFDictionaryRef options)
{
	CFDictionaryRef			config_dict;
	CFIndex				count;
	CFDictionaryRef			dict;
	DHCPv6PDServiceRef		dhcp = NULL;
#define N_KEYS	3
	const void *			keys[N_KEYS];
	char 				ntopbuf[INET6_ADDRSTRLEN];
	Boolean				ok = FALSE;
	CFNumberRef			prefix_length_num = NULL;
	CFStringRef			prefix_str = NULL;
	IPConfigurationServiceRef	service;
	const void *			values[N_KEYS];

	/* initialize the class and logging */
	(void)IPConfigurationServiceGetTypeID();
	ntopbuf[0] = '\0';
	if (prefix != NULL) {
		inet_ntop(AF_INET6, prefix, ntopbuf, sizeof(ntopbuf));
	}
	IPConfigLog(LOG_NOTICE, "%s(%@) prefix '%s' length %d",
		    __func__, interface_name, ntopbuf, prefix_length);
	if (interface_name == NULL || options != NULL || prefix_length > 128) {
		return (NULL);
	}
	keys[0] = kSCPropNetIPv6ConfigMethod;
	values[0] = kSCValNetIPv6ConfigMethodDHCPv6PD;
	count = 1;
	if (prefix != NULL) {
		prefix_str = my_CFStringCreateWithIPv6Address(prefix);
		keys[count] = kSCPropNetIPv6RequestedPrefix;
		values[count] = prefix_str;
		count++;
	}
	if (prefix_length != 0) {
		uint32_t	val = prefix_length;

		val = prefix_length;
		prefix_length_num = CFNumberCreate(NULL, kCFNumberSInt32Type,
						   &val);
		keys[count] = kSCPropNetIPv6RequestedPrefixLength;
		values[count] = prefix_length_num;
		count++;
	}
	config_dict = CFDictionaryCreate(NULL, keys, values,
					 count,
					 &kCFTypeDictionaryKeyCallBacks,
					 &kCFTypeDictionaryValueCallBacks);
	my_CFRelease(&prefix_str);
	my_CFRelease(&prefix_length_num);
	keys[0] = kIPConfigurationServiceOptionIPv6Entity;
	values[0] = config_dict;
	dict = CFDictionaryCreate(NULL, keys, values,
				  1,
				  &kCFTypeDictionaryKeyCallBacks,
				  &kCFTypeDictionaryValueCallBacks);
	CFRelease(config_dict);
	service = IPConfigurationServiceCreateInternal(interface_name,
						       dict);
	CFRelease(dict);
	if (service == NULL) {
		goto done;
	}
	dhcp = __DHCPv6PDServiceAllocate(NULL);
	dhcp->service = service;
	dhcp->queue = dispatch_queue_create("DHCPv6PDService", NULL);
	if (dhcp->queue == NULL) {
		IPConfigLog(LOG_NOTICE,
			    "%s: dispatch_queue_create failed",
			    __func__);
		goto done;
	}
	dhcp->store = store_create(dhcp,
				   CFSTR("DHCPv6PDService"),
				   NULL,
				   DHCPv6PDServiceStoreChange,
				   NULL,
				   &dhcp->wrapper);
	if (dhcp->store == NULL) {
		goto done;
	}
	ok = TRUE;

 done:
	if (!ok) {
		my_CFRelease(&dhcp);
	}
	return (dhcp);
}

STATIC void
DHCPv6PDServiceSetQueueAndHandlerSync(DHCPv6PDServiceRef dhcp,
				      dispatch_queue_t queue,
				      DHCPv6PDServiceHandler handler)
{
	if (dhcp->handler != NULL) {
		Block_release(dhcp->handler);
		dhcp->handler = NULL;
	}
	if (handler != NULL) {
		dhcp->handler = Block_copy(handler);
	}
	if (dhcp->handler_queue != NULL) {
		dispatch_release(dhcp->handler_queue);
		dhcp->handler_queue = NULL;
	}
	if (queue != NULL) {
		dispatch_retain(queue);
		dhcp->handler_queue = queue;
	}
	return;
}

void
DHCPv6PDServiceSetQueueAndHandler(DHCPv6PDServiceRef dhcp,
				  dispatch_queue_t queue,
				  DHCPv6PDServiceHandler handler)
{
	if (dhcp->queue == NULL) {
		IPConfigLog(LOG_NOTICE,
			    "%s: service queue is NULL", __func__);
		return;
	}
	dispatch_sync(dhcp->queue, ^{
			DHCPv6PDServiceSetQueueAndHandlerSync(dhcp, queue,
							      handler);
		});
}

void
DHCPv6PDServiceResume(DHCPv6PDServiceRef dhcp)
{
	if (dhcp->queue == NULL || dhcp->service == NULL) {
		IPConfigLog(LOG_NOTICE, "%s: invalid object",  __func__);
		return;
	}
	dispatch_async(dhcp->queue, ^{ DHCPv6PDServiceResumeSync(dhcp); });
	return;
}

void
DHCPv6PDServiceInfoGetPrefix(DHCPv6PDServiceInfoRef info,
			     struct in6_addr * prefix)
{
	*prefix = info->prefix_info.prefix;
}

uint8_t
DHCPv6PDServiceInfoGetPrefixLength(DHCPv6PDServiceInfoRef info)
{
	return (info->prefix_info.prefix_length);
}

uint32_t
DHCPv6PDServiceInfoGetPrefixValidLifetime(DHCPv6PDServiceInfoRef info)
{
	return (info->prefix_info.valid_lifetime);
}

uint32_t
DHCPv6PDServiceInfoGetPrefixPreferredLifetime(DHCPv6PDServiceInfoRef info)
{
	return (info->prefix_info.preferred_lifetime);
}

CFArrayRef
DHCPv6PDServiceInfoGetOptionData(DHCPv6PDServiceInfoRef info,
				 uint16_t option_code)
{
	CFDictionaryRef	dhcp_dict;
	CFArrayRef	option_data = NULL;
	CFStringRef	option_name;

	dhcp_dict = CFDictionaryGetValue(info->info_dict, kSCEntNetDHCPv6);
	if (isA_CFDictionary(dhcp_dict) == NULL) {
		goto done;
	}
	option_name = CFStringCreateWithFormat(NULL, 0, kDHCPOptionFormat,
					       option_code);
	option_data = CFDictionaryGetValue(dhcp_dict, option_name);
	CFRelease(option_name);
 done:
	return (isA_CFArray(option_data));
}
