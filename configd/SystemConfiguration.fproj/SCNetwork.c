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

#include <SystemConfiguration/SystemConfiguration.h>

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


/*
 * return a dictionary of configured services.
 */
static CFDictionaryRef
getServices(SCDSessionRef session)
{
	CFArrayRef		defined		= NULL;
	int			i;
	CFStringRef		key;
	CFStringRef		prefix;
	CFMutableDictionaryRef	services;
	SCDStatus		status;

	prefix = SCDKeyCreate(CFSTR("%@/%@/%@/"),
			      kSCCacheDomainSetup,
			      kSCCompNetwork,
			      kSCCompService);

	services = CFDictionaryCreateMutable(NULL,
					     0,
					     &kCFTypeDictionaryKeyCallBacks,
					     &kCFTypeDictionaryValueCallBacks);

	key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup,
					       kSCCompAnyRegex,
					       kSCEntNetIPv4);
	status = SCDList(session, key, kSCDRegexKey, &defined);
	CFRelease(key);
	if (status != SCD_OK) {
		goto done;
	}

	for (i = 0; i < CFArrayGetCount(defined); i++) {
		CFDictionaryRef		if_dict;
		SCDHandleRef		if_handle	= NULL;
		CFDictionaryRef		ip_dict;
		SCDHandleRef		ip_handle	= NULL;
		boolean_t		isPPP		= FALSE;
		CFDictionaryRef		ppp_dict;
		SCDHandleRef		ppp_handle	= NULL;
		CFMutableDictionaryRef	sDict		= NULL;
		CFStringRef		sid		= NULL;

		key  = CFArrayGetValueAtIndex(defined, i);

		/* get IPv4 dictionary for service */
		status = SCDGet(session, key, &ip_handle);
		if (status != SCD_OK) {
			/* if service was removed behind our back */
			goto nextService;
		}
		ip_dict = SCDHandleGetData(ip_handle);

		sDict = CFDictionaryCreateMutableCopy(NULL, 0, ip_dict);

		/* add keys from the service's Interface dictionary */
		sid = parse_component(key, prefix);
		if (sid == NULL) {
			goto nextService;
		}

		key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup,
						       sid,
						       kSCEntNetInterface);
		status = SCDGet(session, key, &if_handle);
		CFRelease(key);
		if (status != SCD_OK) {
			goto nextService;
		}
		if_dict = SCDHandleGetData(if_handle);

		/* check the interface "Type", "SubType", and "DeviceName" */
		if (CFDictionaryGetValueIfPresent(if_dict,
						  kSCPropNetInterfaceType,
						  (void **)&key)) {
			CFDictionaryAddValue(sDict, kSCPropNetInterfaceType, key);
			isPPP = CFEqual(key, kSCValNetInterfaceTypePPP);
		}
		if (CFDictionaryGetValueIfPresent(if_dict,
						  kSCPropNetInterfaceSubType,
						  (void **)&key)) {
			CFDictionaryAddValue(sDict, kSCPropNetInterfaceSubType, key);
		}

		if (isPPP) {
			key = SCDKeyCreateNetworkServiceEntity(kSCCacheDomainSetup,
							       sid,
							       kSCEntNetPPP);
			status = SCDGet(session, key, &ppp_handle);
			CFRelease(key);
			if (status != SCD_OK) {
				goto nextService;
			}
			ppp_dict = SCDHandleGetData(ppp_handle);

			/* get Dial-on-Traffic flag */
			if (CFDictionaryGetValueIfPresent(ppp_dict,
							  kSCPropNetPPPDialOnDemand,
							  (void **)&key)) {
				CFDictionaryAddValue(sDict, kSCPropNetPPPDialOnDemand, key);
			}
		}

		CFDictionaryAddValue(services, sid, sDict);

	nextService:

		if (sid)	CFRelease(sid);
		if (if_handle)	SCDHandleRelease(if_handle);
		if (ip_handle)	SCDHandleRelease(ip_handle);
		if (ppp_handle)	SCDHandleRelease(ppp_handle);
		if (sDict)	CFRelease(sDict);
	}

    done:

	if (defined)	CFRelease(defined);
	CFRelease(prefix);

	return services;
}


/*
 * return a dictionary of configured interfaces.
 */
