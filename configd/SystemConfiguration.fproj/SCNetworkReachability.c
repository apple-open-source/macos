/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 * January 19, 2003		Allan Nathanson <ajn@apple.com>
 * - add advanced reachability APIs
 */

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>

#include <CoreFoundation/CFRuntime.h>
#include <pthread.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <netdb_async.h>
#include <resolv.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#define	KERNEL_PRIVATE
#include <net/route.h>
#undef	KERNEL_PRIVATE

#ifndef s6_addr16
#define s6_addr16 __u6_addr.__u6_addr16
#endif

#include "ppp.h"


#define kSCNetworkFlagsFirstResolvePending	(1<<31)


#define	N_QUICK	32


typedef enum {
	reachabilityTypeAddress,
	reachabilityTypeAddressPair,
	reachabilityTypeName
} addressType;


static CFStringRef	__SCNetworkReachabilityCopyDescription	(CFTypeRef cf);
static void		__SCNetworkReachabilityDeallocate	(CFTypeRef cf);


typedef struct {

	/* base CFType information */
	CFRuntimeBase			cfBase;

	/* lock */
	pthread_mutex_t			lock;

	/* address type */
	addressType			type;

	/* target host name */
	const char			*name;
	CFArrayRef			resolvedAddress;	/* CFArray[CFData] */
	int				resolvedAddressError;

	/* local & remote addresses */
	struct sockaddr			*localAddress;
	struct sockaddr			*remoteAddress;

	/* current reachability flags */
	SCNetworkConnectionFlags 	flags;
	uint16_t			if_index;

	/* run loop source, callout, context, rl scheduling info */
	CFRunLoopSourceRef		rls;
	SCNetworkReachabilityCallBack	rlsFunction;
	SCNetworkReachabilityContext	rlsContext;
	CFMutableArrayRef		rlList;

	/* async DNS query info */
	CFMachPortRef			dnsPort;
	CFRunLoopSourceRef		dnsRLS;

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
static Boolean			needDNS		= TRUE;


/* host "something has changed" notifications */
static pthread_mutex_t		hn_lock		= PTHREAD_MUTEX_INITIALIZER;
static SCDynamicStoreRef	hn_store	= NULL;
static CFRunLoopSourceRef	hn_storeRLS	= NULL;
static CFMutableArrayRef	hn_rlList	= NULL;
static CFMutableSetRef		hn_targets	= NULL;


static __inline__ CFTypeRef
isA_SCNetworkReachability(CFTypeRef obj)
{
	return (isA_CFType(obj, SCNetworkReachabilityGetTypeID()));
}


static void
sockaddr_to_string(const struct sockaddr *address, char *buf, size_t bufLen)
{
	bzero(buf, bufLen);
	switch (address->sa_family) {
		case AF_INET :
			(void)inet_ntop(((struct sockaddr_in *)address)->sin_family,
					&((struct sockaddr_in *)address)->sin_addr,
					buf,
					bufLen);
			break;
		case AF_INET6 : {
			(void)inet_ntop(((struct sockaddr_in6 *)address)->sin6_family,
					&((struct sockaddr_in6 *)address)->sin6_addr,
					buf,
					bufLen);
			if (((struct sockaddr_in6 *)address)->sin6_scope_id != 0) {
				int	n;

				n = strlen(buf);
				if ((n+IF_NAMESIZE+1) <= (int)bufLen) {
					buf[n++] = '%';
					if_indextoname(((struct sockaddr_in6 *)address)->sin6_scope_id, &buf[n]);
				}
			}
			break;
		}
		case AF_LINK :
			if (((struct sockaddr_dl *)address)->sdl_len < bufLen) {
				bufLen = ((struct sockaddr_dl *)address)->sdl_len;
			} else {
				bufLen = bufLen - 1;
			}

			bcopy(((struct sockaddr_dl *)address)->sdl_data, buf, bufLen);
			break;
		default :
			snprintf(buf, bufLen, "unexpected address family %d", address->sa_family);
			break;
	}

	return;
}


#ifndef	CHECK_IPV6_REACHABILITY
static char *
__netdb_error(int error)
{
	char	*msg;

	switch(error) {
		case NETDB_INTERNAL :
			msg = strerror(errno);
			break;
		case HOST_NOT_FOUND :
			msg = "Host not found.";
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

	return msg;
}
#endif	/* CHECK_IPV6_REACHABILITY */


static void
__signalRunLoop(CFTypeRef obj, CFRunLoopSourceRef rls, CFArrayRef rlList)
{
	CFRunLoopRef	rl	= NULL;
	CFRunLoopRef	rl1	= NULL;
	CFIndex		i;
	CFIndex		n	= CFArrayGetCount(rlList);

	if (n == 0) {
		return;
	}

	/* get first runLoop for this object */
	for (i = 0; i < n; i += 3) {
		if (!CFEqual(obj, CFArrayGetValueAtIndex(rlList, i))) {
			continue;
		}

		rl1 = (CFRunLoopRef)CFArrayGetValueAtIndex(rlList, i+1);
		break;
	}

	if (!rl1) {
		/* if not scheduled */
		return;
	}

	/* check if we have another runLoop for this object */
	rl = rl1;
	for (i = i+3; i < n; i += 3) {
		CFRunLoopRef	rl2;

		if (!CFEqual(obj, CFArrayGetValueAtIndex(rlList, i))) {
			continue;
		}

		rl2 = (CFRunLoopRef)CFArrayGetValueAtIndex(rlList, i+1);
		if (!CFEqual(rl1, rl2)) {
			/* we've got more than one runLoop */
			rl = NULL;
			break;
		}
	}

	if (rl) {
		/* if we only have one runLoop */
		CFRunLoopWakeUp(rl);
		return;
	}

	/* more than one different runLoop, so we must pick one */
	for (i = 0; i < n; i+=3) {
		CFStringRef	rlMode;

		if (!CFEqual(obj, CFArrayGetValueAtIndex(rlList, i))) {
			continue;
		}

		rl     = (CFRunLoopRef)CFArrayGetValueAtIndex(rlList, i+1);
		rlMode = CFRunLoopCopyCurrentMode(rl);
		if (rlMode && CFRunLoopIsWaiting(rl) && CFRunLoopContainsSource(rl, rls, rlMode)) {
			/* we've found a runLoop that's "ready" */
			CFRelease(rlMode);
			CFRunLoopWakeUp(rl);
			return;
		}
		if (rlMode) CFRelease(rlMode);
	}

	/* didn't choose one above, so choose first */
	CFRunLoopWakeUp(rl1);
	return;
}


static int
updatePPPStatus(SCDynamicStoreRef		*storeP,
		const struct sockaddr		*sa,
		const char			*if_name,
		SCNetworkConnectionFlags	*flags)
{
	CFDictionaryRef		dict		= NULL;
	CFStringRef		entity;
	CFIndex			i;
	const void *		keys_q[N_QUICK];
	const void **		keys		= keys_q;
	CFIndex			n;
	CFStringRef		ppp_if;
	int			sc_status	= kSCStatusReachabilityUnknown;
	SCDynamicStoreRef	store		= (storeP) ? *storeP : NULL;
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

	if (!store) {
		store = SCDynamicStoreCreate(NULL, CFSTR("SCNetworkReachability"), NULL, NULL);
		if (!store) {
			SCLog(_sc_verbose, LOG_INFO, CFSTR("SCDynamicStoreCreate() failed"));
			goto done;
		}
	}

	/*
	 * grab a snapshot of the PPP configuration from the dynamic store
	 */
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
	if (!dict) {
		/* if we could not access the dynamic store */
		goto done;
	}

	sc_status = kSCStatusOK;

	/*
	 * look for the service which matches the provided interface
	 */
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

		/*
		 * extract service ID, get PPP "state" entity (for status), and get
		 * the "setup" entity (for dial-on-traffic flag)
		 */
		components = CFStringCreateArrayBySeparatingStrings(NULL, s_key, CFSTR("/"));
		if (CFArrayGetCount(components) != 5) {
			continue;
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

		// get PPP status
		if (!isA_CFDictionary(p_state)) {
			continue;
		}
		num = CFDictionaryGetValue(p_state, kSCPropNetPPPStatus);
		if (!isA_CFNumber(num)) {
			continue;
		}

		if (!CFNumberGetValue(num, kCFNumberSInt32Type, &ppp_status)) {
			continue;
		}
		switch (ppp_status) {
			case PPP_RUNNING :
				/* if we're really UP and RUNNING */
				break;
			case PPP_ONHOLD :
				/* if we're effectively UP and RUNNING */
				break;
			case PPP_IDLE :
			case PPP_STATERESERVED :
				/* if we're not connected at all */
				SCLog(_sc_debug, LOG_INFO, CFSTR("  PPP link idle, dial-on-traffic to connect"));
				*flags |= kSCNetworkFlagsConnectionRequired;
				break;
			default :
				/* if we're in the process of [dis]connecting */
				SCLog(_sc_debug, LOG_INFO, CFSTR("  PPP link, connection in progress"));
				*flags |= kSCNetworkFlagsConnectionRequired;
				break;
		}

		// check PPP dial-on-traffic status
		if (isA_CFDictionary(p_setup)) {
			num = CFDictionaryGetValue(p_setup, kSCPropNetPPPDialOnDemand);
			if (isA_CFNumber(num)) {
				int32_t	ppp_demand;

				if (CFNumberGetValue(num, kCFNumberSInt32Type, &ppp_demand)) {
					if (ppp_demand) {
						*flags |= kSCNetworkFlagsConnectionAutomatic;
						if (ppp_status == PPP_IDLE) {
							*flags |= kSCNetworkFlagsInterventionRequired;
						}
					}
				}
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

	if (dict)	CFRelease(dict);
	if (storeP)	*storeP = store;
	return sc_status;
}


static int
updatePPPAvailable(SCDynamicStoreRef		*storeP,
		  const struct sockaddr		*sa,
		  SCNetworkConnectionFlags	*flags)
{
	CFDictionaryRef		dict		= NULL;
	CFStringRef		entity;
	CFIndex			i;
	const void *		keys_q[N_QUICK];
	const void **		keys		= keys_q;
	CFIndex			n;
	int			sc_status	= kSCStatusReachabilityUnknown;
	SCDynamicStoreRef	store		= (storeP) ? *storeP : NULL;
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

	if (!store) {
		store = SCDynamicStoreCreate(NULL, CFSTR("SCNetworkReachability"), NULL, NULL);
		if (!store) {
			SCLog(_sc_verbose, LOG_INFO, CFSTR("SCDynamicStoreCreate() failed"));
			goto done;
		}
	}

	/*
	 * grab a snapshot of the PPP configuration from the dynamic store
	 */
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
								      kSCEntNetPPP);
		CFArrayAppendValue(patterns, pattern);
		CFRelease(pattern);
		dict = SCDynamicStoreCopyMultiple(store, NULL, patterns);
		CFRelease(patterns);
	}
	if (!dict) {
		/* if we could not access the dynamic store */
		goto done;
	}

	sc_status = kSCStatusOK;

	/*
	 * look for an available service which will provide connectivity
	 * for the requested address family.
	 */
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
			continue;
		}
		service = CFArrayGetValueAtIndex(components, 3);

		// check for PPP entity
		p_key = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
								    kSCDynamicStoreDomainSetup,
								    service,
								    kSCEntNetPPP);
		p_dict = CFDictionaryGetValue(dict, p_key);
		CFRelease(p_key);

		if (isA_CFDictionary(p_dict)) {
			CFNumberRef	num;

			/*
			 * we have a PPP service for this address family
			 */
			found = TRUE;

			*flags |= kSCNetworkFlagsReachable;
			*flags |= kSCNetworkFlagsTransientConnection;
			*flags |= kSCNetworkFlagsConnectionRequired;

			/*
			 * get PPP dial-on-traffic status
			 */
			num = CFDictionaryGetValue(p_dict, kSCPropNetPPPDialOnDemand);
			if (isA_CFNumber(num)) {
				int32_t	ppp_demand;

				if (CFNumberGetValue(num, kCFNumberSInt32Type, &ppp_demand)) {
					if (ppp_demand) {
						*flags |= kSCNetworkFlagsConnectionAutomatic;
					}
				}
			}

			if (_sc_debug) {
				SCLog(TRUE, LOG_INFO, CFSTR("  status    = isReachable (after connect)"));
				SCLog(TRUE, LOG_INFO, CFSTR("  service   = %@"), service);
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

	if (dict)	CFRelease(dict);
	if (storeP)	*storeP = store;
	return sc_status;
}


#define ROUNDUP(a, size) \
	(((a) & ((size)-1)) ? (1 + ((a) | ((size)-1))) : (a))

#define NEXT_SA(ap) (ap) = (struct sockaddr *) \
	((caddr_t)(ap) + ((ap)->sa_len ? ROUNDUP((ap)->sa_len,\
						 sizeof(u_long)) :\
						 sizeof(u_long)))

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

static Boolean
checkAddress(SCDynamicStoreRef		*storeP,
	     const struct sockaddr	*address,
	     SCNetworkConnectionFlags	*flags,
	     uint16_t			*if_index)
{
	char			buf[BUFLEN];
	struct ifreq		ifr;
	char			if_name[IFNAMSIZ+1];
	int			isock;
	int			n;
	pid_t			pid		= getpid();
	int			rsock;
	struct sockaddr         *rti_info[RTAX_MAX];
	struct rt_msghdr	*rtm;
	struct sockaddr         *sa;
	int			sc_status	= kSCStatusReachabilityUnknown;
	struct sockaddr_dl	*sdl;
	int			seq		= (int)pthread_self();
	SCDynamicStoreRef	store		= (storeP) ? *storeP : NULL;
	char			*statusMessage	= NULL;
#ifndef	RTM_GET_SILENT
#warning Note: Using RTM_GET (and not RTM_GET_SILENT)
	static pthread_mutex_t	lock		= PTHREAD_MUTEX_INITIALIZER;
	int			sosize		= 48 * 1024;
#endif

	if (!address || !flags) {
		sc_status = kSCStatusInvalidArgument;
		goto done;
	}

	switch (address->sa_family) {
		case AF_INET :
		case AF_INET6 :
			if (_sc_debug) {
				sockaddr_to_string(address, buf, sizeof(buf));
				SCLog(TRUE, LOG_INFO, CFSTR("checkAddress(%s)"), buf);
			}
			break;
		default :
			/*
			 * if no code for this address family (yet)
			 */
			SCLog(_sc_verbose, LOG_ERR,
			      CFSTR("checkAddress(): unexpected address family %d"),
			      address->sa_family);
			sc_status = kSCStatusInvalidArgument;
			goto done;
	}

	*flags = 0;
	if (if_index) {
		*if_index = 0;
	}

	if ((address->sa_family == AF_INET) && (((struct sockaddr_in *)address)->sin_addr.s_addr == 0)) {
		/* special case: check for available paths off the system */
		goto checkAvailable;
	}

	bzero(&buf, sizeof(buf));

	rtm = (struct rt_msghdr *)&buf;
	rtm->rtm_msglen  = sizeof(struct rt_msghdr);
	rtm->rtm_version = RTM_VERSION;
#ifdef	RTM_GET_SILENT
	rtm->rtm_type    = RTM_GET_SILENT;
#else
	rtm->rtm_type    = RTM_GET;
#endif
	rtm->rtm_flags   = RTF_STATIC|RTF_UP|RTF_HOST|RTF_GATEWAY;
	rtm->rtm_addrs   = RTA_DST|RTA_IFP; /* Both destination and device */
	rtm->rtm_pid     = pid;
	rtm->rtm_seq     = seq;

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

	sa  = (struct sockaddr *) (rtm + 1);
	bcopy(address, sa, address->sa_len);
	n = ROUNDUP(sa->sa_len, sizeof(u_long));
	rtm->rtm_msglen += n;

	sdl = (struct sockaddr_dl *) ((void *)sa + n);
	sdl->sdl_family = AF_LINK;
	sdl->sdl_len = sizeof (struct sockaddr_dl);
	n = ROUNDUP(sdl->sdl_len, sizeof(u_long));
	rtm->rtm_msglen += n;

#ifndef	RTM_GET_SILENT
	pthread_mutex_lock(&lock);
#endif
	rsock = socket(PF_ROUTE, SOCK_RAW, 0);
	if (rsock == -1) {
#ifndef	RTM_GET_SILENT
		pthread_mutex_unlock(&lock);
#endif
		SCLog(TRUE, LOG_ERR, CFSTR("socket(PF_ROUTE) failed: %s"), strerror(errno));
		sc_status = kSCStatusFailed;
		goto done;
	}

#ifndef	RTM_GET_SILENT
	if (setsockopt(rsock, SOL_SOCKET, SO_RCVBUF, &sosize, sizeof(sosize)) == -1) {
		(void)close(rsock);
		pthread_mutex_unlock(&lock);
		SCLog(TRUE, LOG_ERR, CFSTR("setsockopt(SO_RCVBUF) failed: %s"), strerror(errno));
		sc_status = kSCStatusFailed;
		goto done;
	}
#endif

	if (write(rsock, &buf, rtm->rtm_msglen) == -1) {
		int	err	= errno;

		(void)close(rsock);
#ifndef	RTM_GET_SILENT
		pthread_mutex_unlock(&lock);
#endif
		if (err != ESRCH) {
			SCLog(TRUE, LOG_ERR, CFSTR("write() failed: %s"), strerror(err));
			goto done;
		}
		goto checkAvailable;
	}

	/*
	 * Type, seq, pid identify our response.
	 * Routing sockets are broadcasters on input.
	 */
	do {
		int	n;

		n = read(rsock, (void *)&buf, sizeof(buf));
		if (n == -1) {
			int	err	= errno;

			if (err != EINTR) {
				(void)close(rsock);
				SCLog(TRUE, LOG_ERR, CFSTR("read() failed: %s"), strerror(err));
#ifndef	RTM_GET_SILENT
				pthread_mutex_unlock(&lock);
#endif
				goto done;
			}
		}
	} while (rtm->rtm_type != RTM_GET	||
		 rtm->rtm_seq  != seq		||
		 rtm->rtm_pid  != pid);

	(void)close(rsock);
#ifndef	RTM_GET_SILENT
	pthread_mutex_unlock(&lock);
#endif

	get_rtaddrs(rtm->rtm_addrs, sa, rti_info);

#ifdef	DEBUG
{
	int	i;
	char	buf[200];

	SCLog(_sc_debug, LOG_DEBUG, CFSTR("rtm_flags = 0x%8.8x"), rtm->rtm_flags);

	for (i = 0; i < RTAX_MAX; i++) {
		if (rti_info[i] != NULL) {
			sockaddr_to_string(rti_info[i], buf, sizeof(buf));
			SCLog(_sc_debug, LOG_DEBUG, CFSTR("%d: %s"), i, buf);
		}
	}
}
#endif	/* DEBUG */

	if ((rti_info[RTAX_IFP] == NULL) ||
	    (rti_info[RTAX_IFP]->sa_family != AF_LINK)) {
		/* no interface info */
		goto done;	// ???
	}

	sdl = (struct sockaddr_dl *) rti_info[RTAX_IFP];
	if ((sdl->sdl_nlen == 0) || (sdl->sdl_nlen > IFNAMSIZ)) {
		/* no interface name */
		goto done;	// ???
	}

	/* get the interface flags */

	bzero(&ifr, sizeof(ifr));
	bcopy(sdl->sdl_data, ifr.ifr_name, sdl->sdl_nlen);

	isock = socket(AF_INET, SOCK_DGRAM, 0);
	if (isock < 0) {
		SCLog(TRUE, LOG_NOTICE, CFSTR("socket() failed: %s"), strerror(errno));
		goto done;
	}

	if (ioctl(isock, SIOCGIFFLAGS, (char *)&ifr) < 0) {
		SCLog(TRUE, LOG_NOTICE, CFSTR("ioctl() failed: %s"), strerror(errno));
		(void)close(isock);
		goto done;
	}
	(void)close(isock);

	if (!(ifr.ifr_flags & IFF_UP)) {
		goto checkAvailable;
	}

	statusMessage = "isReachable";
	*flags |= kSCNetworkFlagsReachable;

	if (rtm->rtm_flags & RTF_LOCAL) {
		statusMessage = "isReachable (is a local address)";
		*flags |= kSCNetworkFlagsIsLocalAddress;
	} else if (ifr.ifr_flags & IFF_LOOPBACK) {
		statusMessage = "isReachable (is loopback network)";
		*flags |= kSCNetworkFlagsIsLocalAddress;
	} else if (rti_info[RTAX_IFA]) {
		void	*addr1	= (void *)address;
		void	*addr2	= (void *)rti_info[RTAX_IFA];
		size_t	len	= address->sa_len;

		if ((address->sa_family != rti_info[RTAX_IFA]->sa_family) &&
		    (address->sa_len    != rti_info[RTAX_IFA]->sa_len)) {
			SCLog(TRUE, LOG_NOTICE,
			      CFSTR("address family/length mismatch: %d/%d != %d/%d"),
			      address->sa_family,
			      address->sa_len,
			      rti_info[RTAX_IFA]->sa_family,
			      rti_info[RTAX_IFA]->sa_len);
			goto done;
		}

		switch (address->sa_family) {
			case AF_INET :
				addr1 = &((struct sockaddr_in *)address)->sin_addr;
				addr2 = &((struct sockaddr_in *)rti_info[RTAX_IFA])->sin_addr;
				len = sizeof(struct in_addr);
				break;
			case AF_INET6 :
				addr1 = &((struct sockaddr_in6 *)address)->sin6_addr;
				addr2 = &((struct sockaddr_in6 *)rti_info[RTAX_IFA])->sin6_addr;
				len = sizeof(struct in6_addr);
				break;
			default :
				break;
		}

		if (memcmp(addr1, addr2, len) == 0) {
			statusMessage = "isReachable (is interface address)";
			*flags |= kSCNetworkFlagsIsLocalAddress;
		}
	}

	if (rti_info[RTAX_GATEWAY] && (rti_info[RTAX_GATEWAY]->sa_family == AF_LINK)) {
		*flags |= kSCNetworkFlagsIsDirect;
	}

	bzero(&if_name, sizeof(if_name));
	bcopy(sdl->sdl_data,
	      if_name,
	      (sdl->sdl_nlen <= IFNAMSIZ) ? sdl->sdl_nlen : IFNAMSIZ);

	if (if_index) {
		*if_index = sdl->sdl_index;
	}

	if (_sc_debug) {
		SCLog(TRUE, LOG_INFO, CFSTR("  status    = %s"), statusMessage);
		SCLog(TRUE, LOG_INFO, CFSTR("  device    = %s (%hu)"), if_name, sdl->sdl_index);
		SCLog(TRUE, LOG_INFO, CFSTR("  ifr_flags = 0x%04hx"), ifr.ifr_flags);
		SCLog(TRUE, LOG_INFO, CFSTR("  rtm_flags = 0x%08x"), rtm->rtm_flags);
	}

	sc_status = kSCStatusOK;

	if (ifr.ifr_flags & IFF_POINTOPOINT) {
		/*
		 * We have an interface which "claims" to be a valid path
		 * off of the system.
		 */
		*flags |= kSCNetworkFlagsTransientConnection;

		/*
		 * Check if this is a dial-on-demand PPP link that isn't
		 * connected yet.
		 */
		sc_status = updatePPPStatus(&store, address, if_name, flags);
	}

	goto done;

    checkAvailable :

	sc_status = updatePPPAvailable(&store, address, flags);

    done :

	if (*flags == 0) {
		SCLog(_sc_debug, LOG_INFO, CFSTR("  cannot be reached"));
	}

	if (storeP)		*storeP = store;
	if (sc_status != kSCStatusOK) {
		_SCErrorSet(sc_status);
		return FALSE;
	}

	return TRUE;
}


static Boolean
checkAddressZero(SCDynamicStoreRef		*storeP,
		 SCNetworkConnectionFlags	*flags,
		 uint16_t			*if_index)
{
	Boolean			ok;
	struct sockaddr_in	sin;

	bzero(&sin, sizeof(sin));
	sin.sin_len         = sizeof(sin);
	sin.sin_family      = AF_INET;
	sin.sin_addr.s_addr = 0;

	ok = checkAddress(storeP, (struct sockaddr *)&sin, flags, if_index);

	return ok;
}


static Boolean
isAddressZero(struct sockaddr *sa, SCNetworkConnectionFlags *flags)
{
	/*
	 * Check if 0.0.0.0
	 */
	if (sa->sa_family == AF_INET) {
		struct sockaddr_in      *sin = (struct sockaddr_in *)sa;

		if (sin->sin_addr.s_addr == 0) {
			SCLog(_sc_debug, LOG_INFO, CFSTR("isAddressZero(0.0.0.0)"));
			SCLog(_sc_debug, LOG_INFO, CFSTR("  status     = isReachable (this host)"));
			*flags |= kSCNetworkFlagsReachable;
			*flags |= kSCNetworkFlagsIsLocalAddress;
			return TRUE;
		}
	}

	return FALSE;
}


static CFStringRef
__SCNetworkReachabilityCopyDescription(CFTypeRef cf)
{
	CFAllocatorRef			allocator	= CFGetAllocator(cf);
	CFMutableStringRef		result;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)cf;

	result = CFStringCreateMutable(allocator, 0);
	CFStringAppendFormat(result, NULL, CFSTR("<SCNetworkReachability %p [%p]> { "), cf, allocator);
	switch (targetPrivate->type) {
		case reachabilityTypeAddress :
		case reachabilityTypeAddressPair : {
			char		buf[64];

			if (targetPrivate->localAddress) {
				sockaddr_to_string(targetPrivate->localAddress, buf, sizeof(buf));
				CFStringAppendFormat(result, NULL, CFSTR("local address=%s"),
						     buf);
			}

			if (targetPrivate->remoteAddress) {
				sockaddr_to_string(targetPrivate->remoteAddress, buf, sizeof(buf));
				CFStringAppendFormat(result, NULL, CFSTR("%s%saddress=%s"),
						     targetPrivate->localAddress ? ", " : "",
						     (targetPrivate->type == reachabilityTypeAddressPair) ? "remote " : "",
						     buf);
			}
			break;
		}
		case reachabilityTypeName : {
			CFStringAppendFormat(result, NULL, CFSTR("name=%s"), targetPrivate->name);
			if (targetPrivate->resolvedAddress || (targetPrivate->resolvedAddressError != NETDB_SUCCESS)) {
				if (targetPrivate->resolvedAddress) {
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
							sockaddr_to_string(sa, buf, sizeof(buf));
							CFStringAppendFormat(result, NULL, CFSTR("%s%s"),
									     i > 0 ? ", " : "",
									     buf);
						}
						CFStringAppendFormat(result, NULL, CFSTR(")"));
					} else {
						CFStringAppendFormat(result, NULL, CFSTR(" (no addresses)"));
					}
				} else {
#ifdef	CHECK_IPV6_REACHABILITY
					CFStringAppendFormat(result, NULL, CFSTR(" (%s)"),
							     gai_strerror(targetPrivate->resolvedAddressError));
#else	/* CHECK_IPV6_REACHABILITY */
					CFStringAppendFormat(result, NULL, CFSTR(" (%s)"),
							     __netdb_error(targetPrivate->resolvedAddressError));
#endif	/* CHECK_IPV6_REACHABILITY */
				}
			} else if (targetPrivate->dnsPort) {
				CFStringAppendFormat(result, NULL, CFSTR(" (DNS query active)"));
			}
			break;
		}
	}
	if (targetPrivate->rls) {
		CFStringAppendFormat(result,
				     NULL,
				     CFSTR(", flags=%8.8x, if_index=%hu"),
				     targetPrivate->flags,
				     targetPrivate->if_index);
	}
	CFStringAppendFormat(result, NULL, CFSTR(" }"));

	return result;
}


