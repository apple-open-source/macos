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


#include <stdio.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netat/appletalk.h>
#include <netat/at_var.h>
#include <AppleTalk/at_paths.h>
#include <AppleTalk/at_proto.h>

#define	SYSTEMCONFIGURATION_NEW_API
#include <SystemConfiguration/SystemConfiguration.h>

#include "cfManager.h"

#define HOSTCONFIG	"/etc/hostconfig"


SCDSessionRef		session		= NULL;
CFMutableDictionaryRef	oldGlobals	= NULL;
CFMutableArrayRef	oldConfigFile	= NULL;
CFMutableDictionaryRef	oldDefaults	= NULL;


static __inline__ CFTypeRef
isA_CFType(CFTypeRef obj, CFTypeID type)
{
	if (obj == NULL)
		return (NULL);

	if (CFGetTypeID(obj) != type)
		return (NULL);
	return (obj);
}

static __inline__ CFTypeRef
isA_CFDictionary(CFTypeRef obj)
{
	return (isA_CFType(obj, CFDictionaryGetTypeID()));
}

static __inline__ CFTypeRef
isA_CFArray(CFTypeRef obj)
{
	return (isA_CFType(obj, CFArrayGetTypeID()));
}

static __inline__ CFTypeRef
isA_CFString(CFTypeRef obj)
{
	return (isA_CFType(obj, CFStringGetTypeID()));
}


void
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
			SCDLog(LOG_ERR, CFSTR("CFStringGetCString: could not convert interface name to C string"));
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
				SCDLog(LOG_ERR, CFSTR("at_setdefaultaddr() failed"));
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
					SCDLog(LOG_ERR, CFSTR("at_setdefaultzone() failed"));
				}
			} else {
				SCDLog(LOG_ERR, CFSTR("CFStringGetCString: could not convert default zone to C string"));
			}
		}
	}
	return;
}


void
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
entity_one(SCDSessionRef session, CFStringRef key)
{
	CFDictionaryRef		ent_dict	= NULL;
	SCDHandleRef		ent_handle	= NULL;
	CFDictionaryRef		if_dict		= NULL;
	SCDHandleRef		if_handle	= NULL;
	CFStringRef 		if_key		= NULL;
	CFStringRef 		if_port;
	CFMutableDictionaryRef	new_dict	= NULL;
	static CFStringRef	pre		= NULL;
	CFStringRef		serviceID	= NULL;
	CFStringRef		serviceType;
	SCDStatus		status;

	if (pre == NULL) {
		pre = SCDKeyCreate(CFSTR("%@/%@/%@/"),
				   kSCCacheDomainSetup,
				   kSCCompNetwork,
				   kSCCompService);
	}

	/*
	 * get entity dictionary for service
	 */
	status = SCDGet(session, key, &ent_handle);
	if (status != SCD_OK) {
		goto done;
	}
	ent_dict = isA_CFDictionary(SCDHandleGetData(ent_handle));
	if (ent_dict == NULL) {
		goto done;
	}

	/*
	 * get interface dictionary for service
	 */
	serviceID = parse_component(key, pre);
	if (serviceID == NULL) {
		goto done;
	}

	if_key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup,
						  serviceID,
						  kSCEntNetInterface);
	CFRelease(serviceID);
	status = SCDGet(session, if_key, &if_handle);
	CFRelease(if_key);
	if (status != SCD_OK) {
		goto done;
	}
	if_dict = isA_CFDictionary(SCDHandleGetData(if_handle));
	if (if_dict == NULL) {
		goto done;
	}

	/* check the interface type */
	if (!CFDictionaryGetValueIfPresent(if_dict,
					    kSCPropNetInterfaceType,
					    (void **)&serviceType) ||
	    !CFEqual(serviceType, kSCValNetInterfaceTypeEthernet)) {
		/* sorry, no AT networking on this interface */
		goto done;
	}

	/*
	 * add port name (from interface dictionary) to entity
	 * dictionary and return the result.
	 */
	if_port = CFDictionaryGetValue(if_dict, kSCPropNetInterfaceDeviceName);
	if (if_port == NULL) {
		goto done;
	}
	new_dict = CFDictionaryCreateMutableCopy(NULL, 0, ent_dict);
	CFDictionarySetValue(new_dict, kSCPropNetInterfaceDeviceName, if_port);

    done:

	if (ent_handle)	SCDHandleRelease(ent_handle);
	if (if_handle)	SCDHandleRelease(if_handle);
	return (CFDictionaryRef)new_dict;
}