static CFDictionaryRef
getInterfaces(SCDSessionRef session)
{
	CFMutableArrayRef	defined		= NULL;
	int			i;
	CFStringRef		key;
	CFMutableDictionaryRef	interfaces;
	CFStringRef		prefix;
	SCDStatus		status;

	prefix = SCDKeyCreate(CFSTR("%@/%@/%@/"),
			      kSCCacheDomainState,
			      kSCCompNetwork,
			      kSCCompInterface);

	interfaces = CFDictionaryCreateMutable(NULL,
					       0,
					       &kCFTypeDictionaryKeyCallBacks,
					       &kCFTypeDictionaryValueCallBacks);

	key = SCDKeyCreateNetworkInterfaceEntity(kSCCacheDomainState,
						 kSCCompAnyRegex,
						 kSCEntNetIPv4);
	status = SCDList(session, key, kSCDRegexKey, &defined);
	CFRelease(key);
	if (status != SCD_OK) {
		goto done;
	}

	for (i=0; i<CFArrayGetCount(defined); i++) {
		CFStringRef		iid		= NULL;
		CFDictionaryRef		ip_dict;
		SCDHandleRef		ip_handle	= NULL;

		key  = CFArrayGetValueAtIndex(defined, i);

		/* get IPv4 dictionary for service */
		status = SCDGet(session, key, &ip_handle);
		if (status != SCD_OK) {
			/* if interface was removed behind our back */
			goto nextIF;
		}
		ip_dict = SCDHandleGetData(ip_handle);

		iid = parse_component(key, prefix);
		if (iid == NULL) {
			goto nextIF;
		}

		CFDictionaryAddValue(interfaces, iid, ip_dict);

	    nextIF :

		if (iid)	CFRelease(iid);
		if (ip_handle)	SCDHandleRelease(ip_handle);
	}

    done:

	if (defined)	CFRelease(defined);
	CFRelease(prefix);
	return interfaces;
}


/*
 * return an array of interface names based on a specified service order.
 */
static CFArrayRef
getInterfaceOrder(CFDictionaryRef	interfaces,
		  CFArrayRef		serviceOrder,
		  CFNumberRef		pppOverridePrimary)
{
	CFIndex			i;
	CFIndex			iCnt;
	CFMutableArrayRef	iKeys;
	void			**keys;
	CFMutableArrayRef	order	= NULL;
	CFArrayRef		tKeys;

	order = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	iCnt = CFDictionaryGetCount(interfaces);
	keys = CFAllocatorAllocate(NULL, iCnt * sizeof(CFStringRef), 0);
	CFDictionaryGetKeysAndValues(interfaces, keys, NULL);
	tKeys = CFArrayCreate(NULL, keys, iCnt, &kCFTypeArrayCallBacks);
	CFAllocatorDeallocate(NULL, keys);
	iKeys = CFArrayCreateMutableCopy(NULL, 0, tKeys);
	CFRelease(tKeys);

	for (i = 0; serviceOrder && i < CFArrayGetCount(serviceOrder); i++) {
		CFIndex		j;
		CFStringRef	oSID;

		oSID = CFArrayGetValueAtIndex(serviceOrder, i);
		for (j=0; j<CFArrayGetCount(iKeys); j++) {
			CFDictionaryRef	iDict;
			CFStringRef	iKey;
			CFStringRef	iSID;
			CFArrayRef	iSIDs;
			CFIndex		k;
			boolean_t	match	= FALSE;

			iKey  = CFArrayGetValueAtIndex(iKeys, j);
			iDict = CFDictionaryGetValue(interfaces, iKey);

			iSIDs = CFDictionaryGetValue(iDict, kSCCachePropNetServiceIDs);
			for (k = 0; iSIDs && k < CFArrayGetCount(iSIDs); k++) {
				iSID = CFArrayGetValueAtIndex(iSIDs, k);
				if (CFEqual(oSID, iSID)) {
					match = TRUE;
					break;
				}
			}

			if (match) {
				/* if order ServiceID is associated with this interface */
				CFArrayAppendValue(order, iKey);
				CFArrayRemoveValueAtIndex(iKeys, j);
				break;
			}
		}
	}

	for (i = 0; i < CFArrayGetCount(iKeys); i++) {
		CFStringRef	iKey;

		iKey = CFArrayGetValueAtIndex(iKeys, i);
		CFArrayAppendValue(order, iKey);
	}

	CFRelease(iKeys);
	return order;
}