static void
__SCNetworkReachabilityDeallocate(CFTypeRef cf)
{
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)cf;

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("__SCNetworkReachabilityDeallocate:"));

	/* release resources */

	pthread_mutex_destroy(&targetPrivate->lock);

	if (targetPrivate->name)
		CFAllocatorDeallocate(NULL, (void *)targetPrivate->name);

	if (targetPrivate->resolvedAddress)
		CFRelease(targetPrivate->resolvedAddress);

	if (targetPrivate->localAddress)
		CFAllocatorDeallocate(NULL, (void *)targetPrivate->localAddress);

	if (targetPrivate->remoteAddress)
		CFAllocatorDeallocate(NULL, (void *)targetPrivate->remoteAddress);

	if (targetPrivate->rlsContext.release) {
		targetPrivate->rlsContext.release(targetPrivate->rlsContext.info);
	}

	return;
}


static void
__SCNetworkReachabilityInitialize(void)
{
	__kSCNetworkReachabilityTypeID = _CFRuntimeRegisterClass(&__SCNetworkReachabilityClass);
	return;
}


static SCNetworkReachabilityPrivateRef
__SCNetworkReachabilityCreatePrivate(CFAllocatorRef	allocator)
{
	SCNetworkReachabilityPrivateRef		targetPrivate;
	uint32_t				size;

	/* initialize runtime */
	pthread_once(&initialized, __SCNetworkReachabilityInitialize);

	SCLog(_sc_verbose, LOG_DEBUG, CFSTR("__SCNetworkReachabilityCreatePrivate:"));

	/* allocate target */
	size          = sizeof(SCNetworkReachabilityPrivate) - sizeof(CFRuntimeBase);
	targetPrivate = (SCNetworkReachabilityPrivateRef)_CFRuntimeCreateInstance(allocator,
										  __kSCNetworkReachabilityTypeID,
										  size,
										  NULL);
	if (!targetPrivate) {
		return NULL;
	}

	pthread_mutex_init(&targetPrivate->lock, NULL);

	targetPrivate->name				= NULL;

	targetPrivate->resolvedAddress			= NULL;
	targetPrivate->resolvedAddressError		= NETDB_SUCCESS;

	targetPrivate->localAddress			= NULL;
	targetPrivate->remoteAddress			= NULL;

	targetPrivate->flags				= 0;
	targetPrivate->if_index				= 0;

	targetPrivate->rls				= NULL;
	targetPrivate->rlsFunction			= NULL;
	targetPrivate->rlsContext.info			= NULL;
	targetPrivate->rlsContext.retain		= NULL;
	targetPrivate->rlsContext.release		= NULL;
	targetPrivate->rlsContext.copyDescription	= NULL;
	targetPrivate->rlList				= NULL;

	targetPrivate->dnsPort				= NULL;
	targetPrivate->dnsRLS				= NULL;

	return targetPrivate;
}


