/*
 * Copyright (c) 2002-2017 Apple Inc. All rights reserved.
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
#include "ev_extra.h"

#ifndef kSCEntNetIdleRoute
#define kSCEntNetIdleRoute       CFSTR("IdleRoute")
#endif  /* kSCEntNetIdleRoute */

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
copy_mutable_dictionary(CFDictionaryRef dict)
{
	CFMutableDictionaryRef	newDict;

	if (isA_CFDictionary(dict) != NULL) {
		newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
	}
	else {
		newDict = CFDictionaryCreateMutable(NULL,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
	}
	return (newDict);
}

static CFMutableDictionaryRef
copy_entity(CFStringRef key)
{
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	newDict		= NULL;

	dict = cache_SCDynamicStoreCopyValue(store, key);
	newDict = copy_mutable_dictionary(dict);
	if (dict != NULL) {
		CFRelease(dict);
	}
	return (newDict);
}


static void
interface_update_status(const char *if_name,
			CFBooleanRef active, boolean_t attach,
			CFBooleanRef expensive, boolean_t only_if_different)
{
	CFStringRef		key		= NULL;
	CFMutableDictionaryRef	newDict;
	CFDictionaryRef		oldDict;

	key = create_interface_key(if_name);
	oldDict = cache_SCDynamicStoreCopyValue(store, key);
	if (oldDict != NULL && isA_CFDictionary(oldDict) == NULL) {
		CFRelease(oldDict);
		oldDict = NULL;
	}
	newDict = copy_mutable_dictionary(oldDict);

	/* if new status available, update cache */
	if (active != NULL) {
		CFDictionarySetValue(newDict, kSCPropNetLinkActive, active);
	} else {
		CFDictionaryRemoveValue(newDict, kSCPropNetLinkActive);
	}

	if (attach) {
		/* the interface was attached, remove stale state */
		CFDictionaryRemoveValue(newDict, kSCPropNetLinkDetaching);
	}

	if ((expensive != NULL) && CFBooleanGetValue(expensive)) {
		CFDictionarySetValue(newDict, kSCPropNetLinkExpensive, expensive);
	} else {
		CFDictionaryRemoveValue(newDict, kSCPropNetLinkExpensive);
	}

	/* update the SCDynamicStore */
	if (CFDictionaryGetCount(newDict) > 0) {
		/* set the value */
		if (!only_if_different
		    || oldDict == NULL
		    || !CFEqual(oldDict, newDict)) {
			SC_log(LOG_DEBUG, "Update interface link status: %s: %@", if_name, newDict);
			cache_SCDynamicStoreSetValue(store, key, newDict);
		}
	} else {
		/* remove the value */
		if (oldDict != NULL) {
			SC_log(LOG_DEBUG, "Update interface link status: %s: <removed>", if_name);
		}
		cache_SCDynamicStoreRemoveValue(store, key);
	}

	CFRelease(key);
	CFRelease(newDict);
	if (oldDict != NULL) {
		CFRelease(oldDict);
	}
	return;
}


#ifdef KEV_DL_LINK_QUALITY_METRIC_CHANGED
static CFStringRef
create_linkquality_key(const char * if_name)
{
	CFStringRef		interface;
	CFStringRef		key;

	interface = CFStringCreateWithCString(NULL, if_name, kCFStringEncodingMacRoman);
	key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							    kSCDynamicStoreDomainState,
							    interface,
							    kSCEntNetLinkQuality);
	CFRelease(interface);
	return (key);
}


__private_extern__
void
interface_update_quality_metric(const char *if_name,
				int quality)
{
	CFStringRef  		key             = NULL;
	CFMutableDictionaryRef	newDict         = NULL;
	CFNumberRef		linkquality     = NULL;

	key = create_linkquality_key(if_name);
	newDict = copy_entity(key);

	if (quality != IFNET_LQM_THRESH_UNKNOWN) {
		linkquality = CFNumberCreate(NULL, kCFNumberIntType, &quality);
		CFDictionarySetValue(newDict, kSCPropNetLinkQuality, linkquality);
		CFRelease(linkquality);
	} else {
		CFDictionaryRemoveValue(newDict, kSCPropNetLinkQuality);
	}

	/* update status */
	if (CFDictionaryGetCount(newDict) > 0) {
		SC_log(LOG_DEBUG, "Update interface link quality: %s: %@", if_name, newDict);
		cache_SCDynamicStoreSetValue(store, key, newDict);
	} else {
		SC_log(LOG_DEBUG, "Update interface link quality: %s: <unknown>", if_name);
		cache_SCDynamicStoreRemoveValue(store, key);
	}

	CFRelease(key);
	CFRelease(newDict);
	return;
}


