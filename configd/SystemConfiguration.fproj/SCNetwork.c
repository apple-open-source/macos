/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
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
 * June 10, 2001		Allan Nathanson <ajn@apple.com>
 * - updated to use service-based "State:" information
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * January 30, 2001		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>

#include "ppp.h"

static int
inet_atonCF(CFStringRef cfStr, struct in_addr *addr)
{
	char	cStr[sizeof("255.255.255.255")];

	if (!CFStringGetCString(cfStr, cStr, sizeof(cStr), kCFStringEncodingMacRoman)) {
		return 0;
	}

	return inet_aton(cStr, addr);
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


typedef struct {
	CFMutableDictionaryRef	aDict;		/* active services */
	CFStringRef		aPrefix;	/* prefix for active services */
	CFMutableDictionaryRef	cDict;		/* configured services */
	CFStringRef		cPrefix;	/* prefix for configured services */
	CFMutableDictionaryRef	iDict;		/* active interfaces */
	CFStringRef		iPrefix;	/* prefix for active interfaces */
	CFMutableArrayRef	order;		/* service order */
} initContext, *initContextRef;


static void
collectInfo(const void *key, const void *value, void *context)
{
	initContextRef		info		= (initContextRef)context;
	CFStringRef		interface;
	CFStringRef		interfaceKey;
	CFStringRef		service;
	CFStringRef		serviceKey;

	if (!isA_CFString(key) || !isA_CFDictionary(value)) {
		return;
	}

	service = parse_component((CFStringRef)key, info->cPrefix);
	if (service) {
		serviceKey = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
									 kSCDynamicStoreDomainSetup,
									 service,
									 kSCEntNetIPv4);
		if (CFEqual((CFStringRef)key, serviceKey)) {
			CFMutableDictionaryRef	dict;

			dict = CFDictionaryCreateMutableCopy(NULL, 0, (CFDictionaryRef)value);
			CFDictionaryAddValue(info->cDict, service, dict);
			CFRelease(dict);
		}
		CFRelease(serviceKey);

		if (!CFArrayContainsValue(info->order, CFRangeMake(0, CFArrayGetCount(info->order)), service)) {
			CFArrayAppendValue(info->order, service);
		}

		CFRelease(service);
		return;
	}

	service = parse_component((CFStringRef)key, info->aPrefix);
	if (service) {
		serviceKey = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
									 kSCDynamicStoreDomainState,
									 service,
									 kSCEntNetIPv4);
		if (CFEqual((CFStringRef)key, serviceKey)) {
			CFMutableDictionaryRef	dict;

			dict = CFDictionaryCreateMutableCopy(NULL, 0, (CFDictionaryRef)value);
			CFDictionaryAddValue(info->aDict, service, dict);
			CFRelease(dict);
		}
		CFRelease(serviceKey);

		if (!CFArrayContainsValue(info->order, CFRangeMake(0, CFArrayGetCount(info->order)), service)) {
			CFArrayAppendValue(info->order, service);
		}

		CFRelease(service);
		return;
	}

	interface = parse_component((CFStringRef)key, info->iPrefix);
	if (interface) {
		interfaceKey = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
									     kSCDynamicStoreDomainState,
									     interface,
									     kSCEntNetIPv4);
		if (CFEqual((CFStringRef)key, interfaceKey)) {
			CFMutableDictionaryRef	dict;

			dict = CFDictionaryCreateMutableCopy(NULL, 0, (CFDictionaryRef)value);
			CFDictionaryAddValue(info->iDict, interface, dict);
			CFRelease(dict);
		}
		CFRelease(interfaceKey);
		CFRelease(interface);
		return;
	}

	return;
}


static void
collectExtraInfo(const void *key, const void *value, void *context)
{
	CFStringRef		interfaceKey;
	initContextRef		info	= (initContextRef)context;
	CFMutableDictionaryRef	dict;
	Boolean			match;
	CFStringRef		pppKey;
	CFStringRef		service;

	if (!isA_CFString(key) || !isA_CFDictionary(value)) {
		return;
	}

	service = parse_component((CFStringRef)key, info->cPrefix);
	if (!service) {
		/* this key/value pair contains supplemental information */
		return;
	}

	dict = (CFMutableDictionaryRef)CFDictionaryGetValue(info->cDict, service);
	if (!dict) {
		/*  we don't have any IPv4 information for this service */
		goto done;
	}

	interfaceKey = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								   kSCDynamicStoreDomainSetup,
								   service,
								   kSCEntNetInterface);
	match = CFEqual((CFStringRef)key, interfaceKey);
	CFRelease(interfaceKey);
	if (match) {
		CFStringRef	interface;

		interface = CFDictionaryGetValue((CFDictionaryRef)value,
						 kSCPropNetInterfaceType);
		if (isA_CFString(interface)) {
			/* if "InterfaceType" available */
			CFDictionaryAddValue(dict, kSCPropNetInterfaceType, interface);
			CFDictionarySetValue(info->cDict, service, dict);
		}
		goto done;
	}

	pppKey = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							     kSCDynamicStoreDomainSetup,
							     service,
							     kSCEntNetPPP);
	match = CFEqual((CFStringRef)key, pppKey);
	CFRelease(pppKey);
	if (match) {
		CFNumberRef	dialOnDemand;

		dialOnDemand = CFDictionaryGetValue((CFDictionaryRef)value,
						    kSCPropNetPPPDialOnDemand);
		if (isA_CFNumber(dialOnDemand)) {
			/* if "DialOnDemand" information not available */
			CFDictionaryAddValue(dict, kSCPropNetPPPDialOnDemand, dialOnDemand);
			CFDictionarySetValue(info->cDict, service, dict);
		}
		goto done;
	}

    done :

	CFRelease(service);
	return;
}