SCNetworkReachabilityRef
SCNetworkReachabilityCreateWithAddress(CFAllocatorRef		allocator,
				       const struct sockaddr	*address)
{
	SCNetworkReachabilityPrivateRef	targetPrivate;

	if (!address ||
	    (address->sa_len == 0) ||
	    (address->sa_len > sizeof(struct sockaddr_storage))) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	targetPrivate = __SCNetworkReachabilityCreatePrivate(allocator);
	if (!targetPrivate) {
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

	if (localAddress) {
		if ((localAddress->sa_len == 0) ||
		    (localAddress->sa_len > sizeof(struct sockaddr_storage))) {
			    _SCErrorSet(kSCStatusInvalidArgument);
			    return NULL;
		}
	}

	if (remoteAddress) {
		if ((remoteAddress->sa_len == 0) ||
		    (remoteAddress->sa_len > sizeof(struct sockaddr_storage))) {
			    _SCErrorSet(kSCStatusInvalidArgument);
			    return NULL;
		}
	}

	targetPrivate = __SCNetworkReachabilityCreatePrivate(allocator);
	if (!targetPrivate) {
		return NULL;
	}

	targetPrivate->type = reachabilityTypeAddressPair;

	if (localAddress) {
		targetPrivate->localAddress = CFAllocatorAllocate(NULL, localAddress->sa_len, 0);
		bcopy(localAddress, targetPrivate->localAddress, localAddress->sa_len);
	}

	if (remoteAddress) {
		targetPrivate->remoteAddress = CFAllocatorAllocate(NULL, remoteAddress->sa_len, 0);
		bcopy(remoteAddress, targetPrivate->remoteAddress, remoteAddress->sa_len);
	}

	return (SCNetworkReachabilityRef)targetPrivate;
}


SCNetworkReachabilityRef
SCNetworkReachabilityCreateWithName(CFAllocatorRef	allocator,
				    const char		*nodename)
{
	SCNetworkReachabilityPrivateRef	targetPrivate;

	if (!nodename) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	targetPrivate = __SCNetworkReachabilityCreatePrivate(allocator);
	if (!targetPrivate) {
		return NULL;
	}

	targetPrivate->type = reachabilityTypeName;

	targetPrivate->flags |= kSCNetworkFlagsFirstResolvePending;

	targetPrivate->name = CFAllocatorAllocate(NULL, strlen(nodename) + 1, 0);
	strcpy((char *)targetPrivate->name, nodename);

	return (SCNetworkReachabilityRef)targetPrivate;
}


CFTypeID
SCNetworkReachabilityGetTypeID(void)
{
	pthread_once(&initialized, __SCNetworkReachabilityInitialize);	/* initialize runtime */
	return __kSCNetworkReachabilityTypeID;
}


CFArrayRef
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

	if (targetPrivate->resolvedAddress || (targetPrivate->resolvedAddressError != NETDB_SUCCESS)) {
		if (targetPrivate->resolvedAddress) {
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
__SCNetworkReachabilitySetResolvedAddress(SCNetworkReachabilityRef	target,
					  CFArrayRef			addresses,
					  int				error_num)
{
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	if (targetPrivate->resolvedAddress) {
		CFRelease(targetPrivate->resolvedAddress);
	}
	targetPrivate->resolvedAddress      = addresses ? CFRetain(addresses) : NULL;
	targetPrivate->resolvedAddressError = error_num;
	return;
}


#ifdef	CHECK_IPV6_REACHABILITY
static void
__SCNetworkReachabilityCallbackSetResolvedAddress(int32_t status, struct addrinfo *res, void *context)
{
	Boolean					ok;
	struct addrinfo				*resP;
	SCNetworkReachabilityRef		target		= (SCNetworkReachabilityRef)context;
	SCNetworkReachabilityPrivateRef		targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	ok = (status == 0) && (res != NULL);

	SCLog(_sc_debug, LOG_DEBUG,
	      CFSTR("process async DNS complete%s"),
	      ok ? "" : ", host not found");

	if (ok) {
		CFMutableArrayRef	addresses;

		addresses = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

		for (resP = res; resP; resP = resP->ai_next) {
			CFDataRef	newAddress;

			newAddress = CFDataCreate(NULL, (void *)resP->ai_addr, resP->ai_addr->sa_len);
			CFArrayAppendValue(addresses, newAddress);
			CFRelease(newAddress);
		}

		/* save the resolved address[es] */
		__SCNetworkReachabilitySetResolvedAddress(target, addresses, NETDB_SUCCESS);
		CFRelease(addresses);
	} else {
		SCLog(_sc_debug, LOG_INFO, CFSTR("getaddrinfo() failed: %s"), gai_strerror(status));

		/* save the error associated with the attempt to resolve the name */
		__SCNetworkReachabilitySetResolvedAddress(target, (CFArrayRef)kCFNull, status);
	}

	if (res)	freeaddrinfo(res);

	if (targetPrivate->rls) {
		SCLog(_sc_debug, LOG_INFO, CFSTR("DNS request completed"));
		CFRunLoopSourceSignal(targetPrivate->rls);
		__signalRunLoop(target, targetPrivate->rls, targetPrivate->rlList);
	}

	return;
}
#else	/* CHECK_IPV6_REACHABILITY */
static void
__SCNetworkReachabilityCallbackSetResolvedAddress(struct hostent *h, int error, void *context)
{
	SCNetworkReachabilityRef		target		= (SCNetworkReachabilityRef)context;
	SCNetworkReachabilityPrivateRef		targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	SCLog(_sc_debug, LOG_DEBUG,
	      CFSTR("process async DNS complete%s"),
	      (h == NULL) ? ", host not found" : "");

	if (h && h->h_length) {
		CFMutableArrayRef	addresses;
		union {
			struct sockaddr		sa;
			struct sockaddr_in	sin;
			struct sockaddr_in6	sin6;
			struct sockaddr_storage	ss;
		}	addr;
		char	**ha		= h->h_addr_list;

		addresses = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

		bzero(&addr, sizeof(addr));

		while (*ha) {
			CFDataRef	newAddress;

			switch (h->h_length) {
				case sizeof(struct in_addr) :
					addr.sin.sin_family = AF_INET;
					addr.sin.sin_len    = sizeof(struct sockaddr_in);
					bcopy(*ha, &addr.sin.sin_addr, h->h_length);
					break;
				case sizeof(struct in6_addr) :
					addr.sin6.sin6_family = AF_INET6;
					addr.sin6.sin6_len    = sizeof(struct sockaddr_in6);
					bcopy(*ha, &addr.sin6.sin6_addr, h->h_length);
					break;
			}

			newAddress = CFDataCreate(NULL, (void *)&addr, addr.sa.sa_len);
			CFArrayAppendValue(addresses, newAddress);
			CFRelease(newAddress);

			ha++;
		}

		/* save the resolved address[es] */
		__SCNetworkReachabilitySetResolvedAddress(target, addresses, NETDB_SUCCESS);
		CFRelease(addresses);
	} else {
		SCLog(_sc_debug, LOG_INFO, CFSTR("getipnodebyname() failed: %s"), __netdb_error(error));

		/* save the error associated with the attempt to resolve the name */
		__SCNetworkReachabilitySetResolvedAddress(target, (CFArrayRef)kCFNull, error);
	}

	if (h)	freehostent(h);

	if (targetPrivate->rls) {
		SCLog(_sc_debug, LOG_INFO, CFSTR("DNS request completed"));
		CFRunLoopSourceSignal(targetPrivate->rls);
		__signalRunLoop(target, targetPrivate->rls, targetPrivate->rlList);
	}

	return;
}
#endif	/* CHECK_IPV6_REACHABILITY */


/*
 * rankReachability()
 *   Not reachable       == 0
 *   Connection Required == 1
 *   Reachable           == 2
 */
static int
rankReachability(SCNetworkConnectionFlags flags)
{
	int	rank = 0;

	if (flags & kSCNetworkFlagsReachable)		rank = 2;
	if (flags & kSCNetworkFlagsConnectionRequired)	rank = 1;
	return rank;
}


#ifdef	CHECK_IPV6_REACHABILITY
static void
getaddrinfo_async_handleCFReply(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
	SCNetworkReachabilityRef	target		= (SCNetworkReachabilityRef)info;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	pthread_mutex_lock(&targetPrivate->lock);

	getaddrinfo_async_handle_reply(msg);

	if (port == targetPrivate->dnsPort) {
		CFRelease(targetPrivate->dnsRLS);
		targetPrivate->dnsRLS = NULL;
		CFRelease(targetPrivate->dnsPort);
		targetPrivate->dnsPort = NULL;
	}

	pthread_mutex_unlock(&targetPrivate->lock);

	return;
}
#else	/* CHECK_IPV6_REACHABILITY */
static void
getipnodebyname_async_handleCFReply(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
	SCNetworkReachabilityRef	target		= (SCNetworkReachabilityRef)info;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	pthread_mutex_lock(&targetPrivate->lock);

	getipnodebyname_async_handleReply(msg);

	if (port == targetPrivate->dnsPort) {
		CFRelease(targetPrivate->dnsRLS);
		targetPrivate->dnsRLS = NULL;
		CFRelease(targetPrivate->dnsPort);
		targetPrivate->dnsPort = NULL;
	}

	pthread_mutex_unlock(&targetPrivate->lock);

	return;
}
#endif	/* CHECK_IPV6_REACHABILITY */


static Boolean
checkResolverReachability(SCDynamicStoreRef		*storeP,
			  SCNetworkConnectionFlags	*flags,
			  Boolean			*haveDNS)
{
	int	i;
	Boolean	ok	= TRUE;

	/*
	 * We first assume that all of the configured DNS servers
	 * are available.  Since we don't know which name server will
	 * be consulted to resolve the specified nodename we need to
	 * check the availability of ALL name servers.  We can only
	 * proceed if we know that our query can be answered.
	 */

	*flags   = kSCNetworkFlagsReachable;
	*haveDNS = FALSE;

	if (needDNS) {
		if (hn_store) {
			/* if we are actively watching at least one host */
			needDNS = FALSE;
		}
		res_init();
	}

	for (i = 0; i < _res.nscount; i++) {
		SCNetworkConnectionFlags	ns_flags	= 0;

		if (_res.nsaddr_list[i].sin_addr.s_addr == 0) {
			continue;
		}

		*haveDNS = TRUE;

		if (_res.nsaddr_list[i].sin_len == 0) {
			_res.nsaddr_list[i].sin_len = sizeof(_res.nsaddr_list[i]);
		}

		ok = checkAddress(storeP, (struct sockaddr *)&_res.nsaddr_list[i], &ns_flags, NULL);
		if (!ok) {
			/* not today */
			break;
		}
		if (rankReachability(ns_flags) < rankReachability(*flags)) {
			/* return the worst case result */
			*flags = ns_flags;
		}
	}

	if (!*haveDNS) {
		/* if no DNS server addresses */
		*flags = 0;
	}

	return ok;
}


static Boolean
startAsyncDNSQuery(SCNetworkReachabilityRef target) {
	CFMachPortContext		context		= { 0, (void *)target, CFRetain, CFRelease, CFCopyDescription };
	int				error;
#ifdef	CHECK_IPV6_REACHABILITY
	struct addrinfo			hints;
#endif	/* CHECK_IPV6_REACHABILITY */
	CFIndex				i;
	CFIndex				n;
	mach_port_t			port;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

#ifdef	CHECK_IPV6_REACHABILITY
	bzero(&hints, sizeof(hints));
	hints.ai_flags = AI_ADDRCONFIG;

	error = getaddrinfo_async_start(&port,
					targetPrivate->name,
					NULL,
					&hints,
					__SCNetworkReachabilityCallbackSetResolvedAddress,
					(void *)target);
	if (error != 0) {
		/* save the error associated with the attempt to resolve the name */
		__SCNetworkReachabilitySetResolvedAddress(target, (CFArrayRef)kCFNull, error);
		return FALSE;
	}

	targetPrivate->dnsPort = CFMachPortCreateWithPort(NULL,
							  port,
							  getaddrinfo_async_handleCFReply,
							  &context,
							  NULL);
#else	/* CHECK_IPV6_REACHABILITY */
	port = getipnodebyname_async_start(targetPrivate->name,
					   AF_INET,
					   0,
					   &error,
					   __SCNetworkReachabilityCallbackSetResolvedAddress,
					   (void *)target);
	if (port == MACH_PORT_NULL) {
		/* save the error associated with the attempt to resolve the name */
		__SCNetworkReachabilitySetResolvedAddress(target, (CFArrayRef)kCFNull, error);
		return FALSE;
	}

	targetPrivate->dnsPort = CFMachPortCreateWithPort(NULL,
							  port,
							  getipnodebyname_async_handleCFReply,
							  &context,
							  NULL);
#endif	/* CHECK_IPV6_REACHABILITY */

	targetPrivate->dnsRLS = CFMachPortCreateRunLoopSource(NULL, targetPrivate->dnsPort, 0);

	n = CFArrayGetCount(targetPrivate->rlList);
	for (i = 0; i < n; i += 3) {
		CFRunLoopRef	rl	= (CFRunLoopRef)CFArrayGetValueAtIndex(targetPrivate->rlList, i+1);
		CFStringRef	rlMode	= (CFStringRef) CFArrayGetValueAtIndex(targetPrivate->rlList, i+2);

		CFRunLoopAddSource(rl, targetPrivate->dnsRLS, rlMode);
	}

	return TRUE;
}


static Boolean
__SCNetworkReachabilityGetFlags(SCDynamicStoreRef		*storeP,
				SCNetworkReachabilityRef	target,
				SCNetworkConnectionFlags	*flags,
				uint16_t			*if_index,
				Boolean				async)
{
	CFMutableArrayRef		addresses	= NULL;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;
	SCNetworkConnectionFlags	my_flags	= 0;
	uint16_t			my_index	= 0;
	Boolean				ok		= TRUE;

	*flags = 0;
	if (if_index) {
		*if_index = 0;
	}

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
			if (targetPrivate->localAddress) {
				/*
				 * Check if 0.0.0.0
				 */
				if (isAddressZero(targetPrivate->localAddress, &my_flags)) {
					goto checkRemote;
				}

				/*
				 * Check "local" address
				 */
				ok = checkAddress(storeP, targetPrivate->localAddress, &my_flags, &my_index);
				if (!ok) {
					goto error;	/* not today */
				}

				if (!(my_flags & kSCNetworkFlagsIsLocalAddress)) {
					goto error;	/* not reachable, non-"local" address */
				}
			}

		    checkRemote :

			/*
			 * Check "remote" address
			 */
			if (targetPrivate->remoteAddress) {
				/*
				 * in cases where we have "local" and "remote" addresses
				 * we need to re-initialize the to-be-returned flags.
				 */
				my_flags = 0;
				my_index = 0;

				/*
				 * Check if 0.0.0.0
				 */
				if (isAddressZero(targetPrivate->remoteAddress, &my_flags)) {
					break;
				}

				/*
				 * Check "remote" address
				 */
				ok = checkAddress(storeP, targetPrivate->remoteAddress, &my_flags, &my_index);
				if (!ok) {
					goto error;	/* not today */
				}
			}

			break;

		}

		case reachabilityTypeName : {
			int				error;
#ifndef	CHECK_IPV6_REACHABILITY
			struct hostent			*h;
#endif	/* CHECK_IPV6_REACHABILITY */
			Boolean				haveDNS		= FALSE;
#ifdef	CHECK_IPV6_REACHABILITY
			struct addrinfo			hints;
#endif	/* CHECK_IPV6_REACHABILITY */
			SCNetworkConnectionFlags	ns_flags;
#ifdef	CHECK_IPV6_REACHABILITY
			struct addrinfo			*res;
#endif	/* CHECK_IPV6_REACHABILITY */

			addresses = (CFMutableArrayRef)SCNetworkReachabilityCopyResolvedAddress(target, &error);
			if (addresses || (error != NETDB_SUCCESS)) {
				/* if resolved or an error had been detected */
				goto checkResolvedAddress;
			}

			/* check the reachability of the DNS servers */
			ok = checkResolverReachability(storeP, &ns_flags, &haveDNS);\
			if (!ok) {
				/* if we could not get DNS server info */
				goto error;
			}

			if (rankReachability(ns_flags) < 2) {
				/*
				 * if DNS servers are not (or are no longer) reachable, set
				 * flags based on the availability of configured (but not
				 * active) services.
				 */
				if (!checkAddressZero(storeP, &my_flags, &my_index)) {
					goto error;
				}

				if (async && targetPrivate->rls) {
					/*
					 * return HOST_NOT_FOUND, set flags appropriately,
					 * and schedule notification.
					 */
#ifdef	CHECK_IPV6_REACHABILITY
					__SCNetworkReachabilityCallbackSetResolvedAddress(EAI_NODATA,
											  NULL,
											  (void *)target);
#else	/* CHECK_IPV6_REACHABILITY */
					__SCNetworkReachabilityCallbackSetResolvedAddress(NULL,
											  HOST_NOT_FOUND,
											  (void *)target);
#endif	/* CHECK_IPV6_REACHABILITY */

					my_flags |= (targetPrivate->flags & kSCNetworkFlagsFirstResolvePending);

					SCLog(_sc_debug, LOG_INFO, CFSTR("no DNS servers are reachable"));
					CFRunLoopSourceSignal(targetPrivate->rls);
					__signalRunLoop(target, targetPrivate->rls, targetPrivate->rlList);
				}
				break;
			}

			if (async) {
				/* for async requests we return the last known status */
				my_flags = targetPrivate->flags;
				my_index = targetPrivate->if_index;

				if (targetPrivate->dnsPort) {
					/* if request already in progress */
					break;
				}

				SCLog(_sc_debug, LOG_INFO, CFSTR("start DNS query for \"%s\""), targetPrivate->name);

				/*
				 * initiate an async DNS query
				 */
				if (!startAsyncDNSQuery(target)) {
					/* if we could not initiate the request, process error */
					goto checkResolvedAddress;
				}

				/* if request initiated */
				break;
			}

			SCLog(_sc_debug, LOG_INFO, CFSTR("check DNS for \"%s\""), targetPrivate->name);

			/*
			 * OK, all of the DNS name servers are available.  Let's
			 * resolve the nodename into an address.
			 */
#ifdef	CHECK_IPV6_REACHABILITY
			bzero(&hints, sizeof(hints));
			hints.ai_flags = AI_ADDRCONFIG;

			error = getaddrinfo(targetPrivate->name, NULL, &hints, &res);
			__SCNetworkReachabilityCallbackSetResolvedAddress(error, res, (void *)target);
#else	/* CHECK_IPV6_REACHABILITY */
			h = getipnodebyname(targetPrivate->name, AF_INET, 0, &error);
			__SCNetworkReachabilityCallbackSetResolvedAddress(h, error, (void *)target);
#endif	/* CHECK_IPV6_REACHABILITY */

			addresses = (CFMutableArrayRef)SCNetworkReachabilityCopyResolvedAddress(target, &error);

		    checkResolvedAddress :

			/*
			 * We first assume that the requested host is NOT available.
			 * Then, check each address for accessibility and return the
			 * best status available.
			 */
			my_flags = 0;
			my_index = 0;

			if (isA_CFArray(addresses)) {
				CFIndex		i;
				CFIndex		n	= CFArrayGetCount(addresses);

				for (i = 0; i < n; i++) {
					SCNetworkConnectionFlags	ns_flags	= 0;
					uint16_t			ns_if_index	= 0;
					struct sockaddr			*sa;

					sa = (struct sockaddr *)CFDataGetBytePtr(CFArrayGetValueAtIndex(addresses, i));
					ok = checkAddress(storeP, sa, &ns_flags, &ns_if_index);
					if (!ok) {
						goto error;	/* not today */
					}

					if (rankReachability(ns_flags) > rankReachability(my_flags)) {
						/* return the best case result */
						my_flags = ns_flags;
						my_index = ns_if_index;
						if (rankReachability(my_flags) == 2) {
							/* we're in luck */
							break;
						}
					}
				}
			} else {
				if ((error == HOST_NOT_FOUND) && !haveDNS) {
					/*
					 * No DNS servers are defined. Set flags based on
					 * the availability of configured (but not active)
					 * services.
					 */
					ok = checkAddressZero(storeP, &my_flags, &my_index);
					if (!ok) {
						goto error;	/* not today */
					}

					if ((my_flags & kSCNetworkFlagsReachable) &&
					    (my_flags & kSCNetworkFlagsConnectionRequired)) {
						/*
						 * Since we might pick up a set of DNS servers when this connection
						 * is established, don't reply with a "HOST NOT FOUND" error just yet.
						 */
						break;
					}

					/* Host not found, not reachable! */
					my_flags = 0;
					my_index = 0;
				}
			}

			break;
		}
	}

	*flags = my_flags;
	if (if_index) {
		*if_index = my_index;
	}

    error :

	if (addresses)	CFRelease(addresses);
	return ok;
}


Boolean
SCNetworkReachabilityGetFlags(SCNetworkReachabilityRef	target,
			      SCNetworkConnectionFlags	*flags)
{
	Boolean				ok;
	SCDynamicStoreRef		store		= NULL;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	if (!isA_SCNetworkReachability(target)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	if (targetPrivate->rlList) {
		/* if being watched, return current (OK, last known) status */
		*flags = targetPrivate->flags & ~kSCNetworkFlagsFirstResolvePending;
		return TRUE;
	}

	ok = __SCNetworkReachabilityGetFlags(&store, target, flags, NULL, FALSE);
	*flags &= ~kSCNetworkFlagsFirstResolvePending;
	if (store)	CFRelease(store);
	return ok;
}


static void
__SCNetworkReachabilityReachabilitySetNotifications(SCDynamicStoreRef	store)
{
	CFStringRef			key;
	CFMutableArrayRef		keys;
	CFStringRef			pattern;
	CFMutableArrayRef		patterns;

	keys     = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	/*
	 * Setup:/Network/Global/IPv4 (for the ServiceOrder)
	 */
	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							 kSCDynamicStoreDomainSetup,
							 kSCEntNetIPv4);
	CFArrayAppendValue(keys, key);
	CFRelease(key);

	/*
	 * State:/Network/Global/DNS
	 */
	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							 kSCDynamicStoreDomainState,
							 kSCEntNetDNS);
	CFArrayAppendValue(keys, key);
	CFRelease(key);

	/*
	 * State:/Network/Global/IPv4
	 */
	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							 kSCDynamicStoreDomainState,
							 kSCEntNetIPv4);
	CFArrayAppendValue(keys, key);
	CFRelease(key);

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

	/* State: per-interface IPv6 info */
	pattern = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
								kSCDynamicStoreDomainState,
								kSCCompAnyRegex,
								kSCEntNetIPv6);
	CFArrayAppendValue(patterns, pattern);
	CFRelease(pattern);

	(void)SCDynamicStoreSetNotificationKeys(store, keys, patterns);
	CFRelease(keys);
	CFRelease(patterns);

	return;
}


