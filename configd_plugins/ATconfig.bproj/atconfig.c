/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Modification History
 *
 * June 24, 2001		Allan Nathanson <ajn@apple.com>
 * - update to public SystemConfiguration.framework APIs
 *
 * July 7, 2000			Allan Nathanson <ajn@apple.com>
 * - initial revision
 */


#include <stdio.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netat/appletalk.h>
#include <netat/at_var.h>
#include <AppleTalk/at_paths.h>
#include <AppleTalk/at_proto.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>

#include "cfManager.h"

#define HOSTCONFIG	"/etc/hostconfig"


SCDynamicStoreRef	store		= NULL;
CFRunLoopSourceRef	rls		= NULL;

CFMutableDictionaryRef	oldGlobals	= NULL;
CFMutableArrayRef	oldConfigFile	= NULL;
CFMutableDictionaryRef	oldDefaults	= NULL;

Boolean			_verbose	= FALSE;


static void
updateDefaults(const void *key, const void *val, void *context)
{
	CFStringRef		ifName		= (CFStringRef)key;
	CFDictionaryRef		oldDict;
	CFDictionaryRef		newDict		= (CFDictionaryRef)val;
	CFNumberRef		defaultNode;
	CFNumberRef		defaultNetwork;
	CFStringRef		defaultZone;

	if (!CFDictionaryGetValueIfPresent(oldDefaults, ifName, (void **)&oldDict) ||
	    !CFEqual(oldDict, newDict)) {
		char		ifr_name[IFNAMSIZ];

		bzero(&ifr_name, sizeof(ifr_name));
		if (!CFStringGetCString(ifName, ifr_name, sizeof(ifr_name), kCFStringEncodingMacRoman)) {
			SCLog(TRUE, LOG_ERR, CFSTR("CFStringGetCString: could not convert interface name to C string"));
			return;
		}

		/*
		 * Set preferred Network and Node ID
		 */
		if (CFDictionaryGetValueIfPresent(newDict,
						  kSCPropNetAppleTalkNetworkID,
						  (void **)&defaultNetwork) &&
		    CFDictionaryGetValueIfPresent(newDict,
						  kSCPropNetAppleTalkNodeID,
						  (void **)&defaultNode)
		    ) {
			struct at_addr	init_address;
			int		status;

			/*
			 * set the default node and network
			 */
			CFNumberGetValue(defaultNetwork, kCFNumberShortType, &init_address.s_net);
			CFNumberGetValue(defaultNode,    kCFNumberCharType,  &init_address.s_node);
			status = at_setdefaultaddr(ifr_name, &init_address);
			if (status == -1) {
				SCLog(TRUE, LOG_ERR, CFSTR("at_setdefaultaddr() failed"));
			}
		}

		/*
		 * Set default zone
		 */
		if (CFDictionaryGetValueIfPresent(newDict,
						  kSCPropNetAppleTalkDefaultZone,
						  (void **)&defaultZone)
		    ) {
			at_nvestr_t	zone;

			/*
			 * set the "default zone" for this interface
			 */
			bzero(&zone, sizeof(zone));
			if (CFStringGetCString(defaultZone, zone.str, sizeof(zone.str), kCFStringEncodingMacRoman)) {
				int	status;

				zone.len = strlen(zone.str);
				status = at_setdefaultzone(ifr_name, &zone);
				if (status == -1) {
					SCLog(TRUE, LOG_ERR, CFSTR("at_setdefaultzone() failed"));
				}
			} else {
				SCLog(TRUE, LOG_ERR, CFSTR("CFStringGetCString: could not convert default zone to C string"));
			}
		}
	}
	return;
}


static void
addZoneToPorts(const void *key, const void *val, void *context)
{
	CFStringRef		zone		= (CFStringRef)key;
	CFArrayRef		ifArray		= (CFArrayRef)val;
	CFMutableArrayRef	zones		= (CFMutableArrayRef)context;
	CFStringRef		ifList;
	CFStringRef		configInfo;

	ifList = CFStringCreateByCombiningStrings(NULL, ifArray, CFSTR(":"));
	configInfo = CFStringCreateWithFormat(NULL, NULL, CFSTR(":%@:%@"), zone, ifList);
	CFArrayAppendValue(zones, configInfo);
	CFRelease(configInfo);
	CFRelease(ifList);
	return;
}


/*
 * Function: parse_component
 * Purpose:
 *   Given a string 'key' and a string prefix 'prefix',
 *   return the next component in the slash '/' separated
 *   key.
 *
 * Examples:
 * 1. key = "a/b/c" prefix = "a/"
 *    returns "b"
 * 2. key = "a/b/c" prefix = "a/b/"
 *    returns "c"
 */
