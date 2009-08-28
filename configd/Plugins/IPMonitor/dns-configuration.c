/*
 * Copyright (c) 2004-2009 Apple Inc. All rights reserved.
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
 * March 22, 2004	Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <TargetConditionals.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>
#if	!TARGET_OS_IPHONE
#include <notify.h>
extern uint32_t notify_monitor_file(int token, const char *name, int flags);
#endif	// !TARGET_OS_IPHONE

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>

#include <dnsinfo.h>
#include <dnsinfo_create.h>

#include <dns_sd.h>
#ifndef	kDNSServiceCompMulticastDNS
#define	kDNSServiceCompMulticastDNS	"MulticastDNS"
#endif
#ifndef	kDNSServiceCompPrivateDNS
#define	kDNSServiceCompPrivateDNS	"PrivateDNS"
#endif

/* multicast DNS resolver configurations */
static	CFNumberRef	S_mdns_timeout	= NULL;

/* private DNS resolver configurations */
static	CFNumberRef	S_pdns_timeout	= NULL;


static void
add_resolver(CFMutableArrayRef supplemental, CFMutableDictionaryRef resolver)
{
	CFIndex		i;
	CFIndex		n_supplemental;
	CFNumberRef	order;
	uint32_t	order_val	= 0;

	order = CFDictionaryGetValue(resolver, kSCPropNetDNSSearchOrder);
	if (!isA_CFNumber(order) ||
	    !CFNumberGetValue(order, kCFNumberIntType, &order_val)) {
		order     = NULL;
		order_val = 0;
	}

	n_supplemental = CFArrayGetCount(supplemental);
	for (i = 0; i < n_supplemental; i++) {
		CFDictionaryRef		supplemental_resolver;

		supplemental_resolver = CFArrayGetValueAtIndex(supplemental, i);
		if (CFEqual(resolver, supplemental_resolver)) {
			// a real duplicate
			return;
		}

		if (order != NULL) {
			CFMutableDictionaryRef	compare;
			Boolean			match;

			compare = CFDictionaryCreateMutableCopy(NULL, 0, supplemental_resolver);
			CFDictionarySetValue(compare, kSCPropNetDNSSearchOrder, order);
			match = CFEqual(resolver, compare);
			CFRelease(compare);
			if (match) {
				CFNumberRef	supplemental_order;
				uint32_t	supplemental_order_val	= 0;

				// if only the search order's are different
				supplemental_order = CFDictionaryGetValue(supplemental_resolver, kSCPropNetDNSSearchOrder);
				if (!isA_CFNumber(supplemental_order) ||
				    !CFNumberGetValue(supplemental_order, kCFNumberIntType, &supplemental_order_val)) {
					supplemental_order_val = 0;
				}

				if (order_val < supplemental_order_val ) {
					// if we should prefer this match resolver, else just skip it
					CFArraySetValueAtIndex(supplemental, i, resolver);
				}

				return;
			}
		}
	}

	order = CFNumberCreate(NULL, kCFNumberIntType, &n_supplemental);
	CFDictionarySetValue(resolver, CFSTR("*ORDER*"), order);
	CFRelease(order);

	CFArrayAppendValue(supplemental, resolver);
	return;
}


static void
add_supplemental(CFMutableArrayRef supplemental, CFDictionaryRef dns, uint32_t defaultOrder)
{
	CFArrayRef	domains;
	CFIndex		i;
	CFIndex		n_domains;
	CFArrayRef	orders;

	domains = CFDictionaryGetValue(dns, kSCPropNetDNSSupplementalMatchDomains);
	n_domains = isA_CFArray(domains) ? CFArrayGetCount(domains) : 0;
	if (n_domains == 0) {
		return;
	}

	orders = CFDictionaryGetValue(dns, kSCPropNetDNSSupplementalMatchOrders);
	if (orders != NULL) {
		if (!isA_CFArray(orders) || (n_domains != CFArrayGetCount(orders))) {
			return;
		}
	}

	/*
	 * yes, this is a "supplemental" resolver configuration, expand
	 * the match domains and add each to the supplemental list.
	 */
	for (i = 0; i < n_domains; i++) {
		CFStringRef		match_domain;
		CFNumberRef		match_order;
		CFMutableDictionaryRef	match_resolver;

		match_domain = CFArrayGetValueAtIndex(domains, i);
		if (!isA_CFString(match_domain)) {
			continue;
		}

		match_order = (orders != NULL) ? CFArrayGetValueAtIndex(orders, i) : NULL;

		match_resolver = CFDictionaryCreateMutableCopy(NULL, 0, dns);

		// remove keys we don't want in a supplemental resolver
		CFDictionaryRemoveValue(match_resolver, kSCPropNetDNSSupplementalMatchDomains);
		CFDictionaryRemoveValue(match_resolver, kSCPropNetDNSSupplementalMatchOrders);
		CFDictionaryRemoveValue(match_resolver, kSCPropNetDNSSearchDomains);
		CFDictionaryRemoveValue(match_resolver, kSCPropNetDNSSortList);

		// set supplemental resolver "domain"
		if (CFStringGetLength(match_domain) > 0) {
			CFDictionarySetValue(match_resolver, kSCPropNetDNSDomainName, match_domain);
		} else {
			CFDictionaryRemoveValue(match_resolver, kSCPropNetDNSDomainName);
		}

		// set supplemental resolver "search_order"
		if (isA_CFNumber(match_order)) {
			CFDictionarySetValue(match_resolver, kSCPropNetDNSSearchOrder, match_order);
		} else if (!CFDictionaryContainsKey(match_resolver, kSCPropNetDNSSearchOrder)) {
			CFNumberRef     num;

			num = CFNumberCreate(NULL, kCFNumberIntType, &defaultOrder);
			CFDictionarySetValue(match_resolver, kSCPropNetDNSSearchOrder, num);
			CFRelease(num);

			defaultOrder++;		// if multiple domains, maintain ordering
		}
		add_resolver(supplemental, match_resolver);
		CFRelease(match_resolver);
	}

	return;
}


