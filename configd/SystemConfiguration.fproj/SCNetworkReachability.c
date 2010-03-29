/*
 * Copyright (c) 2003-2009 Apple Inc. All rights reserved.
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
 * March 31, 2004		Allan Nathanson <ajn@apple.com>
 * - use [SC] DNS configuration information
 *
 * January 19, 2003		Allan Nathanson <ajn@apple.com>
 * - add advanced reachability APIs
 */

#include <Availability.h>
#include <TargetConditionals.h>
#include <sys/cdefs.h>
#if	!TARGET_OS_IPHONE
#include <dispatch/dispatch.h>
#endif	// !TARGET_OS_IPHONE
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>
#include <pthread.h>
#include <libkern/OSAtomic.h>

#if	!TARGET_OS_IPHONE
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#endif	// !TARGET_OS_IPHONE

#include <notify.h>
#include <dnsinfo.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netdb_async.h>
#include <resolv.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#define	KERNEL_PRIVATE
#include <net/route.h>
#undef	KERNEL_PRIVATE

#ifndef s6_addr16
#define s6_addr16 __u6_addr.__u6_addr16
#endif

#include <ppp/ppp_msg.h>



#define kSCNetworkReachabilityFlagsFirstResolvePending	(1<<31)


#define	N_QUICK	32


typedef enum {
	reachabilityTypeAddress,
	reachabilityTypeAddressPair,
	reachabilityTypeName
} addressType;


static CFStringRef	__SCNetworkReachabilityCopyDescription	(CFTypeRef cf);
static void		__SCNetworkReachabilityDeallocate	(CFTypeRef cf);
static void		rlsPerform(void *info);


static Boolean
__SCNetworkReachabilityScheduleWithRunLoop	(SCNetworkReachabilityRef	target,
						 CFRunLoopRef			runLoop,
						 CFStringRef			runLoopMode,
#if	!TARGET_OS_IPHONE
						 dispatch_queue_t		queue,
#else	// !TARGET_OS_IPHONE
						 void				*queue,
#endif	// !TARGET_OS_IPHONE
						 Boolean			onDemand);
static Boolean
__SCNetworkReachabilityUnscheduleFromRunLoop	(SCNetworkReachabilityRef	target,
						 CFRunLoopRef			runLoop,
						 CFStringRef			runLoopMode,
						 Boolean			onDemand);


typedef struct {
	SCNetworkReachabilityFlags	flags;
	uint16_t			if_index;
	Boolean				sleeping;
} ReachabilityInfo;


typedef struct {

	/* base CFType information */
	CFRuntimeBase			cfBase;

	/* lock */
	pthread_mutex_t			lock;

	/* address type */
	addressType			type;

	/* target host name */
	const char			*name;
	const char			*serv;
	struct addrinfo			hints;
	Boolean				needResolve;
	CFArrayRef			resolvedAddress;	/* CFArray[CFData] */
	int				resolvedAddressError;

	/* local & remote addresses */
	struct sockaddr			*localAddress;
	struct sockaddr			*remoteAddress;

	/* current reachability flags */
	ReachabilityInfo		info;
	ReachabilityInfo		last_notify;

	/* run loop source, callout, context, rl scheduling info */
	Boolean				scheduled;
	CFRunLoopSourceRef		rls;
	SCNetworkReachabilityCallBack	rlsFunction;
	SCNetworkReachabilityContext	rlsContext;
	CFMutableArrayRef		rlList;

#if	!TARGET_OS_IPHONE
	dispatch_queue_t		dispatchQueue;		// SCNetworkReachabilitySetDispatchQueue
	dispatch_queue_t		asyncDNSQueue;
	dispatch_source_t		asyncDNSSource;
#endif	// !TARGET_OS_IPHONE

	/* [async] DNS query info */
	Boolean				haveDNS;
	mach_port_t			dnsMP;
	CFMachPortRef			dnsPort;
	CFRunLoopSourceRef		dnsRLS;
	struct timeval			dnsQueryStart;

	/* on demand info */
	Boolean				onDemandBypass;
	CFStringRef			onDemandName;
	CFStringRef			onDemandRemoteAddress;
	SCNetworkReachabilityRef	onDemandServer;
	CFStringRef			onDemandServiceID;

	/* logging */
	char				log_prefix[32];

} SCNetworkReachabilityPrivate, *SCNetworkReachabilityPrivateRef;


static CFTypeID __kSCNetworkReachabilityTypeID	= _kCFRuntimeNotATypeID;


static const CFRuntimeClass __SCNetworkReachabilityClass = {
	0,					// version
	"SCNetworkReachability",		// className
	NULL,					// init
	NULL,					// copy
	__SCNetworkReachabilityDeallocate,	// dealloc
	NULL,					// equal
	NULL,					// hash
	NULL,					// copyFormattingDesc
	__SCNetworkReachabilityCopyDescription	// copyDebugDesc
};


static pthread_once_t		initialized	= PTHREAD_ONCE_INIT;
static ReachabilityInfo		NOT_REACHABLE	= { 0,		0,	FALSE };
static ReachabilityInfo		NOT_REPORTED	= { 0xFFFFFFFF,	0,	FALSE };
static int			rtm_seq		= 0;


#if	!TARGET_OS_IPHONE
/*
 * Power capabilities (sleep/wake)
 */
static IOPMSystemPowerStateCapabilities	power_capabilities	= kIOPMSytemPowerStateCapabilitiesMask;
#endif	// !TARGET_OS_IPHONE


/*
 * host "something has changed" notifications
 */

static pthread_mutex_t		hn_lock		= PTHREAD_MUTEX_INITIALIZER;
static SCDynamicStoreRef	hn_store	= NULL;
#if	!TARGET_OS_IPHONE
static dispatch_queue_t		hn_dispatchQueue = NULL;
#else	// !TARGET_OS_IPHONE
static CFRunLoopSourceRef	hn_storeRLS	= NULL;
static CFMutableArrayRef	hn_rlList	= NULL;
#endif	// !TARGET_OS_IPHONE
static CFMutableSetRef		hn_targets	= NULL;


/*
 * DNS configuration
 */

typedef struct {
	dns_config_t	*config;
	int		refs;
} dns_configuration_t;


static pthread_mutex_t		dns_lock		= PTHREAD_MUTEX_INITIALIZER;
static dns_configuration_t	*dns_configuration	= NULL;
static int			dns_token;
static Boolean			dns_token_valid		= FALSE;


static __inline__ CFTypeRef
isA_SCNetworkReachability(CFTypeRef obj)
{
	return (isA_CFType(obj, SCNetworkReachabilityGetTypeID()));
}


static void
__log_query_time(SCNetworkReachabilityRef target, Boolean found, Boolean async, struct timeval *start)
{
	struct timeval			dnsQueryComplete;
	struct timeval			dnsQueryElapsed;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	if (!_sc_debug) {
		return;
	}

	if (start->tv_sec == 0) {
		return;
	}

	(void) gettimeofday(&dnsQueryComplete, NULL);
	timersub(&dnsQueryComplete, start, &dnsQueryElapsed);
	SCLog(TRUE, LOG_INFO,
	      CFSTR("%s%ssync DNS complete%s (query time = %d.%3.3d)"),
	      targetPrivate->log_prefix,
	      async ? "a" : "",
	      found ? "" : ", host not found",
	      dnsQueryElapsed.tv_sec,
	      dnsQueryElapsed.tv_usec / 1000);

	return;
}


static int
updatePPPStatus(SCDynamicStoreRef		*storeP,
		const struct sockaddr		*sa,
		const char			*if_name,
		SCNetworkReachabilityFlags	*flags,
		CFStringRef			*ppp_server,
		const char			*log_prefix)
{
	CFDictionaryRef		dict		= NULL;
	CFStringRef		entity;
	CFIndex			i;
	const void *		keys_q[N_QUICK];
	const void **		keys		= keys_q;
	CFIndex			n;
	CFStringRef		ppp_if;
	int			sc_status	= kSCStatusReachabilityUnknown;
	SCDynamicStoreRef	store		= *storeP;
	const void *		values_q[N_QUICK];
	const void **		values	= values_q;

	switch (sa->sa_family) {
		case AF_INET :
			entity = kSCEntNetIPv4;
			break;
		case AF_INET6 :
			entity = kSCEntNetIPv6;
			break;
		default :
			goto done;
	}

	if (store == NULL) {
		store = SCDynamicStoreCreate(NULL, CFSTR("SCNetworkReachability"), NULL, NULL);
		if (store == NULL) {
			SCLog(TRUE, LOG_ERR, CFSTR("updatePPPStatus SCDynamicStoreCreate() failed"));
			goto done;
		}
		*storeP = store;
	}

	// grab a snapshot of the PPP configuration from the dynamic store
	{
		CFStringRef		pattern;
		CFMutableArrayRef	patterns;

		patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								      kSCDynamicStoreDomainState,
								      kSCCompAnyRegex,
								      entity);
		CFArrayAppendValue(patterns, pattern);
		CFRelease(pattern);
		pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								      kSCDynamicStoreDomainSetup,
								      kSCCompAnyRegex,
								      kSCEntNetPPP);
		CFArrayAppendValue(patterns, pattern);
		CFRelease(pattern);
		pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								      kSCDynamicStoreDomainState,
								      kSCCompAnyRegex,
								      kSCEntNetPPP);
		CFArrayAppendValue(patterns, pattern);
		CFRelease(pattern);
		dict = SCDynamicStoreCopyMultiple(store, NULL, patterns);
		CFRelease(patterns);
	}
	if (dict == NULL) {
		// if we could not access the dynamic store
		goto done;
	}

	sc_status = kSCStatusOK;

	// look for the service which matches the provided interface
	n = CFDictionaryGetCount(dict);
	if (n <= 0) {
		goto done;
	}

	ppp_if = CFStringCreateWithCStringNoCopy(NULL,
						 if_name,
						 kCFStringEncodingASCII,
						 kCFAllocatorNull);

	if (n > (CFIndex)(sizeof(keys_q) / sizeof(CFTypeRef))) {
		keys   = CFAllocatorAllocate(NULL, n * sizeof(CFTypeRef), 0);
		values = CFAllocatorAllocate(NULL, n * sizeof(CFTypeRef), 0);
	}
	CFDictionaryGetKeysAndValues(dict, keys, values);

	for (i=0; i < n; i++) {
		CFArrayRef	components;
		CFStringRef	key;
		CFNumberRef	num;
		CFDictionaryRef	p_setup;
		CFDictionaryRef	p_state;
		int32_t		ppp_demand;
		int32_t		ppp_status;
		CFStringRef	service		= NULL;
		CFStringRef	s_key		= (CFStringRef)    keys[i];
		CFDictionaryRef	s_dict		= (CFDictionaryRef)values[i];
		CFStringRef	s_if;

		if (!isA_CFString(s_key) || !isA_CFDictionary(s_dict)) {
			continue;
		}

		if (!CFStringHasSuffix(s_key, entity)) {
			continue;	// if not an IPv4 or IPv6 entity
		}

		s_if = CFDictionaryGetValue(s_dict, kSCPropInterfaceName);
		if (!isA_CFString(s_if)) {
			continue;	// if no interface
		}

		if (!CFEqual(ppp_if, s_if)) {
			continue;	// if not this interface
		}

		// extract the service ID, get the PPP "state" entity for
		// the "Status", and get the PPP "setup" entity for the
		// the "DialOnDemand" flag
		components = CFStringCreateArrayBySeparatingStrings(NULL, s_key, CFSTR("/"));
		if (CFArrayGetCount(components) != 5) {
			CFRelease(components);
			break;
		}
		service = CFArrayGetValueAtIndex(components, 3);
		key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								  kSCDynamicStoreDomainState,
								  service,
								  kSCEntNetPPP);
		p_state = CFDictionaryGetValue(dict, key);
		CFRelease(key);
		key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								  kSCDynamicStoreDomainSetup,
								  service,
								  kSCEntNetPPP);
		p_setup = CFDictionaryGetValue(dict, key);
		CFRelease(key);
		CFRelease(components);

		// ensure that this is a PPP service
		if (!isA_CFDictionary(p_state)) {
			break;
		}

		*flags |= kSCNetworkReachabilityFlagsTransientConnection;

		// get PPP server
		if (ppp_server != NULL) {
			*ppp_server = CFDictionaryGetValue(s_dict, CFSTR("ServerAddress"));
			*ppp_server = isA_CFString(*ppp_server);
			if (*ppp_server != NULL) {
				CFRetain(*ppp_server);
			}
		}

		// get PPP status
		if (!CFDictionaryGetValueIfPresent(p_state,
						   kSCPropNetPPPStatus,
						   (const void **)&num) ||
		    !isA_CFNumber(num) ||
		    !CFNumberGetValue(num, kCFNumberSInt32Type, &ppp_status)) {
			break;
		}
		switch (ppp_status) {
			case PPP_RUNNING :
				// if we're really UP and RUNNING
				break;
			case PPP_ONHOLD :
				// if we're effectively UP and RUNNING
				break;
			case PPP_IDLE :
			case PPP_STATERESERVED :
				// if we're not connected at all
				SCLog(_sc_debug, LOG_INFO, CFSTR("%s  PPP link idle, dial-on-traffic to connect"),
				      log_prefix);
				*flags |= kSCNetworkReachabilityFlagsConnectionRequired;
				break;
			default :
				// if we're in the process of [dis]connecting
				SCLog(_sc_debug, LOG_INFO, CFSTR("%s  PPP link, connection in progress"),
				      log_prefix);
				*flags |= kSCNetworkReachabilityFlagsConnectionRequired;
				break;
		}

		// get PPP dial-on-traffic status
		if (isA_CFDictionary(p_setup) &&
		    CFDictionaryGetValueIfPresent(p_setup,
						  kSCPropNetPPPDialOnDemand,
						  (const void **)&num) &&
		    isA_CFNumber(num) &&
		    CFNumberGetValue(num, kCFNumberSInt32Type, &ppp_demand) &&
		    (ppp_demand != 0)) {
			*flags |= kSCNetworkReachabilityFlagsConnectionOnTraffic;
			if (ppp_status == PPP_IDLE) {
				*flags |= kSCNetworkReachabilityFlagsInterventionRequired;
			}
		}

		break;
	}

	CFRelease(ppp_if);
	if (keys != keys_q) {
		CFAllocatorDeallocate(NULL, keys);
		CFAllocatorDeallocate(NULL, values);
	}

    done :

	if (dict != NULL)	CFRelease(dict);
	return sc_status;
}