static CFStringRef
parse_component(CFStringRef key, CFStringRef prefix)
{
	CFMutableStringRef	comp;
	CFRange			range;

	if (CFStringHasPrefix(key, prefix) == FALSE) {
		return NULL;
	}
	comp = CFStringCreateMutableCopy(NULL, 0, key);
	CFStringDelete(comp, CFRangeMake(0, CFStringGetLength(prefix)));
	range = CFStringFind(comp, CFSTR("/"), 0);
	if (range.location == kCFNotFound) {
		return comp;
	}
	range.length = CFStringGetLength(comp) - range.location;
	CFStringDelete(comp, range);
	return comp;
}


static CFDictionaryRef
entity_one(SCDynamicStoreRef store, CFStringRef key)
{
	CFDictionaryRef		ent_dict	= NULL;
	CFDictionaryRef		if_dict		= NULL;
	CFStringRef 		if_key		= NULL;
	CFStringRef 		if_port;
	CFMutableDictionaryRef	new_dict	= NULL;
	static CFStringRef	pre		= NULL;
	CFStringRef		serviceID	= NULL;
	CFStringRef		serviceType;

	if (!pre) {
		pre = SCDynamicStoreKeyCreate(NULL,
					      CFSTR("%@/%@/%@/"),
					      kSCDynamicStoreDomainSetup,
					      kSCCompNetwork,
					      kSCCompService);
	}

	/*
	 * get entity dictionary for service
	 */
	ent_dict = SCDynamicStoreCopyValue(store, key);
	if (!isA_CFDictionary(ent_dict)) {
		goto done;
	}

	/*
	 * get interface dictionary for service
	 */
	serviceID = parse_component(key, pre);
	if (!serviceID) {
		goto done;
	}

	if_key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							     kSCDynamicStoreDomainSetup,
							     serviceID,
							     kSCEntNetInterface);
	if_dict = SCDynamicStoreCopyValue(store, if_key);
	CFRelease(if_key);
	if (!isA_CFDictionary(if_dict)) {
		goto done;
	}

	/* check the interface type */
	serviceType = CFDictionaryGetValue(if_dict,
					   kSCPropNetInterfaceType);
	if (!isA_CFString(serviceType) ||
	    !CFEqual(serviceType, kSCValNetInterfaceTypeEthernet)) {
		/* sorry, no AT networking on this interface */
		goto done;
	}

	/*
	 * get port name (from interface dictionary).
	 */
	if_port = CFDictionaryGetValue(if_dict, kSCPropNetInterfaceDeviceName);
	if (!isA_CFString(if_port)) {
		goto done;
	}

	/*
	 * add ServiceID and interface port name to entity dictionary.
	 */
	new_dict = CFDictionaryCreateMutableCopy(NULL, 0, ent_dict);
	CFDictionarySetValue(new_dict, CFSTR("ServiceID"), serviceID);
	CFDictionarySetValue(new_dict, kSCPropNetInterfaceDeviceName, if_port);

    done:

	if (ent_dict)	CFRelease(ent_dict);
	if (if_dict)	CFRelease(if_dict);
	if (serviceID)	CFRelease(serviceID);
	return (CFDictionaryRef)new_dict;
}


static CFArrayRef
entity_all(SCDynamicStoreRef store, CFStringRef entity, CFArrayRef order)
{
	CFMutableArrayRef	defined	= NULL;
	int			i;
	CFMutableArrayRef	ordered	= NULL;
	CFStringRef		pattern;

	ordered = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							      kSCDynamicStoreDomainSetup,
							      kSCCompAnyRegex,
							      entity);
	defined = (CFMutableArrayRef)SCDynamicStoreCopyKeyList(store, pattern);
	CFRelease(pattern);
	if (defined && (CFArrayGetCount(defined) > 0)) {
		CFArrayRef	tmp;

		tmp = defined;
		defined = CFArrayCreateMutableCopy(NULL, 0, tmp);
		CFRelease(tmp);
	} else {
		goto done;
	}

	for (i = 0; order && i < CFArrayGetCount(order); i++) {
		CFDictionaryRef	dict;
		CFStringRef	key;
		CFIndex		j;
		CFStringRef	service;

		service = CFArrayGetValueAtIndex(order, i);
		if (!isA_CFString(service)) {
			continue;
		}

		key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								  kSCDynamicStoreDomainSetup,
								  service,
								  entity);
		dict = entity_one(store, key);
		if (dict) {
			CFArrayAppendValue(ordered, dict);
			CFRelease(dict);
		}

		j = CFArrayGetFirstIndexOfValue(defined,
						CFRangeMake(0, CFArrayGetCount(defined)),
						key);
		if (j >= 0) {
			CFArrayRemoveValueAtIndex(defined, j);
		}

		CFRelease(key);
	}

	for (i = 0; i < CFArrayGetCount(defined); i++) {
		CFDictionaryRef	dict;
		CFStringRef	key;

		key  = CFArrayGetValueAtIndex(defined, i);
		dict = entity_one(store, key);
		if (dict) {
			CFArrayAppendValue(ordered, dict);
			CFRelease(dict);
		}
	}

    done:

	if (defined)	CFRelease(defined);
	if (CFArrayGetCount(ordered) == 0) {
		CFRelease(ordered);
		ordered = NULL;
	}
	return ordered;
}