#define	N_QUICK	32


static void
add_supplemental_resolvers(CFMutableArrayRef supplemental, CFDictionaryRef services, CFArrayRef service_order)
{
	const void *		keys_q[N_QUICK];
	const void **		keys	= keys_q;
	CFIndex			i;
	CFIndex			n_order;
	CFIndex			n_services;
	const void *		vals_q[N_QUICK];
	const void **		vals	= vals_q;

	n_services = isA_CFDictionary(services) ? CFDictionaryGetCount(services) : 0;
	if (n_services == 0) {
		return;		// if no services
	}

	if (n_services > (CFIndex)(sizeof(keys_q) / sizeof(CFTypeRef))) {
		keys = CFAllocatorAllocate(NULL, n_services * sizeof(CFTypeRef), 0);
		vals = CFAllocatorAllocate(NULL, n_services * sizeof(CFTypeRef), 0);
	}

	n_order = isA_CFArray(service_order) ? CFArrayGetCount(service_order) : 0;

	CFDictionaryGetKeysAndValues(services, keys, vals);
	for (i = 0; i < n_services; i++) {
		uint32_t	defaultOrder;
		CFDictionaryRef dns;
		CFDictionaryRef service = (CFDictionaryRef)vals[i];

		if (!isA_CFDictionary(service)) {
			continue;
		}

		dns = CFDictionaryGetValue(service, kSCEntNetDNS);
		if (!isA_CFDictionary(dns)) {
			continue;
		}

		defaultOrder = DEFAULT_SEARCH_ORDER
			       - (DEFAULT_SEARCH_ORDER / 2)
			       + ((DEFAULT_SEARCH_ORDER / 1000) * i);
		if ((n_order > 0) &&
		    !CFArrayContainsValue(service_order, CFRangeMake(0, n_order), keys[i])) {
			// push out services not specified in service order
			defaultOrder += (DEFAULT_SEARCH_ORDER / 1000) * n_services;
		}

		add_supplemental(supplemental, dns, defaultOrder);
	}

	if (keys != keys_q) {
		CFAllocatorDeallocate(NULL, keys);
		CFAllocatorDeallocate(NULL, vals);
	}

	return;
}


static void
add_multicast_resolvers(CFMutableArrayRef supplemental, CFArrayRef multicastResolvers)
{
	CFIndex	i;
	CFIndex	n;

	n = isA_CFArray(multicastResolvers) ? CFArrayGetCount(multicastResolvers) : 0;
	for (i = 0; i < n; i++) {
		uint32_t		defaultOrder;
		CFStringRef		domain;
		CFNumberRef		num;
		CFMutableDictionaryRef	resolver;

		domain = CFArrayGetValueAtIndex(multicastResolvers, i);
		if (!isA_CFString(domain) || (CFStringGetLength(domain) == 0)) {
			continue;
		}

		defaultOrder = DEFAULT_SEARCH_ORDER
		+ (DEFAULT_SEARCH_ORDER / 2)
		+ ((DEFAULT_SEARCH_ORDER / 1000) * i);

		resolver = CFDictionaryCreateMutable(NULL,
						     0,
						     &kCFTypeDictionaryKeyCallBacks,
						     &kCFTypeDictionaryValueCallBacks);
		CFDictionarySetValue(resolver, kSCPropNetDNSDomainName, domain);
		CFDictionarySetValue(resolver, kSCPropNetDNSOptions, CFSTR("mdns"));
		num = CFNumberCreate(NULL, kCFNumberIntType, &defaultOrder);
		CFDictionarySetValue(resolver, kSCPropNetDNSSearchOrder, num);
		CFRelease(num);
		if (S_mdns_timeout != NULL) {
			CFDictionarySetValue(resolver, kSCPropNetDNSServerTimeout, S_mdns_timeout);
		}
		add_resolver(supplemental, resolver);
		CFRelease(resolver);
	}

	return;
}


