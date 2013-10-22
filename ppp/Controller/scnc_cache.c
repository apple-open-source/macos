/*
 * Copyright (c) 2013 Apple Computer, Inc. All rights reserved.
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

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <arpa/inet.h>
#include "scnc_main.h"
#include "scnc_utils.h"
#include "scnc_cache.h"

#define kSCNCCacheFile CFSTR("com.apple.scnc-cache.plist")

static SCPreferencesRef
scnc_cache_get_prefs(void)
{
	static SCPreferencesRef prefs = NULL;
	static dispatch_once_t predicate = 0;
	
	dispatch_once(&predicate, ^{
		prefs = SCPreferencesCreate(kCFAllocatorDefault, CFSTR("PPPController"), kSCNCCacheFile);
		if (prefs == NULL) {
			SCLog(TRUE, LOG_ERR, CFSTR("SCPreferencesCreate failed: %s"), SCErrorString(SCError()));
		}
		
		SCPreferencesSynchronize(prefs);
	});
	
	return prefs;
}

static CFDictionaryRef
scnc_cache_get_routes (struct service *serv)
{
	SCPreferencesRef prefs = NULL;
	prefs = scnc_cache_get_prefs();
	if (prefs == NULL) {
		return NULL;
	}
	
	return SCPreferencesGetValue(prefs, serv->serviceID);
}

/* 
 scnc_cache_update_key
 
 This function should be used for modifying the cache. It merges the desired key (cacheKey) from an existing 
 dictionary (sourceDict) into the cache's mutable dictionary (cacheDict). If the value in the replacementDict is NULL
 and addChildDictionaryIfNULL is not set, the value is removed from the cacheDict; if addChildDictionaryIfNULL is
 set, then an empty dictionary will be added.
 
 If a block is passed as the final argument, it is assumed that the cacheKey points to a subdictionary, and the 
 block will include further recursive calls to scnc_cache_update_key. If the block is NULL, then the entire value 
 for cacheKey is replaced wholesale.
 
 If sourceKey is not NULL, then that key will be used to look up values in sourceDict. When the values are entered
 into cacheDict, they will use cacheKey.
*/
static void
scnc_cache_update_key (CFMutableDictionaryRef cacheDict, CFDictionaryRef sourceDict, Boolean addChildDictionaryIfNULL, CFStringRef cacheKey, CFStringRef sourceKey, void(^block)(CFMutableDictionaryRef, CFDictionaryRef))
{		
	if (block) {
		/* The type of the value being updated is a dictionary */
		CFDictionaryRef oldCacheSubDict = CFDictionaryGetValue(cacheDict, cacheKey);
		CFMutableDictionaryRef newCacheSubDict = NULL;
		CFDictionaryRef sourceChildDict = NULL;
		if (isA_CFDictionary(sourceDict)) {
			sourceChildDict = CFDictionaryGetValue(sourceDict, sourceKey?sourceKey:cacheKey);
		}
		
		if (isA_CFDictionary(sourceChildDict) || addChildDictionaryIfNULL) {
			if (isA_CFDictionary(oldCacheSubDict)) {
				newCacheSubDict = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, oldCacheSubDict);
			} else {
				newCacheSubDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			}
			
			if (newCacheSubDict) {
				block(newCacheSubDict, sourceChildDict);
				
				if (CFDictionaryGetCount(newCacheSubDict) > 0) {
					if (!my_CFEqual(oldCacheSubDict, newCacheSubDict)) {
						CFDictionarySetValue(cacheDict, cacheKey, newCacheSubDict);
					}
				} else if (oldCacheSubDict) {
					CFDictionaryRemoveValue(cacheDict, cacheKey);
				}
				CFRelease(newCacheSubDict);
			}
		} else if (oldCacheSubDict) {
			/* Remove the key if it is not in the replacement dictionary */
			CFDictionaryRemoveValue(cacheDict, cacheKey);
		}
	} else {
		/* The type of the value being updated is ignored */
		CFPropertyListRef sourceValue = NULL;
		if (isA_CFDictionary(sourceDict)) {
			sourceValue = CFDictionaryGetValue(sourceDict, sourceKey?sourceKey:cacheKey);
		}
		
		if (isA_CFPropertyList(sourceValue)) {
			CFDictionarySetValue(cacheDict, cacheKey, sourceValue);
		} else if (CFDictionaryContainsKey(cacheDict, cacheKey)) {
			/* Remove the key if it is not in the replacement dictionary */
			CFDictionaryRemoveValue(cacheDict, cacheKey);
		}
	}
}