static void
removeKnownAddresses(const void *key, const void *value, void *context)
{
	CFMutableDictionaryRef	ifDict;
	CFStringRef		ifName;
	CFMutableDictionaryRef	interfaces	= (CFMutableDictionaryRef)context;
	CFMutableDictionaryRef	serviceDict	= (CFMutableDictionaryRef)value;
	Boolean			updated		= FALSE;

	CFIndex			i;
	CFArrayRef		iAddrs;
	CFArrayRef		iDests;
	CFArrayRef		iMasks;
	CFIndex			n;
	CFMutableArrayRef	nAddrs		= NULL;
	CFMutableArrayRef	nDests		= NULL;
	CFMutableArrayRef	nMasks		= NULL;
	CFIndex			s;
	CFArrayRef		sAddrs;
	CFArrayRef		sDests;
	CFArrayRef		sMasks;

	ifName = CFDictionaryGetValue(serviceDict, kSCPropInterfaceName);
	if (!ifName) {
		/* if no "InterfaceName" for this service */
		return;
	}

	ifDict = (CFMutableDictionaryRef)CFDictionaryGetValue(interfaces, ifName);
	if (!ifDict) {
		/* if the indicated interface is not active */
		return;
	}

	sAddrs = isA_CFArray(CFDictionaryGetValue(serviceDict,
						  kSCPropNetIPv4Addresses));
	sDests = isA_CFArray(CFDictionaryGetValue(serviceDict,
						  kSCPropNetIPv4DestAddresses));
	sMasks = isA_CFArray(CFDictionaryGetValue(serviceDict,
						  kSCPropNetIPv4SubnetMasks));

	if (!sAddrs || ((n = CFArrayGetCount(sAddrs)) == 0)) {
		/* if no addresses */
		return;
	}

	if (((sMasks == NULL) && (sDests == NULL)) ||
	    ((sMasks != NULL) && (sDests != NULL))) {
		/*
		 * sorry, we expect to have "SubnetMasks" or
		 * "DestAddresses" (not both).
		 */
		return;
	}

	if (sMasks && (n != CFArrayGetCount(sMasks))) {
		/* if we don't like the "SubnetMasks" */
		return;
	}

	if (sDests &&  (n != CFArrayGetCount(sDests))) {
		/* if we don't like the "DestAddresses" */
		return;
	}

	iAddrs = isA_CFArray(CFDictionaryGetValue(ifDict,
						  kSCPropNetIPv4Addresses));
	iDests = isA_CFArray(CFDictionaryGetValue(ifDict,
						  kSCPropNetIPv4DestAddresses));
	iMasks = isA_CFArray(CFDictionaryGetValue(ifDict,
						  kSCPropNetIPv4SubnetMasks));

	if (((iMasks == NULL) && (iDests == NULL)) ||
	    ((iMasks != NULL) && (iDests != NULL))) {
		/*
		 * sorry, we expect to have "SubnetMasks" or
		 * "DestAddresses" (not both).
		 */
		return;
	}

	if (!iAddrs || ((i = CFArrayGetCount(iAddrs)) == 0)) {
		/* if no addresses */
		return;
	}

	if (iMasks && (i != CFArrayGetCount(iMasks))) {
		/* if we don't like the "SubnetMasks" */
		return;
	}

	if (iDests && (i != CFArrayGetCount(iDests))) {
		/* if we don't like the "DestAddresses" */
		return;
	}

	if (((sMasks == NULL) && (iMasks != NULL)) ||
	    ((sDests == NULL) && (iDests != NULL))) {
		/* if our addressing schemes are in conflict */
		return;
	}

	nAddrs = CFArrayCreateMutableCopy(NULL, 0, iAddrs);
	if (iMasks) nMasks = CFArrayCreateMutableCopy(NULL, 0, iMasks);
	if (iDests) nDests = CFArrayCreateMutableCopy(NULL, 0, iDests);
	for (s=0; s<n; s++) {
		i = CFArrayGetCount(nAddrs);
		while (--i >= 0) {
			if (sMasks &&
			    CFEqual(CFArrayGetValueAtIndex(sAddrs, s),
				    CFArrayGetValueAtIndex(nAddrs, i)) &&
			    CFEqual(CFArrayGetValueAtIndex(sMasks, s),
				    CFArrayGetValueAtIndex(nMasks, i))
			    ) {
				/* we have a match */
				CFArrayRemoveValueAtIndex(nAddrs, i);
				CFArrayRemoveValueAtIndex(nMasks, i);
				updated = TRUE;
			} else if (sDests &&
				   CFEqual(CFArrayGetValueAtIndex(sAddrs, s),
					   CFArrayGetValueAtIndex(nAddrs, i)) &&
				   CFEqual(CFArrayGetValueAtIndex(sDests, s),
					   CFArrayGetValueAtIndex(nDests, i))
				   ) {
				/* we have a match */
				CFArrayRemoveValueAtIndex(nAddrs, i);
				CFArrayRemoveValueAtIndex(nDests, i);
				updated = TRUE;
			}
		}
	}

	if (updated) {
		if (nAddrs) {
			CFDictionarySetValue(ifDict,
					     kSCPropNetIPv4Addresses,
					     nAddrs);
		}
		if (nMasks) {
			CFDictionarySetValue(ifDict,
					     kSCPropNetIPv4SubnetMasks,
					     nMasks);
		} else {
			CFDictionarySetValue(ifDict,
					     kSCPropNetIPv4DestAddresses,
					     nDests);
		}
		CFDictionarySetValue(interfaces, ifName, ifDict);
	}
	CFRelease(nAddrs);
	if (nMasks) CFRelease(nMasks);
	if (nDests) CFRelease(nDests);

	return;
}