static void
__SCNetworkReachabilityReachabilityHandleChanges(SCDynamicStoreRef	store,
						 CFArrayRef		changedKeys,
						 void			*info)
{
	Boolean		dnsChanged	= FALSE;
	CFIndex		i;
	CFStringRef	key;
	CFIndex		nTargets;
	const void *	targets_q[N_QUICK];
	const void **	targets		= targets_q;

	pthread_mutex_lock(&hn_lock);

	nTargets = CFSetGetCount(hn_targets);
	if (nTargets == 0) {
		/* if no addresses being monitored */
		goto done;
	}

	if (CFArrayGetCount(changedKeys) == 0) {
		/* if no changes */
		goto done;
	}

	SCLog(_sc_debug, LOG_INFO, CFSTR("process configuration change"));

	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							 kSCDynamicStoreDomainState,
							 kSCEntNetDNS);
	if (CFArrayContainsValue(changedKeys,
				 CFRangeMake(0, CFArrayGetCount(changedKeys)),
				 key)) {
		dnsChanged = TRUE;	/* the DNS server(s) have changed */
		needDNS    = TRUE;	/* ... and we need to res_init() on the next query */
	}
	CFRelease(key);

	if (!dnsChanged) {
		/*
		 * if the DNS configuration didn't change we still need to
		 * check that the DNS servers are accessible.
		 */
		Boolean				haveDNS		= FALSE;
		SCNetworkConnectionFlags	ns_flags;
		Boolean				ok;

		/* check the reachability of the DNS servers */
		ok = checkResolverReachability(&store, &ns_flags, &haveDNS);\
		if (!ok || (rankReachability(ns_flags) < 2)) {
			/* if DNS servers are not reachable */
			dnsChanged = TRUE;
		}
	}

	SCLog(_sc_debug && dnsChanged, LOG_INFO, CFSTR("  DNS changed"));

	if (nTargets > (CFIndex)(sizeof(targets_q) / sizeof(CFTypeRef)))
		targets = CFAllocatorAllocate(NULL, nTargets * sizeof(CFTypeRef), 0);
	CFSetGetValues(hn_targets, targets);
	for (i = 0; i < nTargets; i++) {
		SCNetworkReachabilityRef	target		= targets[i];
		SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

		pthread_mutex_lock(&targetPrivate->lock);

		if (dnsChanged) {
			if (targetPrivate->dnsPort) {
				/* cancel the outstanding DNS query */
#ifdef	CHECK_IPV6_REACHABILITY
				lu_async_call_cancel(CFMachPortGetPort(targetPrivate->dnsPort));
#else	/* CHECK_IPV6_REACHABILITY */
				getipnodebyname_async_cancel(CFMachPortGetPort(targetPrivate->dnsPort));
#endif	/* CHECK_IPV6_REACHABILITY */
				CFRelease(targetPrivate->dnsRLS);
				targetPrivate->dnsRLS = NULL;
				CFRelease(targetPrivate->dnsPort);
				targetPrivate->dnsPort = NULL;
			}

			/* schedule request to resolve the name again */
			__SCNetworkReachabilitySetResolvedAddress(target, NULL, NETDB_SUCCESS);
		}

		CFRunLoopSourceSignal(targetPrivate->rls);
		__signalRunLoop(target, targetPrivate->rls, targetPrivate->rlList);

		pthread_mutex_unlock(&targetPrivate->lock);
	}
	if (targets != targets_q)	CFAllocatorDeallocate(NULL, targets);

    done :

	pthread_mutex_unlock(&hn_lock);
	return;
}