static CFStringRef
encodeName(CFStringRef name, CFStringEncoding encoding)
{
	CFDataRef		bytes;
	CFMutableStringRef	encodedName = NULL;
	CFIndex			len;

	if (!isA_CFString(name)) {
		return NULL;
	}

	if (encoding == kCFStringEncodingMacRoman) {
		return CFRetain(name);
	}

	/*
	 * encode the potentially non-printable string
	 */
	bytes = CFStringCreateExternalRepresentation(NULL,
						     name,
						     encoding,
						     0);
	if (bytes) {
		unsigned char	*byte;
		CFIndex		i;

		encodedName = CFStringCreateMutable(NULL, 0);

		len  = CFDataGetLength(bytes);
		byte = (unsigned char *)CFDataGetBytePtr(bytes);
		for (i=0; i<len; i++, byte++) {
			CFStringAppendFormat(encodedName,
					     NULL,
					     CFSTR("%02x"),
					     *byte);
		}

		/*
		 * add "encoded string" markers
		 */
		CFStringInsert(encodedName, 0, CFSTR("*"));
		CFStringAppend(encodedName,    CFSTR("*"));

		CFRelease(bytes);
	}

	return (CFStringRef)encodedName;
}


static boolean_t
updateConfiguration()
{
	boolean_t		changed			= FALSE;
	CFStringRef		computerName;
	CFStringEncoding	computerNameEncoding;
	CFArrayRef		config;
	CFArrayRef		configuredInterfaces	= NULL;
	CFDictionaryRef		dict;
	CFIndex			i;
	CFIndex			ifCount			= 0;
	CFStringRef		key;
	CFArrayRef		keys;
	CFMutableArrayRef	newConfig;
	CFMutableArrayRef	newConfigFile;
	CFMutableDictionaryRef	newDefaults;
	CFMutableDictionaryRef	newDict;
	CFMutableDictionaryRef	newGlobals;
	CFMutableDictionaryRef	newState;
	CFMutableDictionaryRef	newZones;
	CFNumberRef		num;
	CFStringRef		primaryPort		= NULL;
	CFStringRef		primaryZone		= NULL;
	CFArrayRef		serviceOrder		= NULL;
	CFDictionaryRef		setGlobals		= NULL;
	CFMutableArrayRef	state			= NULL;
	CFStringRef		str;
	boolean_t		useFlatFiles		= TRUE;

	if (useFlatFiles) {
		key = SCDynamicStoreKeyCreate(NULL,
					      CFSTR("%@" "UseFlatFiles"),
					      kSCDynamicStoreDomainSetup);
		dict = SCDynamicStoreCopyValue(store, key);
		CFRelease(key);
		if (dict) {
			/* we're not using the network configuration database (yet) */
			CFRelease(dict);
			return FALSE;
		}
		useFlatFiles = FALSE;
	}

	/*
	 * establish the "new" AppleTalk configuration
	 */
	newConfigFile = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	newGlobals    = CFDictionaryCreateMutable(NULL,
						  0,
						  &kCFTypeDictionaryKeyCallBacks,
						  &kCFTypeDictionaryValueCallBacks);
	newDefaults   = CFDictionaryCreateMutable(NULL,
						  0,
						  &kCFTypeDictionaryKeyCallBacks,
						  &kCFTypeDictionaryValueCallBacks);
	newState      = CFDictionaryCreateMutable(NULL,
						  0,
						  &kCFTypeDictionaryKeyCallBacks,
						  &kCFTypeDictionaryValueCallBacks);
	newZones      = CFDictionaryCreateMutable(NULL,
						  0,
						  &kCFTypeDictionaryKeyCallBacks,
						  &kCFTypeDictionaryValueCallBacks);

	/* initialize overall state */
	CFDictionarySetValue(newGlobals, CFSTR("APPLETALK"), CFSTR("-NO-"));

	(void)SCDynamicStoreLock(store);

	/*
	 * get the global settings (ServiceOrder, ComputerName, ...)
	 */
	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							 kSCDynamicStoreDomainSetup,
							 kSCEntNetAppleTalk);
	setGlobals = SCDynamicStoreCopyValue(store, key);
	CFRelease(key);
	if (setGlobals) {
		if (isA_CFDictionary(setGlobals)) {
			/* get service order */
			serviceOrder = CFDictionaryGetValue(setGlobals,
							    kSCPropNetServiceOrder);
			serviceOrder = isA_CFArray(serviceOrder);
			if (serviceOrder) {
				CFRetain(serviceOrder);
			}
		} else {
			CFRelease(setGlobals);
			setGlobals = NULL;
		}
	}

	/*
	 * if we don't have an AppleTalk ServiceOrder, use IPv4's (if defined)
	 */
	if (!serviceOrder) {
		key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
								 kSCDynamicStoreDomainSetup,
								 kSCEntNetIPv4);
		dict = SCDynamicStoreCopyValue(store, key);
		CFRelease(key);
		if (dict) {
			if (isA_CFDictionary(dict)) {
				serviceOrder = CFDictionaryGetValue(dict,
								    kSCPropNetServiceOrder);
				serviceOrder = isA_CFArray(serviceOrder);
				if (serviceOrder) {
					CFRetain(serviceOrder);
				}
			}
			CFRelease(dict);
		}
	}

	/*
	 * get the list of ALL configured interfaces
	 */
	configuredInterfaces = entity_all(store, kSCEntNetAppleTalk, serviceOrder);
	if (configuredInterfaces) {
		ifCount = CFArrayGetCount(configuredInterfaces);
	}

	/*
	 * get the list of previously configured interfaces
	 */
	key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainState,
							  kSCCompAnyRegex,
							  kSCEntNetAppleTalk);
	keys = SCDynamicStoreCopyKeyList(store, key);
	CFRelease(key);
	if (keys) {
		state = CFArrayCreateMutableCopy(NULL, 0, keys);
		CFRelease(keys);
	}

	(void)SCDynamicStoreUnlock(store);
	if (serviceOrder)	CFRelease(serviceOrder);

	/*
	 * iterate over each configured service to establish the new
	 * configuration.
	 */
	for (i=0; i<ifCount; i++) {
		CFDictionaryRef		service;
		CFStringRef		ifName;
		CFStringRef		configMethod;
		CFMutableStringRef	portConfig	= NULL;
		CFArrayRef		networkRange;	/* for seed ports, CFArray[2] of CFNumber (lo, hi) */
		int			sNetwork;
		int			eNetwork;
		CFArrayRef		zoneList;	/* for seed ports, CFArray[] of CFString (zones names) */
		CFIndex			zCount;
		CFIndex			j;
		CFMutableDictionaryRef	ifDefaults	= NULL;
		CFNumberRef		defaultNetwork;
		CFNumberRef		defaultNode;
		CFStringRef		defaultZone;

		/* get AppleTalk service dictionary */
		service = CFArrayGetValueAtIndex(configuredInterfaces, i);

		/* get interface name */
		ifName  = CFDictionaryGetValue(service, kSCPropNetInterfaceDeviceName);

		/* check interface link status */
		key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
								    kSCDynamicStoreDomainState,
								    ifName,
								    kSCEntNetLink);
		dict = SCDynamicStoreCopyValue(store, key);
		CFRelease(key);
		if (dict) {
			Boolean	linkStatus	= TRUE;	/* assume the link is "up" */

			/* the link key for this interface is available */
			if (isA_CFDictionary(dict)) {
				CFBooleanRef	bool;

				bool = CFDictionaryGetValue(dict, kSCPropNetLinkActive);
				if (isA_CFBoolean(bool)) {
					linkStatus = CFBooleanGetValue(bool);
				}
			}
			CFRelease(dict);

			if (!linkStatus) {
				/* if link status down */
				goto nextIF;
			}
		}

		/*
		 * Determine configuration method for this interface
		 */
		configMethod = CFDictionaryGetValue(service, kSCPropNetAppleTalkConfigMethod);
		if (!isA_CFString(configMethod)) {
			/* if no ConfigMethod */
			goto nextIF;
		}

		if (!CFEqual(configMethod, kSCValNetAppleTalkConfigMethodNode      ) &&
		    !CFEqual(configMethod, kSCValNetAppleTalkConfigMethodRouter    ) &&
		    !CFEqual(configMethod, kSCValNetAppleTalkConfigMethodSeedRouter)) {
			/* if not one of the expected values, disable */
			SCLog(TRUE, LOG_NOTICE,
			      CFSTR("Unexpected AppleTalk ConfigMethod: %@"),
			      configMethod);
			goto nextIF;
		}

		/*
		 * define the port
		 */
		portConfig = CFStringCreateMutable(NULL, 0);
		CFStringAppendFormat(portConfig, NULL, CFSTR("%@:"), ifName);

		if (CFEqual(configMethod, kSCValNetAppleTalkConfigMethodSeedRouter)) {
			CFNumberRef	num;

			/*
			 * we have been asked to configure this interface as a
			 * seed port. Ensure that we have been provided at least
			 * one network number, have been provided with at least
			 * one zonename, ...
			 */

			networkRange = CFDictionaryGetValue(service,
							    kSCPropNetAppleTalkSeedNetworkRange);
			if (!isA_CFArray(networkRange) || (CFArrayGetCount(networkRange) == 0)) {
				SCLog(TRUE, LOG_NOTICE,
				      CFSTR("AppleTalk configuration error (%@)"),
				      kSCPropNetAppleTalkSeedNetworkRange);
				goto nextIF;
			}

			/*
			 * establish the starting and ending network numbers
			 */
			num = CFArrayGetValueAtIndex(networkRange, 0);
			if (!isA_CFNumber(num)) {
				SCLog(TRUE, LOG_NOTICE,
				      CFSTR("AppleTalk configuration error (%@)"),
				      kSCPropNetAppleTalkSeedNetworkRange);
				goto nextIF;
			}
			CFNumberGetValue(num, kCFNumberIntType, &sNetwork);
			eNetwork = sNetwork;

			if (CFArrayGetCount(networkRange) > 1) {
				num = CFArrayGetValueAtIndex(networkRange, 1);
				if (!isA_CFNumber(num)) {
					SCLog(TRUE, LOG_NOTICE,
					      CFSTR("AppleTalk configuration error (%@)"),
					      kSCPropNetAppleTalkSeedNetworkRange);
					goto nextIF;
				}
				CFNumberGetValue(num, kCFNumberIntType, &eNetwork);
			}
			CFStringAppendFormat(portConfig, NULL, CFSTR("%d:%d:"), sNetwork, eNetwork);

			/*
			 * establish the zones associated with this port
			 */
			zoneList = CFDictionaryGetValue(service,
							kSCPropNetAppleTalkSeedZones);
			if (!isA_CFArray(zoneList)) {
				SCLog(TRUE, LOG_NOTICE,
				      CFSTR("AppleTalk configuration error (%@)"),
				      kSCPropNetAppleTalkSeedZones);
				goto nextIF;
			}

			zCount = CFArrayGetCount(zoneList);
			for (j=0; j<zCount; j++) {
				CFStringRef		zone;
				CFArrayRef		ifList;
				CFMutableArrayRef	newIFList;

				zone = CFArrayGetValueAtIndex(zoneList, j);
				if (!isA_CFString(zone)) {
					continue;
				}

				if (CFDictionaryGetValueIfPresent(newZones, zone, (void **)&ifList)) {
					/* known zone */
					newIFList = CFArrayCreateMutableCopy(NULL, 0, ifList);
				} else {
					/* new zone */
					newIFList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
				}
				CFArrayAppendValue(newIFList, ifName);
				CFArraySortValues(newIFList,
						  CFRangeMake(0, CFArrayGetCount(newIFList)),
						  (CFComparatorFunction)CFStringCompare,
						  NULL);
				CFDictionarySetValue(newZones, zone, newIFList);
				CFRelease(newIFList);

				/*
				 * flag the default zone
				 */
				if (!primaryZone) {
					primaryZone = CFRetain(zone);
				}
			}
			if (!primaryZone) {
				SCLog(TRUE, LOG_NOTICE,
				      CFSTR("AppleTalk configuration error (%@)"),
				      kSCPropNetAppleTalkSeedZones);
				goto nextIF;
			}
		}

		/* get the (per-interface) "Computer Name" */
		computerName = CFDictionaryGetValue(service,
						    kSCPropNetAppleTalkComputerName);
		if (CFDictionaryGetValueIfPresent(service,
						  kSCPropNetAppleTalkComputerNameEncoding,
						  (void **)&num) &&
		    isA_CFNumber(num)) {
			CFNumberGetValue(num, kCFNumberIntType, &computerNameEncoding);
		} else {
			computerNameEncoding = CFStringGetSystemEncoding();
		}
		str = encodeName(computerName, computerNameEncoding);
		if (str) {
			CFDictionaryAddValue(newGlobals,
					     CFSTR("APPLETALK_HOSTNAME"),
					     str);
			CFRelease(str);
		}

		/*
		 * declare the first configured AppleTalk interface as the "home port".
		 */
		if (CFArrayGetCount(newConfigFile) == 0) {
			CFStringAppend(portConfig, CFSTR("*"));
			primaryPort = CFRetain(ifName);
		}
		CFArrayAppendValue(newConfigFile, portConfig);

		/*
		 * get the per-interface defaults
		 */
		ifDefaults = CFDictionaryCreateMutable(NULL,
						       0,
						       &kCFTypeDictionaryKeyCallBacks,
						       &kCFTypeDictionaryValueCallBacks);

		defaultNetwork = CFDictionaryGetValue(service, kSCPropNetAppleTalkNetworkID);
		defaultNode    = CFDictionaryGetValue(service, kSCPropNetAppleTalkNodeID);
		if (isA_CFNumber(defaultNetwork) && isA_CFNumber(defaultNode)) {
			/*
			 * set the default node and network
			 */
			CFDictionarySetValue(ifDefaults,
					     kSCPropNetAppleTalkNetworkID,
					     defaultNetwork);
			CFDictionarySetValue(ifDefaults,
					     kSCPropNetAppleTalkNodeID,
					     defaultNode);
		}

		if ((CFDictionaryGetValueIfPresent(service,
						   kSCPropNetAppleTalkDefaultZone,
						   (void **)&defaultZone) == TRUE)) {
			/*
			 * set the default zone for this interface
			 */
			CFDictionarySetValue(ifDefaults,
					     kSCPropNetAppleTalkDefaultZone,
					     defaultZone);
		}

		CFDictionarySetValue(newDefaults, ifName, ifDefaults);
		CFRelease(ifDefaults);

		switch (CFArrayGetCount(newConfigFile)) {
			case 1:
				/*
				 * first AppleTalk interface
				 */
				CFDictionarySetValue(newGlobals, CFSTR("APPLETALK"), ifName);
				break;
			case 2:
				/* second AppleTalk interface */
				if (!CFEqual(CFDictionaryGetValue(newGlobals, CFSTR("APPLETALK")),
					     CFSTR("-ROUTER-"))) {
					/*
					 * if not routing (yet), configure as multi-home
					 */
					CFDictionarySetValue(newGlobals, CFSTR("APPLETALK"), CFSTR("-MULTIHOME-"));
				}
				break;
		}

		if (CFEqual(configMethod, kSCValNetAppleTalkConfigMethodRouter) ||
		    CFEqual(configMethod, kSCValNetAppleTalkConfigMethodSeedRouter)) {
			/* if not a simple node, enable routing */
			CFDictionarySetValue(newGlobals, CFSTR("APPLETALK"), CFSTR("-ROUTER-"));
		}

		/*
		 * establish the State:/Network/Service/nnn/AppleTalk key info
		 */
		key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								  kSCDynamicStoreDomainState,
								  CFDictionaryGetValue(service, CFSTR("ServiceID")),
								  kSCEntNetAppleTalk);
		newDict = CFDictionaryCreateMutable(NULL,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
		CFDictionaryAddValue(newDict, kSCPropInterfaceName, ifName);
		CFDictionaryAddValue(newState, key, newDict);
		CFRelease(newDict);
		if (state) {
			j = CFArrayGetFirstIndexOfValue(state,
							CFRangeMake(0, CFArrayGetCount(state)),
							key);
			if (j != kCFNotFound) {
				CFArrayRemoveValueAtIndex(state, j);
			}
		}
		CFRelease(key);

	    nextIF :

		if (portConfig)	CFRelease(portConfig);
	}

	if (primaryZone) {
		CFArrayRef		ifList;
		CFMutableArrayRef	newIFList;

		ifList = CFDictionaryGetValue(newZones, primaryZone);
		if (CFArrayContainsValue(ifList,
					 CFRangeMake(0, CFArrayGetCount(ifList)),
					 primaryPort)) {
			newIFList = CFArrayCreateMutableCopy(NULL, 0, ifList);
			CFArrayAppendValue(newIFList, CFSTR("*"));
			CFDictionarySetValue(newZones, primaryZone, newIFList);
			CFRelease(newIFList);
		}
		CFRelease(primaryZone);
	}
	if (primaryPort) {
		CFRelease(primaryPort);
	}

	/* sort the ports */
	i = CFArrayGetCount(newConfigFile);
	CFArraySortValues(newConfigFile,
			  CFRangeMake(0, i),
			  (CFComparatorFunction)CFStringCompare,
			  NULL);

	/* add the zones to the configuration */
	CFDictionaryApplyFunction(newZones, addZoneToPorts, newConfigFile);
	CFRelease(newZones);

	/* sort the zones */
	CFArraySortValues(newConfigFile,
			  CFRangeMake(i, CFArrayGetCount(newConfigFile)-i),
			  (CFComparatorFunction)CFStringCompare,
			  NULL);

	/* ensure that the last line of the configuration file is terminated */
	CFArrayAppendValue(newConfigFile, CFSTR(""));

	/*
	 * Check if we have a "ComputerName" and look elsewhere if we don't have
	 * one yet.
	 */
	if (!CFDictionaryContainsKey(newGlobals, CFSTR("APPLETALK_HOSTNAME")) &&
	    (setGlobals != NULL)) {
		computerName = CFDictionaryGetValue(setGlobals,
						    kSCPropNetAppleTalkComputerName);
		if (CFDictionaryGetValueIfPresent(setGlobals,
						  kSCPropNetAppleTalkComputerNameEncoding,
						  (void **)&num) &&
		    isA_CFNumber(num)) {
			CFNumberGetValue(num, kCFNumberIntType, &computerNameEncoding);
		} else {
			computerNameEncoding = CFStringGetSystemEncoding();
		}
		str = encodeName(computerName, computerNameEncoding);
		if (str) {
			CFDictionaryAddValue(newGlobals,
					     CFSTR("APPLETALK_HOSTNAME"),
					     str);
			CFRelease(str);
		}
	}
	if (!CFDictionaryContainsKey(newGlobals, CFSTR("APPLETALK_HOSTNAME"))) {
		computerName = SCDynamicStoreCopyComputerName(store, &computerNameEncoding);
		if (computerName) {
			str = encodeName(computerName, computerNameEncoding);
			if (str) {
				CFDictionaryAddValue(newGlobals,
						     CFSTR("APPLETALK_HOSTNAME"),
						     str);
				CFRelease(str);
			}
			CFRelease(computerName);
	    }
	}

	/* establish the new /etc/hostconfig file */

	config    = configRead(HOSTCONFIG);
	newConfig = CFArrayCreateMutableCopy(NULL, 0, config);
	configSet(newConfig,
		  CFSTR("APPLETALK"),
		  CFDictionaryGetValue(newGlobals, CFSTR("APPLETALK")));

	if (CFDictionaryGetValueIfPresent(newGlobals,
					  CFSTR("APPLETALK_HOSTNAME"),
					  (void *)&computerName)) {
		/* if "Computer Name" was specified */
		configSet   (newConfig, CFSTR("APPLETALK_HOSTNAME"), computerName);
	} else {
		/* if "Computer Name" was not specified */
		configRemove(newConfig, CFSTR("APPLETALK_HOSTNAME"));
	}

	/* compare the previous and current configurations */

	if (CFEqual(oldGlobals    , newGlobals   ) &&
	    CFEqual(oldConfigFile , newConfigFile) &&
	    CFEqual(oldDefaults   , newDefaults  )
	    ) {
		/*
		 * the configuration has not changed
		 */
		CFRelease(newGlobals);
		CFRelease(newConfigFile);
		CFRelease(newDefaults);
	} else if (CFArrayGetCount(newConfigFile) <= 1) {
		/*
		 * the configuration has changed but there are no
		 * longer any interfaces configured for AppleTalk
		 * networking.
		 * 1. remove the config file(s)
		 * 2. keep track of the new configuration
		 * 3. flag this as a new configuration
		 */
		configRemove(newConfig, CFSTR("APPLETALK_HOSTNAME"));
		(void)unlink(AT_CFG_FILE);
		changed = TRUE;
	} else {
		/*
		 * the configuration has changed.
		 * 1. write the new /etc/appletalk.cfg config file
		 * 2. keep track of the new configuration
		 * 3. flag this as a new configuration
		 */
		configWrite(AT_CFG_FILE, newConfigFile);
		CFDictionaryApplyFunction(newDefaults, updateDefaults, NULL);
		changed = TRUE;
	}

	/* update State:/Network/Service/XXX/AppleTalk keys */

	if (changed) {
		if (!SCDynamicStoreSetMultiple(store, newState, state, NULL)) {
			SCLog(TRUE,
			      LOG_ERR,
			      CFSTR("SCDynamicStoreSetMultiple() failed: %s"),
			      SCErrorString(SCError()));
		}
	}
	if (state)	CFRelease(state);
	if (newState)	CFRelease(newState);

	if (changed) {
		CFRelease(oldGlobals);
		oldGlobals    = newGlobals;
		CFRelease(oldConfigFile);
		oldConfigFile = newConfigFile;
		CFRelease(oldDefaults);
		oldDefaults   = newDefaults;
	}

	/* update overall state of AppleTalk stack */
	if (!CFEqual(config, newConfig)) {
		configWrite(HOSTCONFIG, newConfig);
		changed = TRUE;
	}
	CFRelease(config);
	CFRelease(newConfig);
	if (configuredInterfaces)
	    CFRelease(configuredInterfaces);
	if (setGlobals)	CFRelease(setGlobals);
	return changed;
}