static void
addUnknownService(const void *key, const void *value, void *context)
{
	CFArrayRef		addrs;
	CFMutableDictionaryRef	ifDict		= (CFMutableDictionaryRef)value;
	initContextRef		info		= (initContextRef)context;
	CFStringRef		service;
	CFUUIDRef		uuid;

	addrs = CFDictionaryGetValue(ifDict, kSCPropNetIPv4Addresses);
	if (!addrs || (CFArrayGetCount(addrs) == 0)) {
		/* if no addresses */
		return;
	}

	/* add the "InterfaceName" to the (new/fake) service dictionary */
	CFDictionaryAddValue(ifDict, kSCPropInterfaceName, (CFStringRef)key);

	/* create a (new/fake) service to hold any remaining addresses */
	uuid    = CFUUIDCreate(NULL);
	service = CFUUIDCreateString(NULL, uuid);
	CFDictionaryAddValue(info->aDict, service, ifDict);
	CFArrayAppendValue(info->order, service);
	CFRelease(service);
	CFRelease(uuid);

	return;
}


static Boolean
getAddresses(CFDictionaryRef	iDict,
	     CFIndex		*nAddrs,
	     CFArrayRef		*addrs,
	     CFArrayRef		*masks,
	     CFArrayRef		*dests)
{
	*addrs = isA_CFArray(CFDictionaryGetValue(iDict,
						  kSCPropNetIPv4Addresses));
	*masks = isA_CFArray(CFDictionaryGetValue(iDict,
						  kSCPropNetIPv4SubnetMasks));
	*dests = isA_CFArray(CFDictionaryGetValue(iDict,
						  kSCPropNetIPv4DestAddresses));

	if ((*addrs == NULL) ||
	    ((*nAddrs = CFArrayGetCount(*addrs)) == 0)) {
		/* sorry, no addresses */
		_SCErrorSet(kSCStatusReachabilityUnknown);
		return FALSE;
	}

	if (((*masks == NULL) && (*dests == NULL)) ||
	    ((*masks != NULL) && (*dests != NULL))) {
		/*
		 * sorry, we expect to have "SubnetMasks" or
		 * "DestAddresses" (not both) and the count
		 * must match the number of "Addresses".
		 */
		_SCErrorSet(kSCStatusReachabilityUnknown);
		return FALSE;
	}

	if (*masks && (*nAddrs != CFArrayGetCount(*masks))) {
		/* if we don't like the netmasks */
		_SCErrorSet(kSCStatusReachabilityUnknown);
		return FALSE;
	}

	if (*dests &&  (*nAddrs != CFArrayGetCount(*dests))) {
		/* if we don't like the destaddresses */
		_SCErrorSet(kSCStatusReachabilityUnknown);
		return FALSE;
	}

	return TRUE;
}