static void
add_private_resolvers(CFMutableArrayRef supplemental, CFArrayRef privateResolvers)
{
	CFIndex	i;
	CFIndex	n;

	n = isA_CFArray(privateResolvers) ? CFArrayGetCount(privateResolvers) : 0;
	for (i = 0; i < n; i++) {
		uint32_t		defaultOrder;
		CFStringRef		domain;
		CFNumberRef		num;
		CFMutableDictionaryRef	resolver;

		domain = CFArrayGetValueAtIndex(privateResolvers, i);
		if (!isA_CFString(domain) || (CFStringGetLength(domain) == 0)) {
			continue;
		}

		defaultOrder = DEFAULT_SEARCH_ORDER
			       - (DEFAULT_SEARCH_ORDER / 4)
			       + ((DEFAULT_SEARCH_ORDER / 1000) * i);

		resolver = CFDictionaryCreateMutable(NULL,
						     0,
						     &kCFTypeDictionaryKeyCallBacks,
						     &kCFTypeDictionaryValueCallBacks);
		CFDictionarySetValue(resolver, kSCPropNetDNSDomainName, domain);
		CFDictionarySetValue(resolver, kSCPropNetDNSOptions, CFSTR("pdns"));
		num = CFNumberCreate(NULL, kCFNumberIntType, &defaultOrder);
		CFDictionarySetValue(resolver, kSCPropNetDNSSearchOrder, num);
		CFRelease(num);
		if (S_pdns_timeout != NULL) {
			CFDictionarySetValue(resolver, kSCPropNetDNSServerTimeout, S_pdns_timeout);
		}
		add_resolver(supplemental, resolver);
		CFRelease(resolver);
	}

	return;
}


static CFComparisonResult
compareBySearchOrder(const void *val1, const void *val2, void *context)
{
	CFDictionaryRef	dns1	= (CFDictionaryRef)val1;
	CFDictionaryRef	dns2	= (CFDictionaryRef)val2;
	CFNumberRef	num1;
	CFNumberRef	num2;
	uint32_t	order1	= DEFAULT_SEARCH_ORDER;
	uint32_t	order2	= DEFAULT_SEARCH_ORDER;

	num1 = CFDictionaryGetValue(dns1, kSCPropNetDNSSearchOrder);
	if (!isA_CFNumber(num1) ||
	    !CFNumberGetValue(num1, kCFNumberIntType, &order1)) {
		order1 = DEFAULT_SEARCH_ORDER;
	}

	num2 = CFDictionaryGetValue(dns2, kSCPropNetDNSSearchOrder);
	if (!isA_CFNumber(num2) ||
	    !CFNumberGetValue(num2, kCFNumberIntType, &order2)) {
		order2 = DEFAULT_SEARCH_ORDER;
	}

	if (order1 == order2) {
		// if same "SearchOrder", retain original orderring for configurations
		if (CFDictionaryGetValueIfPresent(dns1, CFSTR("*ORDER*"), (const void **)&num1) &&
		    CFDictionaryGetValueIfPresent(dns2, CFSTR("*ORDER*"), (const void **)&num2) &&
		    isA_CFNumber(num1) &&
		    isA_CFNumber(num2) &&
		    CFNumberGetValue(num1, kCFNumberIntType, &order1) &&
		    CFNumberGetValue(num2, kCFNumberIntType, &order2)) {
			if (order1 == order2) {
				return kCFCompareEqualTo;
			} else {
				return (order1 < order2) ? kCFCompareLessThan : kCFCompareGreaterThan;
			}
		}

		return kCFCompareEqualTo;
	}

	return (order1 < order2) ? kCFCompareLessThan : kCFCompareGreaterThan;
}


static CFStringRef
trimDomain(CFStringRef domain)
{
	CFIndex	length;

	if (!isA_CFString(domain)) {
		return NULL;
	}

	// remove any leading/trailing dots
	length = CFStringGetLength(domain);
	if ((length > 0) &&
	    (CFStringFindWithOptions(domain,
				     CFSTR("."),
				     CFRangeMake(0, 1),
				     kCFCompareAnchored,
				     NULL) ||
	     CFStringFindWithOptions(domain,
				     CFSTR("."),
				     CFRangeMake(0, length),
				     kCFCompareAnchored|kCFCompareBackwards,
				     NULL))) {
		CFMutableStringRef	trimmed;

		trimmed = CFStringCreateMutableCopy(NULL, 0, domain);
		CFStringTrim(trimmed, CFSTR("."));
		domain = (CFStringRef)trimmed;
		length = CFStringGetLength(domain);
	} else {
		CFRetain(domain);
	}

	if (length == 0) {
		CFRelease(domain);
		domain = NULL;
	}

	return domain;
}


