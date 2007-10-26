/*
 * Copyright (c) 2002-2007 Apple Inc. All rights reserved.
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
 * August 5, 2002	Allan Nathanson <ajn@apple.com>
 * - split code out from eventmon.c
 */

#include "eventmon.h"
#include "cache.h"
#include "ev_appletalk.h"

// from <netat/ddp.h>
#define DDP_MIN_NETWORK	0x0001
#define DDP_MAX_NETWORK	0xfffe


static int
get_atalk_interface_cfg(const char *if_name, at_if_cfg_t *cfg)
{
	int 	fd;

	/* open socket */
	if ((fd = socket(AF_APPLETALK, SOCK_RAW, 0)) == -1)
		return -1;

	/* get config info for given interface */
	strncpy(cfg->ifr_name, if_name, sizeof(cfg->ifr_name));
	if (ioctl(fd, AIOCGETIFCFG, (caddr_t)cfg) == -1) {
		(void)close(fd);
		return -1;
	}

	(void)close(fd);
	return 0;
}


static CFMutableDictionaryRef
getIF(CFStringRef key, CFMutableDictionaryRef oldIFs, CFMutableDictionaryRef newIFs)
{
	CFDictionaryRef		dict		= NULL;
	CFMutableDictionaryRef	newDict		= NULL;

	if (CFDictionaryGetValueIfPresent(newIFs, key, (const void **)&dict)) {
		newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
	} else {
		dict = cache_SCDynamicStoreCopyValue(store, key);
		if (dict) {
			CFDictionarySetValue(oldIFs, key, dict);
			if (isA_CFDictionary(dict)) {
				newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
				CFDictionaryRemoveValue(newDict, kSCPropNetAppleTalkNetworkID);
				CFDictionaryRemoveValue(newDict, kSCPropNetAppleTalkNodeID);
				CFDictionaryRemoveValue(newDict, kSCPropNetAppleTalkNetworkRange);
				CFDictionaryRemoveValue(newDict, kSCPropNetAppleTalkDefaultZone);
			}
			CFRelease(dict);
		}
	}

	if (!newDict) {
		newDict = CFDictionaryCreateMutable(NULL,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
	}

	return newDict;
}


static void
updateStore(const void *key, const void *value, void *context)
{
	CFDictionaryRef dict;
	CFDictionaryRef newDict = (CFDictionaryRef)value;
	CFDictionaryRef	oldIFs	= (CFDictionaryRef)context;

	dict = CFDictionaryGetValue(oldIFs, key);

	if (!dict || !CFEqual(dict, newDict)) {
		if (CFDictionaryGetCount(newDict) > 0) {
			cache_SCDynamicStoreSetValue(store, key, newDict);
		} else if (dict) {
			cache_SCDynamicStoreRemoveValue(store, key);
		}
		network_changed = TRUE;
	}

	return;
}


__private_extern__
void
interface_update_appletalk(struct ifaddrs *ifap, const char *if_name)
{
	struct ifaddrs		*ifa;
	struct ifaddrs		*ifap_temp	= NULL;
	CFStringRef		interface;
	boolean_t		interfaceFound	= FALSE;
	CFStringRef		key		= NULL;
	CFMutableDictionaryRef	oldIFs;
	CFMutableDictionaryRef	newDict		= NULL;
	CFMutableDictionaryRef	newIFs;

	oldIFs = CFDictionaryCreateMutable(NULL,
					   0,
					   &kCFTypeDictionaryKeyCallBacks,
					   &kCFTypeDictionaryValueCallBacks);
	newIFs = CFDictionaryCreateMutable(NULL,
					   0,
					   &kCFTypeDictionaryKeyCallBacks,
					   &kCFTypeDictionaryValueCallBacks);

	if (!ifap) {
		if (getifaddrs(&ifap_temp) == -1) {
			SCLog(TRUE, LOG_ERR, CFSTR("getifaddrs() failed: %s"), strerror(errno));
			goto error;
		}
		ifap = ifap_temp;
	}

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		at_if_cfg_t		cfg;
		int			iVal;
		CFNumberRef		num;
		struct sockaddr_at	*sat;

		if (ifa->ifa_addr->sa_family != AF_APPLETALK) {
			continue;			/* sorry, not interested */
		}

		/* check if this is the requested interface */
		if (if_name) {
			if (strncmp(if_name, ifa->ifa_name, IFNAMSIZ) == 0) {
				interfaceFound = TRUE;	/* yes, this is the one I want */
			} else {
				continue;		/* sorry, not interested */
			}
		}

		/* get the current cache information */
		interface = CFStringCreateWithCString(NULL, ifa->ifa_name, kCFStringEncodingMacRoman);
		key       = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
									  kSCDynamicStoreDomainState,
									  interface,
									  kSCEntNetAppleTalk);
		CFRelease(interface);

		newDict = getIF(key, oldIFs, newIFs);

		sat = (struct sockaddr_at *)ifa->ifa_addr;

		iVal = (int)sat->sat_addr.s_net;
		num  = CFNumberCreate(NULL, kCFNumberIntType, &iVal);
		CFDictionarySetValue(newDict, kSCPropNetAppleTalkNetworkID, num);
		CFRelease(num);

		iVal = (int)sat->sat_addr.s_node;
		num  = CFNumberCreate(NULL, kCFNumberIntType, &iVal);
		CFDictionarySetValue(newDict, kSCPropNetAppleTalkNodeID, num);
		CFRelease(num);

		if (get_atalk_interface_cfg(ifa->ifa_name, &cfg) == 0) {
			CFStringRef		zone;

			/*
			 * Set starting and ending net values
			 */
			if (!(((cfg.netStart == 0) && (cfg.netEnd == 0)) ||
			      ((cfg.netStart == DDP_MIN_NETWORK) && (cfg.netEnd == DDP_MAX_NETWORK)))) {
				CFMutableArrayRef	array;

				array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

				iVal = cfg.netStart;
				num  = CFNumberCreate(NULL, kCFNumberIntType, &iVal);
				CFArrayAppendValue(array, num);
				CFRelease(num);

				iVal = cfg.netEnd;
				num  = CFNumberCreate(NULL, kCFNumberIntType, &iVal);
				CFArrayAppendValue(array, num);
				CFRelease(num);

				CFDictionarySetValue(newDict, kSCPropNetAppleTalkNetworkRange, array);
				CFRelease(array);
			}

			/*
			 * Set the default zone
			 */
			zone = CFStringCreateWithPascalString(NULL,
							      (ConstStr255Param)&cfg.zonename,
							      kCFStringEncodingMacRoman);
			CFDictionarySetValue(newDict, kSCPropNetAppleTalkDefaultZone, zone);
			CFRelease(zone);
		}

		CFDictionarySetValue(newIFs, key, newDict);
		CFRelease(newDict);
		CFRelease(key);
	}

	/* if the last address[es] were removed from the target interface */
	if (if_name && !interfaceFound) {
		interface = CFStringCreateWithCString(NULL, if_name, kCFStringEncodingMacRoman);
		key       = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
									  kSCDynamicStoreDomainState,
									  interface,
									  kSCEntNetAppleTalk);
		CFRelease(interface);

		newDict = getIF(key, oldIFs, newIFs);

		CFDictionarySetValue(newIFs, key, newDict);
		CFRelease(newDict);
		CFRelease(key);
	}

	CFDictionaryApplyFunction(newIFs, updateStore, oldIFs);

    error :

	if (ifap_temp)	freeifaddrs(ifap_temp);
	CFRelease(oldIFs);
	CFRelease(newIFs);

	return;
}


