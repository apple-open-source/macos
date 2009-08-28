/*
 * Copyright (c) 2005-2007, 2009 Apple Inc.  All Rights Reserved.
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
 * NetworkIdentification.c
 * - maintains a history of networks that the system has connected to by
 *   watching the Network Services that post data to the SCDynamicStore
 */

/* 
 * Modification History
 *
 * November 9, 2006	Dieter Siegmund (dieter@apple.com)
 * - created
 */

#include <notify.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>
#include <CoreFoundation/CFDictionary.h>
#include <SystemConfiguration/SCNetworkSignature.h>
#include <SystemConfiguration/SCNetworkSignaturePrivate.h>

/* debug output on/off */
static Boolean	 		S_NetworkIdentification_debug;

/* should we bother keeping track of networks? */
static Boolean			S_NetworkIdentification_disabled;

typedef struct ServiceWatcher_s ServiceWatcher, * ServiceWatcherRef;

/* returns an array of currently available information */
static CFArrayRef
ServiceWatcherCopyCurrent(ServiceWatcherRef watcher);

static ServiceWatcherRef
ServiceWatcherCreate();

static void
ServiceWatcherFree(ServiceWatcherRef * watcher_p);

/* XXX these should be made tunable */
#define SIGNATURE_HISTORY_MAX			150
#define SERVICE_HISTORY_MAX			5

/* don't re-write the prefs file unless this time interval has elapsed */
#define SIGNATURE_UPDATE_INTERVAL_SECS	(24 * 3600) /* 24 hours */

struct ServiceWatcher_s {
    CFRunLoopSourceRef		rls;
    SCDynamicStoreRef		store;
    CFMutableArrayRef		signatures;
    CFArrayRef			active_signatures;
    CFStringRef			primary_ipv4;
    CFStringRef			setup_ipv4_key;
    CFStringRef			state_ipv4_key;
};

#define kIdentifier		CFSTR("Identifier")
#define kService		CFSTR("Service")
#define kServices		CFSTR("Services")
#define kSignature		CFSTR("Signature")
#define kSignatures		CFSTR("Signatures")
#define kTimestamp		CFSTR("Timestamp")
#define kServiceID		CFSTR("ServiceID")
#define kNetworkSignature	CFSTR("NetworkSignature")
#define kServiceIdentifiers	kStoreKeyServiceIdentifiers

static CFArrayRef
make_service_entity_pattern_array(CFStringRef * keys, int n_keys)
{
    int		i;
    CFArrayRef	list;

    for (i = 0; i < n_keys; i++) {
	/* re-use the array that was passed in to get the pattern */
	keys[i] = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL, 
							      kSCDynamicStoreDomainState,
							      kSCCompAnyRegex,
							      keys[i]);
    }
    list = CFArrayCreate(NULL, (const void * *)keys, n_keys,
			 &kCFTypeArrayCallBacks);
    for (i = 0; i < n_keys; i++) {
	/* then release the allocated patterns */
	CFRelease(keys[i]);
    }
    return (list);
}

static CFArrayRef
ServiceWatcherNotificationPatterns(void)
{
    CFStringRef	keys[1] = { kSCEntNetIPv4 };
    
    return (make_service_entity_pattern_array(keys, 
					      sizeof(keys) / sizeof(keys[0])));
}

static CFArrayRef
ServiceWatcherPatterns(void)
{
    CFStringRef	keys[2] = { kSCEntNetIPv4, kSCEntNetDNS };
    
    return (make_service_entity_pattern_array(keys,
					      sizeof(keys) / sizeof(keys[0])));
}

static CFTypeRef
myCFDictionaryArrayGetValue(CFArrayRef array, CFStringRef key, CFTypeRef value,
			    int * ret_index)
{
    int 	count = 0;
    int		i;

    if (array != NULL) {
	count = CFArrayGetCount(array);
    }
    if (count == 0) {
	goto done;
    }
    for (i = 0; i < count; i++) {
	CFDictionaryRef		dict;
	CFTypeRef		this_val;

	dict = CFArrayGetValueAtIndex(array, i);
	if (isA_CFDictionary(dict) == NULL) {
	    continue;
	}
	this_val = CFDictionaryGetValue(dict, key);
	if (CFEqual(this_val, value)) {
	    if (ret_index != NULL) {
		*ret_index = i;
	    }
	    return (dict);
	}
    }
 done:
    if (ret_index != NULL) {
	*ret_index = -1;
    }
    return (NULL);
}

static CFDictionaryRef
copy_airport_dict(SCDynamicStoreRef store, CFStringRef if_name)
{
    CFDictionaryRef	dict;
    CFStringRef		key;

    key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL, 
							kSCDynamicStoreDomainState,
							if_name,
							kSCEntNetAirPort);
    dict = SCDynamicStoreCopyValue(store, key);
    CFRelease(key);
    return (dict);
}