static
void
link_update_quality_metric(const char *if_name)
{
	struct ifreq	ifr;
	int		quality = IFNET_LQM_THRESH_UNKNOWN;
	int		sock;

	sock = dgram_socket(AF_INET);
	if (sock == -1) {
		goto done;
	}

	bzero((char *)&ifr, sizeof(ifr));
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", if_name);

	if (ioctl(sock, SIOCGIFLINKQUALITYMETRIC, (caddr_t)&ifr) != -1) {
		quality = ifr.ifr_link_quality_metric;
	}

    done:

	interface_update_quality_metric(if_name, quality);

	if (sock != -1) {
		close(sock);
	}
	return;

}
#endif /* KEV_DL_LINK_QUALITY_METRIC_CHANGED */


#ifdef	KEV_DL_ISSUES
static CFStringRef
create_link_issues_key(const char * if_name)
{
	CFStringRef	interface;
	CFStringRef	key;

	interface = CFStringCreateWithCString(NULL, if_name, kCFStringEncodingMacRoman);
	key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							    kSCDynamicStoreDomainState,
							    interface,
							    kSCEntNetLinkIssues);
	CFRelease(interface);
	return (key);
}


__private_extern__
void
interface_update_link_issues(const char		*if_name,
			     uint64_t		timestamp,
			     uint8_t		*modid,
			     size_t		modid_size,
			     uint8_t		*info,
			     size_t		info_size)
{
	CFDataRef		infoData;
	CFStringRef		key;
	CFDataRef		modidData;
	CFMutableDictionaryRef	newDict;
	CFDateRef		timeStamp;

	key = create_link_issues_key(if_name);

	newDict = copy_entity(key);

	modidData = CFDataCreate(NULL, modid, modid_size);
	CFDictionarySetValue(newDict, kSCPropNetLinkIssuesModuleID, modidData);
	CFRelease(modidData);

	if (info_size != 0) {
		infoData = CFDataCreate(NULL, info, info_size);
		CFDictionarySetValue(newDict, kSCPropNetLinkIssuesInfo, infoData);
		CFRelease(infoData);
	} else {
		CFDictionaryRemoveValue(newDict, kSCPropNetLinkIssuesInfo);
	}

	timeStamp = CFDateCreate(NULL, timestamp);
	CFDictionarySetValue(newDict, kSCPropNetLinkIssuesTimeStamp, timeStamp);
	CFRelease(timeStamp);

	SC_log(LOG_DEBUG, "Update interface link issues: %s: %@", if_name, newDict);
	cache_SCDynamicStoreSetValue(store, key, newDict);
	CFRelease(newDict);
	CFRelease(key);
	return;
}
#endif	/* KEV_DL_ISSUES */


__private_extern__
void
interface_detaching(const char *if_name)
{
	CFStringRef		key;
	CFMutableDictionaryRef	newDict;

	SC_log(LOG_DEBUG, "Detach interface: %s", if_name);

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

	SC_log(LOG_DEBUG, "Remove interface: %s", if_name);

	key = create_interface_key(if_name);
	cache_SCDynamicStoreRemoveValue(store, key);
	CFRelease(key);

#ifdef	KEV_DL_LINK_QUALITY_METRIC_CHANGED
	key = create_linkquality_key(if_name);
	cache_SCDynamicStoreRemoveValue(store, key);
	CFRelease(key);
#endif	/* KEV_DL_LINK_QUALITY_METRIC_CHANGED */

#ifdef	KEV_DL_ISSUES
	key = create_link_issues_key(if_name);
	cache_SCDynamicStoreRemoveValue(store, key);
	CFRelease(key);
#endif	/* KEV_DL_ISSUES */

	return;
}