__private_extern__
void
interface_update_atalk_address(struct kev_atalk_data *aEvent, const char *if_name)
{
	CFStringRef		interface;
	CFStringRef		key;
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict = NULL;
	CFNumberRef		newNode, newNet;
	int			node;
	int			net;

	/* get the current cache information */
	interface = CFStringCreateWithCString(NULL, if_name, kCFStringEncodingMacRoman);
	key       = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
								  kSCDynamicStoreDomainState,
								  interface,
								  kSCEntNetAppleTalk);
	CFRelease(interface);

	dict = cache_SCDynamicStoreCopyValue(store, key);
	if (dict) {
		if (isA_CFDictionary(dict)) {
			newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
		}
		CFRelease(dict);
	}

	if (!newDict) {
		newDict = CFDictionaryCreateMutable(NULL,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
	}

	/* Update node/net values in cache */
	node	= (int)aEvent->node_data.address.s_node;
	net	= (int)aEvent->node_data.address.s_net;

	newNode	= CFNumberCreate(NULL, kCFNumberIntType, &node);
	newNet 	= CFNumberCreate(NULL, kCFNumberIntType, &net);

	CFDictionarySetValue(newDict, kSCPropNetAppleTalkNodeID, newNode);
	CFDictionarySetValue(newDict, kSCPropNetAppleTalkNetworkID, newNet);

	CFRelease(newNode);
	CFRelease(newNet);

	/* update cache */
	cache_SCDynamicStoreSetValue(store, key, newDict);
	network_changed = TRUE;
	CFRelease(newDict);
	CFRelease(key);
	return;
}