static void
update_search_domains(CFMutableDictionaryRef *defaultDomain, CFArrayRef supplemental)
{
	CFStringRef		defaultDomainName	= NULL;
	uint32_t		defaultOrder		= DEFAULT_SEARCH_ORDER;
	CFArrayRef		defaultSearchDomains	= NULL;
	CFIndex			defaultSearchIndex	= 0;
	CFIndex			i;
	CFMutableArrayRef	mySearchDomains;
	CFMutableArrayRef	mySupplemental		= NULL;
	CFIndex			n_supplemental;
	Boolean			searchDomainAdded	= FALSE;

	n_supplemental = CFArrayGetCount(supplemental);
	if (n_supplemental == 0) {
		// if no supplemental domains
		return;
	}

	if (*defaultDomain != NULL) {
		CFNumberRef	num;

		num = CFDictionaryGetValue(*defaultDomain, kSCPropNetDNSSearchOrder);
		if (!isA_CFNumber(num) ||
		    !CFNumberGetValue(num, kCFNumberIntType, &defaultOrder)) {
			defaultOrder = DEFAULT_SEARCH_ORDER;
		}

		defaultDomainName    = CFDictionaryGetValue(*defaultDomain, kSCPropNetDNSDomainName);
		defaultSearchDomains = CFDictionaryGetValue(*defaultDomain, kSCPropNetDNSSearchDomains);
	}

	mySearchDomains = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	if (isA_CFArray(defaultSearchDomains)) {
		CFIndex	n_search;

		n_search = CFArrayGetCount(defaultSearchDomains);
		for (i = 0; i < n_search; i++) {
			CFStringRef	search;

			search = CFArrayGetValueAtIndex(defaultSearchDomains, i);
			search = trimDomain(search);
			if (search != NULL) {
				CFArrayAppendValue(mySearchDomains, search);
				CFRelease(search);
			}
		}
	} else {
		defaultDomainName = trimDomain(defaultDomainName);
		if (defaultDomainName != NULL) {
			char	*domain;
			int	domain_parts	= 1;
			char	*dp;

			domain = _SC_cfstring_to_cstring(defaultDomainName,
							 NULL,
							 0,
							 kCFStringEncodingUTF8);
			CFRelease(defaultDomainName);

			// count domain parts
			for (dp = domain; *dp != '\0'; dp++) {
				if (*dp == '.') {
					domain_parts++;
				}
			}

			dp = domain;
			for (i = LOCALDOMAINPARTS; i <= domain_parts; i++) {
				CFStringRef	search;

				search = CFStringCreateWithCString(NULL,
								   dp,
								   kCFStringEncodingUTF8);
				CFArrayAppendValue(mySearchDomains, search);
				CFRelease(search);

				dp = strchr(dp, '.') + 1;
			}

			CFAllocatorDeallocate(NULL, domain);
		}
	}

	if (n_supplemental > 1) {
		mySupplemental = CFArrayCreateMutableCopy(NULL, 0, supplemental);
		CFArraySortValues(mySupplemental,
				  CFRangeMake(0, n_supplemental),
				  compareBySearchOrder,
				  NULL);
		supplemental = mySupplemental;
	}

	for (i = 0; i < n_supplemental; i++) {
		CFDictionaryRef dns;
		CFIndex		domainIndex;
		CFNumberRef	num;
		CFStringRef	options;
		CFStringRef	supplementalDomain;
		uint32_t	supplementalOrder;

		dns = CFArrayGetValueAtIndex(supplemental, i);

		options = CFDictionaryGetValue(dns, kSCPropNetDNSOptions);
		if (isA_CFString(options)) {
			CFRange	range;

			if (CFEqual(options, CFSTR("pdns"))) {
				// don't add private resolver domains to the search list
				continue;
			}

			range = CFStringFind(options, CFSTR("interface="), 0);
			if (range.location != kCFNotFound) {
				// don't add scoped resolver domains to the search list
				continue;
			}
		}

		supplementalDomain = CFDictionaryGetValue(dns, kSCPropNetDNSDomainName);
		supplementalDomain = trimDomain(supplementalDomain);
		if (supplementalDomain == NULL) {
			continue;
		}

		if (CFStringHasSuffix(supplementalDomain, CFSTR(".in-addr.arpa")) ||
		    CFStringHasSuffix(supplementalDomain, CFSTR(".ip6.arpa"    ))) {
			CFRelease(supplementalDomain);
			continue;
		}

		domainIndex = CFArrayGetFirstIndexOfValue(mySearchDomains,
							  CFRangeMake(0, CFArrayGetCount(mySearchDomains)),
							  supplementalDomain);

		num = CFDictionaryGetValue(dns, kSCPropNetDNSSearchOrder);
		if (!isA_CFNumber(num) ||
		    !CFNumberGetValue(num, kCFNumberIntType, &supplementalOrder)) {
			supplementalOrder = DEFAULT_SEARCH_ORDER;
		}

		if (supplementalOrder < defaultOrder) {
			if (domainIndex != kCFNotFound) {
				// if supplemental domain is already in the search list
				CFArrayRemoveValueAtIndex(mySearchDomains, domainIndex);
				if (domainIndex < defaultSearchIndex) {
					defaultSearchIndex--;
				}
			}
			CFArrayInsertValueAtIndex(mySearchDomains,
						  defaultSearchIndex,
						  supplementalDomain);
			defaultSearchIndex++;
			searchDomainAdded = TRUE;
		} else {
			if (domainIndex == kCFNotFound) {
				// add to the (end of the) search list
				CFArrayAppendValue(mySearchDomains, supplementalDomain);
				searchDomainAdded = TRUE;
			}
		}

		CFRelease(supplementalDomain);
	}

	if (searchDomainAdded) {
		if (*defaultDomain == NULL) {
			*defaultDomain = CFDictionaryCreateMutable(NULL,
								   0,
								   &kCFTypeDictionaryKeyCallBacks,
								   &kCFTypeDictionaryValueCallBacks);
		}
		CFDictionarySetValue(*defaultDomain, kSCPropNetDNSSearchDomains, mySearchDomains);
	}

	CFRelease(mySearchDomains);
	if (mySupplemental != NULL) CFRelease(mySupplemental);
	return;
}