static Boolean
__isScheduled(CFTypeRef obj, CFRunLoopRef runLoop, CFStringRef runLoopMode, CFMutableArrayRef rlList)
{
	CFIndex	i;
	CFIndex	n	= CFArrayGetCount(rlList);

	for (i = 0; i < n; i += 3) {
		if (obj         && !CFEqual(obj,         CFArrayGetValueAtIndex(rlList, i))) {
			continue;
		}
		if (runLoop     && !CFEqual(runLoop,     CFArrayGetValueAtIndex(rlList, i+1))) {
			continue;
		}
		if (runLoopMode && !CFEqual(runLoopMode, CFArrayGetValueAtIndex(rlList, i+2))) {
			continue;
		}
		return TRUE;
	}

	return FALSE;
}


static void
__schedule(CFTypeRef obj, CFRunLoopRef runLoop, CFStringRef runLoopMode, CFMutableArrayRef rlList)
{
	CFArrayAppendValue(rlList, obj);
	CFArrayAppendValue(rlList, runLoop);
	CFArrayAppendValue(rlList, runLoopMode);

	return;
}


static Boolean
__unschedule(CFTypeRef obj, CFRunLoopRef runLoop, CFStringRef runLoopMode, CFMutableArrayRef rlList, Boolean all)
{
	CFIndex	i	= 0;
	Boolean	found	= FALSE;
	CFIndex	n	= CFArrayGetCount(rlList);

	while (i < n) {
		if (obj         && !CFEqual(obj,         CFArrayGetValueAtIndex(rlList, i))) {
			i += 3;
			continue;
		}
		if (runLoop     && !CFEqual(runLoop,     CFArrayGetValueAtIndex(rlList, i+1))) {
			i += 3;
			continue;
		}
		if (runLoopMode && !CFEqual(runLoopMode, CFArrayGetValueAtIndex(rlList, i+2))) {
			i += 3;
			continue;
		}

		found = TRUE;

		CFArrayRemoveValueAtIndex(rlList, i + 2);
		CFArrayRemoveValueAtIndex(rlList, i + 1);
		CFArrayRemoveValueAtIndex(rlList, i);

		if (!all) {
			return found;
		}

		n -= 3;
	}

	return found;
}