static void
atConfigChangedCallback(SCDynamicStoreRef store, CFArrayRef changedKeys, void *arg)
{
	boolean_t		configChanged;
	static boolean_t	initialConfiguration	= TRUE;
	CFStringRef		key;

	configChanged = updateConfiguration();

	if (initialConfiguration) {
		/*
		 * initial confiuration has completed, post a key for anyone
		 * who wants to wait until we're ready.
		 */
		initialConfiguration = FALSE;
		(void) SCDynamicStoreRemoveWatchedKey(store, kSCDynamicStoreDomainSetup, FALSE);
	} else if (configChanged) {
		/*
		 * the AT configuration files have been updated, tell "Kicker"
		 */
		key = SCDynamicStoreKeyCreate(NULL,
					      CFSTR("%@%s"),
					      kSCDynamicStoreDomainFile,
					      AT_CFG_FILE);
		if (!SCDynamicStoreNotifyValue(store, key)) {
			SCLog(TRUE,
			      LOG_ERR,
			      CFSTR("SCDynamicStoreNotifyValue() failed: %s"),
			      SCErrorString(SCError()));
			/* XXX need to do something more with this FATAL error XXXX */
		}
		CFRelease(key);
	}

	/*
	 * post a key for anyone who wants to wait until we know the
	 * configuration files are stable.
	 */
	key = SCDynamicStoreKeyCreate(NULL,
				      CFSTR("%@%@"),
				      kSCDynamicStoreDomainPlugin,
				      kSCEntNetAppleTalk);
	if (!SCDynamicStoreNotifyValue(store, key)) {
		SCLog(TRUE,
		      LOG_ERR,
		      CFSTR("SCDynamicStoreNotifyValue() failed: %s"),
		      SCErrorString(SCError()));
		/* XXX need to do something more with this FATAL error XXXX */
	}
	CFRelease(key);

	return;
}


