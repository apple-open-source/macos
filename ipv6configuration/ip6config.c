/*
 * Copyright (c) 1999-2008 Apple Inc. All rights reserved.
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
 * ip6config.c
 * - plugin that configures IPv6 on the various interfaces.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/time.h>
#include <netinet/in.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>

#include "configthreads_common.h"
#include "globals.h"
#include "config_method.h"
#include "ip6config_utils.h"


/* Local Variables */
static interface_list_t * S_interfaces = NULL;
static CFStringRef	S_state_interface_prefix = NULL;
static CFStringRef	S_setup_service_prefix = NULL;

/* Global Variables */
int			G_verbose = FALSE;
CFBundleRef		G_bundle = NULL;
SCDynamicStoreRef	G_scd_session = NULL;
int			G_debug = TRUE;
const struct in6_addr	G_ip6_zeroes = IN6ADDR_ANY_INIT;
IFStateList_t		G_ifstate_list;

static boolean_t
update_interface_list()
{
    interface_list_t *	new_interfaces = NULL;

    new_interfaces = ifl_init();

    if (new_interfaces == NULL) {
	my_log(LOG_ERR, "ip6config: ifl_init failed");
	return (FALSE);
    }

    if (S_interfaces) {
	ifl_free(&S_interfaces);
    }

    S_interfaces = new_interfaces;

    return (TRUE);
}

/*
 * Function: check_for_detached_interfaces
 * Purpose:
 *   Remove interface state for any interface that has been removed.
 *   Create a temporary list to store the name of each interface that
 *   has been removed.  Iterate through that list to remove individual
 *   interface state records.  This is done to avoid problems with
 *   iterating over a list while it is modified.
 */
static void
check_for_detached_interfaces()
{
    int		count = dynarray_count(&G_ifstate_list);
    char * *	names = NULL;
    int		names_count = 0;
    int 	i;

    if (count == 0) {
	return;
    }

    /* allocate worst case scenario in which each ifstate needs to be removed */
    names = (char * *)malloc(sizeof(char *) * count);
    if (names == NULL) {
	return;
    }

    for (i = 0; i < count; i++) {
	IFState_t *	ifstate = dynarray_element(&G_ifstate_list, i);

	if (ifl_find_name(S_interfaces, if_name(ifstate->if_p)) == NULL) {
	    names[names_count++] = if_name(ifstate->if_p);
	}
    }

    for (i = 0; i < names_count; i++) {
	IFStateList_ifstate_free(&G_ifstate_list, names[i]);
    }

    free(names);
    return;
}

/*
 * Function: parse_component
 * Purpose:
 *   Given a string 'key' and a string prefix 'prefix',
 *   return the next component in the slash '/' separated
 *   key.  If no slash follows the prefix, return NULL.
 *
 * Examples:
 * 1. key = "a/b/c" prefix = "a/"
 *    returns "b"
 * 2. key = "a/b/c" prefix = "a/b/"
 *    returns NULL
 */
static CFStringRef
parse_component(CFStringRef key, CFStringRef prefix)
{
    CFMutableStringRef	comp;
    CFRange		range;

    if (CFStringHasPrefix(key, prefix) == FALSE) {
	return (NULL);
    }

    comp = CFStringCreateMutableCopy(NULL, 0, key);
    if (comp == NULL) {
	return (NULL);
    }

    CFStringDelete(comp, CFRangeMake(0, CFStringGetLength(prefix)));

    range = CFStringFind(comp, CFSTR("/"), 0);
    if (range.location == kCFNotFound) {
	CFRelease(comp);
	return (NULL);
    }

    range.length = CFStringGetLength(comp) - range.location;
    CFStringDelete(comp, range);

    return (comp);
}

static void
handle_configuration_changed(SCDynamicStoreRef session, CFArrayRef all_ipv6)
{
    int i, n;

    n = ifl_count(S_interfaces);
    for (i = 0; i < n; i++) {
	ServiceConfig_t *	config;
	int			count = 0;
	IFState_t *		ifstate;
	ServiceConfig_t *	if_services = NULL;
	interface_t *		if_p = ifl_at_index(S_interfaces, i);

	if (strcmp(if_name(if_p), "lo0") == 0) {
	    my_log(LOG_DEBUG, "HANDLE_configuration_changed: skipping loopback");
	    continue;
	}

	if_services = service_config_list_init(session, all_ipv6, if_name(if_p), &count);
	if (if_services == NULL) {
	    ifstate =  IFStateList_ifstate_with_name(&G_ifstate_list,
						     if_name(if_p), NULL);
	    if (ifstate != NULL) {
		IFState_services_free(ifstate);
	    }
	    continue;
	}

	/* stop services that are no longer active */
	service_free_inactive_services(if_name(if_p), if_services, count);

	ifstate = IFStateList_ifstate_create(&G_ifstate_list, if_p);
	if (ifstate) {
	    int k;

	    /* update each of the services that are configured */
	    for (k = 0, config = if_services; k < count; k++, config++) {
		(void)service_set_service(ifstate, config);
	    }
	}
	service_config_list_free(&if_services, count);
    }
    return;
}