static dns_create_resolver_t
create_resolver(CFDictionaryRef dns)
{
	CFArrayRef		list;
	CFNumberRef		num;
	dns_create_resolver_t	_resolver;
	CFStringRef		str;

	_resolver = _dns_resolver_create();

	// process domain
	str = CFDictionaryGetValue(dns, kSCPropNetDNSDomainName);
	if (isA_CFString(str) && (CFStringGetLength(str) > 0)) {
		char	domain[NS_MAXDNAME];

		if (_SC_cfstring_to_cstring(str, domain, sizeof(domain), kCFStringEncodingUTF8) != NULL) {
			_dns_resolver_set_domain(&_resolver, domain);
		}
	}

	// process search domains
	list = CFDictionaryGetValue(dns, kSCPropNetDNSSearchDomains);
	if (isA_CFArray(list)) {
		CFIndex	i;
		CFIndex n	= CFArrayGetCount(list);

		// add "search" domains
		for (i = 0; i < n; i++) {
			str = CFArrayGetValueAtIndex(list, i);
			if (isA_CFString(str) && (CFStringGetLength(str) > 0)) {
				char	search[NS_MAXDNAME];

				if (_SC_cfstring_to_cstring(str, search, sizeof(search), kCFStringEncodingUTF8) != NULL) {
					_dns_resolver_add_search(&_resolver, search);
				}
			}
		}
	}

	// process nameserver addresses
	list = CFDictionaryGetValue(dns, kSCPropNetDNSServerAddresses);
	if (isA_CFArray(list)) {
		CFIndex	i;
		CFIndex n	= CFArrayGetCount(list);

		for (i = 0; i < n; i++) {
			union {
				struct sockaddr		sa;
				struct sockaddr_in	sin;
				struct sockaddr_in6	sin6;
			} addr;
			char	buf[128];

			str = CFArrayGetValueAtIndex(list, i);
			if (!isA_CFString(str)) {
				continue;
			}

			if (_SC_cfstring_to_cstring(str, buf, sizeof(buf), kCFStringEncodingASCII) == NULL) {
				continue;
			}

			bzero(&addr, sizeof(addr));
			if (inet_aton(buf, &addr.sin.sin_addr) == 1) {
				/* if IPv4 address */
				addr.sin.sin_len    = sizeof(addr.sin);
				addr.sin.sin_family = AF_INET;
				_dns_resolver_add_nameserver(&_resolver, &addr.sa);
			} else if (inet_pton(AF_INET6, buf, &addr.sin6.sin6_addr) == 1) {
				/* if IPv6 address */
				char	*p;

				p = strchr(buf, '%');
				if (p != NULL) {
					addr.sin6.sin6_scope_id = if_nametoindex(p + 1);
				}

				addr.sin6.sin6_len    = sizeof(addr.sin6);
				addr.sin6.sin6_family = AF_INET6;
				_dns_resolver_add_nameserver(&_resolver, &addr.sa);
			} else {
				continue;
			}
		}
	}

	// process search order
	num = CFDictionaryGetValue(dns, kSCPropNetDNSSearchOrder);
	if (isA_CFNumber(num)) {
		uint32_t	order;

		if (CFNumberGetValue(num, kCFNumberIntType, &order)) {
			_dns_resolver_set_order(&_resolver, order);
		}
	}

	// process sortlist
	list = CFDictionaryGetValue(dns, kSCPropNetDNSSortList);
	if (isA_CFArray(list)) {
		CFIndex	i;
		CFIndex n	= CFArrayGetCount(list);

		for (i = 0; i < n; i++) {
			char		buf[128];
			char		*slash;
			dns_sortaddr_t	sortaddr;

			str = CFArrayGetValueAtIndex(list, i);
			if (!isA_CFString(str)) {
				continue;
			}

			if (_SC_cfstring_to_cstring(str, buf, sizeof(buf), kCFStringEncodingASCII) == NULL) {
				continue;
			}

			slash = strchr(buf, '/');
			if (slash != NULL) {
				*slash = '\0';
			}

			bzero(&sortaddr, sizeof(sortaddr));
			if (inet_aton(buf, &sortaddr.address) != 1) {
				/* if address not valid */
				continue;
			}

			if (slash != NULL) {
				if (inet_aton(slash + 1, &sortaddr.mask) != 1) {
					/* if subnet mask not valid */
					continue;
				}
			} else {
				in_addr_t	a;
				in_addr_t	m;

				a = ntohl(sortaddr.address.s_addr);
				if (IN_CLASSA(a)) {
					m = IN_CLASSA_NET;
				} else if (IN_CLASSB(a)) {
					m = IN_CLASSB_NET;
				} else if (IN_CLASSC(a)) {
					m = IN_CLASSC_NET;
				} else {
					continue;
				}

				sortaddr.mask.s_addr = htonl(m);
			}

			_dns_resolver_add_sortaddr(&_resolver, &sortaddr);
		}
	}

	// process port
	num = CFDictionaryGetValue(dns, kSCPropNetDNSServerPort);
	if (isA_CFNumber(num)) {
		int	port;

		if (CFNumberGetValue(num, kCFNumberIntType, &port)) {
			_dns_resolver_set_port(&_resolver, (uint16_t)port);
		}
	}

	// process timeout
	num = CFDictionaryGetValue(dns, kSCPropNetDNSServerTimeout);
	if (isA_CFNumber(num)) {
		int	timeout;

		if (CFNumberGetValue(num, kCFNumberIntType, &timeout)) {
			_dns_resolver_set_timeout(&_resolver, (uint32_t)timeout);
		}
	}

	// process options
	str = CFDictionaryGetValue(dns, kSCPropNetDNSOptions);
	if (isA_CFString(str)) {
		char	*options;

		options = _SC_cfstring_to_cstring(str, NULL, 0, kCFStringEncodingUTF8);
		if (options != NULL) {
			_dns_resolver_set_options(&_resolver, options);
			CFAllocatorDeallocate(NULL, options);
		}
	}

	return _resolver;
}


