/*
 * Copyright (c) 2017, 2018 Apple Inc. All rights reserved.
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
 * Modification History
 *
 * April 17, 2017	Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#include "nat64-configuration.h"

#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include "ip_plugin.h"

#define	INET6	1

#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#if __has_include(<nw/private.h>)
#include <nw/private.h>
#else // __has_include(<nw/private.h>)
#include <network/nat64.h>
#endif // __has_include(<nw/private.h>)


static CFMutableSetRef	nat64_prefix_requests	= NULL;


static dispatch_queue_t
nat64_dispatch_queue()
{
	static dispatch_once_t	once;
	static dispatch_queue_t	q;

	dispatch_once(&once, ^{
		q = dispatch_queue_create("nat64 prefix request queue", NULL);
	});

	return q;
}


static Boolean
_nat64_prefix_set(const char		*if_name,
		  int32_t		num_prefixes,
		  nw_nat64_prefix_t	*prefixes)
{
	struct if_nat64req	req;
	int			ret;
	int			s;

	SC_log(LOG_DEBUG, "%s: _nat64_prefix_set", if_name);

	// pass NAT64 prefixes to the kernel
	bzero(&req, sizeof(req));
	strlcpy(req.ifnat64_name, if_name, sizeof(req.ifnat64_name));

	if (num_prefixes == 0) {
		SC_log(LOG_INFO, "%s: nat64 prefix not (or no longer) available", if_name);
	}

	for (int32_t i = 0; i < num_prefixes; i++) {
		char	prefix_str[NW_NAT64_PREFIX_STR_LENGTH]	= {0};

		nw_nat64_write_prefix_to_string(&prefixes[i], prefix_str, sizeof(prefix_str));
		SC_log(LOG_DEBUG, "%s: nat64 prefix[%d] = %s", if_name, i, prefix_str);

		if (i < NAT64_MAX_NUM_PREFIXES) {
			req.ifnat64_prefixes[i].prefix_len = prefixes[i].length;
			bcopy(&prefixes[i].data,
			      &req.ifnat64_prefixes[i].ipv6_prefix,
			      MIN(sizeof(req.ifnat64_prefixes[i].ipv6_prefix), sizeof(prefixes[i].data)));	// MIN(16, 12)
		}
	}

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1) {
		SC_log(LOG_ERR, "socket() failed: %s", strerror(errno));
		return (FALSE);
	}
	ret = ioctl(s, SIOCSIFNAT64PREFIX, &req);
	close(s);
	if (ret == -1) {
		if ((errno != ENOENT) || (num_prefixes != 0)) {
			SC_log(LOG_ERR, "%s: ioctl(SIOCSIFNAT64PREFIX) failed: %s", if_name, strerror(errno));
		}
		return (FALSE);
	}

	SC_log(LOG_INFO, "%s: nat64 prefix%s updated", if_name, (num_prefixes != 1) ? "es" : "");
	return (TRUE);
}


static void
_nat64_prefix_post(CFStringRef		interface,
		   int32_t		num_prefixes,
		   nw_nat64_prefix_t	*prefixes,
		   CFAbsoluteTime	start_time)
{
	CFStringRef	key;

	key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							    kSCDynamicStoreDomainState,
							    interface,
							    kSCEntNetNAT64);
	if (num_prefixes >= 0) {
		CFDateRef		date;
		CFMutableDictionaryRef	plat_dict;

		plat_dict = CFDictionaryCreateMutable(NULL,
						      0,
						      &kCFTypeDictionaryKeyCallBacks,
						      &kCFTypeDictionaryValueCallBacks);
		/* prefixes (if available) */
		if (num_prefixes > 0) {
			CFMutableArrayRef	prefix_array;

			prefix_array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			for (int32_t i = 0; i < num_prefixes; i++) {
				char		prefix_str[NW_NAT64_PREFIX_STR_LENGTH]	= {0};
				CFStringRef	str;

				nw_nat64_write_prefix_to_string(&prefixes[i], prefix_str, sizeof(prefix_str));
				str = CFStringCreateWithCString(NULL, prefix_str, kCFStringEncodingASCII);
				CFArrayAppendValue(prefix_array, str);
				CFRelease(str);
			}
			CFDictionarySetValue(plat_dict, kSCPropNetNAT64PrefixList, prefix_array);
			CFRelease(prefix_array);
		}
		/* start time */
		date = CFDateCreate(NULL, start_time);
		CFDictionarySetValue(plat_dict,
				     kSCPropNetNAT64PLATDiscoveryStartTime,
				     date);
		CFRelease(date);

		/* completion time */
		date = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
		CFDictionarySetValue(plat_dict,
				     kSCPropNetNAT64PLATDiscoveryCompletionTime,
				     date);
		CFRelease(date);

		(void)SCDynamicStoreSetValue(NULL, key, plat_dict);
		SC_log(LOG_INFO, "%@: PLAT discovery complete %@",
		       interface, plat_dict);
		CFRelease(plat_dict);
	} else {
		(void)SCDynamicStoreRemoveValue(NULL, key);
	}
	CFRelease(key);
	return;
}