static CFArrayRef
entity_all(SCDynamicStoreRef session)
{
    CFMutableArrayRef	all_services = NULL;
    int			count;
    int			id_count;
    CFMutableArrayRef	get_patterns = NULL;
    int			i;
    CFStringRef		key = NULL;
    void * *	 	keys = NULL;
    CFMutableArrayRef	service_IDs = NULL;
    CFDictionaryRef	values = NULL;

    get_patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    service_IDs = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    all_services = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    if (get_patterns == NULL || service_IDs == NULL || all_services == NULL) {
	goto done;
    }

    /* populate patterns array for Setup:/Network/Service/any/{IPv6,Interface, 6to4} */
    key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						      kSCDynamicStoreDomainSetup,
						      kSCCompAnyRegex,
						      kSCEntNetIPv6);
    if (key == NULL) {
	goto done;
    }
    CFArrayAppendValue(get_patterns, key);
    my_CFRelease(&key);

    key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						      kSCDynamicStoreDomainSetup,
						      kSCCompAnyRegex,
						      kSCEntNetInterface);
    if (key == NULL) {
	goto done;
    }
    CFArrayAppendValue(get_patterns, key);
    my_CFRelease(&key);

    key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
						      kSCDynamicStoreDomainSetup,
						      kSCCompAnyRegex,
						      kSCEntNet6to4);
    if (key == NULL) {
	goto done;
    }
    CFArrayAppendValue(get_patterns, key);
    my_CFRelease(&key);

    /* get keys and values atomically */
    values = SCDynamicStoreCopyMultiple(session, NULL, get_patterns);
    if (values == NULL) {
	goto done;
    }

    /* if there are no values, we're done */
    count = CFDictionaryGetCount(values);
    if (count == 0) {
	goto done;
    }

    /* build a list of configured service ID's */
    keys = (void * *)malloc(sizeof(void *) * count);
    if (keys == NULL) {
	goto done;
    }
    CFDictionaryGetKeysAndValues(values, (const void * *)keys, NULL);

    for (i = 0; i < count; i++) {
	CFStringRef	serviceID;

	serviceID = parse_component(keys[i], S_setup_service_prefix);
	if (serviceID == NULL) {
	    continue;
	}
	my_CFArrayAppendUniqueValue(service_IDs, serviceID);
	my_CFRelease(&serviceID);
    }
    free(keys);
    keys = NULL;

    /* populate all_services array with annotated IPv6 dict's */
    id_count = CFArrayGetCount(service_IDs);
    for (i = 0; i < id_count; i++) {
	CFStringRef 		key = NULL;
	CFDictionaryRef		if_dict;
	CFStringRef 		ifn_cf;
	CFDictionaryRef		ipv6_dict;
	CFMutableDictionaryRef	service_dict = NULL;
	CFStringRef		serviceID;
	CFStringRef		type = NULL;
	CFDictionaryRef         stf_dict;
	CFStringRef             relay = NULL;

	serviceID = CFArrayGetValueAtIndex(service_IDs, i);
	key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainSetup,
							  serviceID,
							  kSCEntNetInterface);
	if (key == NULL) {
	    goto loop_done;
	}

	if_dict = CFDictionaryGetValue(values, key);
	my_CFRelease(&key);

	if_dict = isA_CFDictionary(if_dict);
	if (if_dict == NULL) {
	    goto loop_done;
	}

	type = CFDictionaryGetValue(if_dict, kSCPropNetInterfaceType);

	if (type == NULL) {
	    goto loop_done;
	}

	if (CFEqual(type, kSCValNetInterfaceTypeEthernet) == FALSE
	    && CFEqual(type, kSCValNetInterfaceTypeFireWire) == FALSE
	    && CFEqual(type, kSCValNetInterfaceType6to4) == FALSE) {
	    /* we only configure ethernet, firewire and 6to4 currently */
	    goto loop_done;
	}

	ifn_cf = CFDictionaryGetValue(if_dict, kSCPropNetInterfaceDeviceName);
	if (ifn_cf == NULL) {
	    goto loop_done;
	}

	key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainSetup,
							  serviceID,
							  kSCEntNetIPv6);
	if (key == NULL) {
	    goto loop_done;
	}

	ipv6_dict = CFDictionaryGetValue(values, key);
	my_CFRelease(&key);

	ipv6_dict = isA_CFDictionary(ipv6_dict);
	if (ipv6_dict == NULL) {
	    goto loop_done;
	}

	service_dict = CFDictionaryCreateMutableCopy(NULL, 0, ipv6_dict);
	if (service_dict == NULL) {
	    goto loop_done;
	}

	key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainSetup,
							  serviceID,
							  kSCEntNet6to4);
	if (key != NULL) {
	    stf_dict = CFDictionaryGetValue(values, key);
	    my_CFRelease(&key);

	    stf_dict = isA_CFDictionary(stf_dict);
	    if (stf_dict != NULL) {
		relay = CFDictionaryGetValue(stf_dict, kSCPropNet6to4Relay);
		if (relay == NULL) {
		    my_log(LOG_DEBUG, "entity_all: error getting 6to4 relay");
		}
		else {
		    CFDictionarySetValue(service_dict, kSCPropNet6to4Relay, relay);
		}
	    }
	}


	/* annotate with serviceID and interface name */
	CFDictionarySetValue(service_dict, kSCPropNetInterfaceDeviceName, ifn_cf);
	CFDictionarySetValue(service_dict, PROP_SERVICEID, serviceID);
	CFArrayAppendValue(all_services, service_dict);

     loop_done:
	my_CFRelease(&service_dict);
    }

    SCLog(G_verbose, LOG_INFO, CFSTR("ALL_SERVICES: %@ (%d)"), all_services,
	  CFArrayGetCount(all_services));

 done:
    my_CFRelease(&values);
    my_CFRelease(&get_patterns);
    my_CFRelease(&service_IDs);
    if (all_services == NULL || CFArrayGetCount(all_services) == 0) {
	my_CFRelease(&all_services);
    }
    return (all_services);
}