static int
updatePPPAvailable(SCDynamicStoreRef		*storeP,
		   const struct sockaddr	*sa,
		   SCNetworkReachabilityFlags	*flags,
		   const char			*log_prefix)
{
	CFDictionaryRef		dict		= NULL;
	CFStringRef		entity;
	CFIndex			i;
	const void *		keys_q[N_QUICK];
	const void **		keys		= keys_q;
	CFIndex			n;
	int			sc_status	= kSCStatusReachabilityUnknown;
	SCDynamicStoreRef	store		= *storeP;
	const void *		values_q[N_QUICK];
	const void **		values	= values_q;

	if (sa == NULL) {
		entity = kSCEntNetIPv4;
	} else {
		switch (sa->sa_family) {
			case AF_INET :
				entity = kSCEntNetIPv4;
				break;
			case AF_INET6 :
				entity = kSCEntNetIPv6;
				break;
			default :
				goto done;
		}
	}

	if (store == NULL) {
		store = SCDynamicStoreCreate(NULL, CFSTR("SCNetworkReachability"), NULL, NULL);
		if (store == NULL) {
			SCLog(TRUE, LOG_ERR, CFSTR("updatePPPAvailable SCDynamicStoreCreate() failed"));
			goto done;
		}
		*storeP = store;
	}

	// grab a snapshot of the PPP configuration from the dynamic store
	{
		CFStringRef		pattern;
		CFMutableArrayRef	patterns;

		patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								      kSCDynamicStoreDomainSetup,
								      kSCCompAnyRegex,
								      entity);
		CFArrayAppendValue(patterns, pattern);
		CFRelease(pattern);
		pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								      kSCDynamicStoreDomainSetup,
								      kSCCompAnyRegex,
								      kSCEntNetInterface);
		CFArrayAppendValue(patterns, pattern);
		CFRelease(pattern);
		pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								      kSCDynamicStoreDomainSetup,
								      kSCCompAnyRegex,
								      kSCEntNetPPP);
		CFArrayAppendValue(patterns, pattern);
		CFRelease(pattern);
		dict = SCDynamicStoreCopyMultiple(store, NULL, patterns);
		CFRelease(patterns);
	}
	if (dict == NULL) {
		// if we could not access the dynamic store
		goto done;
	}

	sc_status = kSCStatusOK;

	// look for an available service which will provide connectivity
	// for the requested address family.
	n = CFDictionaryGetCount(dict);
	if (n <= 0) {
		goto done;
	}

	if (n > (CFIndex)(sizeof(keys_q) / sizeof(CFTypeRef))) {
		keys   = CFAllocatorAllocate(NULL, n * sizeof(CFTypeRef), 0);
		values = CFAllocatorAllocate(NULL, n * sizeof(CFTypeRef), 0);
	}
	CFDictionaryGetKeysAndValues(dict, keys, values);

	for (i = 0; i < n; i++) {
		CFArrayRef	components;
		Boolean		found		= FALSE;
		CFStringRef	i_key;
		CFDictionaryRef	i_dict;
		CFStringRef	p_key;
		CFDictionaryRef	p_dict;
		CFStringRef	service;
		CFStringRef	s_key		= (CFStringRef)    keys[i];
		CFDictionaryRef	s_dict		= (CFDictionaryRef)values[i];

		if (!isA_CFString(s_key) || !isA_CFDictionary(s_dict)) {
			continue;
		}

		if (!CFStringHasSuffix(s_key, entity)) {
			continue;	// if not an IPv4 or IPv6 entity
		}

		// extract service ID
		components = CFStringCreateArrayBySeparatingStrings(NULL, s_key, CFSTR("/"));
		if (CFArrayGetCount(components) != 5) {
			CFRelease(components);
			continue;
		}
		service = CFArrayGetValueAtIndex(components, 3);

		// check for [non-VPN] PPP entity
		p_key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								    kSCDynamicStoreDomainSetup,
								    service,
								    kSCEntNetPPP);
		p_dict = CFDictionaryGetValue(dict, p_key);
		CFRelease(p_key);

		i_key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								    kSCDynamicStoreDomainSetup,
								    service,
								    kSCEntNetInterface);
		i_dict = CFDictionaryGetValue(dict, i_key);
		CFRelease(i_key);

		if (isA_CFDictionary(p_dict) &&
		    isA_CFDictionary(i_dict) &&
		    CFDictionaryContainsKey(i_dict, kSCPropNetInterfaceDeviceName)) {
			CFNumberRef	num;

			// we have a PPP service for this address family
			found = TRUE;

			*flags |= kSCNetworkReachabilityFlagsReachable;
			*flags |= kSCNetworkReachabilityFlagsTransientConnection;
			*flags |= kSCNetworkReachabilityFlagsConnectionRequired;

			// get PPP dial-on-traffic status
			num = CFDictionaryGetValue(p_dict, kSCPropNetPPPDialOnDemand);
			if (isA_CFNumber(num)) {
				int32_t	ppp_demand;

				if (CFNumberGetValue(num, kCFNumberSInt32Type, &ppp_demand)) {
					if (ppp_demand) {
						*flags |= kSCNetworkReachabilityFlagsConnectionOnTraffic;
					}
				}
			}

			if (_sc_debug) {
				SCLog(TRUE, LOG_INFO, CFSTR("%s  status    = isReachable (after connect)"),
				      log_prefix);
				SCLog(TRUE, LOG_INFO, CFSTR("%s  service   = %@"),
				      log_prefix,
				      service);
			}

		}

		CFRelease(components);

		if (found) {
			break;
		}
	}

	if (keys != keys_q) {
		CFAllocatorDeallocate(NULL, keys);
		CFAllocatorDeallocate(NULL, values);
	}

    done :

	if (dict != NULL)	CFRelease(dict);
	return sc_status;
}


static int
updateIPSecStatus(SCDynamicStoreRef		*storeP,
		  const struct sockaddr		*sa,
		  const char			*if_name,
		  SCNetworkReachabilityFlags	*flags,
		  CFStringRef			*ipsec_server)
{
	CFDictionaryRef		dict		= NULL;
	CFStringRef		entity;
	CFIndex			i;
	CFStringRef		ipsec_if;
	const void *		keys_q[N_QUICK];
	const void **		keys		= keys_q;
	CFIndex			n;
	int			sc_status	= kSCStatusReachabilityUnknown;
	SCDynamicStoreRef	store		= *storeP;
	const void *		values_q[N_QUICK];
	const void **		values	= values_q;

	switch (sa->sa_family) {
		case AF_INET :
			entity = kSCEntNetIPv4;
			break;
		case AF_INET6 :
			entity = kSCEntNetIPv6;
			break;
		default :
			goto done;
	}

	if (store == NULL) {
		store = SCDynamicStoreCreate(NULL, CFSTR("SCNetworkReachability"), NULL, NULL);
		if (store == NULL) {
			SCLog(TRUE, LOG_ERR, CFSTR("updateIPSecStatus SCDynamicStoreCreate() failed"));
			goto done;
		}
		*storeP = store;
	}

	// grab a snapshot of the IPSec configuration from the dynamic store
	{
		CFStringRef		pattern;
		CFMutableArrayRef	patterns;

		patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								      kSCDynamicStoreDomainState,
								      kSCCompAnyRegex,
								      entity);
		CFArrayAppendValue(patterns, pattern);
		CFRelease(pattern);
		pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								      kSCDynamicStoreDomainSetup,
								      kSCCompAnyRegex,
								      kSCEntNetIPSec);
		CFArrayAppendValue(patterns, pattern);
		CFRelease(pattern);
		dict = SCDynamicStoreCopyMultiple(store, NULL, patterns);
		CFRelease(patterns);
	}
	if (dict == NULL) {
		// if we could not access the dynamic store
		goto done;
	}

	sc_status = kSCStatusOK;

	// look for the service which matches the provided interface
	n = CFDictionaryGetCount(dict);
	if (n <= 0) {
		goto done;
	}

	ipsec_if = CFStringCreateWithCStringNoCopy(NULL,
						   if_name,
						   kCFStringEncodingASCII,
						   kCFAllocatorNull);

	if (n > (CFIndex)(sizeof(keys_q) / sizeof(CFTypeRef))) {
		keys   = CFAllocatorAllocate(NULL, n * sizeof(CFTypeRef), 0);
		values = CFAllocatorAllocate(NULL, n * sizeof(CFTypeRef), 0);
	}
	CFDictionaryGetKeysAndValues(dict, keys, values);

	for (i=0; i < n; i++) {
		CFArrayRef	components;
		CFStringRef	key;
		CFDictionaryRef	i_setup;
		CFStringRef	service		= NULL;
		CFStringRef	s_key		= (CFStringRef)    keys[i];
		CFDictionaryRef	s_dict		= (CFDictionaryRef)values[i];
		CFStringRef	s_if;

		if (!isA_CFString(s_key) || !isA_CFDictionary(s_dict)) {
			continue;
		}

		if (!CFStringHasSuffix(s_key, entity)) {
			continue;	// if not an IPv4 or IPv6 entity
		}

		s_if = CFDictionaryGetValue(s_dict, kSCPropInterfaceName);
		if (!isA_CFString(s_if)) {
			continue;	// if no interface
		}

		if (!CFEqual(ipsec_if, s_if)) {
			continue;	// if not this interface
		}

		// extract the service ID and get the IPSec "setup" entity
		// to confirm that we're looking at what we're expecting
		components = CFStringCreateArrayBySeparatingStrings(NULL, s_key, CFSTR("/"));
		if (CFArrayGetCount(components) != 5) {
			CFRelease(components);
			break;
		}
		service = CFArrayGetValueAtIndex(components, 3);
		key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								  kSCDynamicStoreDomainSetup,
								  service,
								  kSCEntNetIPSec);
		i_setup = CFDictionaryGetValue(dict, key);
		CFRelease(key);
		CFRelease(components);

		// ensure that this is an IPSec service
		if (!isA_CFDictionary(i_setup)) {
			break;
		}

		*flags |= kSCNetworkReachabilityFlagsTransientConnection;

		// get IPSec server
		if (ipsec_server != NULL) {
			*ipsec_server = CFDictionaryGetValue(s_dict, CFSTR("ServerAddress"));
			*ipsec_server = isA_CFString(*ipsec_server);
			if (*ipsec_server != NULL) {
				CFRetain(*ipsec_server);
			}
		}

		break;
	}

	CFRelease(ipsec_if);
	if (keys != keys_q) {
		CFAllocatorDeallocate(NULL, keys);
		CFAllocatorDeallocate(NULL, values);
	}

    done :

	if (dict != NULL)	CFRelease(dict);
	return sc_status;
}




#define ROUNDUP(a, size) \
	(((a) & ((size)-1)) ? (1 + ((a) | ((size)-1))) : (a))

#define NEXT_SA(ap) (ap) = (struct sockaddr *) \
	((caddr_t)(ap) + ((ap)->sa_len ? ROUNDUP((ap)->sa_len,\
						 sizeof(uint32_t)) :\
						 sizeof(uint32_t)))

static void
get_rtaddrs(int addrs, struct sockaddr *sa, struct sockaddr **rti_info)
{
	int             i;

	for (i = 0; i < RTAX_MAX; i++) {
		if (addrs & (1 << i)) {
			rti_info[i] = sa;
			NEXT_SA(sa);
		} else
			rti_info[i] = NULL;
	}
}


#define BUFLEN (sizeof(struct rt_msghdr) + 512)	/* 8 * sizeof(struct sockaddr_in6) = 192 */


typedef struct {
	char			buf[BUFLEN];
	int			error;
	struct sockaddr         *rti_info[RTAX_MAX];
	struct rt_msghdr	*rtm;
	struct sockaddr_dl	*sdl;
} route_info, *route_info_p;


/*
 * route_get()
 *	returns	zero if route exists an data returned, EHOSTUNREACH
 *	if no route, or errno for any other error.
 */
static int
route_get(const struct sockaddr	*address,
	  route_info		*info)
{
	int			n;
	pid_t			pid		= getpid();
	int			rsock;
	struct sockaddr         *sa;
	int32_t			seq		= OSAtomicIncrement32Barrier(&rtm_seq);
#ifndef	RTM_GET_SILENT
#warning Note: Using RTM_GET (and not RTM_GET_SILENT)
	static pthread_mutex_t	lock		= PTHREAD_MUTEX_INITIALIZER;
	int			sosize		= 48 * 1024;
#endif

	bzero(info, sizeof(*info));

	info->rtm = (struct rt_msghdr *)&info->buf;
	info->rtm->rtm_msglen  = sizeof(struct rt_msghdr);
	info->rtm->rtm_version = RTM_VERSION;
#ifdef	RTM_GET_SILENT
	info->rtm->rtm_type    = RTM_GET_SILENT;
#else
	info->rtm->rtm_type    = RTM_GET;
#endif
	info->rtm->rtm_flags   = RTF_STATIC|RTF_UP|RTF_HOST|RTF_GATEWAY;
	info->rtm->rtm_addrs   = RTA_DST|RTA_IFP; /* Both destination and device */
	info->rtm->rtm_pid     = pid;
	info->rtm->rtm_seq     = seq;

	switch (address->sa_family) {
		case AF_INET6: {
			struct sockaddr_in6	*sin6;

			sin6 = (struct sockaddr_in6 *)address;
			if ((IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr) ||
			     IN6_IS_ADDR_MC_LINKLOCAL(&sin6->sin6_addr)) &&
			    (sin6->sin6_scope_id != 0)) {
				sin6->sin6_addr.s6_addr16[1] = htons(sin6->sin6_scope_id);
				sin6->sin6_scope_id = 0;
			}
			break;
		}
	}

	sa  = (struct sockaddr *) (info->rtm + 1);
	bcopy(address, sa, address->sa_len);
	n = ROUNDUP(sa->sa_len, sizeof(uint32_t));
	info->rtm->rtm_msglen += n;

	info->sdl = (struct sockaddr_dl *) ((void *)sa + n);
	info->sdl->sdl_family = AF_LINK;
	info->sdl->sdl_len = sizeof (struct sockaddr_dl);
	n = ROUNDUP(info->sdl->sdl_len, sizeof(uint32_t));
	info->rtm->rtm_msglen += n;

#ifndef	RTM_GET_SILENT
	pthread_mutex_lock(&lock);
#endif
	rsock = socket(PF_ROUTE, SOCK_RAW, 0);
	if (rsock == -1) {
		int	error	= errno;

#ifndef	RTM_GET_SILENT
		pthread_mutex_unlock(&lock);
#endif
		SCLog(TRUE, LOG_ERR, CFSTR("socket(PF_ROUTE) failed: %s"), strerror(error));
		return error;
	}

#ifndef	RTM_GET_SILENT
	if (setsockopt(rsock, SOL_SOCKET, SO_RCVBUF, &sosize, sizeof(sosize)) == -1) {
		int	error	= errno;

		(void)close(rsock);
		pthread_mutex_unlock(&lock);
		SCLog(TRUE, LOG_ERR, CFSTR("setsockopt(SO_RCVBUF) failed: %s"), strerror(error));
		return error;
	}
#endif

	if (write(rsock, &info->buf, info->rtm->rtm_msglen) == -1) {
		int	error	= errno;

		(void)close(rsock);
#ifndef	RTM_GET_SILENT
		pthread_mutex_unlock(&lock);
#endif
		if (error != ESRCH) {
			SCLog(TRUE, LOG_ERR, CFSTR("write() failed: %s"), strerror(error));
			return error;
		}
		return EHOSTUNREACH;
	}

	/*
	 * Type, seq, pid identify our response.
	 * Routing sockets are broadcasters on input.
	 */
	do {
		int	n;

		n = read(rsock, (void *)&info->buf, sizeof(info->buf));
		if (n == -1) {
			if (errno != EINTR) {
				int	error	= errno;

				(void)close(rsock);
#ifndef	RTM_GET_SILENT
				pthread_mutex_unlock(&lock);
#endif
				SCLog(TRUE, LOG_ERR, CFSTR("read() failed: %s"), strerror(error));
				return error;
			}
		}
	} while ((info->rtm->rtm_type != RTM_GET)	||
		 (info->rtm->rtm_seq  != seq)		||
		 (info->rtm->rtm_pid  != pid));

	(void)close(rsock);
#ifndef	RTM_GET_SILENT
	pthread_mutex_unlock(&lock);
#endif

	get_rtaddrs(info->rtm->rtm_addrs, sa, info->rti_info);

#ifdef	LOG_RTADDRS
	{
		int	i;

		SCLog(_sc_debug, LOG_DEBUG, CFSTR("rtm_flags = 0x%8.8x"), info->rtm->rtm_flags);

		if ((info->rti_info[RTAX_NETMASK] != NULL) && (info->rti_info[RTAX_DST] != NULL)) {
			info->rti_info[RTAX_NETMASK]->sa_family = info->rti_info[RTAX_DST]->sa_family;
		}

		for (i = 0; i < RTAX_MAX; i++) {
			if (info->rti_info[i] != NULL) {
				char	addr[128];

				_SC_sockaddr_to_string(info->rti_info[i], addr, sizeof(addr));
				SCLog(_sc_debug, LOG_DEBUG, CFSTR("%d: %s"), i, addr);
			}
		}
	}
#endif	/* LOG_RTADDRS */

	if ((info->rti_info[RTAX_IFP] == NULL) ||
	    (info->rti_info[RTAX_IFP]->sa_family != AF_LINK)) {
		/* no interface info */
		SCLog(TRUE, LOG_DEBUG, CFSTR("route_get() no interface info"));
		return EINVAL;
	}

	info->sdl = (struct sockaddr_dl *) info->rti_info[RTAX_IFP];
	if ((info->sdl->sdl_nlen == 0) || (info->sdl->sdl_nlen > IFNAMSIZ)) {
		/* no interface name */
		return EHOSTUNREACH;
	}

	return 0;
}