static Boolean
checkAddress(SCDynamicStoreRef		store,
	     const struct sockaddr	*address,
	     const int			addrlen,
	     CFDictionaryRef		config,
	     CFDictionaryRef		active,
	     CFArrayRef			serviceOrder,
	     struct in_addr		*defaultRoute,
	     SCNetworkConnectionFlags	*flags)
{
	CFIndex			aCnt;
	CFStringRef		aType		= NULL;
	CFDictionaryRef		cDict		= NULL;
	CFIndex			i;
	CFStringRef		key		= NULL;
	int			pppRef          = -1;
	int			sc_status	= kSCStatusReachabilityUnknown;
	char			*statusMessage	= NULL;

	if (!address || !flags) {
		sc_status = kSCStatusInvalidArgument;
		goto done;
	}

	*flags = 0;

	if (address->sa_family == AF_INET) {
		struct sockaddr_in	*sin = (struct sockaddr_in *)address;

		SCLog(_sc_debug, LOG_INFO, CFSTR("checkAddress(%s)"), inet_ntoa(sin->sin_addr));

		/*
		 * Check if the address is on one of the subnets
		 * associated with our active IPv4 interfaces
		 */
		aCnt = CFArrayGetCount(serviceOrder);
		for (i=0; i<aCnt; i++) {
			CFDictionaryRef		aDict;
			CFArrayRef		addrs;
			CFArrayRef		dests;
			CFIndex			j;
			CFArrayRef		masks;
			CFIndex			nAddrs	= 0;

			key   = CFArrayGetValueAtIndex(serviceOrder, i);
			aDict = CFDictionaryGetValue(active, key);

			if (!aDict ||
			    !getAddresses(aDict, &nAddrs, &addrs, &masks, &dests)) {
				/* if no addresses to check */
				continue;
			}

			for (j=0; j<nAddrs; j++) {
				struct in_addr	ifAddr;

				if (inet_atonCF(CFArrayGetValueAtIndex(addrs, j),
						&ifAddr) == 0) {
					/* if Addresses string is invalid */
					break;
				}

				if (masks) {
					struct in_addr	ifMask;

					if (inet_atonCF(CFArrayGetValueAtIndex(masks, j),
							&ifMask) == 0) {
						/* if SubnetMask string is invalid */
						break;
					}

					if ((ntohl(ifAddr.s_addr)        & ntohl(ifMask.s_addr)) ==
					    (ntohl(sin->sin_addr.s_addr) & ntohl(ifMask.s_addr))) {
						/* the requested address is on this subnet */
						statusMessage = "isReachable (my subnet)";
						*flags |= kSCNetworkFlagsReachable;
						goto checkInterface;
					}
				} else {
					struct in_addr	destAddr;

					/* check remote address */
					if (inet_atonCF(CFArrayGetValueAtIndex(dests, j),
							&destAddr) == 0) {
						/* if DestAddresses string is invalid */
						break;
					}

					/* check local address */
					if (ntohl(sin->sin_addr.s_addr) == ntohl(ifAddr.s_addr)) {
						/* the address is our side of the link */
						statusMessage = "isReachable (my local address)";
						*flags |= kSCNetworkFlagsReachable;
						goto checkInterface;
					}

					if (ntohl(sin->sin_addr.s_addr) == ntohl(destAddr.s_addr)) {
						/* the address is the other side of the link */
						statusMessage = "isReachable (my remote address)";
						*flags |= kSCNetworkFlagsReachable;
						goto checkInterface;
					}
				}
			}
		}

		/*
		 * Check if the address is accessible via the "default" route.
		 */
		for (i=0; i<aCnt; i++) {
			CFDictionaryRef		aDict;
			CFArrayRef		addrs;
			CFArrayRef		dests;
			CFIndex			j;
			CFArrayRef		masks;
			CFIndex			nAddrs	= 0;

			key   = CFArrayGetValueAtIndex(serviceOrder, i);
			aDict = CFDictionaryGetValue(active, key);

			if (!sin->sin_addr.s_addr ||
			    !defaultRoute ||
			    !aDict ||
			    !getAddresses(aDict, &nAddrs, &addrs, &masks, &dests)) {
				/* if no addresses to check */
				continue;
			}

			for (j=0; defaultRoute && j<nAddrs; j++) {
				if (masks) {
					struct in_addr	ifAddr;
					struct in_addr	ifMask;

					if (inet_atonCF(CFArrayGetValueAtIndex(addrs, j),
							&ifAddr) == 0) {
						/* if Addresses string is invalid */
						break;
					}

					if (inet_atonCF(CFArrayGetValueAtIndex(masks, j),
							&ifMask) == 0) {
						/* if SubnetMasks string is invalid */
						break;
					}

					if ((ntohl(ifAddr.s_addr)        & ntohl(ifMask.s_addr)) ==
					    (ntohl(defaultRoute->s_addr) & ntohl(ifMask.s_addr))) {
						/* the requested address is on this subnet */
						statusMessage = "isReachable via default route (my subnet)";
						*flags |= kSCNetworkFlagsReachable;
						goto checkInterface;
					}
				} else {
					struct in_addr	destAddr;

					/* check remote address */
					if (inet_atonCF(CFArrayGetValueAtIndex(dests, j),
							&destAddr) == 0) {
						/* if DestAddresses string is invalid */
						break;
					}

					if (ntohl(destAddr.s_addr) == ntohl(defaultRoute->s_addr)) {
						/* the address is the other side of the link */
						statusMessage = "isReachable via default route (my remote address)";
						*flags |= kSCNetworkFlagsReachable;
						goto checkInterface;
					}
				}
			}
		}

		/*
		 * Check the not active (but configured) IPv4 services
		 */
		for (i=0; i<aCnt; i++) {
			key = CFArrayGetValueAtIndex(serviceOrder, i);

			if (CFDictionaryContainsKey(active, key)) {
				/* if this service is active */
				continue;
			}

			cDict = CFDictionaryGetValue(config, key);
			if (!cDict) {
				/* if no configuration for this service */
				continue;
			}

			/*
			 * We have a service which "claims" to be a potential path
			 * off of the system.  Check to make sure that this is a
			 * type of PPP link before claiming it's viable.
			 */
			aType = CFDictionaryGetValue(cDict, kSCPropNetInterfaceType);
			if (!aType || !CFEqual(aType, kSCValNetInterfaceTypePPP)) {
				/* if we can't get a connection on this service */
				sc_status = kSCStatusOK;
				goto done;
			}

			statusMessage = "is configured w/dynamic addressing";
			*flags |= kSCNetworkFlagsTransientConnection;
			*flags |= kSCNetworkFlagsReachable;
			*flags |= kSCNetworkFlagsConnectionRequired;

			if (_sc_debug) {
				SCLog(TRUE, LOG_INFO, CFSTR("  status     = %s"), statusMessage);
				SCLog(TRUE, LOG_INFO, CFSTR("  service id = %@"), key);
			}

			sc_status = kSCStatusOK;
			goto done;
		}

		SCLog(_sc_debug, LOG_INFO, CFSTR("  cannot be reached"));
		sc_status = kSCStatusOK;
		goto done;

	} else {
		/*
		 * if no code for this address family (yet)
		 */
		SCLog(_sc_verbose, LOG_ERR,
		      CFSTR("checkAddress(): unexpected address family %d"),
		      address->sa_family);
		sc_status = kSCStatusInvalidArgument;
		goto done;
	}

	goto done;

    checkInterface :

	if (_sc_debug) {
		CFDictionaryRef	aDict;
		CFStringRef	interface	= NULL;

		/* attempt to get the interface type from the config info */
		aDict = CFDictionaryGetValue(active, key);
		if (aDict) {
			interface = CFDictionaryGetValue(aDict, kSCPropInterfaceName);
		}

		SCLog(TRUE, LOG_INFO, CFSTR("  status     = %s"), statusMessage);
		SCLog(TRUE, LOG_INFO, CFSTR("  service id = %@"), key);
		SCLog(TRUE, LOG_INFO, CFSTR("  device     = %@"), interface ? interface : CFSTR("?"));
	}

	sc_status = kSCStatusOK;

	/*
	 * We have an interface which "claims" to be a valid path
	 * off of the system.  Check to make sure that this isn't
	 * a dial-on-demand PPP link that isn't connected yet.
	 */
	{
		CFNumberRef	num;
		CFDictionaryRef	cDict;

		/* attempt to get the interface type from the config info */
		cDict = CFDictionaryGetValue(config, key);
		if (cDict) {
			aType = CFDictionaryGetValue(cDict, kSCPropNetInterfaceType);
		}

		if (!aType || !CFEqual(aType, kSCValNetInterfaceTypePPP)) {
			/*
			 * if we don't know the interface type or if
			 * it is not a ppp interface
			 */
			goto done;
		}

		num = CFDictionaryGetValue(cDict, kSCPropNetPPPDialOnDemand);
		if (num) {
			int	dialOnDemand;

			CFNumberGetValue(num, kCFNumberIntType, &dialOnDemand);
			if (dialOnDemand != 0) {
				*flags |= kSCNetworkFlagsConnectionAutomatic;
			}

		}
	}

	*flags |= kSCNetworkFlagsTransientConnection;

	{
		u_int32_t		pppLink;
		struct ppp_status       *pppLinkStatus;
		int			pppStatus;

		/*
		 * The service ID is available, ask the PPP controller
		 * for the extended status.
		 */
		pppStatus = PPPInit(&pppRef);
		if (pppStatus != 0) {
			SCLog(_sc_debug, LOG_DEBUG, CFSTR("  PPPInit() failed: status=%d"), pppStatus);
			sc_status = kSCStatusReachabilityUnknown;
			goto done;
		}

		pppStatus = PPPGetLinkByServiceID(pppRef, key, &pppLink);
		if (pppStatus != 0) {
			SCLog(_sc_debug, LOG_DEBUG, CFSTR("  PPPGetLinkByServiceID() failed: status=%d"), pppStatus);
			sc_status = kSCStatusReachabilityUnknown;
			goto done;
		}

		pppStatus = PPPStatus(pppRef, pppLink, &pppLinkStatus);
		if (pppStatus != 0) {
			SCLog(_sc_debug, LOG_DEBUG, CFSTR("  PPPStatus() failed: status=%d"), pppStatus);
			sc_status = kSCStatusReachabilityUnknown;
			goto done;
		}
#ifdef	DEBUG
		SCLog(_sc_debug, LOG_DEBUG, CFSTR("  PPP link status = %d"), pppLinkStatus->status);
#endif	/* DEBUG */
		switch (pppLinkStatus->status) {
			case PPP_RUNNING :
				/* if we're really UP and RUNNING */
				break;
			case PPP_ONHOLD :
				/* if we're effectively UP and RUNNING */
				break;
			case PPP_IDLE :
				/* if we're not connected at all */
				SCLog(_sc_debug, LOG_INFO, CFSTR("  PPP link idle, dial-on-traffic to connect"));
				*flags |= kSCNetworkFlagsReachable;
				*flags |= kSCNetworkFlagsConnectionRequired;
				sc_status = kSCStatusOK;
				break;
			default :
				/* if we're in the process of [dis]connecting */
				SCLog(_sc_debug, LOG_INFO, CFSTR("  PPP link, connection in progress"));
				*flags |= kSCNetworkFlagsReachable;
				*flags |= kSCNetworkFlagsConnectionRequired;
				sc_status = kSCStatusOK;
				break;
		}
		CFAllocatorDeallocate(NULL, pppLinkStatus);
	}

	goto done;

    done :

	if (pppRef != -1)	(void) PPPDispose(pppRef);

	if (sc_status != kSCStatusOK) {
		_SCErrorSet(sc_status);
		return FALSE;
	}

	return TRUE;
}