/* Returns TRUE if updated the preferences file */
Boolean
scnc_cache_routing_table (struct service *serv, CFDictionaryRef serviceConfig, Boolean useOldKeys, Boolean doFullTunnel)
{
	SCPreferencesRef prefs = NULL;
	CFDictionaryRef oldServiceDict = NULL;
	CFMutableDictionaryRef newServiceDict = NULL;

	prefs = scnc_cache_get_prefs();
	if (prefs == NULL) {
		return FALSE;
	}
	
	oldServiceDict = SCPreferencesGetValue(prefs, serv->serviceID);
	if (isA_CFDictionary(oldServiceDict)) {
		newServiceDict = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, oldServiceDict);
	} else {
		newServiceDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	}

	if (newServiceDict) {	
		scnc_cache_update_key(newServiceDict, serviceConfig, doFullTunnel, kSCNetworkConnectionNetworkInfoIPv4, useOldKeys?kSCEntNetIPv4:NULL, ^(CFMutableDictionaryRef newIPv4Dict, CFDictionaryRef ipv4Config) {
			scnc_cache_update_key(newIPv4Dict, ipv4Config, doFullTunnel, kSCNetworkConnectionNetworkInfoIncludedRoutes, useOldKeys?kSCPropNetIPv4IncludedRoutes:NULL, ^(CFMutableDictionaryRef dict, CFDictionaryRef config) {
				if (doFullTunnel) {
					struct in_addr v4_zeros = {INADDR_ANY};
					CFDataRef v4ZerosData = CFDataCreate(kCFAllocatorDefault, (uint8_t*)&v4_zeros, sizeof(struct in_addr));
					CFMutableDictionaryRef tempDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
					if (v4ZerosData && tempDict) {
						CFDictionaryAddValue(tempDict, kSCNetworkConnectionNetworkInfoAddresses, v4ZerosData);
						CFDictionaryAddValue(tempDict, kSCNetworkConnectionNetworkInfoMasks, v4ZerosData);
					}
					scnc_cache_update_key(dict, tempDict, FALSE, kSCNetworkConnectionNetworkInfoAddresses, NULL, NULL);
					scnc_cache_update_key(dict, tempDict, FALSE, kSCNetworkConnectionNetworkInfoMasks, NULL, NULL);
					my_CFRelease(&tempDict);
					my_CFRelease(&v4ZerosData);
				} else {
					scnc_cache_update_key(dict, config, FALSE, kSCNetworkConnectionNetworkInfoAddresses, useOldKeys?kSCPropNetIPv4RouteDestinationAddress:NULL, NULL);
					scnc_cache_update_key(dict, config, FALSE, kSCNetworkConnectionNetworkInfoMasks, useOldKeys?kSCPropNetIPv4RouteSubnetMask:NULL, NULL);
				}
			});
			scnc_cache_update_key(newIPv4Dict, ipv4Config, FALSE, kSCNetworkConnectionNetworkInfoExcludedRoutes, useOldKeys?kSCPropNetIPv4ExcludedRoutes:NULL, ^(CFMutableDictionaryRef dict, CFDictionaryRef config) {
				scnc_cache_update_key(dict, config, FALSE, kSCNetworkConnectionNetworkInfoAddresses, useOldKeys?kSCPropNetIPv4RouteDestinationAddress:NULL, NULL);
				scnc_cache_update_key(dict, config, FALSE, kSCPropNetIPv4RouteSubnetMask, useOldKeys?kSCNetworkConnectionNetworkInfoMasks:NULL, NULL);
			});
		});
		
		scnc_cache_update_key(newServiceDict, serviceConfig, doFullTunnel, kSCNetworkConnectionNetworkInfoIPv6, useOldKeys?kSCEntNetIPv6:NULL, ^(CFMutableDictionaryRef newIPv6Dict, CFDictionaryRef ipv6Config) {
			scnc_cache_update_key(newIPv6Dict, ipv6Config, doFullTunnel, kSCNetworkConnectionNetworkInfoIncludedRoutes, useOldKeys?kSCPropNetIPv6IncludedRoutes:NULL, ^(CFMutableDictionaryRef dict, CFDictionaryRef config) {
				if (doFullTunnel) {
					struct in6_addr v6_zeros = IN6ADDR_ANY_INIT;
					CFDataRef v6ZerosData = CFDataCreate(kCFAllocatorDefault, (uint8_t*)&v6_zeros, sizeof(struct in6_addr));
					CFMutableDictionaryRef tempDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
					if (v6ZerosData && tempDict) {
						CFDictionaryAddValue(tempDict, kSCNetworkConnectionNetworkInfoAddresses, v6ZerosData);
						CFDictionaryAddValue(tempDict, kSCNetworkConnectionNetworkInfoMasks, v6ZerosData);
					}
					scnc_cache_update_key(dict, tempDict, FALSE, kSCNetworkConnectionNetworkInfoAddresses, NULL, NULL);
					scnc_cache_update_key(dict, tempDict, FALSE, kSCNetworkConnectionNetworkInfoMasks, NULL, NULL);
					my_CFRelease(&tempDict);
					my_CFRelease(&v6ZerosData);
				} else {
					scnc_cache_update_key(dict, config, FALSE, kSCNetworkConnectionNetworkInfoAddresses, useOldKeys?kSCPropNetIPv6RouteDestinationAddress:NULL, NULL);
					scnc_cache_update_key(dict, config, FALSE, kSCNetworkConnectionNetworkInfoMasks, useOldKeys?kSCPropNetIPv6RoutePrefixLength:NULL, NULL);
				}
			});
			scnc_cache_update_key(newIPv6Dict, ipv6Config, FALSE, kSCNetworkConnectionNetworkInfoExcludedRoutes, useOldKeys?kSCPropNetIPv6ExcludedRoutes:NULL, ^(CFMutableDictionaryRef dict, CFDictionaryRef config) {
				scnc_cache_update_key(dict, config, FALSE, kSCNetworkConnectionNetworkInfoAddresses, useOldKeys?kSCPropNetIPv6RouteDestinationAddress:NULL, NULL);
				scnc_cache_update_key(dict, config, FALSE, kSCNetworkConnectionNetworkInfoMasks, useOldKeys?kSCPropNetIPv6RoutePrefixLength:NULL, NULL);
			});
		});
		
		if (!my_CFEqual(oldServiceDict, newServiceDict)) {
			SCPreferencesSetValue(prefs, serv->serviceID, newServiceDict);
			SCPreferencesCommitChanges(prefs);
			SCPreferencesApplyChanges(prefs);
			
			my_CFRelease(&serv->routeCache);
			serv->routeCache = newServiceDict;
			return TRUE;
		}
		CFRelease(newServiceDict);
	}
	
	return FALSE;
}