static Boolean
checkAddress(SCDynamicStoreRef		*storeP,
	     const struct sockaddr	*address,
	     ReachabilityInfo		*reach_info,
	     const char			*log_prefix)
{
	route_info		info;
	struct ifreq		ifr;
	char			if_name[IFNAMSIZ + 1];
	int			isock		= -1;
	int			ret;
	int			sc_status	= kSCStatusReachabilityUnknown;
	CFStringRef		server		= NULL;
	char			*statusMessage	= NULL;
	struct sockaddr_in	v4mapped;

	*reach_info = NOT_REACHABLE;

	if (address == NULL) {
		/* special case: check only for available paths off the system */
		goto checkAvailable;
	}

	switch (address->sa_family) {
		case AF_INET :
		case AF_INET6 :
			if (_sc_debug) {
				char	addr[128];

				_SC_sockaddr_to_string(address, addr, sizeof(addr));
				SCLog(TRUE, LOG_INFO, CFSTR("%scheckAddress(%s)"),
				      log_prefix,
				      addr);
			}
			break;
		default :
			/*
			 * if no code for this address family (yet)
			 */
			SCLog(TRUE, LOG_INFO,
			      CFSTR("checkAddress(): unexpected address family %d"),
			      address->sa_family);
			sc_status = kSCStatusInvalidArgument;
			goto done;
	}

	if (address->sa_family == AF_INET6) {
		struct sockaddr_in6	*sin6	= (struct sockaddr_in6 *)address;

		if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
			bzero(&v4mapped, sizeof(v4mapped));
			v4mapped.sin_len         = sizeof(v4mapped);
			v4mapped.sin_family      = AF_INET;
			v4mapped.sin_port        = sin6->sin6_port;
			v4mapped.sin_addr.s_addr = sin6->sin6_addr.__u6_addr.__u6_addr32[3];
			address = (struct sockaddr *)&v4mapped;
		}
	}

	ret = route_get(address, &info);
	switch (ret) {
		case 0 :
			break;
		case EHOSTUNREACH :
			// if no route
			goto checkAvailable;
		default :
			// if error
			sc_status = ret;
			goto done;
	}

	/* get the interface flags */

	isock = socket(AF_INET, SOCK_DGRAM, 0);
	if (isock == -1) {
		SCLog(TRUE, LOG_ERR, CFSTR("socket() failed: %s"), strerror(errno));
		goto done;
	}

	bzero(&ifr, sizeof(ifr));
	bcopy(info.sdl->sdl_data, ifr.ifr_name, info.sdl->sdl_nlen);

	if (ioctl(isock, SIOCGIFFLAGS, (char *)&ifr) == -1) {
		SCLog(TRUE, LOG_ERR, CFSTR("ioctl() failed: %s"), strerror(errno));
		goto done;
	}

	if (!(ifr.ifr_flags & IFF_UP)) {
		goto checkAvailable;
	}

	statusMessage = "isReachable";
	reach_info->flags |= kSCNetworkReachabilityFlagsReachable;

	if (info.rtm->rtm_flags & RTF_LOCAL) {
		statusMessage = "isReachable (is a local address)";
		reach_info->flags |= kSCNetworkReachabilityFlagsIsLocalAddress;
	} else if (ifr.ifr_flags & IFF_LOOPBACK) {
		statusMessage = "isReachable (is loopback network)";
		reach_info->flags |= kSCNetworkReachabilityFlagsIsLocalAddress;
	} else if ((info.rti_info[RTAX_IFA] != NULL) &&
		   (info.rti_info[RTAX_IFA]->sa_family != AF_LINK)) {
		void	*addr1	= (void *)address;
		void	*addr2	= (void *)info.rti_info[RTAX_IFA];
		size_t	len	= address->sa_len;

		if ((address->sa_family != info.rti_info[RTAX_IFA]->sa_family) &&
		    (address->sa_len    != info.rti_info[RTAX_IFA]->sa_len)) {
			SCLog(TRUE, LOG_NOTICE,
			      CFSTR("address family/length mismatch: %d/%d != %d/%d"),
			      address->sa_family,
			      address->sa_len,
			      info.rti_info[RTAX_IFA]->sa_family,
			      info.rti_info[RTAX_IFA]->sa_len);
			goto done;
		}

		switch (address->sa_family) {
			case AF_INET :
				addr1 = &((struct sockaddr_in *)address)->sin_addr;
				addr2 = &((struct sockaddr_in *)info.rti_info[RTAX_IFA])->sin_addr;
				len = sizeof(struct in_addr);

				/*
				 * check if 0.0.0.0
				 */
				if (((struct sockaddr_in *)address)->sin_addr.s_addr == 0) {
					statusMessage = "isReachable (this host)";
					reach_info->flags |= kSCNetworkReachabilityFlagsIsLocalAddress;
				}
				break;
			case AF_INET6 :
				addr1 = &((struct sockaddr_in6 *)address)->sin6_addr;
				addr2 = &((struct sockaddr_in6 *)info.rti_info[RTAX_IFA])->sin6_addr;
				len = sizeof(struct in6_addr);
				break;
			default :
				break;
		}

		if (bcmp(addr1, addr2, len) == 0) {
			statusMessage = "isReachable (is interface address)";
			reach_info->flags |= kSCNetworkReachabilityFlagsIsLocalAddress;
		}
	}

	if (!(info.rtm->rtm_flags & RTF_GATEWAY) &&
	    (info.rti_info[RTAX_GATEWAY] != NULL) &&
	    (info.rti_info[RTAX_GATEWAY]->sa_family == AF_LINK) &&
	    !(ifr.ifr_flags & IFF_POINTOPOINT)) {
		reach_info->flags |= kSCNetworkReachabilityFlagsIsDirect;
	}

	bzero(&if_name, sizeof(if_name));
	bcopy(info.sdl->sdl_data,
	      if_name,
	      (info.sdl->sdl_nlen <= IFNAMSIZ) ? info.sdl->sdl_nlen : IFNAMSIZ);

	reach_info->if_index = info.sdl->sdl_index;

	if (_sc_debug) {
		SCLog(TRUE, LOG_INFO, CFSTR("%s  status    = %s"), log_prefix, statusMessage);
		SCLog(TRUE, LOG_INFO, CFSTR("%s  device    = %s (%hu)"), log_prefix, if_name, info.sdl->sdl_index);
		SCLog(TRUE, LOG_INFO, CFSTR("%s  sdl_type  = 0x%x"), log_prefix, info.sdl->sdl_type);
		SCLog(TRUE, LOG_INFO, CFSTR("%s  ifr_flags = 0x%04hx"), log_prefix, ifr.ifr_flags);
		SCLog(TRUE, LOG_INFO, CFSTR("%s  rtm_flags = 0x%08x"), log_prefix, info.rtm->rtm_flags);
	}

	sc_status = kSCStatusOK;

	if (ifr.ifr_flags & IFF_POINTOPOINT) {
		reach_info->flags |= kSCNetworkReachabilityFlagsTransientConnection;
	}

	if (info.sdl->sdl_type == IFT_PPP) {
		/*
		 * 1. check if PPP service
		 * 2. check for dial-on-demand PPP link that is not yet connected
		 * 3. get PPP server address
		 */
		sc_status = updatePPPStatus(storeP, address, if_name, &reach_info->flags, &server, log_prefix);
	} else if (info.sdl->sdl_type == IFT_OTHER) {
		/*
		 * 1. check if IPSec service
		 * 2. get IPSec server address
		 */
		sc_status = updateIPSecStatus(storeP, address, if_name, &reach_info->flags, &server);
	}


	goto done;

    checkAvailable :


	sc_status = updatePPPAvailable(storeP, address, &reach_info->flags, log_prefix);

    done :

	if (reach_info->flags == 0) {
		SCLog(_sc_debug, LOG_INFO, CFSTR("%s  cannot be reached"), log_prefix);
	}

	if (isock != -1)	(void)close(isock);
	if (server != NULL)	CFRelease(server);
	if (sc_status != kSCStatusOK) {
		_SCErrorSet(sc_status);
		return FALSE;
	}

	return TRUE;
}


#pragma mark -
#pragma mark SCNetworkReachability APIs


static CFStringRef
__SCNetworkReachabilityCopyDescription(CFTypeRef cf)
{
	CFAllocatorRef			allocator	= CFGetAllocator(cf);
	CFMutableStringRef		result;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)cf;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<SCNetworkReachability %p [%p]> {"), cf, allocator);
	switch (targetPrivate->type) {
		case reachabilityTypeAddress :
		case reachabilityTypeAddressPair : {
			char		buf[64];

			if (targetPrivate->localAddress != NULL) {
				_SC_sockaddr_to_string(targetPrivate->localAddress, buf, sizeof(buf));
				CFStringAppendFormat(result, NULL, CFSTR("local address = %s"),
						     buf);
			}

			if (targetPrivate->remoteAddress != NULL) {
				_SC_sockaddr_to_string(targetPrivate->remoteAddress, buf, sizeof(buf));
				CFStringAppendFormat(result, NULL, CFSTR("%s%saddress = %s"),
						     targetPrivate->localAddress ? ", " : "",
						     (targetPrivate->type == reachabilityTypeAddressPair) ? "remote " : "",
						     buf);
			}
			break;
		}
		case reachabilityTypeName : {
			if ((targetPrivate->name != NULL)) {
				CFStringAppendFormat(result, NULL, CFSTR("name = %s"), targetPrivate->name);
			}
			if ((targetPrivate->serv != NULL)) {
				CFStringAppendFormat(result, NULL, CFSTR("%sserv = %s"),
						     targetPrivate->name != NULL ? ", " : "",
						     targetPrivate->serv);
			}
			if ((targetPrivate->resolvedAddress != NULL) || (targetPrivate->resolvedAddressError != NETDB_SUCCESS)) {
				if (targetPrivate->resolvedAddress != NULL) {
					if (isA_CFArray(targetPrivate->resolvedAddress)) {
						CFIndex	i;
						CFIndex	n	= CFArrayGetCount(targetPrivate->resolvedAddress);

						CFStringAppendFormat(result, NULL, CFSTR(" ("));
						for (i = 0; i < n; i++) {
							CFDataRef	address;
							char		buf[64];
							struct sockaddr	*sa;

							address	= CFArrayGetValueAtIndex(targetPrivate->resolvedAddress, i);
							sa      = (struct sockaddr *)CFDataGetBytePtr(address);
							_SC_sockaddr_to_string(sa, buf, sizeof(buf));
							CFStringAppendFormat(result, NULL, CFSTR("%s%s"),
									     i > 0 ? ", " : "",
									     buf);
						}
						CFStringAppendFormat(result, NULL, CFSTR(")"));
					} else {
						CFStringAppendFormat(result, NULL, CFSTR(" (no addresses)"));
					}
				} else {
					CFStringAppendFormat(result, NULL, CFSTR(" (%s)"),
							     gai_strerror(targetPrivate->resolvedAddressError));
				}
			} else if (targetPrivate->dnsPort != NULL) {
				CFStringAppendFormat(result, NULL, CFSTR(" (DNS query active)"));
			}
			break;
		}
	}
	if (targetPrivate->scheduled) {
		CFStringAppendFormat(result,
				     NULL,
				     CFSTR(", flags = 0x%08x, if_index = %hu"),
				     targetPrivate->info.flags,
				     targetPrivate->info.if_index);
	}
	CFStringAppendFormat(result, NULL, CFSTR("}"));

	return result;
}


static void
__SCNetworkReachabilityDeallocate(CFTypeRef cf)
{
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)cf;

	/* release resources */

	pthread_mutex_destroy(&targetPrivate->lock);

	if (targetPrivate->name != NULL)
		CFAllocatorDeallocate(NULL, (void *)targetPrivate->name);

	if (targetPrivate->serv != NULL)
		CFAllocatorDeallocate(NULL, (void *)targetPrivate->serv);

	if (targetPrivate->resolvedAddress != NULL)
		CFRelease(targetPrivate->resolvedAddress);

	if (targetPrivate->localAddress != NULL)
		CFAllocatorDeallocate(NULL, (void *)targetPrivate->localAddress);

	if (targetPrivate->remoteAddress != NULL)
		CFAllocatorDeallocate(NULL, (void *)targetPrivate->remoteAddress);

	if (targetPrivate->rlsContext.release != NULL) {
		(*targetPrivate->rlsContext.release)(targetPrivate->rlsContext.info);
	}

	if (targetPrivate->onDemandName != NULL) {
		CFRelease(targetPrivate->onDemandName);
	}

	if (targetPrivate->onDemandRemoteAddress != NULL) {
		CFRelease(targetPrivate->onDemandRemoteAddress);
	}

	if (targetPrivate->onDemandServer != NULL) {
		CFRelease(targetPrivate->onDemandServer);
	}

	if (targetPrivate->onDemandServiceID != NULL) {
		CFRelease(targetPrivate->onDemandServiceID);
	}

	return;
}


static void
__SCNetworkReachabilityInitialize(void)
{
	__kSCNetworkReachabilityTypeID = _CFRuntimeRegisterClass(&__SCNetworkReachabilityClass);
	return;
}


static void
__SCNetworkReachabilityPerform(SCNetworkReachabilityRef target)
{
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

#if	!TARGET_OS_IPHONE
	if (targetPrivate->dispatchQueue != NULL) {
		CFRetain(target);
		dispatch_async(targetPrivate->dispatchQueue,
			       ^{
				       rlsPerform((void *)target);
				       CFRelease(target);
			       });
	} else
#endif	// !TARGET_OS_IPHONE
	if (targetPrivate->rls != NULL) {
		CFRunLoopSourceSignal(targetPrivate->rls);
		_SC_signalRunLoop(target, targetPrivate->rls, targetPrivate->rlList);
	}

	return;
}

static SCNetworkReachabilityPrivateRef
__SCNetworkReachabilityCreatePrivate(CFAllocatorRef	allocator)
{
	SCNetworkReachabilityPrivateRef		targetPrivate;
	uint32_t				size;

	/* initialize runtime */
	pthread_once(&initialized, __SCNetworkReachabilityInitialize);

	/* allocate target */
	size          = sizeof(SCNetworkReachabilityPrivate) - sizeof(CFRuntimeBase);
	targetPrivate = (SCNetworkReachabilityPrivateRef)_CFRuntimeCreateInstance(allocator,
										  __kSCNetworkReachabilityTypeID,
										  size,
										  NULL);
	if (targetPrivate == NULL) {
		return NULL;
	}

	pthread_mutex_init(&targetPrivate->lock, NULL);

	targetPrivate->name				= NULL;
	targetPrivate->serv				= NULL;
	bzero(&targetPrivate->hints, sizeof(targetPrivate->hints));
	targetPrivate->hints.ai_flags = AI_ADDRCONFIG;
#ifdef	AI_PARALLEL
	targetPrivate->hints.ai_flags |= AI_PARALLEL;
#endif	/* AI_PARALLEL */

	targetPrivate->needResolve			= FALSE;
	targetPrivate->resolvedAddress			= NULL;
	targetPrivate->resolvedAddressError		= NETDB_SUCCESS;

	targetPrivate->localAddress			= NULL;
	targetPrivate->remoteAddress			= NULL;

	targetPrivate->info				= NOT_REACHABLE;
	targetPrivate->last_notify			= NOT_REPORTED;

	targetPrivate->scheduled			= FALSE;
	targetPrivate->rls				= NULL;
	targetPrivate->rlsFunction			= NULL;
	targetPrivate->rlsContext.info			= NULL;
	targetPrivate->rlsContext.retain		= NULL;
	targetPrivate->rlsContext.release		= NULL;
	targetPrivate->rlsContext.copyDescription	= NULL;
	targetPrivate->rlList				= NULL;

	targetPrivate->haveDNS				= FALSE;
	targetPrivate->dnsMP				= MACH_PORT_NULL;
	targetPrivate->dnsPort				= NULL;
	targetPrivate->dnsRLS				= NULL;

	targetPrivate->onDemandBypass			= FALSE;
	targetPrivate->onDemandName			= NULL;
	targetPrivate->onDemandRemoteAddress		= NULL;
	targetPrivate->onDemandServer			= NULL;
	targetPrivate->onDemandServiceID		= NULL;

	targetPrivate->log_prefix[0] = '\0';
	if (_sc_log > 0) {
		snprintf(targetPrivate->log_prefix,
			 sizeof(targetPrivate->log_prefix),
			 "[%p] ",
			 targetPrivate);
	}

	return targetPrivate;
}


SCNetworkReachabilityRef
SCNetworkReachabilityCreateWithAddress(CFAllocatorRef		allocator,
				       const struct sockaddr	*address)
{
	SCNetworkReachabilityPrivateRef	targetPrivate;

	if ((address == NULL) ||
	    (address->sa_len == 0) ||
	    (address->sa_len > sizeof(struct sockaddr_storage))) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	targetPrivate = __SCNetworkReachabilityCreatePrivate(allocator);
	if (targetPrivate == NULL) {
		return NULL;
	}

	targetPrivate->type = reachabilityTypeAddress;
	targetPrivate->remoteAddress = CFAllocatorAllocate(NULL, address->sa_len, 0);
	bcopy(address, targetPrivate->remoteAddress, address->sa_len);

	return (SCNetworkReachabilityRef)targetPrivate;
}