static void
_CheckReachabilityInit(SCDynamicStoreRef	store,
		       CFDictionaryRef		*config,
		       CFDictionaryRef		*active,
		       CFArrayRef		*serviceOrder,
		       struct in_addr		**defaultRoute)
{
	CFMutableDictionaryRef	activeDict;
	CFMutableDictionaryRef	configDict;
	initContext		context;
	CFDictionaryRef		dict;
	CFMutableDictionaryRef	interfaces;
	CFMutableArrayRef	keys;
	CFMutableArrayRef	orderArray;
	CFDictionaryRef		orderDict;
	CFStringRef		orderKey;
	CFStringRef		pattern;
	CFMutableArrayRef	patterns;
	CFStringRef		routeKey;
	CFDictionaryRef		routeDict;

	configDict = CFDictionaryCreateMutable(NULL,
					       0,
					       &kCFTypeDictionaryKeyCallBacks,
					       &kCFTypeDictionaryValueCallBacks);
	*config = configDict;

	activeDict = CFDictionaryCreateMutable(NULL,
					       0,
					       &kCFTypeDictionaryKeyCallBacks,
					       &kCFTypeDictionaryValueCallBacks);
	*active = activeDict;

	orderArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	*serviceOrder = orderArray;

	*defaultRoute = NULL;