__private_extern__
void
link_update_status(const char *if_name, boolean_t attach, boolean_t only_if_different)
{
	CFBooleanRef		active		= NULL;
	CFBooleanRef		expensive	= NULL;
	struct ifmediareq	ifm;
	int			sock;

	sock = dgram_socket(AF_INET);
	if (sock == -1) {
		return;
	}

	/* get "Link" */
	bzero((char *)&ifm, sizeof(ifm));
	(void) strlcpy(ifm.ifm_name, if_name, sizeof(ifm.ifm_name));

	if (ioctl(sock, SIOCGIFMEDIA, (caddr_t)&ifm) == -1) {
		/* if media status not available for this interface */
		goto update;
	}

	if (ifm.ifm_count == 0) {
		/* no media types */
		goto update;
	}

	if (!(ifm.ifm_status & IFM_AVALID)) {
		/* if active bit not valid */
		goto update;
	}

	if (ifm.ifm_status & IFM_ACTIVE) {
		active = kCFBooleanTrue;
	} else {
		active = kCFBooleanFalse;
	}

 update:

	if ((active == NULL) || CFBooleanGetValue(active)) {
		/*
		 * if link status not available or active (link UP),
		 * set "Expensive"
		 */
		expensive = interface_update_expensive(if_name);
	}

	/* update status */
	interface_update_status(if_name, active, attach, expensive, only_if_different);
	close(sock);
	return;
}


__private_extern__
void
link_update_status_if_missing(const char * if_name)
{
	CFStringRef	key;
	CFDictionaryRef	dict;

	key = create_interface_key(if_name);
	dict = cache_SCDynamicStoreCopyValue(store, key);
	if (dict != NULL) {
		/* it's already present, don't update */
		CFRelease(dict);
		goto done;
	}
	link_update_status(if_name, FALSE, FALSE);
	dict = cache_SCDynamicStoreCopyValue(store, key);
	if (dict != NULL) {
		/* our action made it appear */
		messages_add_msg_with_arg("added missing link status", if_name);
		CFRelease(dict);
	}
 done:
	CFRelease(key);
	return;
}

__private_extern__
CFMutableArrayRef
interfaceListCopy(void)
{
	CFStringRef		cacheKey;
	CFDictionaryRef		dict;
	CFMutableArrayRef	ret_ifList = NULL;

	cacheKey = SCDynamicStoreKeyCreateNetworkInterface(NULL,
							   kSCDynamicStoreDomainState);
	dict = cache_SCDynamicStoreCopyValue(store, cacheKey);
	CFRelease(cacheKey);
	if (dict != NULL) {
		if (isA_CFDictionary(dict) != NULL) {
			CFArrayRef	ifList;

			ifList = CFDictionaryGetValue(dict, kSCPropNetInterfaces);
			if (isA_CFArray(ifList) != NULL) {
				ret_ifList = CFArrayCreateMutableCopy(NULL, 0, ifList);
			}
		}
		CFRelease(dict);
	}
	if (ret_ifList == NULL) {
		ret_ifList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}
	return (ret_ifList);
}


__private_extern__
void
interfaceListUpdate(CFArrayRef ifList)
{
	CFStringRef	cacheKey;
	CFDictionaryRef	dict;

	cacheKey = SCDynamicStoreKeyCreateNetworkInterface(NULL,
							   kSCDynamicStoreDomainState);
	dict = cache_SCDynamicStoreCopyValue(store, cacheKey);
	if (dict != NULL && isA_CFDictionary(dict) == NULL) {
		CFRelease(dict);
		dict = NULL;
	}
	if (dict == NULL) {
		dict = CFDictionaryCreate(NULL,
					  (const void * *)&kSCPropNetInterfaces,
					  (const void * *)&ifList,
					  1,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks);
		cache_SCDynamicStoreSetValue(store, cacheKey, dict);
		CFRelease(dict);

	}
	else {
		CFMutableDictionaryRef	newDict;

		newDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
		CFRelease(dict);
		CFDictionarySetValue(newDict, kSCPropNetInterfaces, ifList);
		cache_SCDynamicStoreSetValue(store, cacheKey, newDict);
		CFRelease(newDict);
	}
	CFRelease(cacheKey);
	return;
}