SCNetworkReachabilityRef
SCNetworkReachabilityCreateWithAddressPair(CFAllocatorRef		allocator,
					   const struct sockaddr	*localAddress,
					   const struct sockaddr	*remoteAddress)
{
	SCNetworkReachabilityPrivateRef	targetPrivate;

	if ((localAddress == NULL) && (remoteAddress == NULL)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	if (localAddress != NULL) {
		if ((localAddress->sa_len == 0) ||
		    (localAddress->sa_len > sizeof(struct sockaddr_storage))) {
			    _SCErrorSet(kSCStatusInvalidArgument);
			    return NULL;
		}
	}

	if (remoteAddress != NULL) {
		if ((remoteAddress->sa_len == 0) ||
		    (remoteAddress->sa_len > sizeof(struct sockaddr_storage))) {
			    _SCErrorSet(kSCStatusInvalidArgument);
			    return NULL;
		}
	}

	targetPrivate = __SCNetworkReachabilityCreatePrivate(allocator);
	if (targetPrivate == NULL) {
		return NULL;
	}

	targetPrivate->type = reachabilityTypeAddressPair;

	if (localAddress != NULL) {
		targetPrivate->localAddress = CFAllocatorAllocate(NULL, localAddress->sa_len, 0);
		bcopy(localAddress, targetPrivate->localAddress, localAddress->sa_len);
	}

	if (remoteAddress != NULL) {
		targetPrivate->remoteAddress = CFAllocatorAllocate(NULL, remoteAddress->sa_len, 0);
		bcopy(remoteAddress, targetPrivate->remoteAddress, remoteAddress->sa_len);
	}

	return (SCNetworkReachabilityRef)targetPrivate;
}


SCNetworkReachabilityRef
SCNetworkReachabilityCreateWithName(CFAllocatorRef	allocator,
				    const char		*nodename)
{
	int				nodenameLen;
	struct sockaddr_in		sin;
	struct sockaddr_in6		sin6;
	SCNetworkReachabilityPrivateRef	targetPrivate;

	if (nodename == NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	nodenameLen = strlen(nodename);
	if (nodenameLen == 0) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	/* check if this "nodename" is really an IP[v6] address in disguise */

	bzero(&sin, sizeof(sin));
	sin.sin_len    = sizeof(sin);
	sin.sin_family = AF_INET;
	if (inet_aton(nodename, &sin.sin_addr) == 1) {
		/* if IPv4 address */
		return SCNetworkReachabilityCreateWithAddress(allocator, (struct sockaddr *)&sin);
	}

	bzero(&sin6, sizeof(sin6));
	sin6.sin6_len    = sizeof(sin6);
	sin6.sin6_family = AF_INET6;
	if (inet_pton(AF_INET6, nodename, &sin6.sin6_addr) == 1) {
		/* if IPv6 address */
		char	*p;

		p = strchr(nodename, '%');
		if (p != NULL) {
			sin6.sin6_scope_id = if_nametoindex(p + 1);
		}

		return SCNetworkReachabilityCreateWithAddress(allocator, (struct sockaddr *)&sin6);
	}

	targetPrivate = __SCNetworkReachabilityCreatePrivate(allocator);
	if (targetPrivate == NULL) {
		return NULL;
	}

	targetPrivate->type = reachabilityTypeName;

	targetPrivate->name = CFAllocatorAllocate(NULL, nodenameLen + 1, 0);
	strlcpy((char *)targetPrivate->name, nodename, nodenameLen + 1);

	targetPrivate->needResolve = TRUE;
	targetPrivate->info.flags |= kSCNetworkReachabilityFlagsFirstResolvePending;

	return (SCNetworkReachabilityRef)targetPrivate;
}


SCNetworkReachabilityRef
SCNetworkReachabilityCreateWithOptions(CFAllocatorRef	allocator,
				       CFDictionaryRef	options)
{
	CFBooleanRef			bypass;
	CFDataRef			data;
	struct addrinfo			*hints	= NULL;
	const char			*name;
	CFStringRef			nodename;
	CFStringRef			servname;
	SCNetworkReachabilityPrivateRef	targetPrivate;

	if (!isA_CFDictionary(options)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	nodename = CFDictionaryGetValue(options, kSCNetworkReachabilityOptionNodeName);
	if ((nodename != NULL) &&
	    (!isA_CFString(nodename) || (CFStringGetLength(nodename) == 0))) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}
	servname = CFDictionaryGetValue(options, kSCNetworkReachabilityOptionServName);
	if ((servname != NULL) &&
	    (!isA_CFString(servname) || (CFStringGetLength(servname) == 0))) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}
	data = CFDictionaryGetValue(options, kSCNetworkReachabilityOptionHints);
	if (data != NULL) {
		if (!isA_CFData(data) || (CFDataGetLength(data) != sizeof(targetPrivate->hints))) {
			_SCErrorSet(kSCStatusInvalidArgument);
			return NULL;
		}

		hints = (struct addrinfo *)CFDataGetBytePtr(data);
		if ((hints->ai_addrlen   != 0)    ||
		    (hints->ai_addr      != NULL) ||
		    (hints->ai_canonname != NULL) ||
		    (hints->ai_next      != NULL)) {
			_SCErrorSet(kSCStatusInvalidArgument);
			return NULL;
		}
	}
	bypass = CFDictionaryGetValue(options, kSCNetworkReachabilityOptionConnectionOnDemandByPass);
	if ((bypass != NULL) && !isA_CFBoolean(bypass)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}
	if ((nodename == NULL) && (servname == NULL)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	name = _SC_cfstring_to_cstring(nodename, NULL, 0, kCFStringEncodingUTF8);
	targetPrivate = (SCNetworkReachabilityPrivateRef)SCNetworkReachabilityCreateWithName(allocator, name);
	CFAllocatorDeallocate(NULL, (void *)name);
	if (targetPrivate == NULL) {
		return NULL;
	}

	if (targetPrivate->type == reachabilityTypeName) {
		if (servname != NULL) {
			targetPrivate->serv = _SC_cfstring_to_cstring(servname, NULL, 0, kCFStringEncodingUTF8);
		}
		if (hints != NULL) {
			bcopy(hints, &targetPrivate->hints, sizeof(targetPrivate->hints));
		}
	}

	if (bypass != NULL) {
		targetPrivate->onDemandBypass = CFBooleanGetValue(bypass);
	}

	return (SCNetworkReachabilityRef)targetPrivate;
}


CFTypeID
SCNetworkReachabilityGetTypeID(void)
{
	pthread_once(&initialized, __SCNetworkReachabilityInitialize);	/* initialize runtime */
	return __kSCNetworkReachabilityTypeID;
}


CFArrayRef	/* CFArray[CFData], where each CFData is a (struct sockaddr *) */
SCNetworkReachabilityCopyResolvedAddress(SCNetworkReachabilityRef	target,
					 int				*error_num)
{
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	if (!isA_SCNetworkReachability(target)) {
		_SCErrorSet(kSCStatusInvalidArgument);
	       return NULL;
	}

	if (targetPrivate->type != reachabilityTypeName) {
		_SCErrorSet(kSCStatusInvalidArgument);
	       return NULL;
	}

	if (error_num) {
		*error_num = targetPrivate->resolvedAddressError;
	}

	if ((targetPrivate->resolvedAddress != NULL) || (targetPrivate->resolvedAddressError != NETDB_SUCCESS)) {
		if (isA_CFArray(targetPrivate->resolvedAddress)) {
			return CFRetain(targetPrivate->resolvedAddress);
		} else {
			/* if status is known but no resolved addresses to return */
			_SCErrorSet(kSCStatusOK);
			return NULL;
		}
	}

	_SCErrorSet(kSCStatusReachabilityUnknown);
	return NULL;
}


static void
__SCNetworkReachabilitySetResolvedAddress(int32_t			status,
					  struct addrinfo		*res,
					  SCNetworkReachabilityRef	target)
{
	struct addrinfo				*resP;
	SCNetworkReachabilityPrivateRef		targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	if (targetPrivate->resolvedAddress != NULL) {
		CFRelease(targetPrivate->resolvedAddress);
		targetPrivate->resolvedAddress = NULL;
	}

	if ((status == 0) && (res != NULL)) {
		CFMutableArrayRef	addresses;

		addresses = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

		for (resP = res; resP; resP = resP->ai_next) {
			CFIndex		n;
			CFDataRef	newAddress;

			newAddress = CFDataCreate(NULL, (void *)resP->ai_addr, resP->ai_addr->sa_len);
			n = CFArrayGetCount(addresses);
			if ((n == 0) ||
			    !CFArrayContainsValue(addresses, CFRangeMake(0, n), newAddress)) {
				CFArrayAppendValue(addresses, newAddress);
			}
			CFRelease(newAddress);
		}

		/* save the resolved address[es] */
		targetPrivate->resolvedAddress      = addresses;
		targetPrivate->resolvedAddressError = NETDB_SUCCESS;
	} else {
		SCLog(_sc_debug, LOG_INFO, CFSTR("%sgetaddrinfo() failed: %s"),
		      targetPrivate->log_prefix,
		      gai_strerror(status));

		/* save the error associated with the attempt to resolve the name */
		targetPrivate->resolvedAddress      = CFRetain(kCFNull);
		targetPrivate->resolvedAddressError = status;
	}
	targetPrivate->needResolve = FALSE;

	if (res != NULL)	freeaddrinfo(res);

	if (targetPrivate->scheduled) {
		__SCNetworkReachabilityPerform(target);
	}

	return;
}


static void
__SCNetworkReachabilityCallbackSetResolvedAddress(int32_t status, struct addrinfo *res, void *context)
{
	SCNetworkReachabilityRef	target		= (SCNetworkReachabilityRef)context;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	__log_query_time(target,
			 ((status == 0) && (res != NULL)),	// if successful query
			 TRUE,					// async
			 &targetPrivate->dnsQueryStart);	// start time

	__SCNetworkReachabilitySetResolvedAddress(status, res, target);
	return;
}


/*
 * rankReachability()
 *   Not reachable       == 0
 *   Connection Required == 1
 *   Reachable           == 2
 */
static int
rankReachability(SCNetworkReachabilityFlags flags)
{
	int	rank = 0;

	if (flags & kSCNetworkReachabilityFlagsReachable)		rank = 2;
	if (flags & kSCNetworkReachabilityFlagsConnectionRequired)	rank = 1;
	return rank;
}


#pragma mark -
#pragma mark DNS name resolution


static CFStringRef
replyMPCopyDescription(const void *info)
{
	SCNetworkReachabilityRef	target		= (SCNetworkReachabilityRef)info;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	return CFStringCreateWithFormat(NULL,
					NULL,
					CFSTR("<getaddrinfo_async_start reply MP> {%s%s%s%s%s, target = %p}"),
					targetPrivate->name != NULL ? "name = " : "",
					targetPrivate->name != NULL ? targetPrivate->name : "",
					targetPrivate->name != NULL && targetPrivate->serv != NULL ? ", " : "",
					targetPrivate->serv != NULL ? "serv = " : "",
					targetPrivate->serv != NULL ? targetPrivate->serv : "",
					target);
}


static void
processAsyncDNSReply(mach_port_t mp, void *msg, SCNetworkReachabilityRef target);


static void
getaddrinfo_async_handleCFReply(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
	mach_port_t			mp	= CFMachPortGetPort(port);
	SCNetworkReachabilityRef	target	= (SCNetworkReachabilityRef)info;

	processAsyncDNSReply(mp, msg, target);
	return;
}


#if	!TARGET_OS_IPHONE
static boolean_t
SCNetworkReachabilityNotifyMIGCallback(mach_msg_header_t *message, mach_msg_header_t *reply)
{
	mach_port_t			mp	= message->msgh_local_port;
	SCNetworkReachabilityRef	target	= dispatch_get_context(dispatch_get_current_queue());

	processAsyncDNSReply(mp, message, target);
	reply->msgh_remote_port = MACH_PORT_NULL;
	return false;
}
#endif	// !TARGET_OS_IPHONE


static Boolean
enqueueAsyncDNSQuery(SCNetworkReachabilityRef target, mach_port_t mp)
{
	CFMachPortContext		context	= { 0
						  , (void *)target
						  , CFRetain
						  , CFRelease
						  , replyMPCopyDescription
						  };
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	targetPrivate->dnsMP = mp;
	targetPrivate->dnsPort = CFMachPortCreateWithPort(NULL,
							  mp,
							  getaddrinfo_async_handleCFReply,
							  &context,
							  NULL);
#if	!TARGET_OS_IPHONE
	if (targetPrivate->dispatchQueue != NULL) {
		targetPrivate->asyncDNSQueue = dispatch_queue_create("com.apple.SCNetworkReachabilty.async_DNS_query", NULL);
		if (targetPrivate->asyncDNSQueue == NULL) {
			SCLog(TRUE, LOG_ERR, CFSTR("SCNetworkReachability dispatch_queue_create() failed"));
			goto fail;
		}
		CFRetain(target);	// Note: will be released when the dispatch queue is released
		dispatch_set_context(targetPrivate->asyncDNSQueue, (void *)target);
		dispatch_set_finalizer_f(targetPrivate->asyncDNSQueue, (dispatch_function_t)CFRelease);

		targetPrivate->asyncDNSSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV,
								       mp,
								       0,
								       targetPrivate->asyncDNSQueue);
		if (targetPrivate->asyncDNSSource == NULL) {
			SCLog(TRUE, LOG_ERR, CFSTR("SCNetworkReachability dispatch_source_create() failed"));
			goto fail;
		}
		dispatch_source_set_event_handler(targetPrivate->asyncDNSSource, ^{
			dispatch_mig_server(targetPrivate->asyncDNSSource,
					    sizeof(mach_msg_header_t),
					    SCNetworkReachabilityNotifyMIGCallback);
		});
		dispatch_resume(targetPrivate->asyncDNSSource);
	} else
#endif	// !TARGET_OS_IPHONE
	if (targetPrivate->rls != NULL) {
		CFIndex	i;
		CFIndex	n;

		targetPrivate->dnsRLS = CFMachPortCreateRunLoopSource(NULL, targetPrivate->dnsPort, 0);

		n = CFArrayGetCount(targetPrivate->rlList);
		for (i = 0; i < n; i += 3) {
			CFRunLoopRef	rl	= (CFRunLoopRef)CFArrayGetValueAtIndex(targetPrivate->rlList, i+1);
			CFStringRef	rlMode	= (CFStringRef) CFArrayGetValueAtIndex(targetPrivate->rlList, i+2);

			CFRunLoopAddSource(rl, targetPrivate->dnsRLS, rlMode);
		}
	}

	return TRUE;

#if	!TARGET_OS_IPHONE
    fail :

	if (targetPrivate->asyncDNSSource != NULL) {
		dispatch_source_cancel(targetPrivate->asyncDNSSource);
		dispatch_release(targetPrivate->asyncDNSSource);
		targetPrivate->asyncDNSSource = NULL;
	}
	if (targetPrivate->asyncDNSQueue != NULL) {
		dispatch_release(targetPrivate->asyncDNSQueue);
		targetPrivate->asyncDNSQueue = NULL;
	}

	CFMachPortInvalidate(targetPrivate->dnsPort);
	CFRelease(targetPrivate->dnsPort);
	targetPrivate->dnsPort = NULL;
	targetPrivate->dnsMP = MACH_PORT_NULL;

	_SCErrorSet(kSCStatusFailed);
	return FALSE;
#endif	// !TARGET_OS_IPHONE
}


static void
dequeueAsyncDNSQuery(SCNetworkReachabilityRef target)
{
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

#if	!TARGET_OS_IPHONE
	if (targetPrivate->asyncDNSSource != NULL) {
		dispatch_source_cancel(targetPrivate->asyncDNSSource);
		if (targetPrivate->asyncDNSQueue != dispatch_get_current_queue()) {
			// ensure the cancellation has completed
			pthread_mutex_unlock(&targetPrivate->lock);
			dispatch_sync(targetPrivate->asyncDNSQueue, ^{});
			pthread_mutex_lock(&targetPrivate->lock);
		}
	}
	if (targetPrivate->asyncDNSSource != NULL) {
		dispatch_release(targetPrivate->asyncDNSSource);
		targetPrivate->asyncDNSSource = NULL;
	}
	if (targetPrivate->asyncDNSQueue != NULL) {
		dispatch_release(targetPrivate->asyncDNSQueue);
		targetPrivate->asyncDNSQueue = NULL;
	}
#endif	// !TARGET_OS_IPHONE

	if (targetPrivate->dnsRLS != NULL) {
		CFRelease(targetPrivate->dnsRLS);
		targetPrivate->dnsRLS = NULL;
	}

	CFMachPortInvalidate(targetPrivate->dnsPort);
	CFRelease(targetPrivate->dnsPort);
	targetPrivate->dnsPort = NULL;
	targetPrivate->dnsMP = MACH_PORT_NULL;

	return;
}


static void
processAsyncDNSReply(mach_port_t mp, void *msg, SCNetworkReachabilityRef target)
{
	int32_t				status;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	pthread_mutex_lock(&targetPrivate->lock);

	if (mp != targetPrivate->dnsMP) {
		// we've received a callback on the async DNS port but since the
		// associated CFMachPort doesn't match than the request must have
		// already been cancelled.
		SCLog(TRUE, LOG_ERR, CFSTR("processAsyncDNSReply(): mp != targetPrivate->dnsMP"));
		pthread_mutex_unlock(&targetPrivate->lock);
		return;
	}

	dequeueAsyncDNSQuery(target);
	status = getaddrinfo_async_handle_reply(msg);
	if ((status == 0) &&
	    (targetPrivate->resolvedAddress == NULL) && (targetPrivate->resolvedAddressError == NETDB_SUCCESS)) {
		Boolean	ok;

		// if the request is not complete and needs to be re-queued
		ok = enqueueAsyncDNSQuery(target, mp);
		if (!ok) {
			SCLog(TRUE, LOG_ERR, CFSTR("processAsyncDNSReply enqueueAsyncDNSQuery() failed"));
		}
	}

	pthread_mutex_unlock(&targetPrivate->lock);

	return;
}


static Boolean
check_resolver_reachability(SCDynamicStoreRef		*storeP,
			    dns_resolver_t		*resolver,
			    SCNetworkReachabilityFlags	*flags,
			    Boolean			*haveDNS,
			    const char			*log_prefix)
{
	int		i;
	Boolean		ok	= TRUE;

	*flags   = kSCNetworkReachabilityFlagsReachable;
	*haveDNS = FALSE;

	for (i = 0; i < resolver->n_nameserver; i++) {
		struct sockaddr		*address	= resolver->nameserver[i];
		ReachabilityInfo	ns_info;

		*haveDNS = TRUE;

		if (address->sa_family != AF_INET) {
			/*
			 * we need to skip non-IPv4 DNS server
			 * addresses (at least until [3510431] has
			 * been resolved).
			 */
			continue;
		}

		ok = checkAddress(storeP, address, &ns_info, log_prefix);
		if (!ok) {
			/* not today */
			goto done;
		}

		if (rankReachability(ns_info.flags) < rankReachability(*flags)) {
			/* return the worst case result */
			*flags = ns_info.flags;
		}
	}

    done :

	return ok;
}


static Boolean
check_matching_resolvers(SCDynamicStoreRef		*storeP,
			 dns_config_t			*dns_config,
			 const char			*fqdn,
			 SCNetworkReachabilityFlags	*flags,
			 Boolean			*haveDNS,
			 const char			*log_prefix)
{
	int		i;
	Boolean		matched	= FALSE;
	const char	*name	= fqdn;

	while (!matched && (name != NULL)) {
		int	len;

		/*
		 * check if the provided name (or sub-component)
		 * matches one of our resolver configurations.
		 */
		len = strlen(name);
		for (i = 0; i < dns_config->n_resolver; i++) {
			char		*domain;
			dns_resolver_t	*resolver;

			resolver = dns_config->resolver[i];
			domain   = resolver->domain;
			if (domain != NULL && (len == strlen(domain))) {
				if (strcasecmp(name, domain) == 0) {
					Boolean	ok;

					/*
					 * if name matches domain
					 */
					matched = TRUE;
					ok = check_resolver_reachability(storeP, resolver, flags, haveDNS, log_prefix);
					if (!ok) {
						/* not today */
						return FALSE;
					}
				}
			}
		}

		if (!matched) {
			/*
			 * we have not found a matching resolver, try
			 * a less qualified domain
			 */
			name = strchr(name, '.');
			if ((name != NULL) && (*name != '\0')) {
				name++;
			} else {
				name = NULL;
			}
		}
	}

	return matched;
}


static dns_configuration_t *
dns_configuration_retain()
{
	pthread_mutex_lock(&dns_lock);

	if ((dns_configuration != NULL) && dns_token_valid) {
		int		check	= 0;
		uint32_t	status;

		/*
		 * check if the global [DNS] configuration snapshot needs
		 * to be updated
		 */
		status = notify_check(dns_token, &check);
		if (status != NOTIFY_STATUS_OK) {
			SCLog(TRUE, LOG_INFO, CFSTR("notify_check() failed, status=%lu"), status);
		}

		if ((status != NOTIFY_STATUS_OK) || (check != 0)) {
			/*
			 * if the snapshot needs to be refreshed
			 */
			if (dns_configuration->refs == 0) {
				dns_configuration_free(dns_configuration->config);
				CFAllocatorDeallocate(NULL, dns_configuration);
			}
			dns_configuration = NULL;
		}
	}

	if (dns_configuration == NULL) {
		dns_config_t	*new_config;

		new_config = dns_configuration_copy();
		if (new_config != NULL) {
			dns_configuration = CFAllocatorAllocate(NULL, sizeof(dns_configuration_t), 0);
			dns_configuration->config = new_config;
			dns_configuration->refs   = 0;
		}
	}

	if (dns_configuration != NULL) {
		dns_configuration->refs++;
	}

	pthread_mutex_unlock(&dns_lock);
	return dns_configuration;
}


static void
dns_configuration_release(dns_configuration_t *config)
{
	pthread_mutex_lock(&dns_lock);

	config->refs--;
	if (config->refs == 0) {
		if ((dns_configuration != config)) {
			dns_configuration_free(config->config);
			CFAllocatorDeallocate(NULL, config);
		}
	}

	pthread_mutex_unlock(&dns_lock);
	return;
}


static Boolean
dns_configuration_watch()
{
	int		dns_check	= 0;
	const char	*dns_key;
	Boolean		ok		= FALSE;
	uint32_t	status;

	pthread_mutex_lock(&dns_lock);

	dns_key = dns_configuration_notify_key();
	if (dns_key == NULL) {
		SCLog(TRUE, LOG_INFO, CFSTR("dns_configuration_notify_key() failed"));
		goto done;
	}

	status = notify_register_check(dns_key, &dns_token);
	if (status == NOTIFY_STATUS_OK) {
		dns_token_valid = TRUE;
	} else {
		SCLog(TRUE, LOG_INFO, CFSTR("notify_register_check() failed, status=%lu"), status);
		goto done;
	}

	status = notify_check(dns_token, &dns_check);
	if (status != NOTIFY_STATUS_OK) {
		SCLog(TRUE, LOG_INFO, CFSTR("notify_check() failed, status=%lu"), status);
		(void)notify_cancel(dns_token);
		dns_token_valid = FALSE;
		goto done;
	}

	ok = TRUE;

    done :

	pthread_mutex_unlock(&dns_lock);
	return ok;
}


static void
dns_configuration_unwatch()
{
	pthread_mutex_lock(&dns_lock);

	(void)notify_cancel(dns_token);
	dns_token_valid = FALSE;

	if ((dns_configuration != NULL) && (dns_configuration->refs == 0)) {
		dns_configuration_free(dns_configuration->config);
		CFAllocatorDeallocate(NULL, dns_configuration);
		dns_configuration = NULL;
	}

	pthread_mutex_unlock(&dns_lock);
	return;
}


static Boolean
_SC_R_checkResolverReachability(SCDynamicStoreRef		*storeP,
				SCNetworkReachabilityFlags	*flags,
				Boolean				*haveDNS,
				const char			*nodename,
				const char			*servname,
				const char			*log_prefix)
{
	dns_resolver_t		*default_resolver;
	dns_configuration_t	*dns;
	Boolean			found			= FALSE;
	char			*fqdn			= (char *)nodename;
	int			i;
	Boolean			isFQDN			= FALSE;
	uint32_t		len;
	Boolean			ok			= TRUE;
	Boolean			useDefault		= FALSE;

	/*
	 * We first assume that all of the configured DNS servers
	 * are available.  Since we don't know which name server will
	 * be consulted to resolve the specified nodename we need to
	 * check the availability of ALL name servers.  We can only
	 * proceed if we know that our query can be answered.
	 */

	*flags   = kSCNetworkReachabilityFlagsReachable;
	*haveDNS = FALSE;

	len = (nodename != NULL) ? strlen(nodename) : 0;
	if (len == 0) {
		if ((servname == NULL) || (strlen(servname) == 0)) {
			// if no nodename or servname, return not reachable
			*flags = 0;
		}
		return ok;
	}

	dns = dns_configuration_retain();
	if (dns == NULL) {
		// if error
		SCLog(_sc_debug, LOG_INFO, CFSTR("%sDNS: no configuration"), log_prefix);
		goto done;
	}

	if (dns->config->n_resolver == 0) {
		// if no resolver configuration
		SCLog(_sc_debug, LOG_INFO, CFSTR("%sDNS: no resolvers"), log_prefix);
		goto done;
	}

	*flags = kSCNetworkReachabilityFlagsReachable;

	if (fqdn[len - 1] == '.') {
		isFQDN = TRUE;

		// trim trailing '.''s
		while ((len > 0) && (fqdn[len-1] == '.')) {
			if (fqdn == nodename) {
				fqdn = strdup(nodename);
			}
			fqdn[--len] = '\0';
		}
	}

	default_resolver = dns->config->resolver[0];

	/*
	 * check if the provided name matches a supplemental domain
	 */
	found = check_matching_resolvers(storeP, dns->config, fqdn, flags, haveDNS, log_prefix);

	if (!found && !isFQDN) {
		/*
		 * if we did not match a supplemental domain name and if the
		 * provided name has enough "."s then the first query will be
		 * directed to the default resolver.
		 */
		char	*cp;
		int	dots;
		int	ndots	= 1;

#define	NDOTS_OPT	"ndots="
#define	NDOTS_OPT_LEN	(sizeof("ndots=") - 1)

		if (default_resolver->options != NULL) {
			cp = strstr(default_resolver->options, NDOTS_OPT);
			if ((cp != NULL) &&
			    ((cp == default_resolver->options) || isspace(cp[-1])) &&
			    ((cp[NDOTS_OPT_LEN] != '\0') && isdigit(cp[NDOTS_OPT_LEN]))) {
				char    *end;
				long    val;

				cp +=  NDOTS_OPT_LEN;
				errno = 0;
				val = strtol(cp, &end, 10);
				if ((*cp != '\0') && (cp != end) && (errno == 0) &&
				((*end == '\0') || isspace(*end))) {
					ndots = val;
				}
			}
		}

		dots = 0;
		for (cp = fqdn; *cp != '\0'; cp++) {
			if (*cp == '.') dots++;
		}

		if (dots > ndots) {
			useDefault = TRUE;
		}
	}

	if (!found && !isFQDN && !useDefault && (dns->config->n_resolver > 1)) {
		/*
		 * FQDN not specified, try matching w/search domains
		 */
		if (default_resolver->n_search > 0) {
			for (i = 0; !found && (i < default_resolver->n_search); i++) {
				int	ret;
				char	*search_fqdn	= NULL;

				ret = asprintf(&search_fqdn, "%s.%s", fqdn, default_resolver->search[i]);
				if (ret == -1) {
					continue;
				}

				// try the provided name with the search domain appended
				found = check_matching_resolvers(storeP,
								 dns->config,
								 search_fqdn,
								 flags,
								 haveDNS,
								 log_prefix);
				free(search_fqdn);
			}
		} else if (default_resolver->domain != NULL) {
			char	*dp;
			int	domain_parts	= 0;

			// count domain parts
			for (dp = default_resolver->domain; *dp != '\0'; dp++) {
				if (*dp == '.') {
					domain_parts++;
				}
			}

			// remove trailing dots
			for (dp--; (dp >= default_resolver->domain) && (*dp == '.'); dp--) {
				*dp = '\0';
				domain_parts--;
			}

			if (dp >= default_resolver->domain) {
				// dots are separators, bump # of components
				domain_parts++;
			}

			dp = default_resolver->domain;
			for (i = LOCALDOMAINPARTS; !found && (i <= domain_parts); i++) {
				int	ret;
				char	*search_fqdn	= NULL;

				ret = asprintf(&search_fqdn, "%s.%s", fqdn, dp);
				if (ret == -1) {
					continue;
				}

				// try the provided name with the [default] domain appended
				found = check_matching_resolvers(storeP,
								 dns->config,
								 search_fqdn,
								 flags,
								 haveDNS,
								 log_prefix);
				free(search_fqdn);

				// move to the next component of the [default] domain
				dp = strchr(dp, '.') + 1;
			}
		}
	}

	if (!found) {
		/*
		 * check the reachability of the default resolver
		 */
		ok = check_resolver_reachability(storeP, default_resolver, flags, haveDNS, log_prefix);
	}

	if (fqdn != nodename)	free(fqdn);

    done :

	if (dns != NULL) {
		dns_configuration_release(dns);
	}

	return ok;
}


Boolean
_SC_checkResolverReachability(SCDynamicStoreRef			*storeP,
			      SCNetworkReachabilityFlags	*flags,
			      Boolean				*haveDNS,
			      const char			*nodename,
			      const char			*servname)
{
	return _SC_R_checkResolverReachability(storeP, flags, haveDNS, nodename, servname, "");
}


/*
 * _SC_checkResolverReachabilityByAddress()
 *
 * Given an IP address, determine whether a reverse DNS query can be issued
 * using the current network configuration.
 */
Boolean
_SC_checkResolverReachabilityByAddress(SCDynamicStoreRef		*storeP,
				       SCNetworkReachabilityFlags	*flags,
				       Boolean				*haveDNS,
				       struct sockaddr			*sa)
{
	int				i;
	Boolean				ok		= FALSE;
	char				ptr_name[128];

	/*
	 * Ideally, we would have an API that given a local IP
	 * address would return the DNS server(s) that would field
	 * a given PTR query.  Fortunately, we do have an SPI which
	 * which will provide this information given a "name" so we
	 * take the address, convert it into the inverse query name,
	 * and find out which servers should be consulted.
	 */

	switch (sa->sa_family) {
		case AF_INET : {
			union {
				in_addr_t	s_addr;
				unsigned char	b[4];
			} rev;
			struct sockaddr_in	*sin	= (struct sockaddr_in *)sa;

			/*
			 * build "PTR" query name
			 *   NNN.NNN.NNN.NNN.in-addr.arpa.
			 */
			rev.s_addr = sin->sin_addr.s_addr;
			(void) snprintf(ptr_name, sizeof(ptr_name), "%u.%u.%u.%u.in-addr.arpa.",
					rev.b[3],
					rev.b[2],
					rev.b[1],
					rev.b[0]);

			break;
		}

		case AF_INET6 : {
			int			s	= 0;
			struct sockaddr_in6	*sin6	= (struct sockaddr_in6 *)sa;
			int			x	= sizeof(ptr_name);
			int			n;

			/*
			 * build IPv6 "nibble" PTR query name (RFC 1886, RFC 3152)
			 *   N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.N.ip6.arpa.
			 */
			for (i = sizeof(sin6->sin6_addr) - 1; i >= 0; i--) {
				n = snprintf(&ptr_name[s], x, "%x.%x.",
					     ( sin6->sin6_addr.s6_addr[i]       & 0xf),
					     ((sin6->sin6_addr.s6_addr[i] >> 4) & 0xf));
				if ((n == -1) || (n >= x)) {
					goto done;
				}

				s += n;
				x -= n;
			}

			n = snprintf(&ptr_name[s], x, "ip6.arpa.");
			if ((n == -1) || (n >= x)) {
				goto done;
			}

			break;
		}

		default :
			goto done;
	}

	ok = _SC_R_checkResolverReachability(storeP, flags, haveDNS, ptr_name, NULL, "");

    done :

	return ok;
}


static Boolean
startAsyncDNSQuery(SCNetworkReachabilityRef target) {
	int				error;
	mach_port_t			mp;
	Boolean				ok;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	(void) gettimeofday(&targetPrivate->dnsQueryStart, NULL);

	error = getaddrinfo_async_start(&mp,
					targetPrivate->name,
					targetPrivate->serv,
					&targetPrivate->hints,
					__SCNetworkReachabilityCallbackSetResolvedAddress,
					(void *)target);
	if (error != 0) {
		/* save the error associated with the attempt to resolve the name */
		__SCNetworkReachabilityCallbackSetResolvedAddress(error, NULL, (void *)target);
		return FALSE;
	}

	ok = enqueueAsyncDNSQuery(target, mp);
	return ok;
}


#pragma mark -
#pragma mark OnDemand


SCNetworkServiceRef
SCNetworkReachabilityCopyOnDemandService(SCNetworkReachabilityRef	target,
					 CFDictionaryRef		*userOptions)
{
	SCNetworkServiceRef		service		= NULL;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	if (!isA_SCNetworkReachability(target)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	if (targetPrivate->onDemandServiceID != NULL) {
		service = _SCNetworkServiceCopyActive(NULL, targetPrivate->onDemandServiceID);
	}

	if (userOptions != NULL) {
		if (targetPrivate->onDemandName != NULL) {
			*userOptions = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			CFDictionarySetValue((CFMutableDictionaryRef)*userOptions, kSCNetworkConnectionSelectionOptionOnDemandHostName, targetPrivate->onDemandName);
		} else {
			*userOptions = NULL;
		}
	}

	return service;
}


static void
__SCNetworkReachabilityOnDemandCheckCallback(SCNetworkReachabilityRef	onDemandServer,
					     SCNetworkReachabilityFlags	onDemandFlags,
					     void			*info)
{
	SCNetworkReachabilityRef	target		= (SCNetworkReachabilityRef)info;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	pthread_mutex_lock(&targetPrivate->lock);

	if (!targetPrivate->scheduled) {
		// if not currently scheduled
		pthread_mutex_unlock(&targetPrivate->lock);
		return;
	}

	SCLog(_sc_debug, LOG_INFO, CFSTR("%sOnDemand \"server\" status changed"),
	      targetPrivate->log_prefix);
	__SCNetworkReachabilityPerform(target);

	pthread_mutex_unlock(&targetPrivate->lock);

	return;
}


static Boolean
__SCNetworkReachabilityOnDemandCheck(SCDynamicStoreRef		*storeP,
				     SCNetworkReachabilityRef	target,
				     Boolean			onDemandRetry,
				     SCNetworkReachabilityFlags	*flags)
{
	Boolean				ok;
	Boolean				onDemand		= FALSE;
	CFStringRef			onDemandRemoteAddress	= NULL;
	CFStringRef			onDemandServiceID	= NULL;
	SCNetworkConnectionStatus	onDemandStatus;
	SCNetworkReachabilityPrivateRef	targetPrivate		= (SCNetworkReachabilityPrivateRef)target;

//	SCLog(_sc_debug, LOG_INFO,
//	      CFSTR("%s__SCNetworkReachabilityOnDemandCheck %s"),
//	      targetPrivate->log_prefix,
//	      onDemandRetry ? "after" : "before");

	if (targetPrivate->onDemandName == NULL) {
		targetPrivate->onDemandName = CFStringCreateWithCString(NULL, targetPrivate->name, kCFStringEncodingUTF8);
	}

	/*
	 * check if an OnDemand VPN configuration matches the name.
	 */
	ok = __SCNetworkConnectionCopyOnDemandInfoWithName(storeP,
							   targetPrivate->onDemandName,
							   onDemandRetry,
							   &onDemandServiceID,
							   &onDemandStatus,
							   &onDemandRemoteAddress);
	if (!_SC_CFEqual(targetPrivate->onDemandRemoteAddress, onDemandRemoteAddress) ||
	    !_SC_CFEqual(targetPrivate->onDemandServiceID, onDemandServiceID)) {
		if (targetPrivate->onDemandRemoteAddress != NULL) {
			CFRelease(targetPrivate->onDemandRemoteAddress);
			targetPrivate->onDemandRemoteAddress = NULL;
		}

		if (targetPrivate->onDemandServer != NULL) {
#if	!TARGET_OS_IPHONE
			if (targetPrivate->dispatchQueue != NULL) {
				// unschedule
				__SCNetworkReachabilityUnscheduleFromRunLoop(targetPrivate->onDemandServer, NULL, NULL, TRUE);
			} else
#endif	// !TARGET_OS_IPHONE
			if (targetPrivate->rls != NULL) {
				CFIndex	i;
				CFIndex	n;

				// unschedule
				n = CFArrayGetCount(targetPrivate->rlList);
				for (i = 0; i < n; i += 3) {
					CFRunLoopRef	rl	= (CFRunLoopRef)CFArrayGetValueAtIndex(targetPrivate->rlList, i+1);
					CFStringRef	rlMode	= (CFStringRef) CFArrayGetValueAtIndex(targetPrivate->rlList, i+2);

					__SCNetworkReachabilityUnscheduleFromRunLoop(targetPrivate->onDemandServer, rl, rlMode, TRUE);
				}
			}

			CFRelease(targetPrivate->onDemandServer);
			targetPrivate->onDemandServer = NULL;
		}

		if (targetPrivate->onDemandServiceID != NULL) {
			CFRelease(targetPrivate->onDemandServiceID);
			targetPrivate->onDemandServiceID = NULL;
		}
	}
	if (ok) {
		if (onDemandStatus != kSCNetworkConnectionConnected) {
			/*
			 * if we have a VPN configuration matching the name *and* we need to
			 * bring the VPN up.  Combine our flags with those of the VPN server.
			 */
			if (targetPrivate->onDemandServer == NULL) {
				CFMutableDictionaryRef		options;

				options = CFDictionaryCreateMutable(NULL,
								    0,
								    &kCFTypeDictionaryKeyCallBacks,
								    &kCFTypeDictionaryValueCallBacks);
				CFDictionarySetValue(options, kSCNetworkReachabilityOptionNodeName, onDemandRemoteAddress);
				CFDictionarySetValue(options, kSCNetworkReachabilityOptionConnectionOnDemandByPass, kCFBooleanTrue);
				targetPrivate->onDemandServer = SCNetworkReachabilityCreateWithOptions(NULL, options);
				CFRelease(options);

				if (targetPrivate->scheduled) {
					SCNetworkReachabilityContext	context	= { 0, NULL, CFRetain, CFRelease, CFCopyDescription };

					context.info = (void *)target;
					SCNetworkReachabilitySetCallback(targetPrivate->onDemandServer,
									 __SCNetworkReachabilityOnDemandCheckCallback,
									 &context);

					// schedule server reachability to match that of the target
#if	!TARGET_OS_IPHONE
					if (targetPrivate->dispatchQueue != NULL) {
						__SCNetworkReachabilityScheduleWithRunLoop(targetPrivate->onDemandServer, NULL, NULL, targetPrivate->dispatchQueue, TRUE);
					} else
#endif	// !TARGET_OS_IPHONE
					{
						CFIndex	i;
						CFIndex	n;

						n = CFArrayGetCount(targetPrivate->rlList);
						for (i = 0; i < n; i += 3) {
							CFRunLoopRef	rl	= (CFRunLoopRef)CFArrayGetValueAtIndex(targetPrivate->rlList, i+1);
							CFStringRef	rlMode	= (CFStringRef) CFArrayGetValueAtIndex(targetPrivate->rlList, i+2);

							__SCNetworkReachabilityScheduleWithRunLoop(targetPrivate->onDemandServer, rl, rlMode, NULL, TRUE);
						}
					}
				}
			}

			ok = SCNetworkReachabilityGetFlags(targetPrivate->onDemandServer, flags);
			SCLog(_sc_debug, LOG_INFO, CFSTR("%s  status  * = 0x%08x"),
			      targetPrivate->log_prefix,
			      *flags);
			if (ok && (*flags & kSCNetworkReachabilityFlagsReachable)) {
				if (!(*flags & kSCNetworkReachabilityFlagsTransientConnection)) {
					// start clean if not already layered on a transient network
					*flags = 0;
				}
				*flags |= kSCNetworkReachabilityFlagsReachable;
				*flags |= kSCNetworkReachabilityFlagsTransientConnection;
				*flags |= kSCNetworkReachabilityFlagsConnectionRequired;
				*flags |= kSCNetworkReachabilityFlagsConnectionOnDemand;

				if (_sc_debug) {
					SCLog(TRUE, LOG_INFO, CFSTR("%s  service * = %@"),
					      targetPrivate->log_prefix,
					      onDemandServiceID);
					SCLog(TRUE, LOG_INFO, CFSTR("%s  status    = isReachable (after OnDemand connect)"),
					      targetPrivate->log_prefix);
				}

				onDemand = TRUE;
			}
		}

		if (onDemandRemoteAddress != NULL) {
			if (targetPrivate->onDemandRemoteAddress == NULL) {
				targetPrivate->onDemandRemoteAddress = onDemandRemoteAddress;
			} else {
				CFRelease(onDemandRemoteAddress);
			}
		}

		if (onDemandServiceID != NULL) {
			if (targetPrivate->onDemandServiceID == NULL) {
				targetPrivate->onDemandServiceID = onDemandServiceID;
			} else {
				CFRelease(onDemandServiceID);
			}
		}
	}

	return onDemand;
}


#pragma mark -
#pragma mark Reachability Flags


static Boolean
__SCNetworkReachabilityGetFlags(SCDynamicStoreRef		*storeP,
				SCNetworkReachabilityRef	target,
				ReachabilityInfo		*reach_info,
				Boolean				async)
{
	CFMutableArrayRef		addresses	= NULL;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;
	ReachabilityInfo		my_info		= NOT_REACHABLE;
	Boolean				ok		= TRUE;

	*reach_info = NOT_REACHABLE;

	if (!isA_SCNetworkReachability(target)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	switch (targetPrivate->type) {
		case reachabilityTypeAddress :
		case reachabilityTypeAddressPair : {
			/*
			 * Check "local" address
			 */
			if (targetPrivate->localAddress != NULL) {
				/*
				 * Check "local" address
				 */
				ok = checkAddress(storeP,
						  targetPrivate->localAddress,
						  &my_info,
						  targetPrivate->log_prefix);
				if (!ok) {
					goto error;	/* not today */
				}

				if (!(my_info.flags & kSCNetworkReachabilityFlagsIsLocalAddress)) {
					goto error;	/* not reachable, non-"local" address */
				}
			}

			/*
			 * Check "remote" address
			 */
			if (targetPrivate->remoteAddress != NULL) {
				/*
				 * in cases where we have "local" and "remote" addresses
				 * we need to re-initialize the to-be-returned flags.
				 */
				my_info = NOT_REACHABLE;

				/*
				 * Check "remote" address
				 */
				ok = checkAddress(storeP,
						  targetPrivate->remoteAddress,
						  &my_info,
						  targetPrivate->log_prefix);
				if (!ok) {
					goto error;	/* not today */
				}
			}

			break;

		}

		case reachabilityTypeName : {
			struct timeval			dnsQueryStart;
			int				error;
			SCNetworkReachabilityFlags	ns_flags;
			struct addrinfo			*res;

			addresses = (CFMutableArrayRef)SCNetworkReachabilityCopyResolvedAddress(target, &error);
			if ((addresses != NULL) || (error != NETDB_SUCCESS)) {
				/* if resolved or an error had been detected */
				if (!async) {
					/* if not an async request */
					goto checkResolvedAddress;
				} else if ((targetPrivate->dnsPort == NULL) && !targetPrivate->needResolve) {
					/* if async request, no query active, and no query needed */
					goto checkResolvedAddress;
				}
			}

			if (!targetPrivate->onDemandBypass) {
				Boolean	onDemand;

				/*
				 * before we attempt our initial DNS query, check if there is
				 * an OnDemand configuration that we should be using.
				 */
				onDemand = __SCNetworkReachabilityOnDemandCheck(storeP, target, FALSE, &my_info.flags);
				if (onDemand) {
					/* if OnDemand connection is needed */
					goto done;
				}
			}

			/* check the reachability of the DNS servers */
			ok = _SC_R_checkResolverReachability(storeP,
							     &ns_flags,
							     &targetPrivate->haveDNS,
							     targetPrivate->name,
							     targetPrivate->serv,
							     targetPrivate->log_prefix);
			if (!ok) {
				/* if we could not get DNS server info */
				SCLog(_sc_debug, LOG_INFO, CFSTR("%sDNS server reachability unknown"),
				      targetPrivate->log_prefix);
				goto error;
			} else if (rankReachability(ns_flags) < 2) {
				/*
				 * if DNS servers are not (or are no longer) reachable, set
				 * flags based on the availability of configured (but not
				 * active) services.
				 */

				SCLog(_sc_debug, LOG_INFO, CFSTR("%sDNS server(s) not available"),
				      targetPrivate->log_prefix);

				ok = checkAddress(storeP,
						  NULL,
						  &my_info,
						  targetPrivate->log_prefix);
				if (!ok) {
					SCLog(_sc_debug, LOG_INFO, CFSTR("%sNo available networks"),
					      targetPrivate->log_prefix);
					goto error;
				}

				if (async && targetPrivate->scheduled) {
					/*
					 * return "host not found", set flags appropriately,
					 * and schedule notification.
					 */
					__SCNetworkReachabilityCallbackSetResolvedAddress(EAI_NONAME,
											  NULL,
											  (void *)target);
					my_info.flags |= (targetPrivate->info.flags & kSCNetworkReachabilityFlagsFirstResolvePending);

					SCLog(_sc_debug, LOG_INFO, CFSTR("%sno DNS servers are reachable"),
					      targetPrivate->log_prefix);
					__SCNetworkReachabilityPerform(target);
				}
				break;
			}

			if (async) {
				/* for async requests we return the last known status */
				my_info = targetPrivate->info;

				if (targetPrivate->dnsPort != NULL) {
					/* if request already in progress */
					SCLog(_sc_debug, LOG_INFO,
					      CFSTR("%swaiting for DNS reply"),
					      targetPrivate->log_prefix);
					if ((addresses != NULL) || (error != NETDB_SUCCESS)) {
						/* updated reachability based on the previous reply */
						goto checkResolvedAddress;
					}
					break;
				}

				SCLog(_sc_debug, LOG_INFO,
				      CFSTR("%sstart DNS query for %s%s%s%s%s"),
				      targetPrivate->log_prefix,
				      targetPrivate->name != NULL ? "name = " : "",
				      targetPrivate->name != NULL ? targetPrivate->name : "",
				      targetPrivate->name != NULL && targetPrivate->serv != NULL ? ", " : "",
				      targetPrivate->serv != NULL ? "serv = " : "",
				      targetPrivate->serv != NULL ? targetPrivate->serv : "");

				/*
				 * initiate an async DNS query
				 */
				if (!startAsyncDNSQuery(target)) {
					/* if we could not initiate the request, process error */
					goto checkResolvedAddress;
				}

				/* request initiated */
				break;
			}

			SCLog(_sc_debug, LOG_INFO,
			      CFSTR("%scheck DNS for %s%s%s%s%s"),
			      targetPrivate->log_prefix,
			      targetPrivate->name != NULL ? "name = " : "",
			      targetPrivate->name != NULL ? targetPrivate->name : "",
			      targetPrivate->name != NULL && targetPrivate->serv != NULL ? ", " : "",
			      targetPrivate->serv != NULL ? "serv = " : "",
			      targetPrivate->serv != NULL ? targetPrivate->serv : "");

			/*
			 * OK, all of the DNS name servers are available.  Let's
			 * resolve the nodename into an address.
			 */
			if (_sc_debug) {
				(void) gettimeofday(&dnsQueryStart, NULL);
			}

			error = getaddrinfo(targetPrivate->name,
					    targetPrivate->serv,
					    &targetPrivate->hints,
					    &res);

			__log_query_time(target,
					 ((error == 0) && (res != NULL)),	// if successful query
					 FALSE,					// sync
					 &dnsQueryStart);			// start time

			__SCNetworkReachabilitySetResolvedAddress(error, res, target);

			addresses = (CFMutableArrayRef)SCNetworkReachabilityCopyResolvedAddress(target, &error);

		    checkResolvedAddress :

			/*
			 * We first assume that the requested host is NOT available.
			 * Then, check each address for accessibility and return the
			 * best status available.
			 */
			my_info = NOT_REACHABLE;

			if (isA_CFArray(addresses)) {
				CFIndex		i;
				CFIndex		n	= CFArrayGetCount(addresses);

				for (i = 0; i < n; i++) {
					ReachabilityInfo	ns_info	= NOT_REACHABLE;
					struct sockaddr		*sa;

					sa = (struct sockaddr *)CFDataGetBytePtr(CFArrayGetValueAtIndex(addresses, i));

					ok = checkAddress(storeP,
							  sa,
							  &ns_info,
							  targetPrivate->log_prefix);
					if (!ok) {
						goto error;	/* not today */
					}

					if (rankReachability(ns_info.flags) > rankReachability(my_info.flags)) {
						/* return the best case result */
						my_info = ns_info;
						if (rankReachability(my_info.flags) == 2) {
							/* we're in luck */
							break;
						}
					}
				}
			} else {
				if ((error == EAI_NONAME)
#if defined(EAI_NODATA) && (EAI_NODATA != EAI_NONAME)
				     || (error == EAI_NODATA)
#endif
				     ) {
					/*
					 * the target host name could not be resolved
					 */
					if (!targetPrivate->onDemandBypass) {
						Boolean	onDemand;

						/*
						 * our initial DNS query failed, check again to see if there
						 * there is an OnDemand configuration that we should be using.
						 */
						onDemand = __SCNetworkReachabilityOnDemandCheck(storeP, target, TRUE, &my_info.flags);
						if (onDemand) {
							/* if OnDemand connection is needed */
							goto done;
						}
					}

					if (!targetPrivate->haveDNS) {
						/*
						 * No DNS servers are defined. Set flags based on
						 * the availability of configured (but not active)
						 * services.
						 */
						ok = checkAddress(storeP,
								  NULL,
								  &my_info,
								  targetPrivate->log_prefix);
						if (!ok) {
							goto error;	/* not today */
						}

						if ((my_info.flags & kSCNetworkReachabilityFlagsReachable) &&
							(my_info.flags & kSCNetworkReachabilityFlagsConnectionRequired)) {
							/*
							 * Since we might pick up a set of DNS servers when this connection
							 * is established, don't reply with a "HOST NOT FOUND" error just yet.
							 */
							break;
						}

						/* Host not found, not reachable! */
						my_info = NOT_REACHABLE;
					}
				}
			}

			break;
		}
	}

    done:

	*reach_info = my_info;

    error :

	if (addresses != NULL)	CFRelease(addresses);
	return ok;
}


Boolean
SCNetworkReachabilityGetFlags(SCNetworkReachabilityRef		target,
			      SCNetworkReachabilityFlags	*flags)
{
	Boolean				ok		= TRUE;
	SCDynamicStoreRef		store		= NULL;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	if (!isA_SCNetworkReachability(target)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	pthread_mutex_lock(&targetPrivate->lock);

	if (targetPrivate->scheduled) {
		// if being watched, return the last known (and what should be current) status
		*flags = targetPrivate->info.flags & ~kSCNetworkReachabilityFlagsFirstResolvePending;
		goto done;
	}


	ok = __SCNetworkReachabilityGetFlags(&store, target, &targetPrivate->info, FALSE);
	*flags = targetPrivate->info.flags & ~kSCNetworkReachabilityFlagsFirstResolvePending;
	if (store != NULL) CFRelease(store);

    done :

	pthread_mutex_unlock(&targetPrivate->lock);
	return ok;
}


#pragma mark -
#pragma mark Notifications


static void
__SCNetworkReachabilityReachabilitySetNotifications(SCDynamicStoreRef	store)
{
	CFStringRef			key;
	CFMutableArrayRef		keys;
	CFStringRef			pattern;
	CFMutableArrayRef		patterns;

	keys     = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	// Setup:/Network/Global/IPv4 (for the ServiceOrder)
	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							 kSCDynamicStoreDomainSetup,
							 kSCEntNetIPv4);
	CFArrayAppendValue(keys, key);
	CFRelease(key);

	// State:/Network/Global/DNS
	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							 kSCDynamicStoreDomainState,
							 kSCEntNetDNS);
	CFArrayAppendValue(keys, key);
	CFRelease(key);

	// State:/Network/Global/IPv4 (default route)
	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							 kSCDynamicStoreDomainState,
							 kSCEntNetIPv4);
	CFArrayAppendValue(keys, key);
	CFRelease(key);

	// State:/Network/Global/OnDemand
	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							 kSCDynamicStoreDomainState,
							 kSCEntNetOnDemand);
	CFArrayAppendValue(keys, key);
	CFRelease(key);

	// Setup: per-service IPv4 info
	pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							      kSCDynamicStoreDomainSetup,
							      kSCCompAnyRegex,
							      kSCEntNetIPv4);
	CFArrayAppendValue(patterns, pattern);
	CFRelease(pattern);

	// Setup: per-service Interface info
	pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							      kSCDynamicStoreDomainSetup,
							      kSCCompAnyRegex,
							      kSCEntNetInterface);
	CFArrayAppendValue(patterns, pattern);
	CFRelease(pattern);

	// Setup: per-service PPP info (for kSCPropNetPPPDialOnDemand)
	pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							      kSCDynamicStoreDomainSetup,
							      kSCCompAnyRegex,
							      kSCEntNetPPP);
	CFArrayAppendValue(patterns, pattern);
	CFRelease(pattern);

	// State: per-service PPP info (for kSCPropNetPPPStatus)
	pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							      kSCDynamicStoreDomainState,
							      kSCCompAnyRegex,
							      kSCEntNetPPP);
	CFArrayAppendValue(patterns, pattern);
	CFRelease(pattern);

	// Setup: per-service IPSec info
	pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							      kSCDynamicStoreDomainSetup,
							      kSCCompAnyRegex,
							      kSCEntNetIPSec);
	CFArrayAppendValue(patterns, pattern);
	CFRelease(pattern);

	// State: per-interface IPv4 info
	pattern = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
								kSCDynamicStoreDomainState,
								kSCCompAnyRegex,
								kSCEntNetIPv4);
	CFArrayAppendValue(patterns, pattern);
	CFRelease(pattern);

	// State: per-interface IPv6 info
	pattern = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
								kSCDynamicStoreDomainState,
								kSCCompAnyRegex,
								kSCEntNetIPv6);
	CFArrayAppendValue(patterns, pattern);
	CFRelease(pattern);

