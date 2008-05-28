/*
 * Copyright (c) 2003-2008 Apple Inc. All rights reserved.
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

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>

#include <CoreFoundation/CFRuntime.h>
#include <pthread.h>
#include <libkern/OSAtomic.h>

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
#define	KERNEL_PRIVATE
#include <net/route.h>
#undef	KERNEL_PRIVATE

#ifndef s6_addr16
#define s6_addr16 __u6_addr.__u6_addr16
#endif

#include <ppp/ppp_msg.h>




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
	const char			*serv;
	struct addrinfo			hints;
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

	/* [async] DNS query info */
	Boolean				haveDNS;
	CFMachPortRef			dnsPort;
	CFRunLoopSourceRef		dnsRLS;
	struct timeval			dnsQueryStart;

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
static int			rtm_seq		= 0;


/*
 * host "something has changed" notifications
 */

static pthread_mutex_t		hn_lock		= PTHREAD_MUTEX_INITIALIZER;
static SCDynamicStoreRef	hn_store	= NULL;
static CFRunLoopSourceRef	hn_storeRLS	= NULL;
static CFMutableArrayRef	hn_rlList	= NULL;
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
__log_query_time(Boolean found, Boolean async, struct timeval *start)
{
	struct timeval	dnsQueryComplete;
	struct timeval	dnsQueryElapsed;

	if (!_sc_debug) {
		return;
	}

	if (start->tv_sec == 0) {
		return;
	}

	(void) gettimeofday(&dnsQueryComplete, NULL);
	timersub(&dnsQueryComplete, start, &dnsQueryElapsed);
	SCLog(TRUE, LOG_DEBUG,
	      CFSTR("%ssync DNS complete%s (query time = %d.%3.3d)"),
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
	SCDynamicStoreRef	store		= (storeP != NULL) ? *storeP : NULL;
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
			SCLog(_sc_verbose, LOG_INFO, CFSTR("updatePPPStatus SCDynamicStoreCreate() failed"));
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
	if (dict == NULL) {
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
			CFRelease(components);
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

	if (dict != NULL)	CFRelease(dict);
	if (storeP != NULL)	*storeP = store;
	return sc_status;
}


static int
updatePPPAvailable(SCDynamicStoreRef		*storeP,
		   const struct sockaddr	*sa,
		   SCNetworkConnectionFlags	*flags)
{
	CFDictionaryRef		dict		= NULL;
	CFStringRef		entity;
	CFIndex			i;
	const void *		keys_q[N_QUICK];
	const void **		keys		= keys_q;
	CFIndex			n;
	int			sc_status	= kSCStatusReachabilityUnknown;
	SCDynamicStoreRef	store		= (storeP != NULL) ? *storeP : NULL;
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
			SCLog(_sc_debug, LOG_INFO, CFSTR("  status    = unknown (could not access SCDynamicStore"));
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
	if (dict == NULL) {
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
			CFRelease(components);
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

	if (dict != NULL)	CFRelease(dict);
	if (storeP != NULL)	*storeP = store;
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

static Boolean
checkAddress(SCDynamicStoreRef		*storeP,
	     const struct sockaddr	*address,
	     SCNetworkConnectionFlags	*flags,
	     uint16_t			*if_index)
{
	char			buf[BUFLEN];
	struct ifreq		ifr;
	char			if_name[IFNAMSIZ + 1];
	int			isock;
	int			n;
	pid_t			pid		= getpid();
	int			rsock;
	struct sockaddr         *rti_info[RTAX_MAX];
	struct rt_msghdr	*rtm;
	struct sockaddr         *sa;
	int			sc_status	= kSCStatusReachabilityUnknown;
	struct sockaddr_dl	*sdl;
	int32_t			seq		= OSAtomicIncrement32Barrier(&rtm_seq);
	SCDynamicStoreRef	store		= (storeP != NULL) ? *storeP : NULL;
	char			*statusMessage	= NULL;
#ifndef	RTM_GET_SILENT
#warning Note: Using RTM_GET (and not RTM_GET_SILENT)
	static pthread_mutex_t	lock		= PTHREAD_MUTEX_INITIALIZER;
	int			sosize		= 48 * 1024;
#endif

	*flags = 0;
	if (if_index != NULL) {
		*if_index = 0;
	}

	if (address == NULL) {
		/* special case: check only for available paths off the system */
		goto checkAvailable;
	}

	switch (address->sa_family) {
		case AF_INET :
		case AF_INET6 :
			if (_sc_debug) {
				_SC_sockaddr_to_string(address, buf, sizeof(buf));
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
	n = ROUNDUP(sa->sa_len, sizeof(uint32_t));
	rtm->rtm_msglen += n;

	sdl = (struct sockaddr_dl *) ((void *)sa + n);
	sdl->sdl_family = AF_LINK;
	sdl->sdl_len = sizeof (struct sockaddr_dl);
	n = ROUNDUP(sdl->sdl_len, sizeof(uint32_t));
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

	if ((rti_info[RTAX_NETMASK] != NULL) && (rti_info[RTAX_DST] != NULL)) {
		rti_info[RTAX_NETMASK]->sa_family = rti_info[RTAX_DST]->sa_family;
	}

	for (i = 0; i < RTAX_MAX; i++) {
		if (rti_info[i] != NULL) {
			_SC_sockaddr_to_string(rti_info[i], buf, sizeof(buf));
			SCLog(_sc_debug, LOG_DEBUG, CFSTR("%d: %s"), i, buf);
		}
	}
}
#endif	/* DEBUG */

	if ((rti_info[RTAX_IFP] == NULL) ||
	    (rti_info[RTAX_IFP]->sa_family != AF_LINK)) {
		/* no interface info */
		goto done;
	}

	sdl = (struct sockaddr_dl *) rti_info[RTAX_IFP];
	if ((sdl->sdl_nlen == 0) || (sdl->sdl_nlen > IFNAMSIZ)) {
		/* no interface name */
		goto checkAvailable;
	}

	/* get the interface flags */

	bzero(&ifr, sizeof(ifr));
	bcopy(sdl->sdl_data, ifr.ifr_name, sdl->sdl_nlen);

	isock = socket(AF_INET, SOCK_DGRAM, 0);
	if (isock == -1) {
		SCLog(TRUE, LOG_NOTICE, CFSTR("socket() failed: %s"), strerror(errno));
		goto done;
	}

	if (ioctl(isock, SIOCGIFFLAGS, (char *)&ifr) == -1) {
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

				/*
				 * check if 0.0.0.0
				 */
				if (((struct sockaddr_in *)address)->sin_addr.s_addr == 0) {
					statusMessage = "isReachable (this host)";
					*flags |= kSCNetworkFlagsIsLocalAddress;
				}
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

	if (!(rtm->rtm_flags & RTF_GATEWAY) &&
	    (rti_info[RTAX_GATEWAY] != NULL) &&
	    (rti_info[RTAX_GATEWAY]->sa_family == AF_LINK) &&
	    !(ifr.ifr_flags & IFF_POINTOPOINT)) {
		*flags |= kSCNetworkFlagsIsDirect;
	}

	bzero(&if_name, sizeof(if_name));
	bcopy(sdl->sdl_data,
	      if_name,
	      (sdl->sdl_nlen <= IFNAMSIZ) ? sdl->sdl_nlen : IFNAMSIZ);

	if (if_index != NULL) {
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

	if (storeP != NULL)	*storeP = store;
	if (sc_status != kSCStatusOK) {
		_SCErrorSet(sc_status);
		return FALSE;
	}

	return TRUE;
}


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
	if (targetPrivate->rls != NULL) {
		CFStringAppendFormat(result,
				     NULL,
				     CFSTR(", flags = %8.8x, if_index = %hu"),
				     targetPrivate->flags,
				     targetPrivate->if_index);
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

	targetPrivate->haveDNS				= FALSE;
	targetPrivate->dnsPort				= NULL;
	targetPrivate->dnsRLS				= NULL;

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

	targetPrivate->flags |= kSCNetworkFlagsFirstResolvePending;

	targetPrivate->name = CFAllocatorAllocate(NULL, nodenameLen + 1, 0);
	strlcpy((char *)targetPrivate->name, nodename, nodenameLen + 1);

	return (SCNetworkReachabilityRef)targetPrivate;
}


SCNetworkReachabilityRef
SCNetworkReachabilityCreateWithOptions(CFAllocatorRef	allocator,
				       CFDictionaryRef	options)
{
	CFDataRef			data;
	struct addrinfo			*hints	= NULL;
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
	if ((nodename == NULL) && (servname == NULL)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return NULL;
	}

	targetPrivate = __SCNetworkReachabilityCreatePrivate(allocator);
	if (targetPrivate == NULL) {
		return NULL;
	}
	targetPrivate->type = reachabilityTypeName;
	targetPrivate->flags |= kSCNetworkFlagsFirstResolvePending;
	if (nodename != NULL) {
		targetPrivate->name = _SC_cfstring_to_cstring(nodename, NULL, 0, kCFStringEncodingUTF8);
	}
	if (servname != NULL) {
		targetPrivate->serv = _SC_cfstring_to_cstring(servname, NULL, 0, kCFStringEncodingUTF8);
	}
	if (hints != NULL) {
		bcopy(hints, &targetPrivate->hints, sizeof(targetPrivate->hints));
	}

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
			CFDataRef	newAddress;

			newAddress = CFDataCreate(NULL, (void *)resP->ai_addr, resP->ai_addr->sa_len);
			CFArrayAppendValue(addresses, newAddress);
			CFRelease(newAddress);
		}

		/* save the resolved address[es] */
		targetPrivate->resolvedAddress      = addresses;
		targetPrivate->resolvedAddressError = NETDB_SUCCESS;
	} else {
		SCLog(_sc_debug, LOG_INFO, CFSTR("getaddrinfo() failed: %s"), gai_strerror(status));

		/* save the error associated with the attempt to resolve the name */
		targetPrivate->resolvedAddress      = CFRetain(kCFNull);
		targetPrivate->resolvedAddressError = status;
	}

	if (res != NULL)	freeaddrinfo(res);

	if (targetPrivate->rls != NULL) {
		SCLog(_sc_debug, LOG_INFO, CFSTR("DNS request completed"));
		CFRunLoopSourceSignal(targetPrivate->rls);
		_SC_signalRunLoop(target, targetPrivate->rls, targetPrivate->rlList);
	}

	return;
}


static void
__SCNetworkReachabilityCallbackSetResolvedAddress(int32_t status, struct addrinfo *res, void *context)
{
	SCNetworkReachabilityRef	target		= (SCNetworkReachabilityRef)context;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	__log_query_time(((status == 0) && (res != NULL)),	// if successful query
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
rankReachability(SCNetworkConnectionFlags flags)
{
	int	rank = 0;

	if (flags & kSCNetworkFlagsReachable)		rank = 2;
	if (flags & kSCNetworkFlagsConnectionRequired)	rank = 1;
	return rank;
}


static void
getaddrinfo_async_handleCFReply(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
	int32_t				status;
	SCNetworkReachabilityRef	target		= (SCNetworkReachabilityRef)info;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	pthread_mutex_lock(&targetPrivate->lock);

	status = getaddrinfo_async_handle_reply(msg);
	if ((status == 0) &&
	    (targetPrivate->resolvedAddress == NULL) && (targetPrivate->resolvedAddressError == NETDB_SUCCESS)) {
		// if request has been re-queued
		goto again;
	}

	if (port == targetPrivate->dnsPort) {
		CFRunLoopSourceInvalidate(targetPrivate->dnsRLS);
		CFRelease(targetPrivate->dnsRLS);
		targetPrivate->dnsRLS = NULL;
		CFRelease(targetPrivate->dnsPort);
		targetPrivate->dnsPort = NULL;
	}

    again :

	pthread_mutex_unlock(&targetPrivate->lock);

	return;
}


static Boolean
check_resolver_reachability(SCDynamicStoreRef		*storeP,
			    dns_resolver_t		*resolver,
			    SCNetworkConnectionFlags	*flags,
			    Boolean			*haveDNS)
{
	int		i;
	Boolean		ok	= TRUE;

	*flags   = kSCNetworkFlagsReachable;
	*haveDNS = FALSE;

	for (i = 0; i < resolver->n_nameserver; i++) {
		struct sockaddr			*address	= resolver->nameserver[i];
		SCNetworkConnectionFlags	ns_flags	= 0;

		*haveDNS = TRUE;

		if (address->sa_family != AF_INET) {
			/*
			 * we need to skip non-IPv4 DNS server
			 * addresses (at least until [3510431] has
			 * been resolved).
			 */
			continue;
		}

		ok = checkAddress(storeP, address, &ns_flags, NULL);
		if (!ok) {
			/* not today */
			goto done;
		}

		if (rankReachability(ns_flags) < rankReachability(*flags)) {
			/* return the worst case result */
			*flags = ns_flags;
		}
	}

    done :

	return ok;
}


static Boolean
check_matching_resolvers(SCDynamicStoreRef		*storeP,
			 dns_config_t			*dns_config,
			 const char			*fqdn,
			 SCNetworkConnectionFlags	*flags,
			 Boolean			*haveDNS)
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
					ok = check_resolver_reachability(storeP, resolver, flags, haveDNS);
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


Boolean
_SC_checkResolverReachability(SCDynamicStoreRef			*storeP,
			      SCNetworkConnectionFlags		*flags,
			      Boolean				*haveDNS,
			      const char *			nodename,
			      const char *			servname)
{
	dns_resolver_t		*default_resolver;
	dns_configuration_t	*dns;
	Boolean			found			= FALSE;
	char			*fqdn			= (char *)nodename;
	int			i;
	Boolean			isFQDN			= FALSE;
	uint32_t		len;
	Boolean			ok			= TRUE;

	/*
	 * We first assume that all of the configured DNS servers
	 * are available.  Since we don't know which name server will
	 * be consulted to resolve the specified nodename we need to
	 * check the availability of ALL name servers.  We can only
	 * proceed if we know that our query can be answered.
	 */

	*flags   = kSCNetworkFlagsReachable;
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
		goto done;
	}

	if (dns->config->n_resolver == 0) {
		// if no resolver configuration
		goto done;
	}

	*flags = kSCNetworkFlagsReachable;

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
	 * try the provided name
	 */
	found = check_matching_resolvers(storeP, dns->config, fqdn, flags, haveDNS);
	if (!found && !isFQDN && (dns->config->n_resolver > 1)) {
		/*
		 * FQDN not specified, try w/search or default domain(s) too
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
				found = check_matching_resolvers(storeP, dns->config, search_fqdn, flags, haveDNS);
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
				found = check_matching_resolvers(storeP, dns->config, search_fqdn, flags, haveDNS);
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
		ok = check_resolver_reachability(storeP, default_resolver, flags, haveDNS);
	}

	if (fqdn != nodename)	free(fqdn);

    done :

	if (dns != NULL) {
		dns_configuration_release(dns);
	}

	return ok;
}


/*
 * _SC_checkResolverReachabilityByAddress()
 *
 * Given an IP address, determine whether a reverse DNS query can be issued
 * using the current network configuration.
 */
Boolean
_SC_checkResolverReachabilityByAddress(SCDynamicStoreRef	*storeP,
				       SCNetworkConnectionFlags	*flags,
				       Boolean			*haveDNS,
				       struct sockaddr		*sa)
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

	ok = _SC_checkResolverReachability(storeP, flags, haveDNS, ptr_name, NULL);

    done :

	return ok;
}


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


static Boolean
startAsyncDNSQuery(SCNetworkReachabilityRef target) {
	CFMachPortContext		context		= { 0
							  , (void *)target
							  , CFRetain
							  , CFRelease
							  , replyMPCopyDescription
							  };
	int				error;
	CFIndex				i;
	CFIndex				n;
	mach_port_t			port;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	(void) gettimeofday(&targetPrivate->dnsQueryStart, NULL);

	error = getaddrinfo_async_start(&port,
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

	targetPrivate->dnsPort = CFMachPortCreateWithPort(NULL,
							  port,
							  getaddrinfo_async_handleCFReply,
							  &context,
							  NULL);
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
	if (if_index != NULL) {
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
			if (targetPrivate->localAddress != NULL) {
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

			/*
			 * Check "remote" address
			 */
			if (targetPrivate->remoteAddress != NULL) {
				/*
				 * in cases where we have "local" and "remote" addresses
				 * we need to re-initialize the to-be-returned flags.
				 */
				my_flags = 0;
				my_index = 0;

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
			struct timeval			dnsQueryStart;
			int				error;
			SCNetworkConnectionFlags	ns_flags;
			struct addrinfo			*res;

			addresses = (CFMutableArrayRef)SCNetworkReachabilityCopyResolvedAddress(target, &error);
			if ((addresses != NULL) || (error != NETDB_SUCCESS)) {
				/* if resolved or an error had been detected */
				goto checkResolvedAddress;
			}

			/* check the reachability of the DNS servers */
			ok = _SC_checkResolverReachability(storeP,
							   &ns_flags,
							   &targetPrivate->haveDNS,
							   targetPrivate->name,
							   targetPrivate->serv);
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

				SCLog(_sc_debug, LOG_INFO, CFSTR("DNS server(s) not available"));

				if (!checkAddress(storeP, NULL, &my_flags, &my_index)) {
					goto error;
				}

				if (async && (targetPrivate->rls != NULL)) {
					/*
					 * return "host not found", set flags appropriately,
					 * and schedule notification.
					 */
					__SCNetworkReachabilityCallbackSetResolvedAddress(EAI_NONAME,
											  NULL,
											  (void *)target);
					my_flags |= (targetPrivate->flags & kSCNetworkFlagsFirstResolvePending);

					SCLog(_sc_debug, LOG_INFO, CFSTR("no DNS servers are reachable"));
					CFRunLoopSourceSignal(targetPrivate->rls);
					_SC_signalRunLoop(target, targetPrivate->rls, targetPrivate->rlList);
				}
				break;
			}

			if (async) {
				/* for async requests we return the last known status */
				my_flags = targetPrivate->flags;
				my_index = targetPrivate->if_index;

				if (targetPrivate->dnsPort != NULL) {
					/* if request already in progress */
					break;
				}

				SCLog(_sc_debug, LOG_INFO,
				      CFSTR("start DNS query for %s%s%s%s%s"),
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
			      CFSTR("check DNS for %s%s%s%s%s"),
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

			__log_query_time(((error == 0) && (res != NULL)),// if successful query
					 FALSE,				// sync
					 &dnsQueryStart);		// start time

			__SCNetworkReachabilitySetResolvedAddress(error, res, target);

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
				if (((error == EAI_NONAME)
#if defined(EAI_NODATA) && (EAI_NODATA != EAI_NONAME)
				     || (error == EAI_NODATA)
#endif
				     ) && !targetPrivate->haveDNS) {
					/*
					 * No DNS servers are defined. Set flags based on
					 * the availability of configured (but not active)
					 * services.
					 */
					ok = checkAddress(storeP, NULL, &my_flags, &my_index);
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
	if (if_index != NULL) {
		*if_index = my_index;
	}

    error :

	if (addresses != NULL)	CFRelease(addresses);
	return ok;
}


Boolean
SCNetworkReachabilityGetFlags(SCNetworkReachabilityRef	target,
			      SCNetworkConnectionFlags	*flags)
{
	Boolean				ok		= TRUE;
	SCDynamicStoreRef		store		= NULL;
	SCNetworkReachabilityPrivateRef	targetPrivate	= (SCNetworkReachabilityPrivateRef)target;

	if (!isA_SCNetworkReachability(target)) {
		_SCErrorSet(kSCStatusInvalidArgument);
		return FALSE;
	}

	pthread_mutex_lock(&targetPrivate->lock);

	if (targetPrivate->rlList != NULL) {
		// if being watched, return the last known (and what should be current) status
		*flags = targetPrivate->flags & ~kSCNetworkFlagsFirstResolvePending;
		goto done;
	}


	ok = __SCNetworkReachabilityGetFlags(&store, target, flags, NULL, FALSE);
	*flags &= ~kSCNetworkFlagsFirstResolvePending;
	if (store != NULL) CFRelease(store);

    done :

	pthread_mutex_unlock(&targetPrivate->lock);
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

	// Setup:/Network/Global/IPv4 (for the ServiceOrder)
	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							 kSCDynamicStoreDomainSetup,
							 kSCEntNetIPv4);
	CFArrayAppendValue(keys, key);
	CFRelease(key);

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
	Boolean		dnsConfigChanged	= FALSE;
	CFIndex		i;
	CFStringRef	key;
	CFIndex		nTargets;
	const void *	targets_q[N_QUICK];
	const void **	targets			= targets_q;

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
		dnsConfigChanged = TRUE;	/* the DNS server(s) have changed */
	}
	CFRelease(key);

	SCLog(_sc_debug && dnsConfigChanged, LOG_INFO, CFSTR("  DNS configuration changed"));

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
				SCNetworkConnectionFlags	ns_flags;
				Boolean				ok;

				/* check the reachability of the DNS servers */
				ok = _SC_checkResolverReachability(&store,
								   &ns_flags,
								   &targetPrivate->haveDNS,
								   targetPrivate->name,
								   targetPrivate->serv);
				if (!ok || (rankReachability(ns_flags) < 2)) {
					/* if DNS servers are not reachable */
					dnsChanged = TRUE;
				}
			}

			if (dnsChanged) {
				if (targetPrivate->dnsPort != NULL) {
					/* cancel the outstanding DNS query */
					getaddrinfo_async_cancel(CFMachPortGetPort(targetPrivate->dnsPort));
					CFRunLoopSourceInvalidate(targetPrivate->dnsRLS);
					CFRelease(targetPrivate->dnsRLS);
					targetPrivate->dnsRLS = NULL;
					CFRelease(targetPrivate->dnsPort);
					targetPrivate->dnsPort = NULL;
				}

				/* schedule request to resolve the name again */
				if (targetPrivate->resolvedAddress != NULL) {
					CFRelease(targetPrivate->resolvedAddress);
					targetPrivate->resolvedAddress = NULL;
				}
				targetPrivate->resolvedAddress      = NULL;
				targetPrivate->resolvedAddressError = NETDB_SUCCESS;
			}
		}

		CFRunLoopSourceSignal(targetPrivate->rls);
		_SC_signalRunLoop(target, targetPrivate->rls, targetPrivate->rlList);

		pthread_mutex_unlock(&targetPrivate->lock);
	}
	if (targets != targets_q)	CFAllocatorDeallocate(NULL, targets);

    done :

	pthread_mutex_unlock(&hn_lock);
	return;
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
	if (store != NULL) CFRelease(store);
	if (!ok) {
		/* if reachability status not available */
		flags    = 0;
		if_index = 0;
	}

	if ((targetPrivate->flags == flags) && (targetPrivate->if_index == if_index)) {
		/* if reachability flags and interface have not changed */
		pthread_mutex_unlock(&targetPrivate->lock);
		SCLog(_sc_debug, LOG_DEBUG,
		      CFSTR("flags/interface match (now %8.8x/%hu)"),
		      flags, if_index);
		return;
	} else {
		SCLog(_sc_debug, LOG_DEBUG,
		      CFSTR("flags/interface have changed (was %8.8x/%hu, now %8.8x/%hu)"),
		      targetPrivate->flags, targetPrivate->if_index,
		      flags, if_index);
	}

	/* update flags / interface */
	targetPrivate->flags    = flags;
	targetPrivate->if_index = if_index;

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
		(*rlsFunction)(target, flags, context_info);
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

	if (hn_store == NULL) {
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
						__SCNetworkReachabilityReachabilityHandleChanges,
						NULL);
		if (hn_store == NULL) {
			SCLog(_sc_verbose, LOG_INFO, CFSTR("SCDynamicStoreCreate() failed"));
			goto done;
		}

		__SCNetworkReachabilityReachabilitySetNotifications(hn_store);

		hn_storeRLS = SCDynamicStoreCreateRunLoopSource(NULL, hn_store, 0);
		hn_rlList   = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		hn_targets  = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
	}

	if (targetPrivate->rls == NULL) {
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

		targetPrivate->rls    = CFRunLoopSourceCreate(NULL, 0, &context);
		targetPrivate->rlList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		init = TRUE;
	}

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

	/* schedule the SCNetworkReachability run loop source */

	if (!_SC_isScheduled(NULL, runLoop, runLoopMode, hn_rlList)) {
		/*
		 * if we do not already have SC notifications scheduled with
		 * this runLoop / runLoopMode
		 */
		CFRunLoopAddSource(runLoop, hn_storeRLS, runLoopMode);
	}

	_SC_schedule(target, runLoop, runLoopMode, hn_rlList);
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
			_SC_signalRunLoop(target, targetPrivate->rls, targetPrivate->rlList);
		} else {
			/* if reachability status not available, async lookup started */
			targetPrivate->flags    = 0;
			targetPrivate->if_index = 0;
		}
		if (store != NULL) CFRelease(store);
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
		return FALSE;
	}

	pthread_mutex_lock(&hn_lock);
	pthread_mutex_lock(&targetPrivate->lock);

	if (targetPrivate->rls == NULL) {
		/* if not currently scheduled */
		_SCErrorSet(kSCStatusInvalidArgument);
		goto done;
	}

	if (!_SC_unschedule(NULL, runLoop, runLoopMode, targetPrivate->rlList, FALSE)) {
		/* if not currently scheduled */
		_SCErrorSet(kSCStatusInvalidArgument);
		goto done;
	}

	n = CFArrayGetCount(targetPrivate->rlList);
	if (n == 0 || !_SC_isScheduled(NULL, runLoop, runLoopMode, targetPrivate->rlList)) {
		/*
		 * if this host is no longer scheduled for this runLoop / runLoopMode
		 */
		CFRunLoopRemoveSource(runLoop, targetPrivate->rls, runLoopMode);

		if (targetPrivate->dnsRLS != NULL) {
			/* if we have an active async DNS query too */
			CFRunLoopRemoveSource(runLoop, targetPrivate->dnsRLS, runLoopMode);
		}

		if (n == 0) {
			/*
			 * if this host is no longer scheduled
			 */
			CFRunLoopSourceInvalidate(targetPrivate->rls);	/* cleanup SCNetworkReachability resources */
			CFRelease(targetPrivate->rls);
			targetPrivate->rls = NULL;
			CFRelease(targetPrivate->rlList);
			targetPrivate->rlList = NULL;
			CFSetRemoveValue(hn_targets, target);		/* cleanup notification resources */

			if (targetPrivate->dnsPort != NULL) {
				/* if we have an active async DNS query too */
				getaddrinfo_async_cancel(CFMachPortGetPort(targetPrivate->dnsPort));
				CFRunLoopSourceInvalidate(targetPrivate->dnsRLS);
				CFRelease(targetPrivate->dnsRLS);
				targetPrivate->dnsRLS = NULL;
				CFRelease(targetPrivate->dnsPort);
				targetPrivate->dnsPort = NULL;
			}
		}
	}

	(void)_SC_unschedule(target, runLoop, runLoopMode, hn_rlList, FALSE);

	n = CFArrayGetCount(hn_rlList);
	if (n == 0 || !_SC_isScheduled(NULL, runLoop, runLoopMode, hn_rlList)) {
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
			hn_targets = NULL;
			CFRelease(hn_rlList);
			hn_rlList = NULL;
			CFRunLoopSourceInvalidate(hn_storeRLS);
			CFRelease(hn_storeRLS);
			hn_storeRLS = NULL;
			CFRelease(hn_store);
			hn_store = NULL;

			/*
			 * until we start monitoring again, ensure that
			 * any resources associated with tracking the
			 * DNS configuration have been released.
			 */
			dns_configuration_unwatch();
		}
	}

	ok = TRUE;

    done :

	pthread_mutex_unlock(&targetPrivate->lock);
	pthread_mutex_unlock(&hn_lock);
	return ok;
}