__private_extern__
void
dns_configuration_set(CFDictionaryRef   defaultResolver,
		      CFDictionaryRef   services,
		      CFArrayRef	serviceOrder,
		      CFArrayRef	multicastResolvers,
		      CFArrayRef	privateResolvers)
{
	CFIndex			i;
	CFMutableDictionaryRef	myDefault;
	uint32_t		myOrder		= DEFAULT_SEARCH_ORDER;
	Boolean			myOrderAdded	= FALSE;
	CFIndex			n_supplemental;
	CFNumberRef		order;
	dns_create_resolver_t	resolver;
	CFMutableArrayRef	supplemental;

#ifdef	SPECIAL_HANDLING_FOR_DEFAULT_DOMAIN
	CFStringRef		myDomain	= NULL;
	CFArrayRef		search;
#endif	// SPECIAL_HANDLING_FOR_DEFAULT_DOMAIN

	if (defaultResolver == NULL) {
		myDefault = CFDictionaryCreateMutable(NULL,
						      0,
						      &kCFTypeDictionaryKeyCallBacks,
						      &kCFTypeDictionaryValueCallBacks);
	} else {
		myDefault = CFDictionaryCreateMutableCopy(NULL, 0, defaultResolver);

		// ensure that the default resolver has a search order

		order = CFDictionaryGetValue(myDefault, kSCPropNetDNSSearchOrder);
		if (!isA_CFNumber(order) ||
		    !CFNumberGetValue(order, kCFNumberIntType, &myOrder)) {
			myOrderAdded = TRUE;
			myOrder = DEFAULT_SEARCH_ORDER;
			order = CFNumberCreate(NULL, kCFNumberIntType, &myOrder);
			CFDictionarySetValue(myDefault, kSCPropNetDNSSearchOrder, order);
			CFRelease(order);
		}
	}

	// establish list of supplemental resolvers

	supplemental = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	// identify search[] list and/or domain name

#ifdef	SPECIAL_HANDLING_FOR_DEFAULT_DOMAIN
	search = CFDictionaryGetValue(myDefault, kSCPropNetDNSSearchDomains);
	if (isA_CFArray(search) && (CFArrayGetCount(search) > 0)) {
		myDomain = CFArrayGetValueAtIndex(search, 0);
		myDomain = isA_CFString(myDomain);
	}

	if (myDomain == NULL) {
		myDomain = CFDictionaryGetValue(myDefault, kSCPropNetDNSDomainName);
		myDomain = isA_CFString(myDomain);
	}

	// add match for default domain

	if (myDomain != NULL) {
		CFMutableDictionaryRef	mySupplemental;

		mySupplemental = CFDictionaryCreateMutableCopy(NULL, 0, myDefault);
		CFDictionarySetValue   (mySupplemental, kSCPropNetDNSDomainName, myDomain);
		CFDictionaryRemoveValue(mySupplemental, kSCPropNetDNSSearchDomains);
		CFDictionaryRemoveValue(mySupplemental, kSCPropNetDNSSupplementalMatchDomains);
		CFDictionaryRemoveValue(mySupplemental, kSCPropNetDNSSupplementalMatchOrders);
		add_resolver(supplemental, mySupplemental);
		CFRelease(mySupplemental);
	}
#endif	// SPECIAL_HANDLING_FOR_DEFAULT_DOMAIN

	// collect (and add) any supplemental resolver configurations

	add_supplemental_resolvers(supplemental, services, serviceOrder);

	// collect (and add) any "private" resolver configurations

	add_private_resolvers(supplemental, privateResolvers);

	// update the "search" list

	update_search_domains(&myDefault, supplemental);

	// collect (and add) any "multicast" resolver configurations

	add_multicast_resolvers(supplemental, multicastResolvers);

	// check if the "match for default domain" (above) is really needed

#ifdef	SPECIAL_HANDLING_FOR_DEFAULT_DOMAIN
	if (myDomain != NULL) {
		Boolean	sharedDomain	= FALSE;

		n_supplemental = CFArrayGetCount(supplemental);
		for (i = 1; i < n_supplemental; i++) {
			CFStringRef	domain;
			CFDictionaryRef	mySupplemental;

			mySupplemental = CFArrayGetValueAtIndex(supplemental, i);
			domain = CFDictionaryGetValue(mySupplemental, kSCPropNetDNSDomainName);
			if (isA_CFString(domain)) {
				if (CFEqual(myDomain, domain)) {
					sharedDomain = TRUE;
					break;
				}

				if (CFStringHasSuffix(myDomain, domain)) {
					CFIndex	dotIndex;

					dotIndex = CFStringGetLength(myDomain) - CFStringGetLength(domain) - 1;
					if (dotIndex > 0) {
						UniChar	dot;

						dot = CFStringGetCharacterAtIndex(myDomain, dotIndex);
						if (dot == (UniChar)'.') {
							sharedDomain = TRUE;
							break;
						}
					}
				}
			}
		}

		if (!sharedDomain) {
			// if the default resolver domain name is not shared
			CFArrayRemoveValueAtIndex(supplemental, 0);
		}
	}
#endif	// SPECIAL_HANDLING_FOR_DEFAULT_DOMAIN

	// establish resolver configuration

	n_supplemental = CFArrayGetCount(supplemental);
	if ((defaultResolver == NULL) && (n_supplemental == 0)) {
		/*
		 * if no default or supplemental resolvers
		 */
		if (!_dns_configuration_store(NULL)) {
			SCLog(TRUE, LOG_ERR, CFSTR("dns_configuration_set: could not store configuration"));
		}
	} else {
		dns_create_config_t	_config;

		/*
		 * if default and/or supplemental resolvers are defined
		 */
		_config = _dns_configuration_create();

		// add [default] resolver

		if ((n_supplemental == 0) && myOrderAdded) {
			CFDictionaryRemoveValue(myDefault, kSCPropNetDNSSearchOrder);
		}
		resolver = create_resolver(myDefault);
		_dns_configuration_add_resolver(&_config, resolver);
		_dns_resolver_free(&resolver);

		// add [supplemental] resolvers

		for (i = 0; i < n_supplemental; i++) {
			CFDictionaryRef	supplementalResolver;

			supplementalResolver = CFArrayGetValueAtIndex(supplemental, i);
			resolver = create_resolver(supplementalResolver);
			_dns_configuration_add_resolver(&_config, resolver);
			_dns_resolver_free(&resolver);
		}

#if	!TARGET_OS_IPHONE
		// add flatfile resolvers

		_dnsinfo_flatfile_add_resolvers(&_config);
#endif	// !TARGET_OS_IPHONE

		// save configuration

		if (!_dns_configuration_store(&_config)) {
			SCLog(TRUE, LOG_ERR, CFSTR("dns_configuration_set: could not store configuration"));
		}

		_dns_configuration_free(&_config);
	}

	CFRelease(myDefault);
	CFRelease(supplemental);

	return;
}