__private_extern__
void
interface_update_atalk_zone(struct kev_atalk_data *aEvent, const char *if_name)
{
	CFStringRef		interface;
	CFStringRef		key;
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict	= NULL;
	CFStringRef		newZone;

	/* get the current cache information */
	interface = CFStringCreateWithCString(NULL, if_name, kCFStringEncodingMacRoman);
	key       = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
								  kSCDynamicStoreDomainState,
								  interface,
								  kSCEntNetAppleTalk);
	CFRelease(interface);

	dict = cache_SCDynamicStoreCopyValue(store, key);
	if (dict) {
		if (isA_CFDictionary(dict)) {
			newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
		}
		CFRelease(dict);
	}

	if (!newDict) {
		newDict = CFDictionaryCreateMutable(NULL,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
	}

	/* Update zone value in cache */
	newZone = CFStringCreateWithPascalString(NULL, (ConstStr255Param)&(aEvent->node_data.zone), kCFStringEncodingMacRoman);

	CFDictionarySetValue(newDict, kSCPropNetAppleTalkDefaultZone, newZone);

	CFRelease(newZone);

	/* update cache */
	cache_SCDynamicStoreSetValue(store, key, newDict);
	network_changed = TRUE;
	CFRelease(newDict);
	CFRelease(key);
	return;
}


__private_extern__
void
interface_update_shutdown_atalk()
{
	CFStringRef			cacheKey;
	CFDictionaryRef			dict;
	CFArrayRef			ifList = NULL;
	CFIndex 			count, index;
	CFStringRef 			interface;
	CFStringRef			key;

	cacheKey  = SCDynamicStoreKeyCreateNetworkInterface(NULL,
							    kSCDynamicStoreDomainState);

	dict = cache_SCDynamicStoreCopyValue(store, cacheKey);
	CFRelease(cacheKey);

	if (dict) {
		if (isA_CFDictionary(dict)) {
			/*get a list of the interfaces*/
			ifList  = isA_CFArray(CFDictionaryGetValue(dict, kSCPropNetInterfaces));
			if (ifList) {
				count = CFArrayGetCount(ifList);

				/*iterate through list and remove AppleTalk data*/
				for (index = 0; index < count; index++) {
					interface = CFArrayGetValueAtIndex(ifList, index);
					key       = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
												  kSCDynamicStoreDomainState,
												  interface,
												  kSCEntNetAppleTalk);
					cache_SCDynamicStoreRemoveValue(store, key);
					network_changed = TRUE;
					CFRelease(key);
				}
			}
		}
		CFRelease(dict);
	}

	return;
}