static void
configure_from_cache(SCDynamicStoreRef session)
{
    CFArrayRef	all_ipv6 = NULL;

    all_ipv6 = entity_all(session);
    handle_configuration_changed(session, all_ipv6);
    my_CFRelease(&all_ipv6);

    return;
}

static void
link_key_changed(SCDynamicStoreRef session, CFStringRef cache_key)
{
    CFDictionaryRef	dict = NULL;
    interface_t *	if_p = NULL;
    CFStringRef		ifn_cf = NULL;
    char		ifn[IFNAMSIZ + 1];
    IFState_t *   	ifstate;
    int			j;
    int			service_count;
    link_status_t	link;
    CFBooleanRef	link_val = NULL;

    ifn_cf = parse_component(cache_key, S_state_interface_prefix);
    if (ifn_cf == NULL) {
	return;
    }
    cfstring_to_cstring(ifn_cf, ifn, sizeof(ifn));
    dict = my_SCDynamicStoreCopyValue(session, cache_key);
    if (dict != NULL) {
	if (CFDictionaryContainsKey(dict, kSCPropNetLinkDetaching)) {
	    ifstate = IFStateList_ifstate_with_name(&G_ifstate_list,
						    ifn, NULL);
	    if (ifstate != NULL) {
		IFState_services_free(ifstate);
	    }
	    goto done;
	}
	link_val = CFDictionaryGetValue(dict, kSCPropNetLinkActive);
	link_val = isA_CFBoolean(link_val);
    }
    if (link_val == NULL) {
	link.valid = link.active = FALSE;
    }
    else {
	link.valid = TRUE;
	link.active = CFEqual(link_val, kCFBooleanTrue);
    }
    if (link.valid) {
	if_p = ifl_find_name(S_interfaces, ifn);
	if (if_p != NULL) {
	    /* make sure address information is up to date */
	    if_link_update(if_p);
	}
	else {
	    if_p = NULL;
	}
    }
    ifstate = IFStateList_ifstate_with_name(&G_ifstate_list, ifn, NULL);
    if (ifstate == NULL) {
	goto done;
    }
    ifstate->link = link;
    if (if_p != NULL) {
	/* update address information in ifstate too */
	if_link_copy(ifstate->if_p, if_p);
    }
    if (link.valid == FALSE) {
	my_log(LOG_DEBUG, "link_key_changed: %s link is unknown", ifn);
    }
    else {
	my_log(LOG_DEBUG, "%s link is %s",
	       ifn, link.active ? "up" : "down");
    }
    if (link.active == TRUE) {
	/* do linklocal first */
	if (ifstate->llocal_service != NULL) {
	    config_method_media(ifstate->llocal_service);
	}
    }
    service_count = dynarray_count(&ifstate->services);
    for (j = 0; j < service_count; j++) {
	Service_t * service_p = dynarray_element(&ifstate->services, j);

	config_method_media(service_p);
    }
    if (link.active == FALSE) {
	/* do linklocal after all others are taken care of */
	if (ifstate->llocal_service != NULL) {
	    config_method_media(ifstate->llocal_service);
	}
    }

 done:
    my_CFRelease(&dict);
    my_CFRelease(&ifn_cf);
    return;
}