static void
add_airport_info(SCDynamicStoreRef store, CFMutableDictionaryRef dict)
{
    CFDictionaryRef	airport_dict = NULL;
    CFStringRef		key;
    CFStringRef		if_name;
    CFDictionaryRef	simple_dict;
    CFStringRef		value;

    if_name = CFDictionaryGetValue(dict, kSCPropInterfaceName);
    if (isA_CFString(if_name) == NULL) {
	goto done;
    }
    airport_dict = copy_airport_dict(store, if_name);
    if (airport_dict == NULL) {
	goto done;
    }
    key = CFSTR("SSID");
    value = CFDictionaryGetValue(airport_dict, key);
    if (value == NULL) {
	goto done;
    }
    simple_dict =
	CFDictionaryCreate(NULL, 
			   (const void * *)&key, (const void * *)&value, 1, 
			   &kCFTypeDictionaryKeyCallBacks,
			   &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(dict, kSCEntNetAirPort, simple_dict);
    CFRelease(simple_dict);

 done:
    if (airport_dict != NULL) {
	CFRelease(airport_dict);
    }
    return;
}

static CFDictionaryRef
get_current_dict(CFDictionaryRef current, CFStringRef entity,
		 CFArrayRef components)
{
    CFDictionaryRef	dict;
    CFStringRef		key;

    if (CFArrayGetCount(components) < 5) {
	/* this can't happen, we already checked */
	return (NULL);
    }
    key = CFStringCreateWithFormat(NULL, NULL,
				   CFSTR("%@/%@/%@/%@/%@"),
				   CFArrayGetValueAtIndex(components, 0),
				   CFArrayGetValueAtIndex(components, 1),
				   CFArrayGetValueAtIndex(components, 2),
				   CFArrayGetValueAtIndex(components, 3),
				   entity);
    dict = CFDictionaryGetValue(current, key);
    CFRelease(key);
    return (isA_CFDictionary(dict));
}

static CFArrayRef
process_dict(SCDynamicStoreRef store, CFDictionaryRef current)
{
    CFMutableArrayRef		array = NULL;
    int				count = 0;
    int				i;
    const void * *		keys = NULL;
    const void * *		values = NULL;

    count = CFDictionaryGetCount(current);
    if (count == 0) {
	goto done;
    }
    array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    keys = (const void * *)malloc(sizeof(keys) * count);
    values = (const void * *)malloc(sizeof(values) * count);
    CFDictionaryGetKeysAndValues(current, keys, values);
    for (i = 0; i < count; i++) {
	CFArrayRef		components = NULL;
	CFDictionaryRef		dns_dict;
	CFStringRef		entity;
	CFMutableDictionaryRef	entity_dict = NULL;
	CFMutableDictionaryRef	new_dict = NULL;
	CFStringRef		sig_str = NULL;
	CFMutableDictionaryRef	service_dict = NULL;
	CFStringRef		serviceID;
	
	if (isA_CFDictionary(values[i]) == NULL) {
	    goto loop_done;
	}
	components = CFStringCreateArrayBySeparatingStrings(NULL, keys[i],
							    CFSTR("/"));
	if (components == NULL) {
	    goto loop_done;
	}
	if (CFArrayGetCount(components) < 5) {
	    /* too few components */
	    goto loop_done;
	}
	entity = CFArrayGetValueAtIndex(components, 4);
	if (!CFEqual(entity, kSCEntNetIPv4)) {
	    goto loop_done;
	}
	serviceID = CFArrayGetValueAtIndex(components, 3);
	sig_str = CFDictionaryGetValue(values[i], kNetworkSignature);
	if (isA_CFString(sig_str) == NULL
	    || CFStringGetLength(sig_str) == 0) {
	    goto loop_done;
	}
	/* create a new entry */
	new_dict = CFDictionaryCreateMutable(NULL, 0, 
					     &kCFTypeDictionaryKeyCallBacks,
					     &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(new_dict, kSignature, sig_str);
	service_dict = CFDictionaryCreateMutable(NULL, 0, 
						 &kCFTypeDictionaryKeyCallBacks,
						 &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(service_dict, kServiceID, serviceID);
	add_airport_info(store, service_dict);
	entity_dict = CFDictionaryCreateMutableCopy(NULL, 0, values[i]);
	CFDictionaryRemoveValue(entity_dict, kNetworkSignature);
	CFDictionarySetValue(service_dict, kSCEntNetIPv4, entity_dict);
	dns_dict = get_current_dict(current, kSCEntNetDNS, components);
	if (dns_dict != NULL) {
	    CFDictionarySetValue(service_dict, kSCEntNetDNS, dns_dict);
	}
	CFDictionarySetValue(new_dict, kService, service_dict);
	CFArrayAppendValue(array, new_dict);

    loop_done:
	if (entity_dict != NULL) {
	    CFRelease(entity_dict);
	}
	if (service_dict != NULL) {
	    CFRelease(service_dict);
	}
	if (components != NULL) {
	    CFRelease(components);
	}
	if (new_dict != NULL) {
	    CFRelease(new_dict);
	}
    }
    count = CFArrayGetCount(array);
    if (count == 0) {
	CFRelease(array);
	array = NULL;
	goto done;
    }

 done:
    if (keys != NULL) {
	free(keys);
    }
    if (values != NULL) {
	free(values);
    }
    return (array);

}

static CFArrayRef
ServiceWatcherCopyCurrent(ServiceWatcherRef watcher)
{
    CFDictionaryRef	current;
    CFArrayRef		list;
    CFArrayRef		ret = NULL;
    
    list = ServiceWatcherPatterns();
    current = SCDynamicStoreCopyMultiple(watcher->store, NULL, list);
    CFRelease(list);
    if (current == NULL) {
	goto done;
    }
    ret = process_dict(watcher->store, current);
 done:
    if (current != NULL) {
	CFRelease(current);
    }
    return (ret);
}

static Boolean
ServiceWatcherSetActiveSignatures(ServiceWatcherRef watcher, CFArrayRef active)
{
    Boolean	changed = FALSE;
    CFArrayRef	prev_active;

    prev_active = watcher->active_signatures;
    if (prev_active == NULL && active == NULL) {
	/* nothing to do */
	goto done;
    }
    if (prev_active != NULL && active != NULL) {
	changed = !CFEqual(prev_active, active);
    }
    else {
	changed = TRUE;
    }
    if (active != NULL) {
	CFRetain(active);
    }
    if (prev_active != NULL) {
	CFRelease(prev_active);
    }
    watcher->active_signatures = active;
    if (changed) {
	if (active != NULL) {
	    SCLog(S_NetworkIdentification_debug,
		  LOG_NOTICE, CFSTR("Active Signatures %@"), active);
	}
	else {
	    SCLog(S_NetworkIdentification_debug,
		  LOG_NOTICE, CFSTR("No Active Signatures"));
	}
    }
 done:
    return (changed);
}

static Boolean
ServiceWatcherSetPrimaryIPv4(ServiceWatcherRef watcher,
			     CFStringRef primary_ipv4)
{
    Boolean		changed = FALSE;
    CFStringRef		prev_ipv4_primary;

    prev_ipv4_primary = watcher->primary_ipv4;
    if (prev_ipv4_primary == NULL && primary_ipv4 == NULL) {
	/* nothing to do */
	goto done;
    }
    if (prev_ipv4_primary != NULL && primary_ipv4 != NULL) {
	changed = !CFEqual(prev_ipv4_primary, primary_ipv4);
    }
    else {
	changed = TRUE;
    }
    if (primary_ipv4 != NULL) {
	CFRetain(primary_ipv4);
    }
    if (prev_ipv4_primary != NULL) {
	CFRelease(prev_ipv4_primary);
    }
    watcher->primary_ipv4 = primary_ipv4;
    if (changed) {
	if (primary_ipv4 != NULL) {
	    SCLog(S_NetworkIdentification_debug,
		  LOG_NOTICE, CFSTR("Primary IPv4 %@"), primary_ipv4);
	}
	else {
	    SCLog(S_NetworkIdentification_debug, LOG_NOTICE,
		  CFSTR("No Primary IPv4"));
	}
    }
 done:
    return (changed);
}


static CFDictionaryRef
signature_add_service(CFDictionaryRef sig_dict, CFDictionaryRef service,
		      CFArrayRef active_services)
{
    CFArrayRef			list;
    CFMutableDictionaryRef	new_dict = NULL;
    CFDateRef			now;

    list = CFDictionaryGetValue(sig_dict, kServices);
    now = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
    if (list == NULL) {
	list = CFArrayCreate(NULL, (const void * *)&service, 1,
			     &kCFTypeArrayCallBacks);
    }
    else {
	int			list_count = CFArrayGetCount(list);
	CFMutableArrayRef	new_list = NULL;
	CFRange			range = CFRangeMake(0, list_count);
	int			where;

	where = CFArrayGetFirstIndexOfValue(list, range, service);
	if (where != kCFNotFound) {
	    CFDateRef		date;

	    date = CFDictionaryGetValue(sig_dict, kTimestamp);
	    if (date != NULL) {
		CFTimeInterval	time_interval;
		
		time_interval = CFDateGetTimeIntervalSinceDate(now, date);
		/* don't bother updating timestamp until interval has passed */
		if (time_interval < (SIGNATURE_UPDATE_INTERVAL_SECS)) {
		    goto done;
		}
	    }
	    if (where == 0) {
		/* it's already in the right place */
		list = NULL;
	    }
	}

	if (list != NULL) {
	    new_list = CFArrayCreateMutableCopy(NULL, 0, list);
	    if (where != kCFNotFound) {
		CFArrayRemoveValueAtIndex(new_list, where);
	    }
	    else {
		list_count++;
	    }
	    CFArrayInsertValueAtIndex(new_list, 0, service);
	    /* try to remove stale entries */
	    if (list_count > SERVICE_HISTORY_MAX) {
		int i;
		int remove_count = list_count - SERVICE_HISTORY_MAX;

		SCLog(S_NetworkIdentification_debug,
		      LOG_NOTICE, CFSTR("Attempting to remove %d services"),
		      remove_count);
		for (i = list_count - 1; i >= 0 && remove_count > 0; i--) {
		    CFDictionaryRef	dict;
	    
		    dict = CFArrayGetValueAtIndex(new_list, i);
		    if (myCFDictionaryArrayGetValue(active_services,
						    kService, dict, NULL)
			!= NULL) {
			/* skip anything that's currently active */
			SCLog(S_NetworkIdentification_debug,
			      LOG_NOTICE, CFSTR("Skipping Service %@"),
			      dict);
		    }
		    else {
			SCLog(S_NetworkIdentification_debug, LOG_NOTICE,
			      CFSTR("Removing Service %@"), dict);
			CFArrayRemoveValueAtIndex(new_list, i);
			remove_count--;
		    }
		}
	    }
	    list = (CFArrayRef)new_list;
	    
	}
    }

    new_dict = CFDictionaryCreateMutableCopy(NULL, 0, sig_dict);
    if (list != NULL) {
	CFDictionarySetValue(new_dict, kServices, list);
	CFRelease(list);
    }
    CFDictionarySetValue(new_dict, kTimestamp, now);

 done:
    CFRelease(now);
    return (new_dict);
}

#define ARBITRARILY_LARGE_NUMBER	(1024 * 1024)
static CFStringRef
get_best_serviceID(CFArrayRef serviceID_list, CFArrayRef order)
{
    int			best_rank;
    CFStringRef		best_serviceID;
    int			count;
    int			i;
    CFRange		range;

    count = CFArrayGetCount(serviceID_list);
    if (count == 1 || order == NULL) {
	return (CFArrayGetValueAtIndex(serviceID_list, 0));
    }
    best_serviceID = NULL;
    best_rank = ARBITRARILY_LARGE_NUMBER;
    range = CFRangeMake(0, CFArrayGetCount(order));
    for (i = 0; i < count; i++) {
	CFStringRef	serviceID = CFArrayGetValueAtIndex(serviceID_list, i);
	int		this_rank;

	this_rank = CFArrayGetFirstIndexOfValue(order, range, serviceID);
	if (this_rank == kCFNotFound) {
	    this_rank = ARBITRARILY_LARGE_NUMBER;
	}
	if (best_serviceID == NULL || this_rank < best_rank) {
	    best_serviceID = serviceID;
	    best_rank = this_rank;
	}
    }
    return (best_serviceID);
}

static CFArrayRef
copy_service_order(SCDynamicStoreRef session, CFStringRef ipv4_key)
{
    CFArrayRef	 		order = NULL;
    CFDictionaryRef 		ipv4_dict = NULL;

    if (session == NULL) {
	return (NULL);
    }
    ipv4_dict = SCDynamicStoreCopyValue(session, ipv4_key);
    if (isA_CFDictionary(ipv4_dict) != NULL) {
	order = CFDictionaryGetValue(ipv4_dict, kSCPropNetServiceOrder);
	order = isA_CFArray(order);
	if (order) {
	    CFRetain(order);
	}
    }
    if (ipv4_dict != NULL) {
	CFRelease(ipv4_dict);
    }
    return (order);
}

typedef struct service_order_with_range {
    CFArrayRef	service_order;
    CFRange	range;
} service_order_with_range_t;

static void
add_netID_and_serviceID(service_order_with_range_t * order, int count,
			CFMutableArrayRef netID_list, CFStringRef netID,
			CFMutableArrayRef serviceID_list, CFStringRef serviceID)
	 
{
    int		i;
    int		serviceID_index;

    if (count == 0 || order->service_order == NULL) {
	goto add_to_end;
    }
    serviceID_index = CFArrayGetFirstIndexOfValue(order->service_order,
						  order->range,
						  serviceID);
    if (serviceID_index == kCFNotFound) {
	goto add_to_end;
    }
    for (i = 0; i < count; i++) {
	CFStringRef	scan = CFArrayGetValueAtIndex(serviceID_list, i);
	int		scan_index;
	
	scan_index = CFArrayGetFirstIndexOfValue(order->service_order,
						 order->range,
						 scan);
	if (scan_index == kCFNotFound
	    || serviceID_index < scan_index) {
	    /* found our insertion point */
	    CFArrayInsertValueAtIndex(netID_list, i, netID);
	    CFArrayInsertValueAtIndex(serviceID_list, i, serviceID);
	    return;
	}
    }

 add_to_end:
    CFArrayAppendValue(netID_list, netID);
    CFArrayAppendValue(serviceID_list, serviceID);
    return;
}

static Boolean
ServiceWatcherPublishActiveIdentifiers(ServiceWatcherRef watcher)
{
    Boolean	updated = FALSE;

    if (watcher->active_signatures == NULL) {
	CFDictionaryRef	dict;

	dict = SCDynamicStoreCopyValue(watcher->store,
				       kSCNetworkIdentificationStoreKey);
	if (dict != NULL) {
	    updated = TRUE;
	    SCLog(S_NetworkIdentification_debug,
		  LOG_NOTICE, CFSTR("Removing %@"),
		  kSCNetworkIdentificationStoreKey);
	    SCDynamicStoreRemoveValue(watcher->store,
				      kSCNetworkIdentificationStoreKey);
	    CFRelease(dict);
	}
    }
    else {
	int			count;
	CFDictionaryRef		dict;
	int			i;
	CFMutableArrayRef	id_list;
	CFStringRef		keys[3];
	int			keys_count;
	service_order_with_range_t order;
	CFStringRef		primary_ipv4_id = NULL;
	CFMutableArrayRef	serviceID_list;
	CFDictionaryRef		store_dict;
	CFTypeRef		values[3];

	order.service_order = copy_service_order(watcher->store,
						 watcher->setup_ipv4_key);
	if (order.service_order != NULL) {
	    order.range = CFRangeMake(0, CFArrayGetCount(order.service_order));
	}
	id_list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	serviceID_list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	count = CFArrayGetCount(watcher->active_signatures);
	for (i = 0; i < count; i++) {
	    CFStringRef			this_id;
	    CFStringRef			serviceID;
	    CFArrayRef			this_list;

	    dict = CFArrayGetValueAtIndex(watcher->active_signatures, i);
	    this_id = CFDictionaryGetValue(dict, kIdentifier);
	    this_list = CFDictionaryGetValue(dict, kServiceIdentifiers);
	    if (primary_ipv4_id == NULL && watcher->primary_ipv4 != NULL) {
		CFRange			range;

		range = CFRangeMake(0, CFArrayGetCount(this_list));
		if (CFArrayContainsValue(this_list, range, 
					 watcher->primary_ipv4)) {
		    primary_ipv4_id = this_id;
		}
	    }
	    serviceID = get_best_serviceID(this_list, order.service_order);
	    add_netID_and_serviceID(&order, i, id_list, this_id,
				    serviceID_list, serviceID);
	}
	keys[0] = kStoreKeyActiveIdentifiers;
	values[0] = id_list;
	keys[1] = kStoreKeyServiceIdentifiers;
	values[1] = serviceID_list;
	if (primary_ipv4_id != NULL) {
	    keys_count = 3;
	    keys[2] = kStoreKeyPrimaryIPv4Identifier;
	    values[2] = primary_ipv4_id;
	}
	else {
	    keys_count = 2;
	}
	dict = CFDictionaryCreate(NULL, (const void * *)keys,
				  (const void * *)values, keys_count,
				  &kCFTypeDictionaryKeyCallBacks,
				  &kCFTypeDictionaryValueCallBacks);
	store_dict
	    = SCDynamicStoreCopyValue(watcher->store,
				      kSCNetworkIdentificationStoreKey);
	if (isA_CFDictionary(store_dict) == NULL
	    || CFEqual(store_dict, dict) == FALSE) {
	    updated = TRUE;
	    SCDynamicStoreSetValue(watcher->store,
				   kSCNetworkIdentificationStoreKey, dict);
	    SCLog(S_NetworkIdentification_debug,
		  LOG_NOTICE, CFSTR("Setting %@ = %@"),
		  kSCNetworkIdentificationStoreKey,
		  dict);
	}
	else {
	    SCLog(S_NetworkIdentification_debug,
		  LOG_NOTICE, CFSTR("Not setting %@"),
		  kSCNetworkIdentificationStoreKey);
	}
	CFRelease(dict);
	CFRelease(id_list);
	CFRelease(serviceID_list);
	if (order.service_order != NULL) {
	    CFRelease(order.service_order);
	}
	if (store_dict != NULL) {
	    CFRelease(store_dict);
	}
    }
    return (updated);
}

static CFDictionaryRef
signature_dict_create(CFStringRef this_sig, CFDictionaryRef service)
{
    CFDictionaryRef		dict;
    const void *		keys[4];
    const void *		values[4];

    keys[0] = kSignature;
    values[0] = this_sig;

    keys[1] = kServices;
    values[1] = CFArrayCreate(NULL, (const void * *)&service, 1,
			      &kCFTypeArrayCallBacks);
    keys[2] = kIdentifier;
    values[2] = this_sig;

    keys[3] = kTimestamp;
    values[3] = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());

    dict = CFDictionaryCreate(NULL, keys, values,
			      sizeof(keys) / sizeof(keys[0]),
			      &kCFTypeDictionaryKeyCallBacks,
			      &kCFTypeDictionaryValueCallBacks);
    CFRelease(values[1]);
    CFRelease(values[3]);
    return (dict);
}

static void
ServiceWatcherRemoveStaleSignatures(ServiceWatcherRef watcher)
{
    int			active_count = 0;
    int			count;
    int			i;
    int			remove_count;

    count = CFArrayGetCount(watcher->signatures);
    if (watcher->active_signatures != NULL) {
	active_count = CFArrayGetCount(watcher->active_signatures);
    }
    if ((count - active_count) <= SIGNATURE_HISTORY_MAX) {
	return;
    }
    remove_count = count - active_count - SIGNATURE_HISTORY_MAX;
    for (i = count - 1; i >= 0 && remove_count > 0; i--) {
	CFDictionaryRef		sig_dict;
	CFStringRef		sig_str;
	
	sig_dict = CFArrayGetValueAtIndex(watcher->signatures, i);
	sig_str = CFDictionaryGetValue(sig_dict, kSignature);
	
	if (myCFDictionaryArrayGetValue(watcher->active_signatures, 
					kSignature, sig_str, NULL)
	    != NULL) {
	    /* skip anything that's currently active */
	    SCLog(S_NetworkIdentification_debug,
		  LOG_NOTICE, CFSTR("Skipping %@"), sig_dict);
	}
	else {
	    SCLog(S_NetworkIdentification_debug,
		  LOG_NOTICE, CFSTR("ServiceWatcher: Removing %@"),
		  sig_dict);
	    CFArrayRemoveValueAtIndex(watcher->signatures, i);
	    remove_count--;
	}
    }
    return;
}

static void
ServiceWatcherSaveSignatures(ServiceWatcherRef watcher)
{
    SCPreferencesRef	prefs;

    prefs = SCPreferencesCreate(NULL, CFSTR("ServiceWatcher"),
				kSCNetworkIdentificationPrefsKey);
    if (prefs == NULL) {
	SCLog(TRUE, LOG_NOTICE, CFSTR("ServiceWatcherSaveSignatures: Create failed %s"),
	      SCErrorString(SCError()));
	return;
    }
    ServiceWatcherRemoveStaleSignatures(watcher);
    if (SCPreferencesSetValue(prefs, kSignatures, watcher->signatures)
	== FALSE) {
	SCLog(TRUE, LOG_NOTICE, CFSTR("ServiceWatcherSaveSignatures: Set failed %s"),
	      SCErrorString(SCError()));
    }
    else if (SCPreferencesCommitChanges(prefs) == FALSE) {
	// An EROFS error is expected during installation.  All other
	// errors should be reported.
	if (SCError() != EROFS) {
	    SCLog(TRUE, LOG_NOTICE, CFSTR("ServiceWatcherSaveSignatures: Commit failed %s"),
		  SCErrorString(SCError()));
	}
    }
    CFRelease(prefs);
    return;

}

static void
ServiceWatcherLoadSignatures(ServiceWatcherRef watcher)
{
    int			count;
    int			i;
    SCPreferencesRef	prefs;
    CFArrayRef		signatures;

    watcher->signatures 
	= CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    prefs = SCPreferencesCreate(NULL, CFSTR("ServiceWatcher"),
				kSCNetworkIdentificationPrefsKey);
    if (prefs == NULL) {
	SCLog(TRUE, LOG_NOTICE, CFSTR("ServiceWatcherLoadSignatures: Create failed %s"),
	      SCErrorString(SCError()));
	return;
    }
    signatures = SCPreferencesGetValue(prefs, kSignatures);
    if (signatures == NULL) {
	goto done;
    }
    if (isA_CFArray(signatures) == NULL) {
	SCLog(TRUE, LOG_NOTICE,
	      CFSTR("ServiceWatcherLoadSignatures: Signatures is not an array"));
	goto done;
    }
    count = CFArrayGetCount(signatures);
    for (i = 0; i < count; i++) {
	CFDictionaryRef		dict;
	CFArrayRef		services;
	CFStringRef		sig_id;
	CFStringRef		sig_str;
	CFDateRef		timestamp;

	dict = CFArrayGetValueAtIndex(signatures, i);
	if (isA_CFDictionary(dict) == NULL) {
	    continue;
	}
	sig_id = CFDictionaryGetValue(dict, kIdentifier);
	if (isA_CFString(sig_id) == NULL) {
	    continue;
	}
	sig_str = CFDictionaryGetValue(dict, kSignature);
	if (isA_CFString(sig_str) == NULL) {
	    continue;
	}
	timestamp = CFDictionaryGetValue(dict, kTimestamp);
	if (isA_CFDate(timestamp) == NULL) {
	    continue;
	}
	services = CFDictionaryGetValue(dict, kServices);
	if (isA_CFArray(services) == NULL) {
	    continue;
	}
	CFArrayAppendValue(watcher->signatures, dict);
    }

 done:
    CFRelease(prefs);
    return;

}

static void
ServiceWatcherUpdate(ServiceWatcherRef watcher, Boolean update_signatures)
{
    CFMutableArrayRef	active_signatures = NULL;
    int			count;
    int			i;
    Boolean		save_signatures = FALSE;
    CFArrayRef		service_list;
    Boolean		update_store = FALSE;

    service_list = ServiceWatcherCopyCurrent(watcher);
    SCLog(S_NetworkIdentification_debug,
	  LOG_NOTICE, CFSTR("service_list = %@"), service_list);
    if (service_list == NULL) {
	goto done;
    }
    active_signatures = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    count = CFArrayGetCount(service_list);
    for (i = 0; i < count; i++) {
	CFDictionaryRef		dict;
	CFDictionaryRef		active_dict;
	CFArrayRef		id_list;
	CFMutableDictionaryRef	new_active_dict;
	CFDictionaryRef		new_sig_dict;
	CFStringRef		serviceID;
	CFStringRef		sig_id;
	CFDictionaryRef		service;
	CFDictionaryRef		sig_dict;
	CFStringRef		this_sig;
	int			where;

	dict = CFArrayGetValueAtIndex(service_list, i);
	service = CFDictionaryGetValue(dict, kService);
	this_sig = CFDictionaryGetValue(dict, kSignature);
	if (this_sig == NULL) {
	    /* service has no signature */
	    continue;
	}
	sig_dict =  myCFDictionaryArrayGetValue(watcher->signatures, kSignature,
						this_sig, &where);
	if (sig_dict == NULL) {
	    /* add a new signature entry */
	    sig_dict = signature_dict_create(this_sig, service);
	    CFArrayInsertValueAtIndex(watcher->signatures, 0, sig_dict);
	    CFRelease(sig_dict);
	    save_signatures = TRUE;
	    sig_id = CFDictionaryGetValue(sig_dict, kIdentifier);
	    active_dict = NULL;
	}
	else {
	    /* update an existing signature entry */
	    
	    sig_id = CFDictionaryGetValue(sig_dict, kIdentifier);
	    new_sig_dict = signature_add_service(sig_dict, service,
						 service_list);
	    if (new_sig_dict != NULL) {
		CFArrayRemoveValueAtIndex(watcher->signatures, where);
		CFArrayInsertValueAtIndex(watcher->signatures, 0,
					  new_sig_dict);
		CFRelease(new_sig_dict);
		save_signatures = TRUE;
	    }
	    active_dict
		= myCFDictionaryArrayGetValue(active_signatures, 
					      kSignature, this_sig,
					      &where);
	}
	if (active_dict == NULL) {
	    /* signature now active, this is the first/only service */
	    new_active_dict 
		= CFDictionaryCreateMutable(NULL, 0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);
	    CFDictionarySetValue(new_active_dict, kSignature, this_sig);
	    CFDictionarySetValue(new_active_dict, kIdentifier, sig_id);
	    serviceID = CFDictionaryGetValue(service, kServiceID);
	    id_list = CFArrayCreate(NULL, (const void * *)&serviceID, 1,
				    &kCFTypeArrayCallBacks);
	    CFDictionarySetValue(new_active_dict, kServiceIdentifiers,
				 id_list);
	    CFArrayAppendValue(active_signatures, new_active_dict);
	    CFRelease(new_active_dict);
	    CFRelease(id_list);
	}
	else {
	    /* signature already active, add this serviceID */
	    CFRange			range;
	    
	    id_list = CFDictionaryGetValue(active_dict,
					   kServiceIdentifiers);
	    range = CFRangeMake(0, CFArrayGetCount(id_list));
	    serviceID = CFDictionaryGetValue(service, kServiceID);
	    if (CFArrayContainsValue(id_list, range, serviceID) == FALSE) {
		CFMutableDictionaryRef	new_active_dict;
		CFMutableArrayRef	new_id_list;
		
		new_id_list = CFArrayCreateMutableCopy(NULL, 0, id_list);
		CFArrayAppendValue(new_id_list, serviceID);
		new_active_dict 
		    = CFDictionaryCreateMutableCopy(NULL, 0, active_dict);
		CFDictionarySetValue(new_active_dict, kServiceIdentifiers,
				     new_id_list);
		CFArraySetValueAtIndex(active_signatures, where,
				       new_active_dict);
		CFRelease(new_active_dict);
		CFRelease(new_id_list);
	    }
	}
    }
 done:
    if (active_signatures == NULL
	|| CFArrayGetCount(active_signatures) == 0) {
	update_store
	    = ServiceWatcherSetActiveSignatures(watcher, NULL);
    }
    else {
	update_store
	    = ServiceWatcherSetActiveSignatures(watcher, active_signatures);
    }
    if (save_signatures) {
	/* write out the file */
	ServiceWatcherSaveSignatures(watcher);
    }

    if (service_list != NULL) {
	CFRelease(service_list);
    }
    if (active_signatures != NULL) {
	CFRelease(active_signatures);
    }
    if (update_signatures || update_store) {
	if (ServiceWatcherPublishActiveIdentifiers(watcher)) {
	    notify_post(kSCNetworkSignatureActiveChangedNotifyName);
	}
    }
    return;
}

static Boolean
update_primary_ipv4(ServiceWatcherRef watcher)
{
    Boolean		changed = FALSE;
    CFDictionaryRef	global_ipv4;
    
    global_ipv4 = SCDynamicStoreCopyValue(watcher->store,
					  watcher->state_ipv4_key);
    if (isA_CFDictionary(global_ipv4) != NULL) {
	CFStringRef		primary_ipv4;

	primary_ipv4 
	    = CFDictionaryGetValue(global_ipv4,
				   kSCDynamicStorePropNetPrimaryService);
	changed = ServiceWatcherSetPrimaryIPv4(watcher,
					       isA_CFString(primary_ipv4));
    }
    if (global_ipv4 != NULL) {
	CFRelease(global_ipv4);
    }
    return (changed);
}

static void
ServiceWatcherNotifier(SCDynamicStoreRef not_used, CFArrayRef changes,
		       void * info)
{
    int			count;
    int			i;
    Boolean		order_changed = FALSE;
    Boolean		global_ipv4_changed = FALSE;
    Boolean		primary_ipv4_changed = FALSE;
    ServiceWatcherRef	watcher = (ServiceWatcherRef)info;

    count = CFArrayGetCount(changes);
    if (count == 0) {
	return;
    }
    for (i = 0; i < count; i++) {
	CFStringRef	key = CFArrayGetValueAtIndex(changes, i);

	if (CFStringHasPrefix(key, kSCDynamicStoreDomainSetup)) {
	    order_changed = TRUE;
	}
	else if (CFEqual(key, watcher->state_ipv4_key)) {
	    global_ipv4_changed = TRUE;
	}
    }
    if (global_ipv4_changed) {
	primary_ipv4_changed = update_primary_ipv4(watcher);
    }
    if (count == 1
	&& (order_changed || primary_ipv4_changed)) {
	/* just the service order or the primary service changed */
	if (ServiceWatcherPublishActiveIdentifiers(watcher)) {
	    notify_post(kSCNetworkSignatureActiveChangedNotifyName);
	}
    }
    else {
	ServiceWatcherUpdate(watcher, order_changed || primary_ipv4_changed);
    }
    return;
}

static ServiceWatcherRef
ServiceWatcherCreate()
{
    SCDynamicStoreContext	context = { 0, 0, 0, 0, 0};
    CFArrayRef			patterns;
    CFStringRef			keys[2];
    CFArrayRef			key_list;
    ServiceWatcherRef		watcher;

    watcher = malloc(sizeof(*watcher));
    bzero(watcher, sizeof(*watcher));
    context.info = watcher;
    watcher->store = SCDynamicStoreCreate(NULL, CFSTR("Service Watcher"),
					  ServiceWatcherNotifier, &context);
    if (watcher->store == NULL) {
	SCLog(TRUE, LOG_NOTICE, CFSTR("SCDynamicStoreCreate failed: %s"),
	      SCErrorString(SCError()));
	goto failed;
    }
    watcher->setup_ipv4_key 
	= SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, 
						     kSCDynamicStoreDomainSetup,
						     kSCEntNetIPv4);
    watcher->state_ipv4_key 
	= SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, 
						     kSCDynamicStoreDomainState,
						     kSCEntNetIPv4);
    keys[0] = watcher->setup_ipv4_key;
    keys[1] = watcher->state_ipv4_key;
    key_list = CFArrayCreate(NULL, (const void * *)keys, sizeof(keys) / sizeof(keys[0]),
			     &kCFTypeArrayCallBacks);
    patterns = ServiceWatcherNotificationPatterns();
    (void)SCDynamicStoreSetNotificationKeys(watcher->store, key_list, patterns);
    CFRelease(patterns);
    CFRelease(key_list);
    watcher->rls = SCDynamicStoreCreateRunLoopSource(NULL, watcher->store, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), watcher->rls, 
		       kCFRunLoopDefaultMode);
    ServiceWatcherLoadSignatures(watcher);
    update_primary_ipv4(watcher);
    return (watcher);
 failed:
    ServiceWatcherFree(&watcher);
    return (NULL);
}