static void
_nat64_prefix_request_start(const void *value)
{
	unsigned int	if_index;
	char		*if_name;
	CFStringRef	interface	= (CFStringRef)value;
	bool		ok;
	CFAbsoluteTime	start_time;

	SC_log(LOG_DEBUG, "%@: _nat64_prefix_request_start", interface);

	if_name = _SC_cfstring_to_cstring(interface, NULL, 0, kCFStringEncodingASCII);
	if (if_name == NULL) {
		SC_log(LOG_NOTICE, "%@: could not convert interface name", interface);
		return;
	}

	if_index = my_if_nametoindex(if_name);
	if (if_index == 0) {
		SC_log(LOG_NOTICE, "%s: no interface index", if_name);
		CFAllocatorDeallocate(NULL, if_name);
		return;
	}

	// keep track of interfaces with active nat64 prefix requests
	CFSetAddValue(nat64_prefix_requests, interface);

	CFRetain(interface);
	start_time = CFAbsoluteTimeGetCurrent();
	ok = nw_nat64_copy_prefixes_async(&if_index,
					  nat64_dispatch_queue(),
					  ^(int32_t num_prefixes, nw_nat64_prefix_t *prefixes) {
						  if (num_prefixes >= 0) {
							  // update interface
							  if (!_nat64_prefix_set(if_name, num_prefixes, prefixes)) {
								  num_prefixes = -1;
							  }
						  } else {
							  SC_log(LOG_ERR,
								 "%s: nw_nat64_copy_prefixes_async() num_prefixes(%d) < 0",
								 if_name,
								 num_prefixes);
						  }

						  if (num_prefixes <= 0) {
							  // remove from active list
							  CFSetRemoveValue(nat64_prefix_requests, interface);
						  }

						  _nat64_prefix_post(interface, num_prefixes, prefixes, start_time);

						  // cleanup
						  CFRelease(interface);
						  CFAllocatorDeallocate(NULL, if_name);
					  });
	if (!ok) {
		SC_log(LOG_ERR, "%s: nw_nat64_copy_prefixes_async() failed", if_name);

		// remove from active list
		CFSetRemoveValue(nat64_prefix_requests, interface);

		CFRelease(interface);
		CFAllocatorDeallocate(NULL, if_name);
	}

	return;
}


static void
_nat64_prefix_request(const void *value, void *context)
{
	CFSetRef	changes		= (CFSetRef)context;
	CFStringRef	interface	= (CFStringRef)value;

	if (!CFSetContainsValue(nat64_prefix_requests, interface) ||
	    ((changes != NULL) && CFSetContainsValue(changes, interface))) {
		// if new request
		// ... or a [refresh] request that hasn't already been started
		_nat64_prefix_request_start(interface);
	}

	return;
}


static void
_nat64_prefix_update(const void *value, void *context)
{
#pragma unused(context)
	CFStringRef	interface	= (CFStringRef)value;

	if (CFSetContainsValue(nat64_prefix_requests, interface)) {
		_nat64_prefix_request_start(interface);
	}

	return;
}


#pragma mark -
#pragma mark NAT64 prefix functions (for IPMonitor)


__private_extern__
Boolean
is_nat64_prefix_request(CFStringRef change, CFStringRef *interface)
{
	CFArrayRef		components;
	static CFStringRef	prefix	= NULL;
	Boolean			yn	= FALSE;
	static dispatch_once_t	once;

	dispatch_once(&once, ^{
		prefix = SCDynamicStoreKeyCreateNetworkInterface(NULL, kSCDynamicStoreDomainState);
	});

	*interface = NULL;
	if (!CFStringHasPrefix(change, prefix) ||
	    !CFStringHasSuffix(change, kSCEntNetNAT64PrefixRequest)) {
		return FALSE;
	}

	components = CFStringCreateArrayBySeparatingStrings(NULL, change, CFSTR("/"));
	if (CFArrayGetCount(components) == 5) {
		*interface = CFArrayGetValueAtIndex(components, 3);
		CFRetain(*interface);
		yn = TRUE;
	}
	CFRelease(components);

	return yn;
}


__private_extern__ void
nat64_prefix_request_add_pattern(CFMutableArrayRef patterns)
{
	CFStringRef	pattern;

	pattern = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
								kSCDynamicStoreDomainState,
								kSCCompAnyRegex,
								kSCEntNetNAT64PrefixRequest);
	CFArrayAppendValue(patterns, pattern);
	CFRelease(pattern);
	return;
}


__private_extern__
void
nat64_configuration_init(CFBundleRef bundle)
{
#pragma unused(bundle)
	nat64_prefix_requests = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
	return;
}


__private_extern__
void
nat64_configuration_update(CFSetRef requests, CFSetRef changes)
{
	// for any interface that changed, refresh the nat64 prefix
	if (changes != NULL) {
		CFSetApplyFunction(changes, _nat64_prefix_update, NULL);
	}

	// for any requested interface, query the nat64 prefix
	if (requests != NULL) {
		CFSetApplyFunction(requests, _nat64_prefix_request, (void *)changes);
	}

	return;
}
