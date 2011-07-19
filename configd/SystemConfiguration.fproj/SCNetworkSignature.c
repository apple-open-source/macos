/*
 * Copyright (c) 2006, 2011 Apple Computer, Inc. All rights reserved.
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
 * SCNetworkSignature.c
 * - implementation of SCNetworkSignatureRef API that allows access to
     network identification information
 *
 */
/*
 * Modification History
 *
 * November 6, 2006	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */


#include <netinet/in.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFRuntime.h>
#include <SystemConfiguration/SCDynamicStore.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>
#include "SCNetworkSignature.h"
#include "SCNetworkSignaturePrivate.h"
#include <arpa/inet.h>

const char * kSCNetworkSignatureActiveChangedNotifyName = NETWORK_ID_KEY ".active";


#pragma mark SCNetworkSignature support routines

static __inline__ SCDynamicStoreRef
store_create(CFAllocatorRef alloc)
{
	return (SCDynamicStoreCreate(alloc, CFSTR("SCNetworkSignature"),
				     NULL, NULL));
}

static CFDictionaryRef
store_copy_id_dict(CFAllocatorRef alloc, SCDynamicStoreRef store)
{
	CFDictionaryRef	id_dict = NULL;
	Boolean		release_store = FALSE;

	if (store == NULL) {
		store = store_create(alloc);
		if (store == NULL) {
			goto done;
		}
		release_store = TRUE;
	}
	id_dict = SCDynamicStoreCopyValue(store,
					  kSCNetworkIdentificationStoreKey);
	if (isA_CFDictionary(id_dict) == NULL) {
		if (id_dict != NULL) {
			CFRelease(id_dict);
			id_dict = NULL;
		}
		goto done;
	}
 done:
	if (release_store) {
		CFRelease(store);
	}
	return (id_dict);
}

#pragma -

#pragma mark SCNetworkSignature APIs

CFStringRef
SCNetworkSignatureCopyActiveIdentifierForAddress(CFAllocatorRef alloc,
						 const struct sockaddr * addr)
{
	CFDictionaryRef		id_dict = NULL;
	CFStringRef		ident = NULL;
	struct sockaddr_in *	sin_p;


	/* only accept 0.0.0.0 (i.e. default) for now */
	sin_p = (struct sockaddr_in *)addr;
	if (addr == NULL
	    || addr->sa_family != AF_INET
	    || addr->sa_len != sizeof(struct sockaddr_in)
	    || sin_p->sin_addr.s_addr != 0) {
		_SCErrorSet(kSCStatusInvalidArgument);
		goto done;
	}
	id_dict = store_copy_id_dict(alloc, NULL);
	if (id_dict == NULL) {
		_SCErrorSet(kSCStatusFailed);
		goto done;
	}
	ident = CFDictionaryGetValue(id_dict, kStoreKeyPrimaryIPv4Identifier);
	if (isA_CFString(ident) != NULL) {
		CFRetain(ident);
	}
	else {
		_SCErrorSet(kSCStatusFailed);
	}
 done:
	if (id_dict != NULL) {
		CFRelease(id_dict);
	}
	return (ident);
}

CFArrayRef /* of CFStringRef's */
SCNetworkSignatureCopyActiveIdentifiers(CFAllocatorRef alloc)
{
	CFArrayRef		active = NULL;
	int			i;
	int			count = 0;
	CFDictionaryRef		id_dict = NULL;

	id_dict = store_copy_id_dict(alloc, NULL);
	if (id_dict == NULL) {
		goto done;
	}
	active = CFDictionaryGetValue(id_dict, kStoreKeyActiveIdentifiers);
	if (isA_CFArray(active) != NULL) {
		count = CFArrayGetCount(active);
	}
	if (count == 0) {
		active = NULL;
		goto done;
	}
	for (i = 0; i < count; i++) {
		CFStringRef	ident = CFArrayGetValueAtIndex(active, i);

		if (isA_CFString(ident) == NULL) {
			active = NULL;
			goto done;
		}
	}
	CFRetain(active);

 done:
	if (id_dict != NULL) {
		CFRelease(id_dict);
	}
	if (active == NULL) {
		_SCErrorSet(kSCStatusFailed);
	}
	return (active);
}