static CFArrayRef
entity_all(SCDSessionRef session, CFStringRef entity, CFArrayRef order)
{
	CFMutableArrayRef	defined	= NULL;
	int			i;
	CFStringRef		key;
	CFMutableArrayRef	ordered	= NULL;
	SCDStatus		status;

	ordered = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup, kSCCompAnyRegex, entity);
	status = SCDList(session, key, kSCDRegexKey, &defined);
	CFRelease(key);
	if (status != SCD_OK) {
		goto done;
	}
	if (CFArrayGetCount(defined) > 0) {
		CFMutableArrayRef	tmp;

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

		key  = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup,
							CFArrayGetValueAtIndex(order, i),
							entity);
		dict = entity_one(session, key);
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
		dict = entity_one(session, key);
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

	if (name == NULL) {
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


boolean_t
updateConfiguration()
{
	boolean_t		changed			= FALSE;
	CFStringRef		computerName;
	CFStringEncoding	computerNameEncoding;
	CFArrayRef		config;
	CFArrayRef		configuredInterfaces	= NULL;
	SCDHandleRef		handle;
	CFIndex			i;
	CFIndex			ifCount			= 0;
	CFStringRef		key;
	CFMutableArrayRef	newConfig;
	CFMutableArrayRef	newConfigFile;
	CFMutableDictionaryRef	newDefaults;
	CFMutableDictionaryRef	newGlobals;
	CFMutableDictionaryRef	newZones;
	CFNumberRef		num;
	CFStringRef		primaryPort		= NULL;
	CFStringRef		primaryZone		= NULL;
	SCDStatus		scd_status;
	CFArrayRef		serviceOrder		= NULL;
	CFDictionaryRef		setGlobals		= NULL;
	CFStringRef		str;
	boolean_t		useFlatFiles		= TRUE;

	if (useFlatFiles) {
		key = SCDKeyCreate(CFSTR("%@" "UseFlatFiles"), kSCCacheDomainSetup);
		scd_status = SCDGet(session, key, &handle);
		CFRelease(key);
		switch (scd_status) {
			case SCD_OK :
				/* we're not using the network configuration database (yet) */
				SCDHandleRelease(handle);
				return FALSE;
			case SCD_NOKEY :
				/*
				 * we're using the network configuration preferences
				 */
				useFlatFiles = FALSE;
				break;
			default :
				SCDLog(LOG_ERR, CFSTR("SCDGet() failed: %s"), SCDError(scd_status));
				/* XXX need to do something more with this FATAL error XXXX */
				return FALSE;
		}
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
	newZones      = CFDictionaryCreateMutable(NULL,
						  0,
						  &kCFTypeDictionaryKeyCallBacks,
						  &kCFTypeDictionaryValueCallBacks);

	/* initialize overall state */
	CFDictionarySetValue(newGlobals, CFSTR("APPLETALK"), CFSTR("-NO-"));

	(void)SCDLock(session);

	/*
	 * get the global settings (ServiceOrder, ComputerName, ...)
	 */
	key = SCDKeyCreateNetworkGlobalEntity(kSCCacheDomainSetup, kSCEntNetAppleTalk);
	scd_status = SCDGet(session, key, &handle);
	CFRelease(key);
	switch (scd_status) {
		case SCD_OK :
			/* if global settings are available */
			setGlobals = SCDHandleGetData(handle);
			CFRetain(setGlobals);

			/* get service order */
			if ((CFDictionaryGetValueIfPresent(setGlobals,
							   kSCPropNetServiceOrder,
							   (void **)&serviceOrder) == TRUE)) {
				CFRetain(serviceOrder);
			}

			SCDHandleRelease(handle);
			break;
		case SCD_NOKEY :
			/* if no global settings */
			break;
		default :
			SCDLog(LOG_ERR, CFSTR("SCDGet() failed: %s"), SCDError(scd_status));
			/* XXX need to do something more with this FATAL error XXXX */
			goto globalDone;
	}

	/*
	 * if we don't have an AppleTalk ServiceOrder, use IPv4's (if defined)
	 */
	if (!serviceOrder) {
		key = SCDKeyCreateNetworkGlobalEntity(kSCCacheDomainSetup, kSCEntNetIPv4);
		scd_status = SCDGet(session, key, &handle);
		CFRelease(key);
		switch (scd_status) {
			CFDictionaryRef	ipv4Globals;

			case SCD_OK :
				ipv4Globals = SCDHandleGetData(handle);
				if ((CFDictionaryGetValueIfPresent(ipv4Globals,
								   kSCPropNetServiceOrder,
								   (void **)&serviceOrder) == TRUE)) {
					CFRetain(serviceOrder);
				}

				SCDHandleRelease(handle);
				break;
			case SCD_NOKEY :
				break;
			default :
				SCDLog(LOG_ERR, CFSTR("SCDGet() failed: %s"), SCDError(scd_status));
				/* XXX need to do something more with this FATAL error XXXX */
				goto globalDone;
		}
	}

	/*
	 * get the list of ALL configured interfaces
	 */
	configuredInterfaces = entity_all(session, kSCEntNetAppleTalk, serviceOrder);
	if (configuredInterfaces) {
		ifCount = CFArrayGetCount(configuredInterfaces);
	}


    globalDone :

	(void)SCDUnlock(session);
	if (serviceOrder)	CFRelease(serviceOrder);

	/*
	 * iterate over each configured service to establish the new
	 * configuration.
	 */
	for (i=0; i<ifCount; i++) {
		CFDictionaryRef		service;
		CFStringRef		ifName;
		SCDHandleRef		handle		= NULL;
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
		key = SCDKeyCreateNetworkInterfaceEntity(kSCCacheDomainState, ifName, kSCEntNetLink);
		scd_status = SCDGet(session, key, &handle);
		CFRelease(key);
		switch (scd_status) {
			CFDictionaryRef	linkDict;
			CFBooleanRef	linkStatus;

			case SCD_OK :
				/* this link status for this interface is available */
				linkDict   = SCDHandleGetData(handle);
				linkStatus = CFDictionaryGetValue(linkDict, kSCPropNetLinkActive);
				if ((linkStatus == NULL) || (linkStatus != kCFBooleanTrue)) {
					/* if link status unknown or not "up" */
					goto nextIF;
				}
				break;
			case SCD_NOKEY :
				/* if no link status, assume it's "up" */
				break;
			default :
				SCDLog(LOG_ERR, CFSTR("SCDGet() failed: %s"), SCDError(scd_status));
				/* XXX need to do something more with this FATAL error XXXX */
				goto nextIF;
		}

		/*
		 * Determine configuration method for this interface
		 */
		if (!CFDictionaryGetValueIfPresent(service,
						   kSCPropNetAppleTalkConfigMethod,
						   (void **)&configMethod)) {
			/* if no ConfigMethod */
			goto nextIF;
		}

		if (!CFEqual(configMethod, kSCValNetAppleTalkConfigMethodNode      ) &&
		    !CFEqual(configMethod, kSCValNetAppleTalkConfigMethodRouter    ) &&
		    !CFEqual(configMethod, kSCValNetAppleTalkConfigMethodSeedRouter)) {
			/* if not one of the expected values, disable */
			SCDLog(LOG_NOTICE,
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
			networkRange = CFDictionaryGetValue(service,
							    kSCPropNetAppleTalkSeedNetworkRange);
			if ((networkRange == NULL) || (CFArrayGetCount(networkRange) == 0)) {
				SCDLog(LOG_NOTICE,
				       CFSTR("AppleTalk configuration error (%@)"),
				       kSCPropNetAppleTalkSeedNetworkRange);
				goto nextIF;
			}

			zoneList = CFDictionaryGetValue(service,
							kSCPropNetAppleTalkSeedZones);
			if ((zoneList == NULL) || ((zCount = CFArrayGetCount(zoneList)) == 0)) {
				SCDLog(LOG_NOTICE,
				       CFSTR("AppleTalk configuration error (%@)"),
				       kSCPropNetAppleTalkSeedZones);
				goto nextIF;
			}

			/*
			 * we have been asked to configure this interface as a
			 * seed port, have been provided at least one network
			 * number, and have been provided with at least one
			 * zonename...
			 */

			/*
			 * establish the starting and ending network numbers
			 */
			CFNumberGetValue(CFArrayGetValueAtIndex(networkRange, 0), kCFNumberIntType, &sNetwork);
			if (CFArrayGetCount(networkRange) > 1) {
				CFNumberGetValue(CFArrayGetValueAtIndex(networkRange, 1), kCFNumberIntType, &eNetwork);
			} else {
				eNetwork = sNetwork;
			}
			CFStringAppendFormat(portConfig, NULL, CFSTR("%d:%d:"), sNetwork, eNetwork);

			/*
			 * establish the zones associated with this port
			 */
			for (j=0; j<zCount; j++) {
				CFStringRef		zone;
				CFArrayRef		ifList;
				CFMutableArrayRef	newIFList;

				zone = CFArrayGetValueAtIndex(zoneList, j);
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
				if (primaryZone == NULL) {
					primaryZone = CFRetain(zone);
				}
			}
		}

		/* get the (per-interface) "Computer Name" */
		computerName = CFDictionaryGetValue(service,
						    kSCPropNetAppleTalkComputerName);
		if (CFDictionaryGetValueIfPresent(service,
				    kSCPropNetAppleTalkComputerNameEncoding,
				    (void **)&num)) {
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

		if (CFDictionaryGetValueIfPresent(service,
						  kSCPropNetAppleTalkNetworkID,
						  (void **)&defaultNetwork) &&
		    CFDictionaryGetValueIfPresent(service,
						  kSCPropNetAppleTalkNodeID,
						  (void **)&defaultNode)
		    ) {
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

	    nextIF :

		if (portConfig)	CFRelease(portConfig);
		if (handle)	SCDHandleRelease(handle);
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
		computerName = CFDictionaryGetValue(setGlobals, kSCPropNetAppleTalkComputerName);
		if (CFDictionaryGetValueIfPresent(setGlobals,
						  kSCPropNetAppleTalkComputerNameEncoding,
						  (void **)&num)) {
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
	if (!CFDictionaryContainsKey(newGlobals, CFSTR("APPLETALK_HOSTNAME")) &&
	    (SCDHostNameGet(&computerName, &computerNameEncoding) == SCD_OK) &&
	    (computerName != NULL)) {
		str = encodeName(computerName, computerNameEncoding);
		if (str) {
			CFDictionaryAddValue(newGlobals,
					     CFSTR("APPLETALK_HOSTNAME"),
					     str);
			CFRelease(str);
		}
		CFRelease(computerName);
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

	if (setGlobals)	CFRelease(setGlobals);
	return changed;
}


boolean_t
atConfigChangedCallback(SCDSessionRef session, void *arg)
{
	boolean_t	configChanged;
	SCDStatus	scd_status;
	CFArrayRef	changedKeys;

	/*
	 * Fetched the changed keys
	 */
	scd_status = SCDNotifierGetChanges(session, &changedKeys);
	if (scd_status == SCD_OK) {
		CFRelease(changedKeys);
	} else {
		SCDLog(LOG_ERR, CFSTR("SCDNotifierGetChanges() failed: %s"), SCDError(scd_status));
		/* XXX need to do something more with this FATAL error XXXX */
	}

	configChanged = updateConfiguration();

	if (configChanged) {
		/*
		 * the AT configuration files have been updated, tell "Kicker"
		 */
		CFStringRef		key;

		key = SCDKeyCreate(CFSTR("%@%s"), kSCCacheDomainFile, AT_CFG_FILE);
		scd_status = SCDTouch(session, key);
		CFRelease(key);
		if (scd_status != SCD_OK) {
			SCDLog(LOG_ERR, CFSTR("SCDTouch() failed: %s"), SCDError(scd_status));
			/* XXX need to do something more with this FATAL error XXXX */
		}
	}

	return TRUE;
}


void
prime()
{
	SCDLog(LOG_DEBUG, CFSTR("prime() called"));

	/* load the initial configuration from the database */
	(void) updateConfiguration();

	return;
}


void
start(const char *bundleName, const char *bundleDir)
{
	SCDStatus	scd_status;
	CFStringRef	key;

	SCDLog(LOG_DEBUG, CFSTR("start() called"));
	SCDLog(LOG_DEBUG, CFSTR("  bundle name      = %s"), bundleName);
	SCDLog(LOG_DEBUG, CFSTR("  bundle directory = %s"), bundleDir);

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
	/* open a "configd" session to allow cache updates */
	scd_status = SCDOpen(&session, CFSTR("AppleTalk Configuraton plug-in"));
	if (scd_status != SCD_OK) {
		SCDLog(LOG_ERR, CFSTR("SCDOpen() failed: %s"), SCDError(scd_status));
		goto error;
	}

	/* watch for (global) AppleTalk configuration changes */
	key = SCDKeyCreateNetworkGlobalEntity(kSCCacheDomainSetup,
					       kSCEntNetAppleTalk);
	scd_status = SCDNotifierAdd(session, key, kSCDRegexKey);
	CFRelease(key);
	if (scd_status != SCD_OK) {
		SCDLog(LOG_NOTICE, CFSTR("  SCDNotifierAdd(): %s"), SCDError(scd_status));
		goto error;
	}

	/* watch for (per-service) AppleTalk configuration changes */
	key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup,
					       kSCCompAnyRegex,
					       kSCEntNetAppleTalk);
	scd_status = SCDNotifierAdd(session, key, kSCDRegexKey);
	CFRelease(key);
	if (scd_status != SCD_OK) {
		SCDLog(LOG_NOTICE, CFSTR("  SCDNotifierAdd(): %s"), SCDError(scd_status));
		goto error;
	}

	/* watch for network interface link status changes */
	key = SCDKeyCreateNetworkInterfaceEntity(kSCCacheDomainState,
						 kSCCompAnyRegex,
						 kSCEntNetLink);
	scd_status = SCDNotifierAdd(session, key, kSCDRegexKey);
	CFRelease(key);
	if (scd_status != SCD_OK) {
		SCDLog(LOG_NOTICE, CFSTR("  SCDNotifierAdd(): %s"), SCDError(scd_status));
		goto error;
	}

	/* watch for computer name changes */
	key = SCDKeyCreateHostName();
	scd_status = SCDNotifierAdd(session, key, 0);
	CFRelease(key);
	if (scd_status != SCD_OK) {
		SCDLog(LOG_NOTICE, CFSTR("  SCDNotifierAdd(): %s"), SCDError(scd_status));
		goto error;
	}

	scd_status = SCDNotifierInformViaCallback(session, atConfigChangedCallback, NULL);
	if (scd_status != SCD_OK) {
		SCDLog(LOG_NOTICE, CFSTR("SCDNotifierInformViaCallback() failed: %s"), SCDError(scd_status));
		goto error;
	}

	return;

    error :

	if (session) 	(void) SCDClose(&session);
	return;
}
