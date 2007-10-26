/*
 * Copyright (c) 2000-2006 Apple Computer, Inc. All rights reserved.
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
 * March 15, 2003		Allan Nathanson <ajn@apple.com>
 * - startup/shutdown AT networking without Kicker's help and
 *   publish the state information after the configuration is
 *   active.
 *
 * April 29, 2002		Allan Nathanson <ajn@apple.com>
 * - add global state information (primary service, interface)
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
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <net/if.h>
#include <netat/appletalk.h>
#include <netat/at_var.h>
#include <AppleTalk/at_paths.h>
#include <AppleTalk/at_proto.h>

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCDPlugin.h>
#include <SystemConfiguration/SCValidation.h>

#include "cache.h"
#include "cfManager.h"

#define HOSTCONFIG	"/etc/hostconfig"

static SCDynamicStoreRef	store		= NULL;
static CFRunLoopSourceRef	storeRls	= NULL;

static int			curState	= 0;	// abs(state) == sequence #, < 0 == stop, > 0 == start
static CFMutableDictionaryRef	curGlobals	= NULL;
static CFMutableArrayRef	curConfigFile	= NULL;
static CFMutableDictionaryRef	curDefaults	= NULL;
static CFMutableDictionaryRef	curStartup	= NULL;

static Boolean			_verbose	= FALSE;


#define	RETRY_DELAY	2.0	// seconds
#define	RETRY_LIMIT	5	// # of startup/shutdown attempts

static void	startAppleTalk(CFRunLoopTimerRef timer, void *info);
static void	stopAppleTalk (CFRunLoopTimerRef timer, void *info);


static void *
__loadAppleTalk(void) {
	static void *image = NULL;
	if (NULL == image) {
		const char	*framework		= "/System/Library/Frameworks/AppleTalk.framework/Versions/A/AppleTalk";
		struct stat	statbuf;
		const char	*suffix			= getenv("DYLD_IMAGE_SUFFIX");
		char		path[MAXPATHLEN];

		strlcpy(path, framework, sizeof(path));
		if (suffix) strlcat(path, suffix, sizeof(path));
		if (0 <= stat(path, &statbuf)) {
			image = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
		} else {
			image = dlopen(framework, RTLD_LAZY | RTLD_LOCAL);
		}
	}
	return (void *)image;
}


static int
_at_setdefaultaddr(char *ifName, struct at_addr *init_address)
{
	#undef at_setdefaultaddr
	static typeof (at_setdefaultaddr) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadAppleTalk();
		if (image) dyfunc = dlsym(image, "at_setdefaultaddr");
	}
	return dyfunc ? dyfunc(ifName, init_address) : -1;
}
#define at_setdefaultaddr _at_setdefaultaddr


static int
_at_setdefaultzone(char *ifName, at_nvestr_t *zone)
{
	#undef at_setdefaultzone
	static typeof (at_setdefaultzone) *dyfunc = NULL;
	if (!dyfunc) {
		void *image = __loadAppleTalk();
		if (image) dyfunc = dlsym(image, "at_setdefaultzone");
	}
	return dyfunc ? dyfunc(ifName, zone) : -1;
}
#define at_setdefaultzone _at_setdefaultzone


static void
updateDefaults(const void *key, const void *val, void *context)
{
	CFStringRef		ifName		= (CFStringRef)key;
	CFDictionaryRef		oldDict;
	CFDictionaryRef		newDict		= (CFDictionaryRef)val;
	CFNumberRef		defaultNode;
	CFNumberRef		defaultNetwork;
	CFStringRef		defaultZone;

	if (!CFDictionaryGetValueIfPresent(curDefaults, ifName, (const void **)&oldDict) ||
	    !CFEqual(oldDict, newDict)) {
		char		ifr_name[IFNAMSIZ + 1];

		bzero(&ifr_name, sizeof(ifr_name));
		if (!_SC_cfstring_to_cstring(ifName, ifr_name, sizeof(ifr_name), kCFStringEncodingASCII)) {
			SCLog(TRUE, LOG_ERR, CFSTR("could not convert interface name to C string"));
			return;
		}

		/*
		 * Set preferred Network and Node ID
		 */
		if (CFDictionaryGetValueIfPresent(newDict,
						  kSCPropNetAppleTalkNetworkID,
						  (const void **)&defaultNetwork) &&
		    CFDictionaryGetValueIfPresent(newDict,
						  kSCPropNetAppleTalkNodeID,
						  (const void **)&defaultNode)
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
				return;
			}
		}

		/*
		 * Set default zone
		 */
		if (CFDictionaryGetValueIfPresent(newDict,
						  kSCPropNetAppleTalkDefaultZone,
						  (const void **)&defaultZone)
		    ) {
			int		status;
			at_nvestr_t	zone;

			/*
			 * set the "default zone" for this interface
			 */
			bzero(&zone, sizeof(zone));
			if (!_SC_cfstring_to_cstring(defaultZone,
						     (char *)zone.str,
						     sizeof(zone.str),
						     kCFStringEncodingASCII)) {
				SCLog(TRUE, LOG_ERR, CFSTR("could not convert default zone to C string"));
				return;
			}

			zone.len = strlen((const char *)zone.str);
			status = at_setdefaultzone(ifr_name, &zone);
			if (status == -1) {
				SCLog(TRUE, LOG_ERR, CFSTR("at_setdefaultzone() failed"));
				return;
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
	ent_dict = cache_SCDynamicStoreCopyValue(store, key);
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
	if_dict = cache_SCDynamicStoreCopyValue(store, if_key);
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
	CFIndex			i;
	CFIndex			n;
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

	n = order ? CFArrayGetCount(order) : 0;
	for (i = 0; i < n; i++) {
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
		if (j != kCFNotFound) {
			CFArrayRemoveValueAtIndex(defined, j);
		}

		CFRelease(key);
	}

	n = CFArrayGetCount(defined);
	for (i = 0; i < n; i++) {
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


static void
encodeName(CFStringRef			name,
	   CFStringEncoding		encoding,
	   CFMutableDictionaryRef	startup,
	   CFMutableDictionaryRef	globals)
{
	CFDataRef		bytes;
	CFMutableStringRef	encodedName = NULL;
	CFIndex			len;

	if (!isA_CFString(name)) {
		return;
	}

	if (encoding == kCFStringEncodingASCII) {
		encodedName = (CFMutableStringRef)CFStringCreateCopy(NULL, name);
		goto done;
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

		/*
		 * check if the MacRoman string can be represented as ASCII
		 */
		if (encoding == kCFStringEncodingMacRoman) {
			CFDataRef       ascii;

			ascii = CFStringCreateExternalRepresentation(NULL,
								     name,
								     kCFStringEncodingASCII,
								     0);
			if (ascii) {
				CFRelease(ascii);
				CFRelease(bytes);
				encodedName = (CFMutableStringRef)CFStringCreateCopy(NULL, name);
				goto done;
			}
		}

		encodedName = CFStringCreateMutable(NULL, 0);

		len  = CFDataGetLength(bytes);
		byte = (unsigned char *)CFDataGetBytePtr(bytes);
		for (i = 0; i < len; i++, byte++) {
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

    done :

	if (encodedName) {
		if (startup) {
			/* update "startup" dictionary */
			CFDictionaryAddValue(startup, CFSTR("APPLETALK_HOSTNAME"), encodedName);
		}

		if (globals) {
			CFNumberRef	num;

			/* update "global" dictionary */
			num = CFNumberCreate(NULL, kCFNumberIntType, &encoding);
			CFDictionaryAddValue(globals, kSCPropNetAppleTalkComputerName,         name);
			CFDictionaryAddValue(globals, kSCPropNetAppleTalkComputerNameEncoding, num);
			CFRelease(num);
		}

		CFRelease(encodedName);
	}

	return;
}


static boolean_t
updateConfiguration(int *newState)
{
	boolean_t		changed			= FALSE;
	CFStringRef		computerName;
	CFStringEncoding	computerNameEncoding;
	CFArrayRef		configuredServices	= NULL;
	CFDictionaryRef		dict;
	CFIndex			i;
	CFIndex			ifCount			= 0;
	CFMutableArrayRef	info			= NULL;
	CFArrayRef		interfaces		= NULL;
	CFStringRef		key;
	CFArrayRef		keys;
	CFIndex			n;
	CFMutableArrayRef	newConfigFile;
	CFMutableDictionaryRef	newDefaults;
	CFMutableDictionaryRef	newDict;
	CFMutableDictionaryRef	newGlobals;
	CFMutableDictionaryRef	newGlobalsX;			/* newGlobals without ServiceID */
	CFMutableDictionaryRef	newStartup;
	CFMutableDictionaryRef	newZones;
	CFNumberRef		num;
	CFMutableDictionaryRef	curGlobalsX;			/* curGlobals without ServiceID */
	CFStringRef		pattern;
	boolean_t		postGlobals		= FALSE;
	CFStringRef		primaryPort		= NULL;	/* primary interface */
	CFStringRef		primaryZone		= NULL;
	CFArrayRef		serviceOrder		= NULL;
	CFDictionaryRef		setGlobals		= NULL;

	cache_open();

	/*
	 * establish the "new" AppleTalk configuration
	 */
	*newState     = curState;
	newConfigFile = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	newGlobals    = CFDictionaryCreateMutable(NULL,
						  0,
						  &kCFTypeDictionaryKeyCallBacks,
						  &kCFTypeDictionaryValueCallBacks);
	newDefaults   = CFDictionaryCreateMutable(NULL,
						  0,
						  &kCFTypeDictionaryKeyCallBacks,
						  &kCFTypeDictionaryValueCallBacks);
	newStartup    = CFDictionaryCreateMutable(NULL,
						  0,
						  &kCFTypeDictionaryKeyCallBacks,
						  &kCFTypeDictionaryValueCallBacks);
	newZones      = CFDictionaryCreateMutable(NULL,
						  0,
						  &kCFTypeDictionaryKeyCallBacks,
						  &kCFTypeDictionaryValueCallBacks);

	/* initialize overall state */
	CFDictionarySetValue(newStartup, CFSTR("APPLETALK"), CFSTR("-NO-"));

	/*
	 * get the global settings (ServiceOrder, ComputerName, ...)
	 */
	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							 kSCDynamicStoreDomainSetup,
							 kSCEntNetAppleTalk);
	setGlobals = cache_SCDynamicStoreCopyValue(store, key);
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
		dict = cache_SCDynamicStoreCopyValue(store, key);
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
	 * get the list of ALL configured services
	 */
	configuredServices = entity_all(store, kSCEntNetAppleTalk, serviceOrder);
	if (configuredServices) {
		ifCount = CFArrayGetCount(configuredServices);
	}

	if (serviceOrder)	CFRelease(serviceOrder);

	/*
	 * get the list of ALL active interfaces
	 */
	key  = SCDynamicStoreKeyCreateNetworkInterface(NULL, kSCDynamicStoreDomainState);
	dict = cache_SCDynamicStoreCopyValue(store, key);
	CFRelease(key);
	if (dict) {
		if (isA_CFDictionary(dict)) {
			interfaces = CFDictionaryGetValue(dict,
							  kSCDynamicStorePropNetInterfaces);
			interfaces = isA_CFArray(interfaces);
			if (interfaces) {
				CFRetain(interfaces);
			}
		}
		CFRelease(dict);
	}

	/*
	 * get the list of previously configured services
	 */
	pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							      kSCDynamicStoreDomainState,
							      kSCCompAnyRegex,
							      kSCEntNetAppleTalk);
	keys = SCDynamicStoreCopyKeyList(store, pattern);
	CFRelease(pattern);
	if (keys) {
		info = CFArrayCreateMutableCopy(NULL, 0, keys);
		CFRelease(keys);
	}

	/*
	 * iterate over each configured service to establish the new
	 * configuration.
	 */
	for (i = 0; i < ifCount; i++) {
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
		service = CFArrayGetValueAtIndex(configuredServices, i);

		/* get interface name */
		ifName  = CFDictionaryGetValue(service, kSCPropNetInterfaceDeviceName);

		/* check inteface availability */
		if (!interfaces ||
		    !CFArrayContainsValue(interfaces, CFRangeMake(0, CFArrayGetCount(interfaces)), ifName)) {
			/* if interface not available */
			goto nextIF;
		}

		/* check interface link status */
		key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
								    kSCDynamicStoreDomainState,
								    ifName,
								    kSCEntNetLink);
		dict = cache_SCDynamicStoreCopyValue(store, key);
		CFRelease(key);
		if (dict) {
			Boolean	linkStatus	= TRUE;  /* assume the link is "up" */
			Boolean	ifDetaching	= FALSE; /* assume link is not detaching */

			/* the link key for this interface is available */
			if (isA_CFDictionary(dict)) {
				CFBooleanRef	bVal;

				bVal = CFDictionaryGetValue(dict, kSCPropNetLinkActive);
				if (isA_CFBoolean(bVal)) {
					linkStatus = CFBooleanGetValue(bVal);
				}

				/* check if interface is detaching - value
				   doesn't really matter, only that it exists */
				ifDetaching = CFDictionaryContainsKey(dict, kSCPropNetLinkDetaching);
			}
			CFRelease(dict);

			if (!linkStatus || ifDetaching) {
				/* if link status down or the interface is detaching */
				goto nextIF;
			}
		}

		/*
		 * Determine configuration method for this service
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
		 * the first service to be defined will always be "primary"
		 */
		if (CFArrayGetCount(newConfigFile) == 0) {
			CFDictionaryRef	active;

			CFDictionarySetValue(newGlobals,
					     kSCDynamicStorePropNetPrimaryService,
					     CFDictionaryGetValue(service, CFSTR("ServiceID")));
			CFDictionarySetValue(newGlobals,
					     kSCDynamicStorePropNetPrimaryInterface,
					     ifName);

			/* and check if AT newtorking is active on the primary interface */
			key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
									    kSCDynamicStoreDomainState,
									    ifName,
									    kSCEntNetAppleTalk);
			active = cache_SCDynamicStoreCopyValue(store, key);
			CFRelease(key);
			if (active) {
				if (isA_CFDictionary(active)) {
					postGlobals = TRUE;
				}
				CFRelease(active);
			}
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
			for (j = 0; j < zCount; j++) {
				CFStringRef		zone;
				CFArrayRef		ifList;
				CFMutableArrayRef	newIFList;

				zone = CFArrayGetValueAtIndex(zoneList, j);
				if (!isA_CFString(zone)) {
					continue;
				}

				if (CFDictionaryGetValueIfPresent(newZones, zone, (const void **)&ifList)) {
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
						  (const void **)&num) &&
		    isA_CFNumber(num)) {
			CFNumberGetValue(num, kCFNumberIntType, &computerNameEncoding);
		} else {
			computerNameEncoding = CFStringGetSystemEncoding();
		}
		encodeName(computerName, computerNameEncoding, newStartup, newGlobals);

		/*
		 * declare the first configured AppleTalk service / interface
		 * as the "home port".
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
						   (const void **)&defaultZone) == TRUE)) {
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
				CFDictionarySetValue(newStartup, CFSTR("APPLETALK"), ifName);
				break;
			case 2:
				/* second AppleTalk interface */
				if (!CFEqual(CFDictionaryGetValue(newStartup, CFSTR("APPLETALK")),
					     CFSTR("-ROUTER-"))) {
					/*
					 * if not routing (yet), configure as multi-home
					 */
					CFDictionarySetValue(newStartup, CFSTR("APPLETALK"), CFSTR("-MULTIHOME-"));
				}
				break;
		}

		if (CFEqual(configMethod, kSCValNetAppleTalkConfigMethodRouter) ||
		    CFEqual(configMethod, kSCValNetAppleTalkConfigMethodSeedRouter)) {
			/* if not a simple node, enable routing */
			CFDictionarySetValue(newStartup, CFSTR("APPLETALK"), CFSTR("-ROUTER-"));
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
		cache_SCDynamicStoreSetValue(store, key, newDict);
		CFRelease(newDict);
		if (info) {
			j = CFArrayGetFirstIndexOfValue(info,
							CFRangeMake(0, CFArrayGetCount(info)),
							key);
			if (j != kCFNotFound) {
				CFArrayRemoveValueAtIndex(info, j);
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
	if (!CFDictionaryContainsKey(newStartup, CFSTR("APPLETALK_HOSTNAME")) &&
	    (setGlobals != NULL)) {
		computerName = CFDictionaryGetValue(setGlobals,
						    kSCPropNetAppleTalkComputerName);
		if (CFDictionaryGetValueIfPresent(setGlobals,
						  kSCPropNetAppleTalkComputerNameEncoding,
						  (const void **)&num) &&
		    isA_CFNumber(num)) {
			CFNumberGetValue(num, kCFNumberIntType, &computerNameEncoding);
		} else {
			computerNameEncoding = CFStringGetSystemEncoding();
		}
		encodeName(computerName, computerNameEncoding, newStartup, newGlobals);
	}
	if (!CFDictionaryContainsKey(newStartup, CFSTR("APPLETALK_HOSTNAME"))) {
		computerName = SCDynamicStoreCopyComputerName(store, &computerNameEncoding);
		if (computerName) {
			encodeName(computerName, computerNameEncoding, newStartup, newGlobals);
			CFRelease(computerName);
		}
	}
	if (!CFDictionaryContainsKey(newStartup, CFSTR("APPLETALK_HOSTNAME"))) {
		struct utsname	name;

		if (uname(&name) == 0) {
			computerName = CFStringCreateWithCString(NULL, name.nodename, kCFStringEncodingASCII);
			if (computerName) {
				encodeName(computerName, kCFStringEncodingASCII, NULL, newGlobals);
				CFRelease(computerName);
			}
		}
	}

	/* compare the previous and current configurations */

	curGlobalsX = CFDictionaryCreateMutableCopy(NULL, 0, curGlobals);
	CFDictionaryRemoveValue(curGlobalsX, kSCDynamicStorePropNetPrimaryService);

	newGlobalsX = CFDictionaryCreateMutableCopy(NULL, 0, newGlobals);
	CFDictionaryRemoveValue(newGlobalsX, kSCDynamicStorePropNetPrimaryService);

	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							 kSCDynamicStoreDomainState,
							 kSCEntNetAppleTalk);

	if (CFEqual(curGlobalsX   , newGlobalsX   ) &&
	    CFEqual(curConfigFile , newConfigFile) &&
	    CFEqual(curDefaults   , newDefaults  ) &&
	    CFEqual(curStartup    , newStartup   )
	    ) {
		/*
		 * the configuration has not changed.
		 */

		if (postGlobals) {
			/*
			 * the requested configuration hasn't changed but we
			 * now need to tell everyone that AppleTalk is active.
			 */
			if (!SCDynamicStoreSetValue(store, key, newGlobals)) {
				SCLog(TRUE,
				      LOG_ERR,
				      CFSTR("SCDynamicStoreSetValue() failed: %s"),
				      SCErrorString(SCError()));
			}
		}

		CFRelease(newGlobals);
		CFRelease(newConfigFile);
		CFRelease(newDefaults);
		CFRelease(newStartup);
	} else if (CFArrayGetCount(newConfigFile) <= 1) {
		/*
		 * the configuration has changed but there are no
		 * longer any interfaces configured for AppleTalk
		 * networking.
		 */

		/*
		 * remove the global (State:/Network/Global/AppleTalk) key.
		 *
		 * Note: it will be restored later after AT networking has
		 *       been activated.
		 */

		/* remove the (/etc/appletalk.cfg) configuration file */
		(void)unlink(AT_CFG_FILE);

		/*
		 * update the per-service (and global) state
		 */
		cache_SCDynamicStoreRemoveValue(store, key);	// remove State:/Network/Global/AppleTalk
		n = CFArrayGetCount(info);
		for (i = 0; i < n; i++) {
			CFStringRef	xKey	= CFArrayGetValueAtIndex(info, i);

			cache_SCDynamicStoreRemoveValue(store, xKey);
		}
		cache_write(store);

		/* flag this as a new configuration */
		*newState = -(abs(curState) + 1);
		changed = TRUE;
	} else {
		/*
		 * the configuration has changed.
		 */

		/* update the (/etc/appletalk.cfg) configuration file */
		configWrite(AT_CFG_FILE, newConfigFile);

		/*
		 * update the per-service (and global) state
		 *
		 * Note: if present, we remove any existing global state key and allow it
		 *       to be restored after the stack has been re-started.
		 */
		CFDictionaryApplyFunction(newDefaults, updateDefaults, NULL);
		cache_SCDynamicStoreRemoveValue(store, key);	// remove State:/Network/Global/AppleTalk
		n = CFArrayGetCount(info);
		for (i = 0; i < n; i++) {
			CFStringRef	xKey	= CFArrayGetValueAtIndex(info, i);

			cache_SCDynamicStoreRemoveValue(store, xKey);
		}
		cache_write(store);

		/* flag this as a new configuration */
		*newState = abs(curState) + 1;
		changed = TRUE;
	}

	CFRelease(curGlobalsX);
	CFRelease(newGlobalsX);
	CFRelease(key);

	if (changed) {
		CFRelease(curGlobals);
		curGlobals    = newGlobals;
		CFRelease(curConfigFile);
		curConfigFile = newConfigFile;
		CFRelease(curDefaults);
		curDefaults   = newDefaults;
		CFRelease(curStartup);
		curStartup    = newStartup;
	}

	if (info)		CFRelease(info);
	if (interfaces)		CFRelease(interfaces);
	if (configuredServices)	CFRelease(configuredServices);
	if (setGlobals)		CFRelease(setGlobals);

	cache_close();

	return changed;
}


#include <sysexits.h>
#define AT_CMD_SUCCESS		EX_OK   /* success */
#define AT_CMD_ALREADY_RUNNING	EX__MAX + 10
#define AT_CMD_NOT_RUNNING	EX__MAX + 11


static int
stackState()
{
	int		ret;
	int		sock;
	at_state_t	state;

	sock = socket(AF_APPLETALK, SOCK_RAW, 0);
	ret  = ioctl(sock, AIOCGETSTATE, (caddr_t)&state);
	(void)close(sock);
	if (ret == -1) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("ioctl(AIOCGETSTATE) failed: %s"), strerror(errno));
		return FALSE;
	}

	if (state.flags & AT_ST_STARTED) {
		return abs(curState);
	} else {
		return -(abs(curState));
	}
}


static pid_t	execCommand	= 0;
static int	execRetry;


static void
stopComplete(pid_t pid, int status, struct rusage *rusage, void *context)
{
	execCommand = 0;

	if (WIFEXITED(status)) {
		switch (WEXITSTATUS(status)) {
			case AT_CMD_SUCCESS :
			case AT_CMD_NOT_RUNNING :
				SCLog(TRUE, LOG_NOTICE, CFSTR("AppleTalk shutdown complete"));
				if (curState > 0) {
					// the stack is down but we really want it up
					startAppleTalk(NULL, (void *)curState);
				}
				return;
			default :
				break;
		}
	}

	SCLog(TRUE,
	      (execRetry > 1) ? LOG_NOTICE : LOG_ERR,
	      CFSTR("AppleTalk shutdown failed, status = %d%s"),
	      WEXITSTATUS(status),
	      (execRetry > 1) ? " (retrying)" : "");

	// shutdown failed, retry
	if (--execRetry > 0) {
		CFRunLoopTimerContext	timerContext	= { 0, (void *)curState, NULL, NULL, NULL };
		CFRunLoopTimerRef	timer;

		timer = CFRunLoopTimerCreate(NULL,
					     CFAbsoluteTimeGetCurrent() + RETRY_DELAY,
					     0.0,
					     0,
					     0,
					     stopAppleTalk,
					     &timerContext);
		CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);
		CFRelease(timer);
		return;
	} else {
		// we weren't able to stop
		curState = stackState();
	}

	return;
}


static void
stopAppleTalk(CFRunLoopTimerRef timer, void *info)
{
	char	*argv[]	= { "appletalk",
			    "-d",
			    NULL };

	if (execCommand == 0) {
		SCLog(TRUE, LOG_NOTICE, CFSTR("AppleTalk shutdown"));
	} else {
		SCLog(TRUE, LOG_NOTICE, CFSTR("AppleTalk shutdown skipped, transition in progress"));
		return;
	}

	execCommand = _SCDPluginExecCommand(stopComplete,		// callback
					    info,			// context
					    0,				// uid
					    0,				// gid
					    "/usr/sbin/appletalk",	// path
					    argv);			// argv

	if (!timer) {
		execRetry = RETRY_LIMIT;	// initialize retry count
	}

	return;
}


static void
startComplete(pid_t pid, int status, struct rusage *rusage, void *context)
{
	execCommand = 0;

	if (WIFEXITED(status)) {
		switch (WEXITSTATUS(status)) {
			case AT_CMD_SUCCESS :
				SCLog(TRUE, LOG_NOTICE, CFSTR("AppleTalk startup complete"));
				if ((curState < 0) || (curState > (int)context)) {
					// the stack is now up but we really want it down
					stopAppleTalk(NULL, (void *)curState);
				}
				return;
			case AT_CMD_ALREADY_RUNNING :
				// the stack is already up but we're not sure
				// if the configuration is correct, restart
				SCLog(TRUE, LOG_NOTICE, CFSTR("AppleTalk already running, restarting"));
				stopAppleTalk(NULL, (void *)curState);
				return;
			default :
				break;
		}
	}

	SCLog(TRUE,
	      (execRetry > 1) ? LOG_NOTICE : LOG_ERR,
	      CFSTR("AppleTalk startup failed, status = %d%s"),
	      WEXITSTATUS(status),
	      (execRetry > 1) ? " (retrying)" : "");

	// startup failed, retry
	if (--execRetry > 0) {
		CFRunLoopTimerContext	timerContext	= { 0, (void *)curState, NULL, NULL, NULL };
		CFRunLoopTimerRef	timer;

		timer = CFRunLoopTimerCreate(NULL,
					     CFAbsoluteTimeGetCurrent() + RETRY_DELAY,
					     0.0,
					     0,
					     0,
					     startAppleTalk,
					     &timerContext);
		CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);
		CFRelease(timer);
		return;
	} else {
		// we weren't able to start
		curState = stackState();
	}

	return;
}