void
scnc_cache_init_service (struct service *serv)
{
	CFDictionaryRef routeCache = scnc_cache_get_routes(serv);
	my_CFRelease(&serv->routeCache);
	serv->routeCache = my_CFRetain(routeCache);
}

void
scnc_cache_flush_removed_services (CFArrayRef activeServices)
{
	SCPreferencesRef prefs = NULL;
	CFArrayRef keys = NULL;
	CFIndex numKeys = 0;
	CFIndex numActiveServices = 0;
	Boolean removed_values = FALSE;
	
	prefs = scnc_cache_get_prefs();
	keys = SCPreferencesCopyKeyList(prefs);
	numKeys = CFArrayGetCount(keys);
	numActiveServices = CFArrayGetCount(activeServices);
	
	for (CFIndex i = 0; i < numKeys; i++) {
		CFStringRef key = CFArrayGetValueAtIndex(keys, i);
		if (!CFArrayContainsValue(activeServices, CFRangeMake(0, numActiveServices), CFArrayGetValueAtIndex(keys, i))) {
			SCPreferencesRemoveValue(prefs, key);
			removed_values = TRUE;
		}
	}
	
	my_CFRelease(&keys);

	if (removed_values) {
		SCPreferencesCommitChanges(prefs);
		SCPreferencesApplyChanges(prefs);
	}
}