static boolean_t
getAddresses(CFDictionaryRef	iDict,
	     CFIndex		*nAddrs,
	     CFArrayRef		*addrs,
	     CFArrayRef		*masks,
	     CFArrayRef		*dests)
{
	*addrs = CFDictionaryGetValue(iDict, kSCPropNetIPv4Addresses);
	*masks = CFDictionaryGetValue(iDict, kSCPropNetIPv4SubnetMasks);
	*dests = CFDictionaryGetValue(iDict, kSCPropNetIPv4DestAddresses);

	if ((*addrs == NULL) ||
	    ((*nAddrs = CFArrayGetCount(*addrs)) == 0)) {
		/* sorry, no addresses */
		return FALSE;
	}

	if ((*masks && *dests) ||
	    (*masks == NULL) && (*dests == NULL)) {
		/*
		 * sorry, we expect to have "SubnetMasks" or
		 * "DestAddresses" (not both) and if the count
		 * must match the number of "Addresses".
		 */
		return FALSE;
	}

	if (*masks && (*nAddrs != CFArrayGetCount(*masks))) {
		/* if we don't like the netmasks */
		return FALSE;
	}

	if (*dests &&  (*nAddrs != CFArrayGetCount(*dests))) {
		/* if we don't like the destaddresses */
		return FALSE;
	}

	return TRUE;
}