	interfaces = CFDictionaryCreateMutable(NULL,
					       0,
					       &kCFTypeDictionaryKeyCallBacks,
					       &kCFTypeDictionaryValueCallBacks);

	/*
	 * collect information on the configured services and their
	 * associated interface type.
	 */
	keys     = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	/*
	 * Setup:/Network/Global/IPv4 (for the ServiceOrder)
	 */
	orderKey = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							      kSCDynamicStoreDomainSetup,
							      kSCEntNetIPv4);
	CFArrayAppendValue(keys, orderKey);

	/*
	 * State:/Network/Global/IPv4 (for the DefaultRoute)
	 */
	routeKey = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							      kSCDynamicStoreDomainState,
							      kSCEntNetIPv4);
	CFArrayAppendValue(keys, routeKey);

	/* Setup: per-service IPv4 info */
	pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							      kSCDynamicStoreDomainSetup,
							      kSCCompAnyRegex,
							      kSCEntNetIPv4);
	CFArrayAppendValue(patterns, pattern);
	CFRelease(pattern);

	/* Setup: per-service Interface info */
	pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							      kSCDynamicStoreDomainSetup,
							      kSCCompAnyRegex,
							      kSCEntNetInterface);
	CFArrayAppendValue(patterns, pattern);
	CFRelease(pattern);

	/* Setup: per-service PPP info */
	pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							      kSCDynamicStoreDomainSetup,
							      kSCCompAnyRegex,
							      kSCEntNetPPP);
	CFArrayAppendValue(patterns, pattern);
	CFRelease(pattern);

	/* State: per-service IPv4 info */
	pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							      kSCDynamicStoreDomainState,
							      kSCCompAnyRegex,
							      kSCEntNetIPv4);
	CFArrayAppendValue(patterns, pattern);
	CFRelease(pattern);

	/* State: per-interface IPv4 info */
	pattern = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
								kSCDynamicStoreDomainState,
								kSCCompAnyRegex,
								kSCEntNetIPv4);
	CFArrayAppendValue(patterns, pattern);
	CFRelease(pattern);

	/* fetch the configuration information */
	dict = SCDynamicStoreCopyMultiple(store, keys, patterns);
	CFRelease(keys);
	CFRelease(patterns);
	if (!dict) {
		goto done;
	}

	/*
	 * get the ServiceOrder key from the global settings.
	 */
	orderDict = CFDictionaryGetValue(dict, orderKey);
	if (isA_CFDictionary(orderDict)) {
		CFArrayRef	array;

		/* global settings are available */
		array = (CFMutableArrayRef)CFDictionaryGetValue(orderDict, kSCPropNetServiceOrder);
		if (isA_CFArray(array)) {
			CFArrayAppendArray(orderArray,
					   array,
					   CFRangeMake(0, CFArrayGetCount(array)));
		}
	}

	/*
	 * get the DefaultRoute
	 */
	routeDict = CFDictionaryGetValue(dict, routeKey);
	if (isA_CFDictionary(routeDict)) {
		CFStringRef	addr;

		/* global state is available, get default route */
		addr = CFDictionaryGetValue(routeDict, kSCPropNetIPv4Router);
		if (isA_CFString(addr)) {
			struct in_addr	*route;

			route = CFAllocatorAllocate(NULL, sizeof(struct in_addr), 0);
			if (inet_atonCF(addr, route) == 0) {
				/* if address string is invalid */
				CFAllocatorDeallocate(NULL, route);
				route = NULL;
			} else {
				*defaultRoute = route;
			}
		}
	}

	/*
	 * collect the configured services, the active services, and
	 * the active interfaces.
	 */
	context.cDict   = configDict;
	context.cPrefix = SCDynamicStoreKeyCreate(NULL,
						  CFSTR("%@/%@/%@/"),
						  kSCDynamicStoreDomainSetup,
						  kSCCompNetwork,
						  kSCCompService);
	context.aDict   = activeDict;
	context.aPrefix = SCDynamicStoreKeyCreate(NULL,
						  CFSTR("%@/%@/%@/"),
						  kSCDynamicStoreDomainState,
						  kSCCompNetwork,
						  kSCCompService);
	context.iDict   = interfaces;
	context.iPrefix = SCDynamicStoreKeyCreate(NULL,
						  CFSTR("%@/%@/%@/"),
						  kSCDynamicStoreDomainState,
						  kSCCompNetwork,
						  kSCCompInterface);
	context.order = orderArray;

	CFDictionaryApplyFunction(dict, collectInfo, &context);

	/*
	 * add additional information for the configured services
	 */
	CFDictionaryApplyFunction(dict, collectExtraInfo, &context);

	/*
	 * remove any addresses associated with known services
	 */
	CFDictionaryApplyFunction(activeDict, removeKnownAddresses, interfaces);

	/*
	 * create new services for any remaining addresses
	 */
	CFDictionaryApplyFunction(interfaces, addUnknownService, &context);

	CFRelease(context.cPrefix);
	CFRelease(context.aPrefix);
	CFRelease(context.iPrefix);
	CFRelease(dict);

    done :

	CFRelease(interfaces);
	CFRelease(orderKey);
	CFRelease(routeKey);