#if	!TARGET_OS_IPHONE
static SCDynamicStoreRef	dns_configuration_store;
static SCDynamicStoreCallBack	dns_configuration_callout;

static void
dns_configuration_changed(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
	CFStringRef	key		= CFSTR(_PATH_RESOLVER_DIR);
	CFArrayRef	keys;
	Boolean		resolvers_now;
	static Boolean	resolvers_save	= FALSE;
	struct stat	statbuf;

	resolvers_now = (stat(_PATH_RESOLVER_DIR, &statbuf) == 0);
	if (!resolvers_save && (resolvers_save == resolvers_now)) {
		// if we did not (and still do not) have an "/etc/resolvers"
		// directory than this notification is the result of a change
		// to the "/etc" directory.
		return;
	}
	resolvers_save = resolvers_now;

	SCLog(TRUE, LOG_DEBUG, CFSTR(_PATH_RESOLVER_DIR " changed"));

	// fake a "DNS" change
	keys = CFArrayCreate(NULL, (const void **)&key, 1, &kCFTypeArrayCallBacks);
	(*dns_configuration_callout)(dns_configuration_store, keys, NULL);
	CFRelease(keys);
	return;
}


__private_extern__
void
dns_configuration_monitor(SCDynamicStoreRef store, SCDynamicStoreCallBack callout)
{
	CFMachPortRef		mp;
	mach_port_t		notify_port;
	int			notify_token;
	CFRunLoopSourceRef	rls;
	uint32_t		status;

	dns_configuration_store   = store;
	dns_configuration_callout = callout;

	status = notify_register_mach_port(_PATH_RESOLVER_DIR, &notify_port, 0, &notify_token);
	if (status != NOTIFY_STATUS_OK) {
		SCLOG(NULL, NULL, ASL_LEVEL_ERR, CFSTR("notify_register_mach_port() failed"));
		return;
	}

	status = notify_monitor_file(notify_token, "/private" _PATH_RESOLVER_DIR, 0);
	if (status != NOTIFY_STATUS_OK) {
		SCLOG(NULL, NULL, ASL_LEVEL_ERR, CFSTR("notify_monitor_file() failed"));
		(void)notify_cancel(notify_token);
		return;
	}

	mp = CFMachPortCreateWithPort(NULL, notify_port, dns_configuration_changed, NULL, NULL);
	if (mp == NULL) {
		SCLOG(NULL, NULL, ASL_LEVEL_ERR, CFSTR("CFMachPortCreateWithPort() failed"));
		(void)notify_cancel(notify_token);
		return;
	}

	rls = CFMachPortCreateRunLoopSource(NULL, mp, -1);
	if (rls == NULL) {
		SCLOG(NULL, NULL, ASL_LEVEL_ERR, CFSTR("SCDynamicStoreCreateRunLoopSource() failed"));
		CFRelease(mp);
		(void)notify_cancel(notify_token);
		return;
	}
	CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
	CFRelease(rls);

	CFRelease(mp);
	return;
}
#endif	// !TARGET_OS_IPHONE