void
load(CFBundleRef bundle, Boolean bundleVerbose)
{
	CFStringRef		key;
	CFMutableArrayRef	keys		= NULL;
	CFMutableArrayRef	patterns	= NULL;

	if (bundleVerbose) {
		_verbose = TRUE;
	}       
        
	SCLog(_verbose, LOG_DEBUG, CFSTR("load() called"));
	SCLog(_verbose, LOG_DEBUG, CFSTR("  bundle ID = %@"), CFBundleGetIdentifier(bundle));

	/* initialize a few globals */

	oldGlobals    = CFDictionaryCreateMutable(NULL,
						  0,
						  &kCFTypeDictionaryKeyCallBacks,
						  &kCFTypeDictionaryValueCallBacks);
	oldConfigFile = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	oldDefaults   = CFDictionaryCreateMutable(NULL,
						  0,
						  &kCFTypeDictionaryKeyCallBacks,
						  &kCFTypeDictionaryValueCallBacks);

	/* open a "configd" store to allow cache updates */
	store = SCDynamicStoreCreate(NULL,
				     CFSTR("AppleTalk Configuraton plug-in"),
				     atConfigChangedCallback,
				     NULL);
	if (!store) {
		SCLog(TRUE, LOG_ERR, CFSTR("SCDynamicStoreCreate() failed: %s"), SCErrorString(SCError()));
		goto error;
	}

	/* establish notificaiton keys and patterns */

	keys     = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	/* ...we want to watch for any configuration changes */
	CFArrayAppendValue(keys, kSCDynamicStoreDomainSetup);

	/* ...watch for (global) AppleTalk configuration changes */
	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							 kSCDynamicStoreDomainSetup,
							 kSCEntNetAppleTalk);
	CFArrayAppendValue(patterns, key);
	CFRelease(key);

	/* ...watch for (per-service) AppleTalk configuration changes */
	key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							  kSCDynamicStoreDomainSetup,
							  kSCCompAnyRegex,
							  kSCEntNetAppleTalk);
	CFArrayAppendValue(patterns, key);
	CFRelease(key);

	/* ...watch for network interface link status changes */
	key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							    kSCDynamicStoreDomainState,
							    kSCCompAnyRegex,
							    kSCEntNetLink);
	CFArrayAppendValue(patterns, key);
	CFRelease(key);

	/* ...watch for computer name changes */
	key = SCDynamicStoreKeyCreateComputerName(NULL);
	CFArrayAppendValue(keys, key);
	CFRelease(key);

	/* register the keys/patterns */
	if (!SCDynamicStoreSetNotificationKeys(store, keys, patterns)) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("SCDynamicStoreSetNotificationKeys() failed: %s"),
		      SCErrorString(SCError()));
		goto error;
	}

	rls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
	if (!rls) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("SCDynamicStoreCreateRunLoopSource() failed: %s"),
		      SCErrorString(SCError()));
		goto error;
	}

	CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);

	CFRelease(keys);
	CFRelease(patterns);
	return;

    error :

	if (oldGlobals)		CFRelease(oldGlobals);
	if (oldConfigFile)	CFRelease(oldConfigFile);
	if (oldDefaults)	CFRelease(oldDefaults);
	if (store) 		CFRelease(store);
	if (keys)		CFRelease(keys);
	if (patterns)		CFRelease(patterns);
	return;
}


#ifdef	MAIN
#include "cfManager.c"
int
main(int argc, char **argv)
{
	_sc_log     = FALSE;
	_sc_verbose = (argc > 1) ? TRUE : FALSE;

	load(CFBundleGetMainBundle(), (argc > 1) ? TRUE : FALSE);
	CFRunLoopRun();
	/* not reached */
	exit(0);
	return 0;
}
#endif