static SCNStatus
checkAddress(SCDSessionRef		session,
	     const struct sockaddr	*address,
	     const int			addrlen,
	     CFDictionaryRef		services,
	     CFDictionaryRef		interfaces,
	     CFArrayRef			interfaceOrder,
	     struct in_addr		*defaultRoute,
	     int			*flags,
	     const char			**errorMessage)
{
	CFIndex			i;
	struct ifreq		ifr;
	CFIndex			iCnt;
	CFStringRef		iKey		= NULL;
	CFStringRef		iType		= NULL;
	void			**keys;
	int			pppRef          = -1;
	SCNStatus		scn_status	= SCN_REACHABLE_UNKNOWN;
	CFIndex			sCnt;
	CFMutableArrayRef	sKeys		= NULL;
	CFStringRef		sID		= NULL;
	CFArrayRef		sIDs		= NULL;
	CFArrayRef		sList		= NULL;
	int			sock		= -1;
	CFStringRef		sKey		= NULL;
	CFDictionaryRef		sDict		= NULL;
	CFArrayRef		tKeys;

	if (flags != NULL) {
		*flags = 0;
	}

	if (address == NULL) {
		return SCN_REACHABLE_NO;
	}

	sCnt = CFDictionaryGetCount(services);
	keys = CFAllocatorAllocate(NULL, sCnt * sizeof(CFStringRef), 0);
	CFDictionaryGetKeysAndValues(services, keys, NULL);
	tKeys = CFArrayCreate(NULL, keys, sCnt, &kCFTypeArrayCallBacks);
	CFAllocatorDeallocate(NULL, keys);
	sKeys = CFArrayCreateMutableCopy(NULL, 0, tKeys);
	CFRelease(tKeys);

	if (address->sa_family == AF_INET) {
		struct sockaddr_in	*sin = (struct sockaddr_in *)address;

#ifdef	DEBUG
		if (SCDOptionGet(session, kSCDOptionDebug))
			SCDLog(LOG_INFO, CFSTR("checkAddress(%s)"), inet_ntoa(sin->sin_addr));
#endif	/* DEBUG */
		/*
		 * Check for loopback address
		 */
		if (ntohl(sin->sin_addr.s_addr) == ntohl(INADDR_LOOPBACK)) {
			/* if asking about the loopback address */
#ifdef	DEBUG
			if (SCDOptionGet(session, kSCDOptionDebug))
				SCDLog(LOG_INFO, CFSTR("  isReachable via loopback"));
#endif	/* DEBUG */
			scn_status = SCN_REACHABLE_YES;
			goto done;
		}

		/*
		 * Check if the address is on one of the subnets
		 * associated with our active IPv4 interfaces
		 */
		iCnt = CFArrayGetCount(interfaceOrder);
		for (i=0; i<iCnt; i++) {
			CFArrayRef		addrs;
			CFArrayRef		dests;
			CFDictionaryRef		iDict;
			CFIndex			j;
			CFArrayRef		masks;
			CFIndex			nAddrs	= 0;

			iKey  = CFArrayGetValueAtIndex(interfaceOrder, i);
			iDict = CFDictionaryGetValue(interfaces, iKey);

			/* remove active services */
			sIDs = CFDictionaryGetValue(iDict, kSCCachePropNetServiceIDs);
			for (j = 0; sIDs && j < CFArrayGetCount(sIDs); j++) {
				CFIndex		k;
				CFStringRef	sID;

				sID = CFArrayGetValueAtIndex(sIDs, j);
				k   = CFArrayGetFirstIndexOfValue(sKeys,
								  CFRangeMake(0, CFArrayGetCount(sKeys)),
								  sID);
				if (k != -1) {
					CFArrayRemoveValueAtIndex(sKeys, k);
				}
			}

			if (!getAddresses(iDict, &nAddrs, &addrs, &masks, &dests)) {
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
#ifdef	DEBUG
						if (SCDOptionGet(session, kSCDOptionDebug))
							SCDLog(LOG_INFO, CFSTR("  isReachable (my subnet)"));
#endif	/* DEBUG */
						scn_status = SCN_REACHABLE_YES;
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
#ifdef	DEBUG
						if (SCDOptionGet(session, kSCDOptionDebug))
							SCDLog(LOG_INFO, CFSTR("  isReachable (my local address)"));
#endif	/* DEBUG */
						scn_status = SCN_REACHABLE_YES;
						goto checkInterface;
					}

					if (ntohl(sin->sin_addr.s_addr) == ntohl(destAddr.s_addr)) {
						/* the address is the other side of the link */
#ifdef	DEBUG
						if (SCDOptionGet(session, kSCDOptionDebug))
							SCDLog(LOG_INFO, CFSTR("  isReachable (my remote address)"));
#endif	/* DEBUG */
						scn_status = SCN_REACHABLE_YES;
						goto checkInterface;
					}
				}
			}
		}

		/*
		 * Check if the address is accessible via the "default" route.
		 */
		for (i=0; i<iCnt; i++) {
			CFArrayRef		addrs;
			CFArrayRef		dests;
			CFDictionaryRef		iDict;
			CFIndex			j;
			CFArrayRef		masks;
			CFIndex			nAddrs	= 0;

			iKey  = CFArrayGetValueAtIndex(interfaceOrder, i);
			iDict = CFDictionaryGetValue(interfaces, iKey);

			if (!getAddresses(iDict, &nAddrs, &addrs, &masks, &dests)) {
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
#ifdef	DEBUG
						if (SCDOptionGet(session, kSCDOptionDebug))
							SCDLog(LOG_INFO, CFSTR("  isReachable via default route (my subnet)"));
#endif	/* DEBUG */
						scn_status = SCN_REACHABLE_YES;
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
#ifdef	DEBUG
						if (SCDOptionGet(session, kSCDOptionDebug))
							SCDLog(LOG_INFO, CFSTR("  isReachable via default route (my remote address)"));
#endif	/* DEBUG */
						scn_status = SCN_REACHABLE_YES;
						goto checkInterface;
					}
				}
			}
		}

		/*
		 * Check the not active (but configured) IPv4 services
		 */
		sCnt = CFArrayGetCount(sKeys);
		for (i=0; i<sCnt; i++) {
			CFArrayRef		addrs;
			CFStringRef		configMethod	= NULL;
			CFArrayRef		dests;
			CFIndex			j;
			CFArrayRef		masks;
			CFIndex			nAddrs		= 0;

			sKey  = CFArrayGetValueAtIndex(sKeys, i);
			sDict = CFDictionaryGetValue(services, sKey);

			/*
			 * check configured network addresses
			 */
			for (j=0; j<nAddrs; j++) {
				struct in_addr	ifAddr;

				if (inet_atonCF(CFArrayGetValueAtIndex(addrs, j),
						&ifAddr) == 0) {
					/* if Addresses string is invalid */
					break;
				}

				if (masks) {
					struct in_addr	ifMask;

					/* check address/netmask */
					if (inet_atonCF(CFArrayGetValueAtIndex(masks, j),
							&ifMask) == 0) {
						/* if SubnetMasks string is invalid */
						break;
					}

					if ((ntohl(ifAddr.s_addr)        & ntohl(ifMask.s_addr)) !=
					    (ntohl(sin->sin_addr.s_addr) & ntohl(ifMask.s_addr))) {
						/* the requested address is on this subnet */
#ifdef	DEBUG
						if (SCDOptionGet(session, kSCDOptionDebug))
							SCDLog(LOG_INFO, CFSTR("  is configured w/static info (my subnet)"));
#endif	/* DEBUG */
						goto checkService;
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
#ifdef	DEBUG
						if (SCDOptionGet(session, kSCDOptionDebug))
							SCDLog(LOG_INFO, CFSTR("  is configured w/static info (my local address)"));
#endif	/* DEBUG */
						goto checkService;
					}

					if (ntohl(sin->sin_addr.s_addr) == ntohl(destAddr.s_addr)) {
						/* the address is the other side of the link */
#ifdef	DEBUG
						if (SCDOptionGet(session, kSCDOptionDebug))
							SCDLog(LOG_INFO, CFSTR("  is configured w/static info (my remote address)"));
#endif	/* DEBUG */
						goto checkService;
					}
				}
			}

			/*
			 * check for dynamic (i.e. not manual) configuration
			 * method.
			 */
			if (CFDictionaryGetValueIfPresent(sDict,
							  kSCPropNetIPv4ConfigMethod,
							  (void **)&configMethod) &&
			    !CFEqual(configMethod, kSCValNetIPv4ConfigMethodManual)) {
				/* if only we were "connected" */
#ifdef	DEBUG
				if (SCDOptionGet(session, kSCDOptionDebug))
					SCDLog(LOG_INFO, CFSTR("  is configured w/dynamic addressing"));
#endif	/* DEBUG */
				goto checkService;
			}
		}