#if	!TARGET_OS_IPHONE
	// State: Power Management Capabilities
	key = SCDynamicStoreKeyCreate(NULL, CFSTR("%@%@"),
				      kSCDynamicStoreDomainState,
				      CFSTR(kIOPMSystemPowerCapabilitiesKeySuffix));
	CFArrayAppendValue(keys, key);
	CFRelease(key);
#endif	// TARGET_OS_IPHONE


	(void)SCDynamicStoreSetNotificationKeys(store, keys, patterns);
	CFRelease(keys);
	CFRelease(patterns);

	return;
}


static void
__SCNetworkReachabilityHandleChanges(SCDynamicStoreRef	store,
				     CFArrayRef		changedKeys,
				     void		*info)
{
	Boolean		dnsConfigChanged	= FALSE;
	CFIndex		i;
	CFStringRef	key;
	CFIndex		nChanges		= CFArrayGetCount(changedKeys);
	CFIndex		nTargets;
#if	!TARGET_OS_IPHONE
	Boolean		powerStatusChanged	= FALSE;
#endif	// !TARGET_OS_IPHONE
	const void *	targets_q[N_QUICK];
	const void **	targets			= targets_q;

	if (nChanges == 0) {
		/* if no changes */
		return;
	}

	pthread_mutex_lock(&hn_lock);

	nTargets = (hn_targets != NULL) ? CFSetGetCount(hn_targets) : 0;
	if (nTargets == 0) {
		/* if no addresses being monitored */
		goto done;
	}

#if	!TARGET_OS_IPHONE
	key = SCDynamicStoreKeyCreate(NULL, CFSTR("%@%@"),
				      kSCDynamicStoreDomainState,
				      CFSTR(kIOPMSystemPowerCapabilitiesKeySuffix));
	if (CFArrayContainsValue(changedKeys, CFRangeMake(0, nChanges), key)) {
		CFNumberRef	num;

		num = SCDynamicStoreCopyValue(store, key);
		if (num != NULL) {
			if (isA_CFNumber(num) &&
			    CFNumberGetValue(num, kCFNumberSInt32Type, &power_capabilities)) {
				powerStatusChanged = TRUE;
			}

			CFRelease(num);
		}
	}
	CFRelease(key);
#endif	// !TARGET_OS_IPHONE

	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							 kSCDynamicStoreDomainState,
							 kSCEntNetDNS);
	if (CFArrayContainsValue(changedKeys, CFRangeMake(0, nChanges), key)) {
		dnsConfigChanged = TRUE;	/* the DNS server(s) have changed */
	}
	CFRelease(key);

	if (_sc_debug) {
		int		changes	= 0;
		const char	*str;

#if	!TARGET_OS_IPHONE
		if (powerStatusChanged) {
			changes |= 4;
			nChanges -= 1;
		}
#endif	// !TARGET_OS_IPHONE

		if (dnsConfigChanged) {
			changes |= 2;
			nChanges -= 1;
		}

		if (nChanges > 0) {
			changes |= 1;
		}

		switch (changes) {
			case 0	: str = "";				break;
			case 1	: str = "network ";			break;
			case 2	: str = "DNS ";				break;
			case 3	: str = "network and DNS ";		break;
#if	!TARGET_OS_IPHONE
			case 4	: str = "power ";			break;
			case 5	: str = "network and power ";		break;
			case 6	: str = "DNS and power ";		break;
			case 7	: str = "network, DNS, and power ";	break;
#endif	// !TARGET_OS_IPHONE
			default	: str = "??? ";
		}

		SCLog(TRUE, LOG_INFO, CFSTR("process %sconfiguration change"), str);
	}

	if (nTargets > (CFIndex)(sizeof(targets_q) / sizeof(CFTypeRef)))
		targets = CFAllocatorAllocate(NULL, nTargets * sizeof(CFTypeRef), 0);
	CFSetGetValues(hn_targets, targets);
	for (i = 0; i < nTargets; i++) {
		SCNetworkReachabilityRef	target		= targets[i];
		SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

		pthread_mutex_lock(&targetPrivate->lock);

		if (targetPrivate->type == reachabilityTypeName) {
			Boolean		dnsChanged	= dnsConfigChanged;

			if (!dnsChanged) {
				/*
				 * if the DNS configuration didn't change we still need to
				 * check that the DNS servers are accessible.
				 */
				SCNetworkReachabilityFlags	ns_flags;
				Boolean				ok;

				/* check the reachability of the DNS servers */
				ok = _SC_R_checkResolverReachability(&store,
								     &ns_flags,
								     &targetPrivate->haveDNS,
								     targetPrivate->name,
								     targetPrivate->serv,
								     targetPrivate->log_prefix);
				if (!ok) {
					/* if we could not get DNS server info */
					SCLog(_sc_debug, LOG_INFO, CFSTR("%sDNS server reachability unknown"),
					      targetPrivate->log_prefix);
					dnsChanged = TRUE;
				} else if (rankReachability(ns_flags) < 2) {
					/*
					 * if DNS servers are not (or are no longer) reachable, set
					 * flags based on the availability of configured (but not
					 * active) services.
					 */
					SCLog(_sc_debug, LOG_INFO, CFSTR("%sDNS server(s) not available"),
					      targetPrivate->log_prefix);
					dnsChanged = TRUE;
				}
			}

			if (dnsChanged) {
				if (targetPrivate->dnsPort != NULL) {
					mach_port_t	mp	= CFMachPortGetPort(targetPrivate->dnsPort);

					/* cancel the outstanding DNS query */
					SCLog(_sc_debug, LOG_INFO,
					      CFSTR("%scancel DNS query for %s%s%s%s%s"),
					      targetPrivate->log_prefix,
					      targetPrivate->name != NULL ? "name = " : "",
					      targetPrivate->name != NULL ? targetPrivate->name : "",
					      targetPrivate->name != NULL && targetPrivate->serv != NULL ? ", " : "",
					      targetPrivate->serv != NULL ? "serv = " : "",
					      targetPrivate->serv != NULL ? targetPrivate->serv : "");
					dequeueAsyncDNSQuery(target);
					getaddrinfo_async_cancel(mp);
				}

				/* schedule request to resolve the name again */
				targetPrivate->needResolve = TRUE;
			}
		}

		__SCNetworkReachabilityPerform(target);

		pthread_mutex_unlock(&targetPrivate->lock);
	}
	if (targets != targets_q)	CFAllocatorDeallocate(NULL, targets);

    done :

	pthread_mutex_unlock(&hn_lock);
	return;
}