static CFDictionaryRef
copy_services_for_address_family(CFAllocatorRef alloc,
				 SCDynamicStoreRef store, int af)
{
	CFDictionaryRef	info;
	CFArrayRef	patterns;
	CFStringRef	pattern;
	CFStringRef	prop;
	Boolean		release_store = FALSE;

	if (store == NULL) {
		store = store_create(alloc);
		if (store == NULL) {
			return (NULL);
		}
		release_store = TRUE;
	}
	prop = (af == AF_INET) ? kSCEntNetIPv4 : kSCEntNetIPv6;
	pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							      kSCDynamicStoreDomainState,
							      kSCCompAnyRegex,
							      prop);
	patterns = CFArrayCreate(NULL,
				 (const void * *)&pattern, 1,
				 &kCFTypeArrayCallBacks);
	CFRelease(pattern);
	info = SCDynamicStoreCopyMultiple(store, NULL, patterns);
	CFRelease(patterns);
	if (release_store) {
		CFRelease(store);
	}
	return (info);
}

static CFStringRef
my_IPAddressToCFString(int af, const void * src_p)
{
	char		ntopbuf[INET6_ADDRSTRLEN];

	if (inet_ntop(af, src_p, ntopbuf, sizeof(ntopbuf)) != NULL) {
		return (CFStringCreateWithCString(NULL, ntopbuf,
						  kCFStringEncodingASCII));
	}
	return (NULL);
}

CFStringRef
SCNetworkSignatureCopyIdentifierForConnectedSocket(CFAllocatorRef alloc,
						   int sock_fd)
{
	CFStringRef		addresses_key;
	int			af;
	int			count;
	int			i;
	const void * *		keys = NULL;
#define KEYS_STATIC_COUNT	10
	const void *		keys_static[KEYS_STATIC_COUNT];
	static const void *	local_ip_p;
	CFStringRef		local_ip_str = NULL;
	CFStringRef		ret_signature = NULL;
	CFDictionaryRef		service_info = NULL;
	union {
		struct sockaddr_in	inet;
		struct sockaddr_in6	inet6;
		struct sockaddr		sa;
	} 			ss;
	socklen_t		ss_len = sizeof(ss);
	int			status = kSCStatusFailed;

	if (getsockname(sock_fd, &ss.sa, &ss_len) != 0) {
		status = kSCStatusInvalidArgument;
		goto done;
	}
	af = ss.inet.sin_family;
	switch (af) {
	case AF_INET:
		addresses_key = kSCPropNetIPv4Addresses;
		local_ip_p = &ss.inet.sin_addr;
		break;
	case AF_INET6:
		addresses_key = kSCPropNetIPv6Addresses;
		local_ip_p = &ss.inet6.sin6_addr;
		break;
	default:
		status = kSCStatusInvalidArgument;
		goto done;
	}

	/* find a service matching the local IP and get its network signature */
	service_info = copy_services_for_address_family(alloc, NULL, af);
	if (service_info == NULL) {
		goto done;
	}
	local_ip_str = my_IPAddressToCFString(af, local_ip_p);
	if (local_ip_str == NULL) {
		goto done;
	}
	count = CFDictionaryGetCount(service_info);
	if (count > KEYS_STATIC_COUNT) {
		keys = (const void * *)malloc(sizeof(*keys) * count);
	}
	else {
		keys = keys_static;
	}
	CFDictionaryGetKeysAndValues(service_info, keys, NULL);
	for (i = 0; i < count; i++) {
		CFArrayRef		addrs;
		CFRange			range;
		CFStringRef		signature;
		CFDictionaryRef		value;

		value = CFDictionaryGetValue(service_info, keys[i]);
		if (isA_CFDictionary(value) == NULL) {
			continue;
		}
		signature = CFDictionaryGetValue(value,
						 kStoreKeyNetworkSignature);
		if (isA_CFString(signature) == NULL) {
			/* no signature */
			continue;
		}
		addrs = CFDictionaryGetValue(value, addresses_key);
		if (isA_CFArray(addrs) == NULL) {
			continue;
		}
		range = CFRangeMake(0, CFArrayGetCount(addrs));
		if (CFArrayContainsValue(addrs, range, local_ip_str)) {
			ret_signature = CFRetain(signature);
			status = kSCStatusOK;
			break;
		}
	}

 done:
	if (local_ip_str != NULL) {
		CFRelease(local_ip_str);
	}
	if (keys != NULL && keys != keys_static) {
		free(keys);
	}
	if (service_info != NULL) {
		CFRelease(service_info);
	}
	if (status != kSCStatusOK) {
		_SCErrorSet(status);
	}
	return (ret_signature);
}

#pragma mark -