static void
startAppleTalk(CFRunLoopTimerRef timer, void *info)
{
	int		argc		= 0;
	char		*argv[8];
	char		*computerName	= NULL;
	char		*interface	= NULL;
	CFStringRef	mode		= CFDictionaryGetValue(curStartup, CFSTR("APPLETALK"));
	CFStringRef	name		= CFDictionaryGetValue(curStartup, CFSTR("APPLETALK_HOSTNAME"));

	if (execCommand == 0) {
		SCLog(TRUE, LOG_NOTICE, CFSTR("AppleTalk startup"));
	} else {
		SCLog(TRUE, LOG_NOTICE, CFSTR("AppleTalk startup skipped, transition in progress"));
		return;
	}

	if (!mode) {
		// Huh?
		return;
	}

	// set command name
	argv[argc++] = "appletalk";

	// set hostname
	if (name) {
		computerName = _SC_cfstring_to_cstring(name, NULL, 0, kCFStringEncodingASCII);
		if (computerName) {
			argv[argc++] = "-C";
			argv[argc++] = computerName;
		} else {
			// could not convert name
			goto done;
		}
	}

	// set mode
	if (CFEqual(mode, CFSTR("-ROUTER-"))) {
		argv[argc++] = "-r";
	} else if (CFEqual(mode, CFSTR("-MULTIHOME-"))) {
		argv[argc++] = "-x";
	} else {
		interface = _SC_cfstring_to_cstring(mode, NULL, 0, kCFStringEncodingASCII);
		if (interface) {
			argv[argc++] = "-u";
			argv[argc++] = interface;
		} else {
			// could not convert interface
			goto done;
		}
	}

	// set non-interactive
	argv[argc++] = "-q";

	// close argument list
	argv[argc++] = NULL;

	execCommand = _SCDPluginExecCommand(startComplete,		// callback
					    info,			// context
					    0,				// uid
					    0,				// gid
					    "/usr/sbin/appletalk",	// path
					    argv);			// argv

	if (!timer) {
		execRetry = RETRY_LIMIT;	// initialize retry count
	}

    done :

	if (computerName)	CFAllocatorDeallocate(NULL, computerName);
	if (interface)		CFAllocatorDeallocate(NULL, interface);

	return;
}