#if	!TARGET_OS_IPHONE
static __inline__ Boolean
systemIsAwake(IOPMSystemPowerStateCapabilities power_capabilities)
{

#define POWER_CAPABILITIES_NEED	(kIOPMSystemPowerStateCapabilityCPU		\
				 | kIOPMSystemPowerStateCapabilityNetwork	\
				 | kIOPMSystemPowerStateCapabilityDisk)

	if ((power_capabilities & POWER_CAPABILITIES_NEED) != POWER_CAPABILITIES_NEED) {
		/*
		 * we're not awake (from a networking point of view) unless we
		 * have the CPU, disk, *and* network.
		 */
		return FALSE;
	}

	if ((power_capabilities & kIOPMSytemPowerStateCapabilitiesMask) == POWER_CAPABILITIES_NEED) {
		/*
		 * if all we have is the CPU, disk, and network than this must
		 * be a "maintenance" wake.
		 */
		return FALSE;
	}

	return TRUE;
}
#endif	// !TARGET_OS_IPHONE


static void
rlsPerform(void *info)
{
	void				*context_info;
	void				(*context_release)(const void *);
	Boolean				defer		= FALSE;
	Boolean				ok;
	ReachabilityInfo		reach_info	= NOT_REACHABLE;
	SCNetworkReachabilityCallBack	rlsFunction;
	SCDynamicStoreRef		store		= NULL;
	SCNetworkReachabilityRef	target		= (SCNetworkReachabilityRef)info;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	SCLog(_sc_debug, LOG_INFO, CFSTR("%schecking target reachability"),
	      targetPrivate->log_prefix);


	pthread_mutex_lock(&targetPrivate->lock);

	if (!targetPrivate->scheduled) {
		// if not currently scheduled
		pthread_mutex_unlock(&targetPrivate->lock);
		return;
	}

	/* update reachability, notify if status changed */
	ok = __SCNetworkReachabilityGetFlags(&store, target, &reach_info, TRUE);
	if (store != NULL) CFRelease(store);
	if (!ok) {
		/* if reachability status not available */
		SCLog(_sc_debug, LOG_INFO, CFSTR("%flags not available"),
		      targetPrivate->log_prefix);
		reach_info = NOT_REACHABLE;
	}

#if	!TARGET_OS_IPHONE
	/*
	 * We want to defer the notification if this is a maintenance wake *and*
	 * the reachability flags that we would be reporting to the application
	 * are better than those that we last reported.
	 */
	if (!systemIsAwake(power_capabilities)) {
		/* if this is a maintenace wake */
		reach_info.sleeping = TRUE;

		if (rankReachability(reach_info.flags) >= rankReachability(targetPrivate->info.flags)) {
			/*
			 * don't report the change if the new reachability flags are
			 * the same or "better"
			 */
			defer = TRUE;
		} else if (bcmp(&targetPrivate->last_notify, &reach_info, sizeof(reach_info)) == 0) {
			/* if we have already posted this change */
			defer = TRUE;
		}
	}
#endif	// !TARGET_OS_IPHONE

	if (bcmp(&targetPrivate->info, &reach_info, sizeof(reach_info)) == 0) {
		SCLog(_sc_debug, LOG_INFO,
		      CFSTR("%sflags/interface match (now 0x%08x/%hu%s)"),
		      targetPrivate->log_prefix,
		      reach_info.flags,
		      reach_info.if_index,
		      reach_info.sleeping ? "*" : "");
		pthread_mutex_unlock(&targetPrivate->lock);
		return;
	}

	SCLog(_sc_debug, LOG_INFO,
	      CFSTR("%sflags/interface have changed (was 0x%08x/%hu%s, now 0x%08x/%hu%s)%s"),
	      targetPrivate->log_prefix,
	      targetPrivate->info.flags,
	      targetPrivate->info.if_index,
	      targetPrivate->info.sleeping ? "*" : "",
	      reach_info.flags,
	      reach_info.if_index,
	      reach_info.sleeping ? "*" : "",
	      defer ? ", deferred" : "");

	/* update flags / interface */
	targetPrivate->info = reach_info;

	/* as needed, defer the notification */
	if (defer) {
		pthread_mutex_unlock(&targetPrivate->lock);
		return;
	}

	/* save last notification info */
	targetPrivate->last_notify = reach_info;

	/* callout */
	rlsFunction = targetPrivate->rlsFunction;
	if (targetPrivate->rlsContext.retain != NULL) {
		context_info	= (void *)(*targetPrivate->rlsContext.retain)(targetPrivate->rlsContext.info);
		context_release	= targetPrivate->rlsContext.release;
	} else {
		context_info	= targetPrivate->rlsContext.info;
		context_release	= NULL;
	}

	pthread_mutex_unlock(&targetPrivate->lock);

	if (rlsFunction != NULL) {
		(*rlsFunction)(target, reach_info.flags, context_info);
	}

	if (context_release != NULL) {
		(*context_release)(context_info);
	}

	return;
}