#ifdef	DEBUG
	SCLog(_sc_debug, LOG_NOTICE, CFSTR("config = %@"), *config);
	SCLog(_sc_debug, LOG_NOTICE, CFSTR("active = %@"), *active);
	SCLog(_sc_debug, LOG_NOTICE, CFSTR("serviceOrder = %@"), *serviceOrder);
	SCLog(_sc_debug, LOG_NOTICE, CFSTR("defaultRoute = %s"), *defaultRoute?inet_ntoa(**defaultRoute):"None");
#endif	/* DEBUG */
	return;
}


static void
_CheckReachabilityFree(CFDictionaryRef	config,
		       CFDictionaryRef	active,
		       CFArrayRef	serviceOrder,
		       struct in_addr	*defaultRoute)
{
	if (config)		CFRelease(config);
	if (active)		CFRelease(active);
	if (serviceOrder)	CFRelease(serviceOrder);
	if (defaultRoute)	CFAllocatorDeallocate(NULL, defaultRoute);
	return;
}


Boolean
SCNetworkCheckReachabilityByAddress(const struct sockaddr	*address,
				    const int			addrlen,
				    SCNetworkConnectionFlags	*flags)
{
	CFDictionaryRef		active		= NULL;
	CFDictionaryRef		config		= NULL;
	struct in_addr		*defaultRoute	= NULL;
	Boolean			ok;
	CFArrayRef		serviceOrder	= NULL;
	SCDynamicStoreRef	store		= NULL;

	*flags = 0;

	/*
	 * Check if 0.0.0.0
	 */
	if (address->sa_family == AF_INET) {
		struct sockaddr_in      *sin = (struct sockaddr_in *)address;

		if (sin->sin_addr.s_addr == 0) {
			SCLog(_sc_debug, LOG_INFO, CFSTR("checkAddress(0.0.0.0)"));
			SCLog(_sc_debug, LOG_INFO, CFSTR("  status     = isReachable (this host)"));
			*flags |= kSCNetworkFlagsReachable;
			return TRUE;
		}
	}

	store = SCDynamicStoreCreate(NULL,
				     CFSTR("SCNetworkCheckReachabilityByAddress"),
				     NULL,
				     NULL);
	if (!store) {
		SCLog(_sc_verbose, LOG_INFO, CFSTR("SCDynamicStoreCreate() failed"));
		return FALSE;
	}

	_CheckReachabilityInit(store, &config, &active, &serviceOrder, &defaultRoute);
	ok = checkAddress(store,
			  address,
			  addrlen,
			  config,
			  active,
			  serviceOrder,
			  defaultRoute,
			  flags);
	_CheckReachabilityFree(config, active, serviceOrder, defaultRoute);

	CFRelease(store);
	return ok;
}


/*
 * rankReachability()
 *   Not reachable       == 0
 *   Connection Required == 1
 *   Reachable           == 2
 */
static int
rankReachability(int flags)
{
	int	rank = 0;

	if (flags & kSCNetworkFlagsReachable)		rank = 2;
	if (flags & kSCNetworkFlagsConnectionRequired)	rank = 1;
	return rank;
}