__private_extern__
Boolean
interfaceListAddInterface(CFMutableArrayRef ifList, const char * if_name)
{
	Boolean		added = FALSE;
	CFStringRef	interface;

	interface = CFStringCreateWithCString(NULL, if_name, kCFStringEncodingMacRoman);
	if (!CFArrayContainsValue(ifList,
				  CFRangeMake(0, CFArrayGetCount(ifList)),
				  interface)) {
		/* interface was added, prime the link-specific values */
		added = TRUE;
		CFArrayAppendValue(ifList, interface);
		link_update_status(if_name, TRUE, FALSE);
#ifdef KEV_DL_LINK_QUALITY_METRIC_CHANGED
		link_update_quality_metric(if_name);
#endif /* KEV_DL_LINK_QUALITY_METRIC_CHANGED */
	}
	else {
		/* only update the link status if it is different */
		link_update_status(if_name, FALSE, TRUE);
	}
	CFRelease(interface);
	return (added);
}


static Boolean
interfaceListRemoveInterface(CFMutableArrayRef ifList, const char * if_name)
{
	CFStringRef	interface;
	CFIndex		where;

	interface = CFStringCreateWithCString(NULL, if_name, kCFStringEncodingMacRoman);
	where = CFArrayGetFirstIndexOfValue(ifList,
					    CFRangeMake(0, CFArrayGetCount(ifList)),
					    interface);
	CFRelease(interface);
	if (where != kCFNotFound) {
		CFArrayRemoveValueAtIndex(ifList, where);
		interface_remove(if_name);
	}
	return (where != kCFNotFound);
}


__private_extern__
void
link_add(const char *if_name)
{
	CFMutableArrayRef	ifList;

	ifList = interfaceListCopy();
	if (interfaceListAddInterface(ifList, if_name)) {
		/* interface was added, update the global list */
		messages_add_msg_with_arg("link_add", if_name);
		interfaceListUpdate(ifList);
		config_new_interface(if_name);
	}
	CFRelease(ifList);
	return;
}


__private_extern__
void
link_remove(const char *if_name)
{
	CFMutableArrayRef	ifList;

	ifList = interfaceListCopy();
	if (interfaceListRemoveInterface(ifList, if_name)) {
		/* interface was removed, update the global list */
		interfaceListUpdate(ifList);
	}
	CFRelease(ifList);
	return;
}


#ifdef KEV_DL_IF_IDLE_ROUTE_REFCNT
#define INVALID_SOCKET_REF 	-1
static
int
socket_reference_count(const char* if_name) {
	struct ifreq	ifr;
	int		ref = INVALID_SOCKET_REF;
	int		s;

	s = dgram_socket(AF_INET);
	if (s == -1) {
		return (ref);
	}

	bzero((char *)&ifr, sizeof(ifr));
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", if_name);

	if (ioctl(s, SIOCGIFGETRTREFCNT, (caddr_t)&ifr) != -1) {
		ref = ifr.ifr_route_refcnt;
	} else {
		ref = INVALID_SOCKET_REF;
	}
	close(s);
	return (ref);
}


__private_extern__
void
interface_update_idle_state(const char *if_name)
{
	CFStringRef		if_name_cf;
	CFStringRef		key;
	int			ref;

	/* We will only update the SCDynamicStore if the idle ref count
	 * is still 0 */
	ref = socket_reference_count(if_name);
	if (ref != 0) {
		return;
	}

	if_name_cf = CFStringCreateWithCString(NULL, if_name,
					       kCFStringEncodingASCII);

	key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							    kSCDynamicStoreDomainState,
							    if_name_cf,
							    kSCEntNetIdleRoute);

	SC_log(LOG_DEBUG, "Post interface idle: %s", if_name);
	cache_SCDynamicStoreNotifyValue(store, key);
	CFRelease(key);
	CFRelease(if_name_cf);
	return;
}
#endif	// KEV_DL_IF_IDLE_ROUTE_REFCNT