Boolean
SCNetworkReachabilitySetCallback(SCNetworkReachabilityRef	target,
				 SCNetworkReachabilityCallBack	callout,
				 SCNetworkReachabilityContext	*context)
{
	SCNetworkReachabilityPrivateRef	targetPrivate = (SCNetworkReachabilityPrivateRef)target;

	pthread_mutex_lock(&targetPrivate->lock);

	if (targetPrivate->rlsContext.release != NULL) {
		/* let go of the current context */
		(*targetPrivate->rlsContext.release)(targetPrivate->rlsContext.info);
	}

	targetPrivate->rlsFunction   			= callout;
	targetPrivate->rlsContext.info			= NULL;
	targetPrivate->rlsContext.retain		= NULL;
	targetPrivate->rlsContext.release		= NULL;
	targetPrivate->rlsContext.copyDescription	= NULL;
	if (context) {
		bcopy(context, &targetPrivate->rlsContext, sizeof(SCNetworkReachabilityContext));
		if (context->retain != NULL) {
			targetPrivate->rlsContext.info = (void *)(*context->retain)(context->info);
		}
	}

	pthread_mutex_unlock(&targetPrivate->lock);

	return TRUE;
}


static CFStringRef
reachRLSCopyDescription(const void *info)
{
	SCNetworkReachabilityRef		target	= (SCNetworkReachabilityRef)info;

	return CFStringCreateWithFormat(NULL,
					NULL,
					CFSTR("<SCNetworkReachability RLS> {target = %p}"),
					target);
}