static void
interface_ipv6_changed(SCDynamicStoreRef session, CFStringRef cache_key)
{
    CFDictionaryRef	dict = NULL;
    CFStringRef		ifn_cf = NULL;
    char		ifn[IFNAMSIZ + 1];
    IFState_t *   	ifstate;
    ip6_addrinfo_list_t	ip6_addrs;
    int			j, service_count;

    ifn_cf = parse_component(cache_key, S_state_interface_prefix);
    if (ifn_cf == NULL) {
	return;
    }
    ip6_addrs.addr_list = NULL;
    cfstring_to_cstring(ifn_cf, ifn, sizeof(ifn));
    ifstate = IFStateList_ifstate_with_name(&G_ifstate_list, ifn, NULL);
    if (ifstate == NULL) {
	goto done;
    }
    dict = my_SCDynamicStoreCopyValue(session, cache_key);
    if (dict == NULL) {
	goto done;
    }
    if (ip6config_address_data_from_state(dict, &ip6_addrs) != 0) {
	goto done;
    }
    service_count = dynarray_count(&ifstate->services);
    for (j = 0; j < service_count; j++) {
	Service_t * service_p = dynarray_element(&ifstate->services, j);
	(void)config_method_state_change(service_p, &ip6_addrs);
    }

    /* do linklocal after all others are taken care of */
    if (ifstate->llocal_service != NULL) {
	(void)config_method_state_change(ifstate->llocal_service, &ip6_addrs);
    }

 done:
    my_CFRelease(&dict);
    my_CFRelease(&ifn_cf);
    if (ip6_addrs.addr_list) {
	free(ip6_addrs.addr_list);
    }
    return;
}

static void
ipv4_primary_service_changed(SCDynamicStoreRef session, CFStringRef cache_key)
{
    IFState_t *   		ifstate;
    ip6config_method_t		method;
    ip6config_method_data_t	method_data = { 0 };
    int				i, service_count;

    ifstate = IFStateList_ifstate_with_name(&G_ifstate_list, "stf0", NULL);
    if (ifstate == NULL) {
	goto done;
    }

    method = ip6config_method_6to4_e;
    ip6config_get_6to4_address_data(session, &method_data);

    service_count = dynarray_count(&ifstate->services);
    for (i = 0; i < service_count; i++) {
	Service_t * service_p = dynarray_element(&ifstate->services, i);
	(void)config_method_ipv4_primary_change(service_p, method, &method_data);
    }

 done:
    if (method_data.stf_data.ip4_addrs_list) {
	free(method_data.stf_data.ip4_addrs_list);
    }
    return;
}

static void
handle_change(SCDynamicStoreRef session, CFArrayRef changes, void * arg)
{
    boolean_t	config_changed = FALSE;
    CFIndex	count;
    CFIndex	i;
    boolean_t	iflist_changed = FALSE;

    count = CFArrayGetCount(changes);
    if (count == 0) {
	goto done;
    }

    SCLog(G_verbose, LOG_INFO, CFSTR("Changes: %@ (%d)"), changes,
	  count);

    for (i = 0; i < count; i++) {
	CFStringRef	cache_key = CFArrayGetValueAtIndex(changes, i);

	if (CFStringHasPrefix(cache_key, kSCDynamicStoreDomainSetup)) {
	    /* IPv6 configuration changed */
	    config_changed = TRUE;
	}
	else if (CFStringHasSuffix(cache_key, kSCCompInterface)) {
	    /* list of interfaces changed */
	    iflist_changed = TRUE;
	}
    }

    /* an interface was added/removed */
    if (iflist_changed) {
	if (update_interface_list()) {
	    config_changed = TRUE;
	    check_for_detached_interfaces();
	}
    }

    /* configuration changed */
    if (config_changed) {
	configure_from_cache(session);
    }

    for (i = 0; i < count; i++) {
	CFStringRef	cache_key = CFArrayGetValueAtIndex(changes, i);

	if (CFStringHasSuffix(cache_key, kSCEntNetLink)) {
	    link_key_changed(session, cache_key);
	}
	else if (CFStringHasSuffix(cache_key, kSCEntNetIPv6) &&
		 CFStringHasPrefix(cache_key, kSCDynamicStoreDomainState)) {
	    interface_ipv6_changed(session, cache_key);
	}
	else if (CFStringHasSuffix(cache_key, kSCEntNetIPv4)) {
	    ipv4_primary_service_changed(session, cache_key);
	}
    }

 done:
    return;
}