__private_extern__
void
dns_configuration_init(CFBundleRef bundle)
{
	CFDictionaryRef	dict;

	dict = CFBundleGetInfoDictionary(bundle);
	if (isA_CFDictionary(dict)) {
		S_mdns_timeout = CFDictionaryGetValue(dict, CFSTR("mdns_timeout"));
		S_mdns_timeout = isA_CFNumber(S_mdns_timeout);

		S_pdns_timeout = CFDictionaryGetValue(dict, CFSTR("pdns_timeout"));
		S_pdns_timeout = isA_CFNumber(S_pdns_timeout);
	}

	return;
}


#ifdef	MAIN
#undef	MAIN

static void
split(const void * key, const void * value, void * context)
{
	CFArrayRef		components;
	CFStringRef		entity_id;
	CFStringRef		service_id;
	CFMutableDictionaryRef	state_dict;

	components = CFStringCreateArrayBySeparatingStrings(NULL, (CFStringRef)key, CFSTR("/"));
	service_id = CFArrayGetValueAtIndex(components, 3);
	entity_id  = CFArrayGetValueAtIndex(components, 4);
	state_dict = CFDictionaryCreateMutable(NULL,
					       0,
					       &kCFTypeDictionaryKeyCallBacks,
					       &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(state_dict, entity_id, (CFDictionaryRef)value);
	CFDictionarySetValue((CFMutableDictionaryRef)context, service_id, state_dict);
	CFRelease(state_dict);
	CFRelease(components);

	return;
}

int
main(int argc, char **argv)
{
	CFDictionaryRef		entities;
	CFStringRef		key;
	CFArrayRef		multicast_resolvers;
	CFStringRef		pattern;
	CFMutableArrayRef	patterns;
	CFStringRef		primary		= NULL;
	CFDictionaryRef		primaryDNS	= NULL;
	CFArrayRef		private_resolvers;
	CFArrayRef		service_order	= NULL;
	CFMutableDictionaryRef	service_state_dict;
	CFDictionaryRef		setup_global_ipv4;
	CFDictionaryRef		state_global_ipv4;
	SCDynamicStoreRef	store;

	_sc_log     = FALSE;
	_sc_verbose = (argc > 1) ? TRUE : FALSE;

	store = SCDynamicStoreCreate(NULL, CFSTR("TEST"), NULL, NULL);

	// get DNS entities
	pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							      kSCDynamicStoreDomainState,
							      kSCCompAnyRegex,
							      kSCEntNetDNS);
	patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFArrayAppendValue(patterns, pattern);
	CFRelease(pattern);
	entities = SCDynamicStoreCopyMultiple(store, NULL, patterns);
	CFRelease(patterns);

	service_state_dict = CFDictionaryCreateMutable(NULL,
						       0,
						       &kCFTypeDictionaryKeyCallBacks,
						       &kCFTypeDictionaryValueCallBacks);
	CFDictionaryApplyFunction(entities, split, service_state_dict);
	CFRelease(entities);

	// get primary service ID
	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							 kSCDynamicStoreDomainState,
							 kSCEntNetIPv4);
	state_global_ipv4 = SCDynamicStoreCopyValue(store, key);
	CFRelease(key);
	if (state_global_ipv4 != NULL) {
		primary = CFDictionaryGetValue(state_global_ipv4, kSCDynamicStorePropNetPrimaryService);
		if (primary != NULL) {
			CFDictionaryRef	service_dict;

			// get DNS configuration for primary service
			service_dict = CFDictionaryGetValue(service_state_dict, primary);
			if (service_dict != NULL) {
				primaryDNS = CFDictionaryGetValue(service_dict, kSCEntNetDNS);
			}
		}
	}

	// get serviceOrder
	key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL,
							 kSCDynamicStoreDomainSetup,
							 kSCEntNetIPv4);
	setup_global_ipv4 = SCDynamicStoreCopyValue(store, key);
	CFRelease(key);
	if (setup_global_ipv4 != NULL) {
		service_order = CFDictionaryGetValue(setup_global_ipv4, kSCPropNetServiceOrder);
	}

	// get multicast resolvers
	key = SCDynamicStoreKeyCreate(NULL, CFSTR("%@/%@/%@"),
				      kSCDynamicStoreDomainState,
				      kSCCompNetwork,
				      CFSTR(kDNSServiceCompMulticastDNS));
	multicast_resolvers = SCDynamicStoreCopyValue(store, key);
	CFRelease(key);

	// get private resolvers
	key = SCDynamicStoreKeyCreate(NULL, CFSTR("%@/%@/%@"),
				      kSCDynamicStoreDomainState,
				      kSCCompNetwork,
				      CFSTR(kDNSServiceCompPrivateDNS));
	private_resolvers = SCDynamicStoreCopyValue(store, key);
	CFRelease(key);

	// update DNS configuration
	dns_configuration_init(CFBundleGetMainBundle());
	dns_configuration_set(primaryDNS,
			      service_state_dict,
			      service_order,
			      multicast_resolvers,
			      private_resolvers);

	// cleanup
	if (setup_global_ipv4 != NULL)	CFRelease(setup_global_ipv4);
	if (state_global_ipv4 != NULL)	CFRelease(state_global_ipv4);
	if (multicast_resolvers != NULL) CFRelease(multicast_resolvers);
	if (private_resolvers != NULL)	CFRelease(private_resolvers);
	CFRelease(service_state_dict);
	CFRelease(store);

	/* not reached */
	exit(0);
	return 0;
}
#endif