static void
atConfigChangedCallback(SCDynamicStoreRef store, CFArrayRef changedKeys, void *arg)
{
	boolean_t	configChanged;
	int		newState;

	configChanged = updateConfiguration(&newState);

	if (configChanged && (execCommand == 0)) {
		// if the configuration has changed and we're not already transitioning
		if (newState > 0) {
			if (curState > 0) {
				// already running, restart [with new configuration]
				stopAppleTalk(NULL, (void *)newState);
			} else {
				startAppleTalk(NULL, (void *)newState);
			}
		} else {
			if (curState > 0) {
				stopAppleTalk(NULL, (void *)newState);
			}
		}
	}

	curState = newState;

	return;
}


void
stop_ATconfig(CFRunLoopSourceRef stopRls)
{
	// cleanup

	if (storeRls != NULL) {
		CFRunLoopSourceInvalidate(storeRls);
		CFRelease(storeRls);
		storeRls = NULL;
	}

	if (store != NULL) {
		CFRelease(store);
		store = NULL;
		CFRelease(curGlobals);
		CFRelease(curConfigFile);
		CFRelease(curDefaults);
		CFRelease(curStartup);
	}

	CFRunLoopSourceSignal(stopRls);
	return;
}


void
load_ATconfig(CFBundleRef bundle, Boolean bundleVerbose)
{
	CFStringRef		key;
	CFMutableArrayRef	keys		= NULL;
	CFStringRef		pattern;
	CFMutableArrayRef	patterns	= NULL;

	if (bundleVerbose) {
		_verbose = TRUE;
	}

	SCLog(_verbose, LOG_DEBUG, CFSTR("load() called"));
	SCLog(_verbose, LOG_DEBUG, CFSTR("  bundle ID = %@"), CFBundleGetIdentifier(bundle));

	/* initialize a few globals */

	curGlobals    = CFDictionaryCreateMutable(NULL,
						  0,
						  &kCFTypeDictionaryKeyCallBacks,
						  &kCFTypeDictionaryValueCallBacks);
	curConfigFile = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	curDefaults   = CFDictionaryCreateMutable(NULL,
						  0,
						  &kCFTypeDictionaryKeyCallBacks,
						  &kCFTypeDictionaryValueCallBacks);
	curStartup    = CFDictionaryCreateMutable(NULL,
						  0,
						  &kCFTypeDictionaryKeyCallBacks,
						  &kCFTypeDictionaryValueCallBacks);

	/* open a "configd" store to allow cache updates */
	store = SCDynamicStoreCreate(NULL,
				     CFSTR("AppleTalk Configuraton plug-in"),
				     atConfigChangedCallback,
				     NULL);
	if (store == NULL) {
		SCLog(TRUE, LOG_ERR, CFSTR("SCDynamicStoreCreate() failed: %s"), SCErrorString(SCError()));
		goto error;
	}

	/* establish notification keys and patterns */

	keys     = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	/* ...watch for (global) AppleTalk configuration changes */
	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							 kSCDynamicStoreDomainSetup,
							 kSCEntNetAppleTalk);
	CFArrayAppendValue(keys, key);
	CFRelease(key);

	/* ...watch for (per-service) AppleTalk configuration changes */
	pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							      kSCDynamicStoreDomainSetup,
							      kSCCompAnyRegex,
							      kSCEntNetAppleTalk);
	CFArrayAppendValue(patterns, pattern);
	CFRelease(pattern);

	/* ...watch for network interface link status changes */
	pattern = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
								kSCDynamicStoreDomainState,
								kSCCompAnyRegex,
								kSCEntNetLink);
	CFArrayAppendValue(patterns, pattern);
	CFRelease(pattern);

	/* ...watch for (per-interface) AppleTalk configuration changes */
	pattern = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
								kSCDynamicStoreDomainState,
								kSCCompAnyRegex,
								kSCEntNetAppleTalk);
	CFArrayAppendValue(patterns, pattern);
	CFRelease(pattern);

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
	CFRelease(keys);
	CFRelease(patterns);

	storeRls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
	if (!storeRls) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("SCDynamicStoreCreateRunLoopSource() failed: %s"),
		      SCErrorString(SCError()));
		goto error;
	}
	CFRunLoopAddSource(CFRunLoopGetCurrent(), storeRls, kCFRunLoopDefaultMode);

	return;

    error :

	if (curGlobals)		CFRelease(curGlobals);
	if (curConfigFile)	CFRelease(curConfigFile);
	if (curDefaults)	CFRelease(curDefaults);
	if (curStartup)		CFRelease(curStartup);
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

	load_ATconfig(CFBundleGetMainBundle(), (argc > 1) ? TRUE : FALSE);
	CFRunLoopRun();
	/* not reached */
	exit(0);
	return 0;
}
#endif