#ifdef	DEBUG
		if (SCDOptionGet(session, kSCDOptionDebug))
			SCDLog(LOG_INFO, CFSTR("  cannot be reached"));
#endif	/* DEBUG */
		scn_status = SCN_REACHABLE_NO;
		goto done;

	} else {
		/*
		 * if no code for this address family (yet)
		 */
		SCDSessionLog(session,
			      LOG_ERR,
			      CFSTR("checkAddress(): unexpected address family %d"),
			      address->sa_family);
		if (errorMessage != NULL) {
			*errorMessage = "unexpected address family";
		}
		goto done;
	}

	goto done;

    checkInterface :

	/*
	 * We have an interface which "claims" to be a valid path
	 * off of the system.  Check to make sure that this isn't
	 * a dial-on-demand PPP link that isn't connected yet.
	 */
	if (sIDs) {
		CFNumberRef	num;
		CFDictionaryRef	sDict;

		/* attempt to get the interface type from the first service */
		sID   = CFArrayGetValueAtIndex(sIDs, 0);
		sDict = CFDictionaryGetValue(services, sID);
		if (sDict) {
			iType = CFDictionaryGetValue(sDict, kSCPropNetInterfaceType);
		}

		if (!iType) {
			/* if we don't know the interface type */
			goto done;
		}

		if (!CFEqual(iType, kSCValNetInterfaceTypePPP)) {
			/* if not a ppp interface */
			goto done;
		}

		num = CFDictionaryGetValue(sDict, kSCPropNetPPPDialOnDemand);
		if (num) {
			int	dialOnDemand;

			CFNumberGetValue(num, kCFNumberIntType, &dialOnDemand);	
			if (flags && (dialOnDemand != 0)) {
				*flags |= kSCNFlagsConnectionAutomatic;
			}
			
		}
	} else if (!CFStringHasPrefix(iKey, CFSTR("ppp"))) {
		/* if not a ppp interface */
		goto done;
	}

	if (flags != NULL) {
		*flags |= kSCNFlagsTransientConnection;
	}

	if (sID) {
		u_int32_t		pppLink;
		struct ppp_status       *pppLinkStatus;
		int			pppStatus;

		/*
		 * The service ID is available, ask the PPP controller
		 * for the extended status.
		 */
		pppStatus = PPPInit(&pppRef);
		if (pppStatus != 0) {
#ifdef	DEBUG
			if (SCDOptionGet(session, kSCDOptionDebug))
				SCDLog(LOG_DEBUG, CFSTR("  PPPInit() failed: status=%d"), pppStatus);
#endif	/* DEBUG */
			scn_status = SCN_REACHABLE_UNKNOWN;
			if (errorMessage != NULL) {
				*errorMessage = "PPPInit() failed";
			}
			goto done;
		}

		pppStatus = PPPGetLinkByServiceID(pppRef, sID, &pppLink);
		if (pppStatus != 0) {
#ifdef	DEBUG
			if (SCDOptionGet(session, kSCDOptionDebug))
				SCDLog(LOG_DEBUG, CFSTR("  PPPGetLinkByServiceID() failed: status=%d"), pppStatus);
#endif	/* DEBUG */
			scn_status = SCN_REACHABLE_UNKNOWN;
			if (errorMessage != NULL) {
				*errorMessage = "PPPGetLinkByServiceID() failed";
			}
			goto done;
		}

		pppStatus = PPPStatus(pppRef, pppLink, &pppLinkStatus);
		if (pppStatus != 0) {
#ifdef	DEBUG
			if (SCDOptionGet(session, kSCDOptionDebug))
				SCDLog(LOG_DEBUG, CFSTR("  PPPStatus() failed: status=%d"), pppStatus);
#endif	/* DEBUG */
			scn_status = SCN_REACHABLE_UNKNOWN;
			if (errorMessage != NULL) {
				*errorMessage = "PPPStatus() failed";
			}
			goto done;
		}
#ifdef	DEBUG
		if (SCDOptionGet(session, kSCDOptionDebug))
			SCDLog(LOG_DEBUG, CFSTR("  PPP link status = %d"), pppLinkStatus->status);
#endif	/* DEBUG */
		switch (pppLinkStatus->status) {
			case PPP_RUNNING :
				/* if we're really UP and RUNNING */
				break;
			case PPP_IDLE :
				/* if we're not connected at all */
#ifdef	DEBUG
				if (SCDOptionGet(session, kSCDOptionDebug))
					SCDLog(LOG_INFO, CFSTR("  PPP link idle, dial-on-traffic to connect"));
#endif	/* DEBUG */
				scn_status = SCN_REACHABLE_CONNECTION_REQUIRED;
				break;
			default :
				/* if we're in the process of [dis]connecting */
#ifdef	DEBUG
				if (SCDOptionGet(session, kSCDOptionDebug))
					SCDLog(LOG_INFO, CFSTR("  PPP link, connection in progress"));
#endif	/* DEBUG */
				scn_status = SCN_REACHABLE_CONNECTION_REQUIRED;
				break;
		}
		CFAllocatorDeallocate(NULL, pppLinkStatus);
	} else {
		/*
		 * The service ID is not available, check the interfaces
		 * UP and RUNNING flags.
		 */
		bzero(&ifr, sizeof(ifr));
		if (!CFStringGetCString(iKey,
					(char *)&ifr.ifr_name,
					sizeof(ifr.ifr_name),
					kCFStringEncodingMacRoman)) {
			scn_status = SCN_REACHABLE_UNKNOWN;
			if (errorMessage != NULL) {
				*errorMessage = "could not convert interface name to C string";
			}
			goto done;
		}

		sock = socket(AF_INET, SOCK_DGRAM, 0);
		if (sock == -1) {
			scn_status = SCN_REACHABLE_UNKNOWN;
			if (errorMessage != NULL) {
				*errorMessage = strerror(errno);
			}
			goto done;
		}

		if (ioctl(sock, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
			scn_status = SCN_REACHABLE_UNKNOWN;
			if (errorMessage != NULL) {
				*errorMessage = strerror(errno);
			}
			goto done;
		}

#ifdef	DEBUG
		if (SCDOptionGet(session, kSCDOptionDebug))
			SCDLog(LOG_INFO, CFSTR("  flags for %s == 0x%hx"), ifr.ifr_name, ifr.ifr_flags);
#endif	/* DEBUG */
		if ((ifr.ifr_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) {
			if ((ifr.ifr_flags & IFF_UP) == IFF_UP) {
				/* if we're "up" but not "running" */
#ifdef	DEBUG
				if (SCDOptionGet(session, kSCDOptionDebug))
					SCDLog(LOG_INFO, CFSTR("  up & not running, dial-on-traffic to connect"));
#endif	/* DEBUG */
				scn_status = SCN_REACHABLE_CONNECTION_REQUIRED;
				if (flags != NULL) {
					*flags |= kSCNFlagsConnectionAutomatic;
				}
			} else {
				/* if we're not "up" and "running" */
#ifdef	DEBUG
				if (SCDOptionGet(session, kSCDOptionDebug))
					SCDLog(LOG_INFO, CFSTR("  not up & running, connection required"));
#endif	/* DEBUG */
				scn_status = SCN_REACHABLE_CONNECTION_REQUIRED;
			}
			goto done;
		}
	}

	goto done;

    checkService :

	/*
	 * We have a service which "claims" to be a potential path
	 * off of the system.  Check to make sure that this is a
	 * type of PPP link before claiming it's viable.
	 */
	if (sDict &&
	    CFDictionaryGetValueIfPresent(sDict,
					  kSCPropNetInterfaceType,
					  (void **)&iType) &&
	    !CFEqual(iType, kSCValNetInterfaceTypePPP)) {
		/* no path if this not a ppp interface */
#ifdef	DEBUG
		if (SCDOptionGet(session, kSCDOptionDebug))
			SCDLog(LOG_INFO, CFSTR("  cannot be reached"));
#endif	/* DEBUG */
		scn_status = SCN_REACHABLE_NO;
		goto done;
	}

	scn_status = SCN_REACHABLE_CONNECTION_REQUIRED;
	if (flags != NULL) {
		*flags |= kSCNFlagsTransientConnection;
	}

    done :

	if (sKeys)		CFRelease(sKeys);
	if (sList)		CFRelease(sList);
	if (pppRef != -1)	(void) PPPDispose(pppRef);
	if (sock != -1)		(void)close(sock);

	return scn_status;
}


static void
_IsReachableInit(SCDSessionRef	session,
	CFDictionaryRef	*services,
	CFDictionaryRef	*interfaces,
	CFArrayRef	*interfaceOrder,
	struct in_addr	**defaultRoute)
{
	CFStringRef	addr;
	CFDictionaryRef	dict;
	CFStringRef	key;
	SCDHandleRef	handle;
	CFNumberRef	pppOverridePrimary	= NULL;
	CFArrayRef	serviceOrder		= NULL;
	struct in_addr	*route			= NULL;
	SCDStatus	status;

	/*
	 * get the ServiceOrder and PPPOverridePrimary keys
	 * from the global settings.
	 */
	key = SCDKeyCreateNetworkGlobalEntity(kSCCacheDomainSetup, kSCEntNetIPv4);
	status = SCDGet(session, key, &handle);
	CFRelease(key);
	switch (status) {
		case SCD_OK :
			/* if global settings are available */
			dict = SCDHandleGetData(handle);

			/* get service order */
			if ((CFDictionaryGetValueIfPresent(dict,
							   kSCPropNetServiceOrder,
							   (void **)&serviceOrder) == TRUE)) {
				CFRetain(serviceOrder);
			}

			/* get PPP overrides primary flag */
			if ((CFDictionaryGetValueIfPresent(dict,
							   kSCPropNetPPPOverridePrimary,
							   (void **)&pppOverridePrimary) == TRUE)) {
				CFRetain(pppOverridePrimary);
			}

			SCDHandleRelease(handle);
			break;
		case SCD_NOKEY :
			/* if no global settings */
			break;
		default :
			SCDLog(LOG_ERR, CFSTR("SCDGet() failed: %s"), SCDError(status));
			/* XXX need to do something more with this FATAL error XXXX */
			goto error;
	}

	/*
	 * Get default route
	 */
	key = SCDKeyCreateNetworkGlobalEntity(kSCCacheDomainState,
					      kSCEntNetIPv4);
	status = SCDGet(session, key, &handle);
	CFRelease(key);
	switch (status) {
		case SCD_OK :
			dict = SCDHandleGetData(handle);
			addr = CFDictionaryGetValue(dict, kSCPropNetIPv4Router);
			if (addr == NULL) {
				/* if no default route */
				break;
			}

			route = CFAllocatorAllocate(NULL, sizeof(struct in_addr), 0);
			if (inet_atonCF(addr, route) == 0) {
				/* if address string is invalid */
				CFAllocatorDeallocate(NULL, route);
				route = NULL;
				break;
			}
			*defaultRoute = route;

			break;
		case SCD_NOKEY :
			/* if no default route */
			break;
		default :
			SCDSessionLog(session,
				      LOG_ERR,
				      CFSTR("SCDGet() failed: %s"),
				      SCDError(status));
			goto error;
	}
	if (handle) {
		SCDHandleRelease(handle);
		handle = NULL;
	}

	/*
	 * get the configured services and interfaces
	 */
	*services       = getServices  (session);
	*interfaces     = getInterfaces(session);
	*interfaceOrder = getInterfaceOrder(*interfaces,
					    serviceOrder,
					    pppOverridePrimary);

    error :

	if (serviceOrder)	CFRelease(serviceOrder);
	if (pppOverridePrimary)	CFRelease(pppOverridePrimary);

#ifdef	DEBUG
	if (SCDOptionGet(session, kSCDOptionDebug)) {
		SCDLog(LOG_NOTICE, CFSTR("interfaces     = %@"), *interfaces);
		SCDLog(LOG_NOTICE, CFSTR("services       = %@"), *services);
		SCDLog(LOG_NOTICE, CFSTR("interfaceOrder = %@"), *interfaceOrder);
		SCDLog(LOG_NOTICE, CFSTR("defaultRoute   = %s"), *defaultRoute?inet_ntoa(**defaultRoute):"None");
	}
#endif	/* DEBUG */
	return;

}


static void
_IsReachableFree(CFDictionaryRef	services,
		 CFDictionaryRef	interfaces,
		 CFArrayRef		interfaceOrder,
		 struct in_addr		*defaultRoute)
{
	if (services)		CFRelease(services);
	if (interfaces)		CFRelease(interfaces);
	if (interfaceOrder)	CFRelease(interfaceOrder);
	if (defaultRoute)	CFAllocatorDeallocate(NULL, defaultRoute);
	return;
}


SCNStatus
SCNIsReachableByAddress(const struct sockaddr	*address,
			const int		addrlen,
			int			*flags,
			const char		**errorMessage)
{
	struct in_addr	*defaultRoute	= NULL;
	CFDictionaryRef	interfaces	= NULL;
	CFArrayRef	interfaceOrder	= NULL;
	CFDictionaryRef	services	= NULL;
	SCDSessionRef	session		= NULL;
	SCDStatus	scd_status;
	SCNStatus	scn_status;

	scd_status = SCDOpen(&session, CFSTR("SCNIsReachableByAddress"));
	if (scd_status != SCD_OK) {
		if (errorMessage != NULL) {
			*errorMessage = SCDError(scd_status);
		}
		return SCN_REACHABLE_UNKNOWN;
	}

	_IsReachableInit(session, &services, &interfaces, &interfaceOrder, &defaultRoute);
	scn_status = checkAddress(session,
				  address,
				  addrlen,
				  services,
				  interfaces,
				  interfaceOrder,
				  defaultRoute,
				  flags,
				  errorMessage);
	_IsReachableFree(services, interfaces, interfaceOrder, defaultRoute);

	(void) SCDClose(&session);
	return scn_status;
}


SCNStatus
SCNIsReachableByName(const char		*nodename,
		     int		*flags,
		     const char		**errorMessage)
{
	struct in_addr	*defaultRoute	= NULL;
	struct hostent	*h;
	int		i;
	CFDictionaryRef	interfaces	= NULL;
	CFArrayRef	interfaceOrder	= NULL;
	SCDStatus	scd_status	= SCD_OK;
	SCNStatus	ns_status	= SCN_REACHABLE_YES;
	struct addrinfo	*res		= NULL;
	struct addrinfo *resP;
	CFDictionaryRef	services	= NULL;
	SCDSessionRef	session		= NULL;
	SCNStatus	scn_status	= SCN_REACHABLE_YES;

	scd_status = SCDOpen(&session, CFSTR("SCNIsReachableByName"));
	if (scd_status != SCD_OK) {
		scn_status = SCN_REACHABLE_UNKNOWN;
		if (errorMessage != NULL) {
			*errorMessage = SCDError(scd_status);
		}
		goto done;
	}

	_IsReachableInit(session, &services, &interfaces, &interfaceOrder, &defaultRoute);

	/*
	 * since we don't know which name server will be consulted
	 * to resolve the specified nodename we need to check the
	 * availability of ALL name servers.
	 */
	res_init();
	for (i=0; i<_res.nscount; i++) {
		ns_status = checkAddress(session,
					 (struct sockaddr *)&_res.nsaddr_list[i],
					 _res.nsaddr_list[i].sin_len,
					 services,
					 interfaces,
					 interfaceOrder,
					 defaultRoute,
					 flags,
					 errorMessage);
		if (ns_status < scn_status) {
			/* return the worst case result */
			scn_status = ns_status;
			if (ns_status == SCN_REACHABLE_UNKNOWN) {
				/* not today */
				break;
			}
		}
	}

	if (ns_status < SCN_REACHABLE_YES) {
		goto done;
	}

	/*
	 * OK, all of the DNS name servers are available.  Let's
	 * first assume that the requested host is NOT available,
	 * resolve the nodename, and check its address for
	 * accessibility. We return the best status available.
	 */
	scn_status = SCN_REACHABLE_UNKNOWN;

	/*
	 * resolve the nodename into an address
	 */
	i = getaddrinfo(nodename, NULL, NULL, &res);
	if (i != 0) {
		SCDSessionLog(session,
			      LOG_ERR,
			      CFSTR("getaddrinfo() failed: %s"),
			      gai_strerror(i));
		goto done;
	}

	for (resP=res; resP!=NULL; resP=resP->ai_next) {
		ns_status = checkAddress(session,
					 resP->ai_addr,
					 resP->ai_addrlen,
					 services,
					 interfaces,
					 interfaceOrder,
					 defaultRoute,
					 flags,
					 errorMessage);
		if (ns_status > scn_status) {
			/* return the best case result */
			scn_status = ns_status;
			if (ns_status == SCN_REACHABLE_YES) {
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
	if (SCDOptionGet(session, kSCDOptionDebug))
		SCDLog(LOG_INFO, CFSTR("getaddrinfo() returned no addresses, try gethostbyname()"));
#endif	/* DEBUG */

	h = gethostbyname(nodename);
	if (h && h->h_length) {
		struct in_addr **s	= (struct in_addr **)h->h_addr_list;

		while (*s) {   
			struct sockaddr_in	sa;

			bzero(&sa, sizeof(sa));
			sa.sin_len    = sizeof(sa);
			sa.sin_family = AF_INET;
			sa.sin_addr   = **s;

			ns_status = checkAddress(session,
						 (struct sockaddr *)&sa,
						 sizeof(sa),
						 services,
						 interfaces,
						 interfaceOrder,
						 defaultRoute,
						 flags,
						 errorMessage);
			if (ns_status > scn_status) {
				/* return the best case result */
				scn_status = ns_status;
				if (ns_status == SCN_REACHABLE_YES) {
					/* we're in luck */
					break;
				}
			}

			s++;
		}
	}

    done :

	_IsReachableFree(services, interfaces, interfaceOrder, defaultRoute);
	if (session)	(void)SCDClose(&session);
	if (res)	freeaddrinfo(res);

	return scn_status;
}