void
ServiceWatcherFree(ServiceWatcherRef * watcher_p)
{
    ServiceWatcherRef	watcher;

    if (watcher_p == NULL) {
	return;
    }
    watcher = *watcher_p;
    if (watcher == NULL) {
	return;
    }
    *watcher_p = NULL;
    if (watcher->store != NULL) {
	CFRelease(watcher->store);
	watcher->store = NULL;
    }
    if (watcher->rls != NULL) {
	CFRunLoopSourceInvalidate(watcher->rls);
	CFRelease(watcher->rls);
	watcher->rls = NULL;
    }
    if (watcher->signatures != NULL) {
	CFRelease(watcher->signatures);
	watcher->signatures = NULL;
    }
    if (watcher->state_ipv4_key != NULL) {
	CFRelease(watcher->state_ipv4_key);
	watcher->state_ipv4_key = NULL;
    }
    if (watcher->setup_ipv4_key != NULL) {
	CFRelease(watcher->setup_ipv4_key);
	watcher->setup_ipv4_key = NULL;
    }
    free(watcher);
    return;
}

/* global service watcher instance */
static ServiceWatcherRef	S_watcher;

__private_extern__
void
prime_NetworkIdentification()
{
    if (S_NetworkIdentification_disabled) {
	return;
    }
    S_watcher = ServiceWatcherCreate();
    ServiceWatcherUpdate(S_watcher, TRUE);
}

__private_extern__
void
load_NetworkIdentification(CFBundleRef bundle, Boolean bundleVerbose)
{
    if (bundleVerbose) {
	S_NetworkIdentification_debug = 1;
    }
    return;
}

#ifdef  TEST_NETWORKIDENTIFICATION
#undef  TEST_NETWORKIDENTIFICATION

int
main(int argc, char **argv)
{
    _sc_log     = FALSE;
    _sc_verbose = (argc > 1) ? TRUE : FALSE;

    load_NetworkIdentification(CFBundleGetMainBundle(), (argc > 1) ? TRUE : FALSE);
    prime_NetworkIdentification();
    CFRunLoopRun();
    /* not reached */
    exit(0);
    return 0;
}
#endif