static Boolean
__SCNetworkReachabilityScheduleWithRunLoop(SCNetworkReachabilityRef	target,
					   CFRunLoopRef			runLoop,
					   CFStringRef			runLoopMode,
#if	!TARGET_OS_IPHONE
					   dispatch_queue_t		queue,
#else	// !TARGET_OS_IPHONE
					   void				*queue,
#endif	// !TARGET_OS_IPHONE
					   Boolean			onDemand)
{
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;
	Boolean				init		= FALSE;
	Boolean				ok		= FALSE;

	if (!onDemand) {
		pthread_mutex_lock(&hn_lock);
	}
	pthread_mutex_lock(&targetPrivate->lock);

#if	!TARGET_OS_IPHONE
	if ((targetPrivate->dispatchQueue != NULL) ||		// if we are already scheduled with a dispatch queue
	    ((queue != NULL) && targetPrivate->scheduled)) {	// if we are already scheduled on a CFRunLoop
		_SCErrorSet(kSCStatusInvalidArgument);
		goto done;
	}
#endif	// !TARGET_OS_IPHONE

	/* schedule the SCNetworkReachability run loop source */

	if (!onDemand && (hn_store == NULL)) {
		/*
		 * if we are not monitoring any hosts, start watching
		 */
		if (!dns_configuration_watch()) {
			// if error
			_SCErrorSet(kSCStatusFailed);
			goto done;
		}

		hn_store = SCDynamicStoreCreate(NULL,
						CFSTR("SCNetworkReachability"),
						__SCNetworkReachabilityHandleChanges,
						NULL);
		if (hn_store == NULL) {
			SCLog(TRUE, LOG_ERR, CFSTR("SCDynamicStoreCreate() failed"));
			goto done;
		}

		__SCNetworkReachabilityReachabilitySetNotifications(hn_store);

#if	!TARGET_OS_IPHONE
		hn_dispatchQueue = dispatch_queue_create("com.apple.SCNetworkReachabilty.network_changes", NULL);
		if (hn_dispatchQueue == NULL) {
			SCLog(TRUE, LOG_ERR, CFSTR("__SCNetworkReachabilityScheduleWithRunLoop dispatch_queue_create() failed"));
			_SCErrorSet(kSCStatusFailed);
			CFRelease(hn_store);
			hn_store = NULL;
			goto done;
		}
		CFRetain(hn_store);	// Note: will be released when the dispatch queue is released
		dispatch_set_context(hn_dispatchQueue, (void *)hn_store);
		dispatch_set_finalizer_f(hn_dispatchQueue, (dispatch_function_t)CFRelease);

		ok = SCDynamicStoreSetDispatchQueue(hn_store, hn_dispatchQueue);
		if (!ok) {
			SCLog(TRUE, LOG_ERR, CFSTR("SCDynamicStoreSetDispatchQueue() failed"));
			dispatch_release(hn_dispatchQueue);
			hn_dispatchQueue = NULL;
			CFRelease(hn_store);
			hn_store = NULL;
			goto done;
		}
#else	// !TARGET_OS_IPHONE
		hn_storeRLS = SCDynamicStoreCreateRunLoopSource(NULL, hn_store, 0);
		hn_rlList   = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
#endif	// !TARGET_OS_IPHONE
		hn_targets  = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
	}

	if (!targetPrivate->scheduled) {
		CFRunLoopSourceContext	context = { 0				// version
						  , (void *)target		// info
						  , CFRetain			// retain
						  , CFRelease			// release
						  , reachRLSCopyDescription	// copyDescription
						  , CFEqual			// equal
						  , CFHash			// hash
						  , NULL			// schedule
						  , NULL			// cancel
						  , rlsPerform			// perform
						  };

		if (runLoop != NULL) {
			targetPrivate->rls    = CFRunLoopSourceCreate(NULL, 0, &context);
			targetPrivate->rlList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		}

		targetPrivate->scheduled = TRUE;
		if (targetPrivate->type == reachabilityTypeName) {
			targetPrivate->needResolve = TRUE;
		}
		init = TRUE;
	}

#if	!TARGET_OS_IPHONE
	if (queue != NULL) {
		targetPrivate->dispatchQueue = queue;
		dispatch_retain(targetPrivate->dispatchQueue);
	} else
#endif	// !TARGET_OS_IPHONE
	{
		if (!_SC_isScheduled(NULL, runLoop, runLoopMode, targetPrivate->rlList)) {
			/*
			 * if we do not already have host notifications scheduled with
			 * this runLoop / runLoopMode
			 */
			CFRunLoopAddSource(runLoop, targetPrivate->rls, runLoopMode);

			if (targetPrivate->dnsRLS != NULL) {
				/* if we have an active async DNS query too */
				CFRunLoopAddSource(runLoop, targetPrivate->dnsRLS, runLoopMode);
			}
		}

		_SC_schedule(target, runLoop, runLoopMode, targetPrivate->rlList);

#if	TARGET_OS_IPHONE
		if (!onDemand) {
			/* schedule the global SCDynamicStore run loop source */

			if (!_SC_isScheduled(NULL, runLoop, runLoopMode, hn_rlList)) {
				/*
				 * if we do not already have SC notifications scheduled with
				 * this runLoop / runLoopMode
				 */
				CFRunLoopAddSource(runLoop, hn_storeRLS, runLoopMode);
			}

			_SC_schedule(target, runLoop, runLoopMode, hn_rlList);
		}
#endif	// TARGET_OS_IPHONE
	}

	CFSetAddValue(hn_targets, target);

	if (init) {
		ReachabilityInfo	reach_info	= NOT_REACHABLE;
		SCDynamicStoreRef	store		= NULL;

		/*
		 * if we have yet to schedule SC notifications for this address
		 * - initialize current reachability status
		 */
		if (__SCNetworkReachabilityGetFlags(&store, target, &reach_info, TRUE)) {
			/*
			 * if reachability status available
			 * - set flags
			 * - schedule notification to report status via callback
			 */
			targetPrivate->info = reach_info;
			__SCNetworkReachabilityPerform(target);
		} else {
			/* if reachability status not available, async lookup started */
			targetPrivate->info = NOT_REACHABLE;
		}
		if (store != NULL) CFRelease(store);
	}

	if (targetPrivate->onDemandServer != NULL) {
		__SCNetworkReachabilityScheduleWithRunLoop(targetPrivate->onDemandServer, runLoop, runLoopMode, queue, TRUE);
	}

	ok = TRUE;

    done :

	pthread_mutex_unlock(&targetPrivate->lock);
	if (!onDemand) {
		pthread_mutex_unlock(&hn_lock);
	}
	return ok;
}


static Boolean
__SCNetworkReachabilityUnscheduleFromRunLoop(SCNetworkReachabilityRef	target,
					     CFRunLoopRef		runLoop,
					     CFStringRef		runLoopMode,
					     Boolean			onDemand)
{
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;
	CFIndex				n		= 0;
	Boolean				ok		= FALSE;

	if (!onDemand) {
		pthread_mutex_lock(&hn_lock);
	}
	pthread_mutex_lock(&targetPrivate->lock);

#if	!TARGET_OS_IPHONE
	if (((runLoop == NULL) && (targetPrivate->dispatchQueue == NULL)) ||	// if we should be scheduled on a dispatch queue (but are not)
	    ((runLoop != NULL) && (targetPrivate->dispatchQueue != NULL))) {	// if we should be scheduled on a CFRunLoop (but are not)
		_SCErrorSet(kSCStatusInvalidArgument);
		goto done;
	}
#endif	// !TARGET_OS_IPHONE

	if (!targetPrivate->scheduled) {
		// if not currently scheduled
		_SCErrorSet(kSCStatusInvalidArgument);
		goto done;
	}

	// first, unschedule the target specific sources
#if	!TARGET_OS_IPHONE
	if (targetPrivate->dispatchQueue != NULL) {
		if (targetPrivate->onDemandServer != NULL) {
			__SCNetworkReachabilityUnscheduleFromRunLoop(targetPrivate->onDemandServer, NULL, NULL, TRUE);
		}
	} else
#endif	// !TARGET_OS_IPHONE
	{
		if (!_SC_unschedule(target, runLoop, runLoopMode, targetPrivate->rlList, FALSE)) {
			// if not currently scheduled
			_SCErrorSet(kSCStatusInvalidArgument);
			goto done;
		}

		if (targetPrivate->onDemandServer != NULL) {
			__SCNetworkReachabilityUnscheduleFromRunLoop(targetPrivate->onDemandServer, runLoop, runLoopMode, TRUE);
		}

		n = CFArrayGetCount(targetPrivate->rlList);
		if ((n == 0) || !_SC_isScheduled(NULL, runLoop, runLoopMode, targetPrivate->rlList)) {
			// if target is no longer scheduled for this runLoop / runLoopMode
			CFRunLoopRemoveSource(runLoop, targetPrivate->rls, runLoopMode);

			if (targetPrivate->dnsRLS != NULL) {
				// if we have an active async DNS query too
				CFRunLoopRemoveSource(runLoop, targetPrivate->dnsRLS, runLoopMode);
			}

			if (n == 0) {
				// if *all* notifications have been unscheduled
				CFRelease(targetPrivate->rlList);
				targetPrivate->rlList = NULL;
				CFRunLoopSourceInvalidate(targetPrivate->rls);
				CFRelease(targetPrivate->rls);
				targetPrivate->rls = NULL;
			}
		}
	}

	if (n == 0) {
		// if *all* notifications have been unscheduled
		targetPrivate->scheduled = FALSE;

		if (!onDemand) {
			CFSetRemoveValue(hn_targets, target);	// cleanup notification resources
		}

		if (targetPrivate->dnsPort != NULL) {
			mach_port_t	mp	= CFMachPortGetPort(targetPrivate->dnsPort);

			// if we have an active async DNS query
			dequeueAsyncDNSQuery(target);
			getaddrinfo_async_cancel(mp);
		}
	}

#if	!TARGET_OS_IPHONE
	if (runLoop == NULL) {
		dispatch_release(targetPrivate->dispatchQueue);
		targetPrivate->dispatchQueue = NULL;
	}
#endif	// !TARGET_OS_IPHONE

	// now, unschedule the global dynamic store source
#if	TARGET_OS_IPHONE
	if (!onDemand) {
		(void)_SC_unschedule(target, runLoop, runLoopMode, hn_rlList, FALSE);

		n = CFArrayGetCount(hn_rlList);
		if ((n == 0) || !_SC_isScheduled(NULL, runLoop, runLoopMode, hn_rlList)) {
			/*
			 * if we no longer have any addresses scheduled for
			 * this runLoop / runLoopMode
			 */
			CFRunLoopRemoveSource(runLoop, hn_storeRLS, runLoopMode);

			if (n > 0) {
				if (CFEqual(runLoopMode, kCFRunLoopCommonModes)) {
					CFArrayRef	modes;

					modes = CFRunLoopCopyAllModes(runLoop);
					if (modes != NULL) {
						CFIndex	i;
						CFIndex	n	= CFArrayGetCount(modes);

						for (i = 0; i < n; i++) {
							CFStringRef	mode;

							mode = CFArrayGetValueAtIndex(modes, i);
							if (_SC_isScheduled(NULL, runLoop, mode, hn_rlList)) {
								/*
								 * removing kCFRunLoopCommonModes cleaned up more
								 * than we wanted.  Add back the modes that were
								 * expect to be present.
								 */
								CFRunLoopAddSource(runLoop, hn_storeRLS, mode);
							}
						}

						CFRelease(modes);
					}
				} else if (_SC_isScheduled(NULL, runLoop, kCFRunLoopCommonModes, hn_rlList)) {
					/*
					 * if we are still scheduling kCFRunLoopCommonModes, make sure that
					 * none of the common modes were inadvertently removed.
					 */
					CFRunLoopAddSource(runLoop, hn_storeRLS, kCFRunLoopCommonModes);
				}
			}
		}
	}
#endif	// TARGET_OS_IPHONE

	n = CFSetGetCount(hn_targets);
	if (n == 0) {
		// if we are no longer monitoring any targets
#if	!TARGET_OS_IPHONE
		SCDynamicStoreSetDispatchQueue(hn_store, NULL);
		dispatch_release(hn_dispatchQueue);
		hn_dispatchQueue = NULL;
#else	// !TARGET_OS_IPHONE
		CFRunLoopSourceInvalidate(hn_storeRLS);
		CFRelease(hn_storeRLS);
		hn_storeRLS = NULL;
		CFRelease(hn_rlList);
		hn_rlList = NULL;
#endif	// !TARGET_OS_IPHONE
		CFRelease(hn_store);
		hn_store = NULL;
		CFRelease(hn_targets);
		hn_targets = NULL;

		/*
		 * until we start monitoring again, ensure that
		 * any resources associated with tracking the
		 * DNS configuration have been released.
		 */
		dns_configuration_unwatch();
	}

	ok = TRUE;

    done :

	pthread_mutex_unlock(&targetPrivate->lock);
	if (!onDemand) {
		pthread_mutex_unlock(&hn_lock);
	}
	return ok;
}

Boolean
SCNetworkReachabilityScheduleWithRunLoop(SCNetworkReachabilityRef	target,
					 CFRunLoopRef			runLoop,
					 CFStringRef			runLoopMode)
{
	if (!isA_SCNetworkReachability(target) || (runLoop == NULL) || (runLoopMode == NULL)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	return __SCNetworkReachabilityScheduleWithRunLoop(target, runLoop, runLoopMode, NULL, FALSE);
}

Boolean
SCNetworkReachabilityUnscheduleFromRunLoop(SCNetworkReachabilityRef	target,
					   CFRunLoopRef			runLoop,
					   CFStringRef			runLoopMode)
{
	if (!isA_SCNetworkReachability(target) || (runLoop == NULL) || (runLoopMode == NULL)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	return __SCNetworkReachabilityUnscheduleFromRunLoop(target, runLoop, runLoopMode, FALSE);
}

#if	!TARGET_OS_IPHONE
Boolean
SCNetworkReachabilitySetDispatchQueue(SCNetworkReachabilityRef	target,
				      dispatch_queue_t		queue)
{
	Boolean	ok	= FALSE;

	if (!isA_SCNetworkReachability(target)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (queue != NULL) {
		ok = __SCNetworkReachabilityScheduleWithRunLoop(target, NULL, NULL, queue, FALSE);
	} else {
		ok = __SCNetworkReachabilityUnscheduleFromRunLoop(target, NULL, NULL, FALSE);
	}

	return ok;
}
#endif	// !TARGET_OS_IPHONE