static void
notifier_init(SCDynamicStoreRef session)
{
    CFMutableArrayRef	keys = NULL;
    CFStringRef		key;
    CFMutableArrayRef	patterns = NULL;
    CFStringRef		pattern;
    CFRunLoopSourceRef	rls;

    if (session == NULL) {
	return;
    }
    keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    /* notify when IPv6 config of any service changes */
    pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainSetup,
							  kSCCompAnyRegex,
							  kSCEntNetIPv6);
    CFArrayAppendValue(patterns, pattern);
    my_CFRelease(&pattern);

    /* notify when Interface service <-> interface binding changes */
    pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainSetup,
							  kSCCompAnyRegex,
							  kSCEntNetInterface);
    CFArrayAppendValue(patterns, pattern);
    my_CFRelease(&pattern);

    /* notify when the 6to4 relay changes */
    pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainSetup,
							  kSCCompAnyRegex,
							  kSCEntNet6to4);
    CFArrayAppendValue(patterns, pattern);
    my_CFRelease(&pattern);

    /* notify when the status of any address changes */
    pattern = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							    kSCDynamicStoreDomainState,
							    kSCCompAnyRegex,
							    kSCEntNetIPv6);
    CFArrayAppendValue(patterns, pattern);
    my_CFRelease(&pattern);

    /* notify when the link status of any interface changes */
    pattern = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							    kSCDynamicStoreDomainState,
							    kSCCompAnyRegex,
							    kSCEntNetLink);
    CFArrayAppendValue(patterns, pattern);
    my_CFRelease(&pattern);

    /* notify when list of interfaces changes */
    key = SCDynamicStoreKeyCreateNetworkInterface(NULL,
						  kSCDynamicStoreDomainState);
    CFArrayAppendValue(keys, key);
    my_CFRelease(&key);

    /* notify when the IPv4 primary service changes (for 6to4) */
    key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
						     kSCDynamicStoreDomainState,
						     kSCEntNetIPv4);
    CFArrayAppendValue(keys, key);
    my_CFRelease(&key);

    SCDynamicStoreSetNotificationKeys(session, keys, patterns);
    my_CFRelease(&keys);
    my_CFRelease(&patterns);

    rls = SCDynamicStoreCreateRunLoopSource(NULL, session, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);

    return;
}

void
load(CFBundleRef bundle, Boolean bundleVerbose)
{
    G_bundle = (CFBundleRef)CFRetain(bundle);
    G_verbose = bundleVerbose;
    {
	struct timeval	start_time;

	gettimeofday(&start_time, 0);
	srandom(start_time.tv_usec & ~start_time.tv_sec);
    }

    G_scd_session = SCDynamicStoreCreate(NULL,
					 CFSTR("IP6Configuration"),
					 handle_change, NULL);
    if (G_scd_session == NULL) {
	G_scd_session = NULL;
	my_log(LOG_ERR, "SCDynamicStoreCreate failed: %s",
	       SCErrorString(SCError()));
    }

    dynarray_init(&G_ifstate_list, IFState_free, NULL);

    return;
}

void
prime()
{
    CFPropertyListRef	value = NULL;

    if (G_scd_session == NULL) {
	return;
    }

    /* begin interface initialization */
    S_setup_service_prefix = SCDynamicStoreKeyCreate(NULL,
						     CFSTR("%@/%@/%@/"),
						     kSCDynamicStoreDomainSetup,
						     kSCCompNetwork,
						     kSCCompService);

    S_state_interface_prefix = SCDynamicStoreKeyCreate(NULL,
						       CFSTR("%@/%@/%@/"),
						       kSCDynamicStoreDomainState,
						       kSCCompNetwork,
						       kSCCompInterface);

    value = SCDynamicStoreCopyValue(G_scd_session, kSCDynamicStoreDomainSetup);
    if (value == NULL) {
	my_log(LOG_INFO, "IP6Configuration needs PreferencesMonitor to run first");
    }
    my_CFRelease(&value);

    /* install run-time notifiers */
    notifier_init(G_scd_session);

    (void)update_interface_list();

    /* cache should already be populated by prefs monitor */
    configure_from_cache(G_scd_session);

    return;
}