Boolean
SCNetworkCheckReachabilityByName(const char			*nodename,
				 SCNetworkConnectionFlags	*flags)
{
	CFDictionaryRef		active		= NULL;
	CFDictionaryRef		config		= NULL;
	struct in_addr		*defaultRoute	= NULL;
	struct hostent		*h;
	Boolean			haveDNS		= FALSE;
	int			i;
	Boolean			ok		= TRUE;
#ifdef	CHECK_IPV6_REACHABILITY
	struct addrinfo		*res		= NULL;
	struct addrinfo 	*resP;
#endif	/* CHECK_IPV6_REACHABILITY */
	CFArrayRef		serviceOrder	= NULL;
	SCDynamicStoreRef	store		= NULL;

	store = SCDynamicStoreCreate(NULL,
				     CFSTR("SCNetworkCheckReachabilityByName"),
				     NULL,
				     NULL);
	if (!store) {
		SCLog(_sc_verbose, LOG_INFO, CFSTR("SCDynamicStoreCreate() failed"));
		return FALSE;
	}

	_CheckReachabilityInit(store, &config, &active, &serviceOrder, &defaultRoute);

	/*
	 * We first assume that all of the configured DNS servers
	 * are available.  Since we don't know which name server will
	 * be consulted to resolve the specified nodename we need to
	 * check the availability of ALL name servers.  We can only
	 * proceed if we know that our query can be answered.
	 */

	*flags = kSCNetworkFlagsReachable;

	res_init();
	for (i=0; i<_res.nscount; i++) {
		SCNetworkConnectionFlags	ns_flags	= 0;

		if (_res.nsaddr_list[i].sin_addr.s_addr == 0) {
			continue;
		}

		haveDNS = TRUE;

		if (_res.nsaddr_list[i].sin_len == 0) {
			_res.nsaddr_list[i].sin_len = sizeof(_res.nsaddr_list[i]);
		}

		ok = checkAddress(store,
				  (struct sockaddr *)&_res.nsaddr_list[i],
				  _res.nsaddr_list[i].sin_len,
				  config,
				  active,
				  serviceOrder,
				  defaultRoute,
				  &ns_flags);
		if (!ok) {
			/* not today */
			break;
		}
		if (rankReachability(ns_flags) < rankReachability(*flags)) {
			/* return the worst case result */
			*flags = ns_flags;
		}
	}

	if (!ok || (rankReachability(*flags) < 2)) {
		goto done;
	}

	SCLog(_sc_debug, LOG_INFO, CFSTR("check DNS for \"%s\""), nodename);

	/*
	 * OK, all of the DNS name servers are available.  Let's
	 * first assume that the requested host is NOT available,
	 * resolve the nodename, and check its address for
	 * accessibility. We return the best status available.
	 */
	*flags = 0;

	/*
	 * resolve the nodename into an address
	 */

#ifdef	CHECK_IPV6_REACHABILITY
	i = getaddrinfo(nodename, NULL, NULL, &res);
	if (i != 0) {
		SCLog(_sc_verbose, LOG_ERR,
		      CFSTR("getaddrinfo() failed: %s"),
		      gai_strerror(i));
		goto done;
	}

	for (resP=res; resP!=NULL; resP=resP->ai_next) {
		SCNetworkConnectionFlags	ns_flags	= 0;

		if (resP->ai_addr->sa_family == AF_INET) {
			struct sockaddr_in      *sin = (struct sockaddr_in *)resP->ai_addr;

			if (sin->sin_addr.s_addr == 0) {
				SCLog(_sc_debug, LOG_INFO, CFSTR("checkAddress(0.0.0.0)"));
				SCLog(_sc_debug, LOG_INFO, CFSTR("  status     = isReachable (this host)"));
				*flags |= kSCNetworkFlagsReachable;
				break;
			}
		}

		ok = checkAddress(store,
				  resP->ai_addr,
				  resP->ai_addrlen,
				  config,
				  active,
				  serviceOrder,
				  defaultRoute,
				  &ns_flags);
		if (!ok) {
			/* not today */
			break;
		}
		if (rankReachability(ns_flags) > rankReachability(*flags)) {
			/* return the best case result */
			*flags = ns_flags;
			if (rankReachability(*flags) == 2) {
				/* we're in luck */
				break;
			}
		}
	}

	if (res) {
		goto done;
	}

	/*
	 * The getaddrinfo() function call didn't return any addresses.  While
	 * this may be the correct answer we have found that some DNS servers
	 * may, depending on what has been cached, not return all available
	 * records when issued a T_ANY query.  To accomodate these servers
	 * we double check by using the gethostbyname() function which uses
	 * a simple T_A query.
	 */

#ifdef	DEBUG
	SCLog(_sc_debug,
	      LOG_INFO,
	      CFSTR("getaddrinfo() returned no addresses, try gethostbyname()"));
#endif	/* DEBUG */
#endif	/* CHECK_IPV6_REACHABILITY */

	h = gethostbyname(nodename);
	if (h && h->h_length) {
		struct in_addr **s	= (struct in_addr **)h->h_addr_list;

		while (*s) {
			SCNetworkConnectionFlags	ns_flags	= 0;
			struct sockaddr_in		sa;

			bzero(&sa, sizeof(sa));
			sa.sin_len    = sizeof(sa);
			sa.sin_family = AF_INET;
			sa.sin_addr   = **s;

			if (sa.sin_addr.s_addr == 0) {
				SCLog(_sc_debug, LOG_INFO, CFSTR("checkAddress(0.0.0.0)"));
				SCLog(_sc_debug, LOG_INFO, CFSTR("  status     = isReachable (this host)"));
				*flags |= kSCNetworkFlagsReachable;
				break;
			}

			ok = checkAddress(store,
					  (struct sockaddr *)&sa,
					  sizeof(sa),
					  config,
					  active,
					  serviceOrder,
					  defaultRoute,
					  &ns_flags);
			if (!ok) {
				/* not today */
				break;
			}
			if (rankReachability(ns_flags) > rankReachability(*flags)) {
				/* return the best case result */
				*flags = ns_flags;
				if (rankReachability(*flags) == 2) {
					/* we're in luck */
					break;
				}
			}

			s++;
		}
	} else {
		char	*msg;

		switch(h_errno) {
			case NETDB_INTERNAL :
				msg = strerror(errno);
				break;
			case HOST_NOT_FOUND :
				msg = "Host not found.";
				if (!haveDNS) {
					/*
					 * No DNS servers are defined. Set flags based on
					 * the availability of configured (but not active)
					 * services.
					 */
					struct sockaddr_in	sa;

					bzero(&sa, sizeof(sa));
					sa.sin_len         = sizeof(sa);
					sa.sin_family      = AF_INET;
					sa.sin_addr.s_addr = 0;
					ok = checkAddress(store,
							  (struct sockaddr *)&sa,
							  sizeof(sa),
							  config,
							  active,
							  serviceOrder,
							  defaultRoute,
							  flags);
					if (ok &&
					    (*flags & kSCNetworkFlagsReachable) &&
					    (*flags & kSCNetworkFlagsConnectionRequired)) {
						/*
						 * We might pick up a set of DNS servers
						 * from this connection, don't reply with
						 * "Host not found." just yet.
						 */
						goto done;
					}
					*flags = 0;
				}
				break;
			case TRY_AGAIN :
				msg = "Try again.";
				break;
			case NO_RECOVERY :
				msg = "No recovery.";
				break;
			case NO_DATA :
				msg = "No data available.";
				break;
			default :
				msg = "Unknown";
				break;
		}
		SCLog(_sc_debug, LOG_INFO, CFSTR("gethostbyname() failed: %s"), msg);
	}

    done :

	_CheckReachabilityFree(config, active, serviceOrder, defaultRoute);
	if (store)	CFRelease(store);
#ifdef	CHECK_IPV6_REACHABILITY
	if (res)	freeaddrinfo(res);
#endif	/* CHECK_IPV6_REACHABILITY */

	return ok;
}