static void
rlsPerform(void *info)
{
	void				*context_info;
	void				(*context_release)(const void *);
	SCNetworkConnectionFlags	flags;
	uint16_t			if_index;
	Boolean				ok;
	SCNetworkReachabilityCallBack	rlsFunction;
	SCDynamicStoreRef		store		= NULL;
	SCNetworkReachabilityRef	target		= (SCNetworkReachabilityRef)info;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	SCLog(_sc_debug, LOG_DEBUG, CFSTR("process reachability change"));

	pthread_mutex_lock(&targetPrivate->lock);

	/* update reachability, notify if status changed */
	ok = __SCNetworkReachabilityGetFlags(&store, target, &flags, &if_index, TRUE);
	if (store) CFRelease(store);
	if (!ok) {
		/* if reachability status not available */
		flags    = 0;
		if_index = 0;
	}

	if ((targetPrivate->flags == flags) && (targetPrivate->if_index == if_index)) {
		/* if reachability flags and interface have not changed */
		pthread_mutex_unlock(&targetPrivate->lock);
		SCLog(_sc_debug, LOG_DEBUG, CFSTR("flags/interface match"));
		return;
	}

	/* update flags / interface */
	targetPrivate->flags    = flags;
	targetPrivate->if_index = if_index;

	/* callout */
	rlsFunction = targetPrivate->rlsFunction;
	if (NULL != targetPrivate->rlsContext.retain) {
		context_info	= (void *)targetPrivate->rlsContext.retain(targetPrivate->rlsContext.info);
		context_release	= targetPrivate->rlsContext.release;
	} else {
		context_info	= targetPrivate->rlsContext.info;
		context_release	= NULL;
	}

	pthread_mutex_unlock(&targetPrivate->lock);

	if (rlsFunction) {
		SCLog(_sc_debug, LOG_DEBUG, CFSTR("flags/interface have changed"));
		(*rlsFunction)(target, flags, context_info);
	}

	if (context_release) {
		context_release(context_info);
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

	if (targetPrivate->rlsContext.release) {
		/* let go of the current context */
		targetPrivate->rlsContext.release(targetPrivate->rlsContext.info);
	}

	targetPrivate->rlsFunction   			= callout;
	targetPrivate->rlsContext.info			= NULL;
	targetPrivate->rlsContext.retain		= NULL;
	targetPrivate->rlsContext.release		= NULL;
	targetPrivate->rlsContext.copyDescription	= NULL;
	if (context) {
		bcopy(context, &targetPrivate->rlsContext, sizeof(SCNetworkReachabilityContext));
		if (context->retain) {
			targetPrivate->rlsContext.info = (void *)context->retain(context->info);
		}
	}

	pthread_mutex_unlock(&targetPrivate->lock);

	return TRUE;
}


Boolean
SCNetworkReachabilityScheduleWithRunLoop(SCNetworkReachabilityRef	target,
					 CFRunLoopRef			runLoop,
					 CFStringRef			runLoopMode)
{
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;
	Boolean				init		= FALSE;
	Boolean				ok		= FALSE;

	if (!isA_SCNetworkReachability(target) || runLoop == NULL || runLoopMode == NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
	       return FALSE;
	}

	/* schedule the SCNetworkReachability run loop source */

	pthread_mutex_lock(&hn_lock);
	pthread_mutex_lock(&targetPrivate->lock);

	if (!hn_store) {
		/*
		 * if we are not monitoring any hosts
		 */
		hn_store = SCDynamicStoreCreate(NULL,
						CFSTR("SCNetworkReachability"),
						__SCNetworkReachabilityReachabilityHandleChanges,
						NULL);
		if (!hn_store) {
			SCLog(_sc_verbose, LOG_INFO, CFSTR("SCDynamicStoreCreate() failed"));
			goto done;
		}

		__SCNetworkReachabilityReachabilitySetNotifications(hn_store);

		hn_storeRLS = SCDynamicStoreCreateRunLoopSource(NULL, hn_store, 0);
		hn_rlList   = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		hn_targets  = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
	}

	if (!targetPrivate->rls) {
		CFRunLoopSourceContext	context = { 0			// version
						  , (void *)target	// info
						  , CFRetain		// retain
						  , CFRelease		// release
						  , CFCopyDescription	// copyDescription
						  , CFEqual		// equal
						  , CFHash		// hash
						  , NULL		// schedule
						  , NULL		// cancel
						  , rlsPerform		// perform
						  };

		targetPrivate->rls    = CFRunLoopSourceCreate(NULL, 0, &context);
		targetPrivate->rlList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		init = TRUE;
	}

	if (!__isScheduled(NULL, runLoop, runLoopMode, targetPrivate->rlList)) {
		/*
		 * if we do not already have host notifications scheduled with
		 * this runLoop / runLoopMode
		 */
		CFRunLoopAddSource(runLoop, targetPrivate->rls, runLoopMode);

		if (targetPrivate->dnsRLS) {
			/* if we have an active async DNS query too */
			CFRunLoopAddSource(runLoop, targetPrivate->dnsRLS, runLoopMode);
		}
	}

	__schedule(target, runLoop, runLoopMode, targetPrivate->rlList);

	/* schedule the SCNetworkReachability run loop source */

	if (!__isScheduled(NULL, runLoop, runLoopMode, hn_rlList)) {
		/*
		 * if we do not already have SC notifications scheduled with
		 * this runLoop / runLoopMode
		 */
		CFRunLoopAddSource(runLoop, hn_storeRLS, runLoopMode);
	}

	__schedule(target, runLoop, runLoopMode, hn_rlList);
	CFSetAddValue(hn_targets, target);

	if (init) {
		SCNetworkConnectionFlags	flags;
		uint16_t			if_index;
		SCDynamicStoreRef		store	= NULL;

		/*
		 * if we have yet to schedule SC notifications for this address
		 * - initialize current reachability status
		 */
		if (__SCNetworkReachabilityGetFlags(&store, target, &flags, &if_index, TRUE)) {
			/*
			 * if reachability status available
			 * - set flags
			 * - schedule notification to report status via callback
			 */
			targetPrivate->flags    = flags;
			targetPrivate->if_index = if_index;
			CFRunLoopSourceSignal(targetPrivate->rls);
			__signalRunLoop(target, targetPrivate->rls, targetPrivate->rlList);
		} else {
			/* if reachability status not available, async lookup started */
			targetPrivate->flags    = 0;
			targetPrivate->if_index = 0;
		}
		if (store) CFRelease(store);
	}

	ok = TRUE;

    done :

	pthread_mutex_unlock(&targetPrivate->lock);
	pthread_mutex_unlock(&hn_lock);
	return ok;
}


Boolean
SCNetworkReachabilityUnscheduleFromRunLoop(SCNetworkReachabilityRef	target,
					   CFRunLoopRef			runLoop,
					   CFStringRef			runLoopMode)
{
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;
	CFIndex				n;
	Boolean				ok		= FALSE;

	if (!isA_SCNetworkReachability(target) || runLoop == NULL || runLoopMode == NULL) {
		_SCErrorSet(kSCStatusInvalidArgument);
		goto done;
	}

	pthread_mutex_lock(&hn_lock);
	pthread_mutex_lock(&targetPrivate->lock);

	if (!targetPrivate->rls) {
		/* if not currently scheduled */
		goto done;
	}

	if (!__unschedule(NULL, runLoop, runLoopMode, targetPrivate->rlList, FALSE)) {
		/* if not currently scheduled */
		goto done;
	}

	n = CFArrayGetCount(targetPrivate->rlList);
	if (n == 0 || !__isScheduled(NULL, runLoop, runLoopMode, targetPrivate->rlList)) {
		/*
		 * if this host is no longer scheduled for this runLoop / runLoopMode
		 */
		CFRunLoopRemoveSource(runLoop, targetPrivate->rls, runLoopMode);

		if (targetPrivate->dnsRLS) {
			/* if we have an active async DNS query too */
			CFRunLoopRemoveSource(runLoop, targetPrivate->dnsRLS, runLoopMode);
		}

		if (n == 0) {
			/*
			 * if this host is no longer scheduled
			 */
			CFRelease(targetPrivate->rls);		/* cleanup SCNetworkReachability resources */
			targetPrivate->rls = NULL;
			CFRelease(targetPrivate->rlList);
			targetPrivate->rlList = NULL;
			CFSetRemoveValue(hn_targets, target);	/* cleanup notification resources */

			if (targetPrivate->dnsPort) {
				/* if we have an active async DNS query too */
#ifdef	CHECK_IPV6_REACHABILITY
				lu_async_call_cancel(CFMachPortGetPort(targetPrivate->dnsPort));
#else	/* CHECK_IPV6_REACHABILITY */
				getipnodebyname_async_cancel(CFMachPortGetPort(targetPrivate->dnsPort));
#endif	/* CHECK_IPV6_REACHABILITY */
				CFRelease(targetPrivate->dnsRLS);
				targetPrivate->dnsRLS = NULL;
				CFRelease(targetPrivate->dnsPort);
				targetPrivate->dnsPort = NULL;
			}
		}
	}

	(void)__unschedule(target, runLoop, runLoopMode, hn_rlList, FALSE);

	n = CFArrayGetCount(hn_rlList);
	if (n == 0 || !__isScheduled(NULL, runLoop, runLoopMode, hn_rlList)) {
		/*
		 * if we no longer have any addresses scheduled for
		 * this runLoop / runLoopMode
		 */
		CFRunLoopRemoveSource(runLoop, hn_storeRLS, runLoopMode);

		if (n == 0) {
			/*
			 * if we are no longer monitoring any addresses
			 */
			CFRelease(hn_targets);
			CFRelease(hn_rlList);
			CFRelease(hn_storeRLS);
			CFRelease(hn_store);
			hn_store = NULL;

			/*
			 * until we start monitoring again, ensure that
			 * all subsequent reachability-by-name checks
			 * call res_init()
			 */
			needDNS = TRUE;
		}
	}

	ok = TRUE;

    done :

	pthread_mutex_unlock(&targetPrivate->lock);
	pthread_mutex_unlock(&hn_lock);
	return ok;
}
