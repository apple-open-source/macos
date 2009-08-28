/*
 * Copyright (c) 2002-2006, 2009 Apple Inc. All rights reserved.
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
#include "ev_dlil.h"

static CFStringRef
create_interface_key(const char * if_name)
{
	CFStringRef		interface;
	CFStringRef		key;

	interface = CFStringCreateWithCString(NULL, if_name, kCFStringEncodingMacRoman);
	key       = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
								  kSCDynamicStoreDomainState,
								  interface,
								  kSCEntNetLink);
	CFRelease(interface);
	return (key);
}


static CFMutableDictionaryRef
copy_entity(CFStringRef key)
{
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict		= NULL;

	dict = cache_SCDynamicStoreCopyValue(store, key);
	if (dict != NULL) {
		if (isA_CFDictionary(dict) != NULL) {
			newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
		}
		CFRelease(dict);
	}
	if (newDict == NULL) {
		newDict = CFDictionaryCreateMutable(NULL,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
	}
	return (newDict);
}


static void
interface_update_status(const char *if_name, CFBooleanRef active,
			boolean_t attach)
{
	CFStringRef		key		= NULL;
	CFMutableDictionaryRef	newDict		= NULL;

	key = create_interface_key(if_name);
	newDict = copy_entity(key);
	/* if new status available, update cache */
	if (active == NULL) {
	    CFDictionaryRemoveValue(newDict, kSCPropNetLinkActive);
	} else {
	    CFDictionarySetValue(newDict, kSCPropNetLinkActive, active);
	}
	if (attach == TRUE) {
		/* the interface was attached, remove stale state */
		CFDictionaryRemoveValue(newDict, kSCPropNetLinkDetaching);
	}

	/* update status */
	if (CFDictionaryGetCount(newDict) > 0) {
		cache_SCDynamicStoreSetValue(store, key, newDict);
	} else {
		cache_SCDynamicStoreRemoveValue(store, key);
	}

	CFRelease(key);
	CFRelease(newDict);
	return;
}

__private_extern__
void
interface_detaching(const char *if_name)
{
	CFStringRef		key;
	CFMutableDictionaryRef	newDict;

	key = create_interface_key(if_name);
	newDict = copy_entity(key);
	CFDictionarySetValue(newDict, kSCPropNetLinkDetaching,
			     kCFBooleanTrue);
	cache_SCDynamicStoreSetValue(store, key, newDict);
	CFRelease(newDict);
	CFRelease(key);
	return;
}

static void
interface_remove(const char *if_name)
{
	CFStringRef		key;

	key = create_interface_key(if_name);
	cache_SCDynamicStoreRemoveValue(store, key);
	CFRelease(key);
	return;
}


__private_extern__
void
link_update_status(const char *if_name, boolean_t attach)
{
	CFBooleanRef		active	= NULL;
	struct ifmediareq	ifm;
	int			sock;

	sock = dgram_socket(AF_INET);
	if (sock == -1) {
		SCLog(TRUE, LOG_NOTICE, CFSTR("link_update_status: socket open failed,  %s"), strerror(errno));
		goto done;
	}
	bzero((char *)&ifm, sizeof(ifm));
	(void) strncpy(ifm.ifm_name, if_name, sizeof(ifm.ifm_name));

	if (ioctl(sock, SIOCGIFMEDIA, (caddr_t)&ifm) == -1) {
		/* if media status not available for this interface */
		goto done;
	}

	if (ifm.ifm_count == 0) {
		/* no media types */
		goto done;
	}

	if (!(ifm.ifm_status & IFM_AVALID)) {
		/* if active bit not valid */
		goto done;
	}

	if (ifm.ifm_status & IFM_ACTIVE) {
		active = kCFBooleanTrue;
	} else {
		active = kCFBooleanFalse;
	}

 done:
	interface_update_status(if_name, active, attach);
	if (sock != -1)
		close(sock);
	return;
}


__private_extern__
void
link_add(const char *if_name)
{
	CFStringRef		interface;
	CFStringRef		cacheKey;
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict		= NULL;
	CFArrayRef		ifList;
	CFMutableArrayRef	newIFList	= NULL;

	interface = CFStringCreateWithCString(NULL, if_name, kCFStringEncodingMacRoman);
	cacheKey  = SCDynamicStoreKeyCreateNetworkInterface(NULL,
							    kSCDynamicStoreDomainState);

	dict = cache_SCDynamicStoreCopyValue(store, cacheKey);
	if (dict) {
		if (isA_CFDictionary(dict)) {
			newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
			ifList  = CFDictionaryGetValue(newDict, kSCPropNetInterfaces);
			if (isA_CFArray(ifList)) {
				newIFList = CFArrayCreateMutableCopy(NULL, 0, ifList);
			}
		}
		CFRelease(dict);
	}

	if (!newDict) {
		newDict = CFDictionaryCreateMutable(NULL,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
	}

	if (!newIFList) {
		newIFList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}

	if (CFArrayContainsValue(newIFList,
				 CFRangeMake(0, CFArrayGetCount(newIFList)),
				 interface) == FALSE) {
		CFArrayAppendValue(newIFList, interface);
		CFDictionarySetValue(newDict, kSCPropNetInterfaces, newIFList);
	}
	cache_SCDynamicStoreSetValue(store, cacheKey, newDict);
	link_update_status(if_name, TRUE);
	CFRelease(cacheKey);
	CFRelease(interface);
	if (newDict)	CFRelease(newDict);
	if (newIFList)	CFRelease(newIFList);

	return;
}


__private_extern__
void
link_remove(const char *if_name)
{
	CFStringRef		interface;
	CFStringRef		cacheKey;
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict		= NULL;
	CFArrayRef		ifList;
	CFMutableArrayRef	newIFList	= NULL;
	CFIndex			i;

	interface = CFStringCreateWithCString(NULL, if_name, kCFStringEncodingMacRoman);
	cacheKey  = SCDynamicStoreKeyCreateNetworkInterface(NULL,
							    kSCDynamicStoreDomainState);

	dict = cache_SCDynamicStoreCopyValue(store, cacheKey);
	if (dict) {
		if (isA_CFDictionary(dict)) {
			newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
			ifList  = CFDictionaryGetValue(newDict, kSCPropNetInterfaces);
			if (isA_CFArray(ifList)) {
				newIFList = CFArrayCreateMutableCopy(NULL, 0, ifList);
			}
		}
		CFRelease(dict);
	}

	if (!newIFList ||
	    ((i = CFArrayGetFirstIndexOfValue(newIFList,
					     CFRangeMake(0, CFArrayGetCount(newIFList)),
					     interface)) == kCFNotFound)
	   ) {
		/* we're not tracking this interface */
		goto done;
	}

	CFArrayRemoveValueAtIndex(newIFList, i);
	CFDictionarySetValue(newDict, kSCPropNetInterfaces, newIFList);
	cache_SCDynamicStoreSetValue(store, cacheKey, newDict);

	interface_remove(if_name);

    done:

	CFRelease(cacheKey);
	CFRelease(interface);
	if (newDict)	CFRelease(newDict);
	if (newIFList)	CFRelease(newIFList);

	return;
}
