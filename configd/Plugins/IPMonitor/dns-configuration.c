/*
 * Copyright (c) 2004-2012 Apple Inc. All rights reserved.
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
#include <CommonCrypto/CommonDigest.h>

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>

#include "dns-configuration.h"

#include <dnsinfo.h>
#include <dnsinfo_create.h>

#ifdef	MAIN
#undef	MAIN
#include "dnsinfo_copy.c"
#define	MAIN
#endif	// MAIN

#include <dns_sd.h>
#ifndef	kDNSServiceCompMulticastDNS
#define	kDNSServiceCompMulticastDNS	"MulticastDNS"
#endif
#ifndef	kDNSServiceCompPrivateDNS
#define	kDNSServiceCompPrivateDNS	"PrivateDNS"
#endif

#define DNS_CONFIGURATION_FLAGS_KEY	CFSTR("__FLAGS__")
#define DNS_CONFIGURATION_IF_INDEX_KEY	CFSTR("__IF_INDEX__")
#define DNS_CONFIGURATION_ORDER_KEY	CFSTR("__ORDER__")

/* multicast DNS resolver configurations */
static	CFNumberRef	S_mdns_timeout	= NULL;

/* private DNS resolver configurations */
static	CFNumberRef	S_pdns_timeout	= NULL;


static void
add_resolver(CFMutableArrayRef resolvers, CFMutableDictionaryRef resolver)
{
	CFIndex		i;
	CFStringRef	interface;
	CFIndex		n_resolvers;
	CFNumberRef	order;
	uint32_t	order_val	= 0;

	order = CFDictionaryGetValue(resolver, kSCPropNetDNSSearchOrder);
	if (!isA_CFNumber(order) ||
	    !CFNumberGetValue(order, kCFNumberIntType, &order_val)) {
		order     = NULL;
		order_val = 0;
	}

	n_resolvers = CFArrayGetCount(resolvers);
	for (i = 0; i < n_resolvers; i++) {
		CFDictionaryRef		match_resolver;

		match_resolver = CFArrayGetValueAtIndex(resolvers, i);
		if (CFEqual(resolver, match_resolver)) {
			// a real duplicate
			return;
		}

		if (order != NULL) {
			CFMutableDictionaryRef	compare;
			Boolean			match;

			compare = CFDictionaryCreateMutableCopy(NULL, 0, match_resolver);
			CFDictionarySetValue(compare, kSCPropNetDNSSearchOrder, order);
			match = CFEqual(resolver, compare);
			CFRelease(compare);
			if (match) {
				CFNumberRef	match_order;
				uint32_t	match_order_val	= 0;

				// if only the search order's are different
				match_order = CFDictionaryGetValue(match_resolver, kSCPropNetDNSSearchOrder);
				if (!isA_CFNumber(match_order) ||
				    !CFNumberGetValue(match_order, kCFNumberIntType, &match_order_val)) {
					match_order_val = 0;
				}

				if (order_val < match_order_val ) {
					// if we should prefer this match resolver, else just skip it
					CFArraySetValueAtIndex(resolvers, i, resolver);
				}

				return;
			}
		}
	}

	order = CFNumberCreate(NULL, kCFNumberCFIndexType, &n_resolvers);
	CFDictionarySetValue(resolver, DNS_CONFIGURATION_ORDER_KEY, order);
	CFRelease(order);

	interface = CFDictionaryGetValue(resolver, kSCPropInterfaceName);
	if (interface != NULL) {
		uint32_t	flags;
		unsigned int	if_index		= 0;
		char		if_name[IF_NAMESIZE];
		CFNumberRef	num;
		CFBooleanRef	val;

		if (_SC_cfstring_to_cstring(interface,
					    if_name,
					    sizeof(if_name),
					    kCFStringEncodingASCII) != NULL) {
			if_index = if_nametoindex(if_name);
		}

		if ((if_index != 0) &&
		    (
		     // check if this is a "scoped" configuration
		     (CFDictionaryGetValueIfPresent(resolver, DNS_CONFIGURATION_FLAGS_KEY, (const void **)&num) &&
		      isA_CFNumber(num) &&
		      CFNumberGetValue(num, kCFNumberSInt32Type, &flags) &&
		      (flags & DNS_RESOLVER_FLAGS_SCOPED) != 0)
		     ||
		     // check if we should scope all queries with this configuration
		     (CFDictionaryGetValueIfPresent(resolver, DNS_CONFIGURATION_SCOPED_QUERY_KEY, (const void **)&val) &&
		      isA_CFBoolean(val) &&
		      CFBooleanGetValue(val))
		    )
		   ) {
			// if interface index available and it should be used
			num = CFNumberCreate(NULL, kCFNumberIntType, &if_index);
			CFDictionarySetValue(resolver, DNS_CONFIGURATION_IF_INDEX_KEY, num);
			CFRelease(num);
		}
	}

	CFArrayAppendValue(resolvers, resolver);
	return;
}


static void
add_supplemental(CFMutableArrayRef resolvers, CFDictionaryRef dns, uint32_t defaultOrder)
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
	 * the match domains and add each to the resolvers list.
	 */
	for (i = 0; i < n_domains; i++) {
		CFStringRef		match_domain;
		CFNumberRef		match_order;
		CFMutableDictionaryRef	match_resolver;

		match_domain = CFArrayGetValueAtIndex(domains, i);
		if (!isA_CFString(match_domain)) {
			continue;
		}

		match_resolver = CFDictionaryCreateMutableCopy(NULL, 0, dns);

		// set supplemental resolver "domain"
		if (CFStringGetLength(match_domain) > 0) {
			CFDictionarySetValue(match_resolver, kSCPropNetDNSDomainName, match_domain);
		} else {
			CFDictionaryRemoveValue(match_resolver, kSCPropNetDNSDomainName);
		}

		// set supplemental resolver "search_order"
		match_order = (orders != NULL) ? CFArrayGetValueAtIndex(orders, i) : NULL;
		if (isA_CFNumber(match_order)) {
			CFDictionarySetValue(match_resolver, kSCPropNetDNSSearchOrder, match_order);
		} else if (!CFDictionaryContainsKey(match_resolver, kSCPropNetDNSSearchOrder)) {
			CFNumberRef     num;

			num = CFNumberCreate(NULL, kCFNumberIntType, &defaultOrder);
			CFDictionarySetValue(match_resolver, kSCPropNetDNSSearchOrder, num);
			CFRelease(num);

			defaultOrder++;		// if multiple domains, maintain ordering
		}

		// remove keys we don't want in a supplemental resolver
		CFDictionaryRemoveValue(match_resolver, kSCPropNetDNSSupplementalMatchDomains);
		CFDictionaryRemoveValue(match_resolver, kSCPropNetDNSSupplementalMatchOrders);
		CFDictionaryRemoveValue(match_resolver, kSCPropNetDNSSearchDomains);
		CFDictionaryRemoveValue(match_resolver, kSCPropNetDNSSortList);

		add_resolver(resolvers, match_resolver);
		CFRelease(match_resolver);
	}

	return;
}


#define	N_QUICK	32


static void
add_supplemental_resolvers(CFMutableArrayRef resolvers, CFDictionaryRef services, CFArrayRef service_order)
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

		add_supplemental(resolvers, dns, defaultOrder);
	}

	if (keys != keys_q) {
		CFAllocatorDeallocate(NULL, keys);
		CFAllocatorDeallocate(NULL, vals);
	}

	return;
}


static void
add_multicast_resolvers(CFMutableArrayRef resolvers, CFArrayRef multicastResolvers)
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
		domain = _SC_trimDomain(domain);
		if (domain == NULL) {
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
		add_resolver(resolvers, resolver);
		CFRelease(resolver);
		CFRelease(domain);
	}

	return;
}


static void
add_private_resolvers(CFMutableArrayRef resolvers, CFArrayRef privateResolvers)
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
		domain = _SC_trimDomain(domain);
		if (domain == NULL) {
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
		add_resolver(resolvers, resolver);
		CFRelease(resolver);
		CFRelease(domain);
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
		if (CFDictionaryGetValueIfPresent(dns1, DNS_CONFIGURATION_ORDER_KEY, (const void **)&num1) &&
		    CFDictionaryGetValueIfPresent(dns2, DNS_CONFIGURATION_ORDER_KEY, (const void **)&num2) &&
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


static CF_RETURNS_RETAINED CFArrayRef
extract_search_domains(CFMutableDictionaryRef defaultDomain, CFArrayRef supplemental)
{
	CFStringRef		defaultDomainName	= NULL;
	uint32_t		defaultOrder		= DEFAULT_SEARCH_ORDER;
	CFArrayRef		defaultSearchDomains	= NULL;
	CFIndex			defaultSearchIndex	= 0;
	CFIndex			i;
	CFMutableArrayRef	mySearchDomains;
	CFMutableArrayRef	mySupplemental		= NULL;
	CFIndex			n_supplemental;

	mySearchDomains = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	if (defaultDomain != NULL) {
		CFNumberRef	num;

		num = CFDictionaryGetValue(defaultDomain, kSCPropNetDNSSearchOrder);
		if (!isA_CFNumber(num) ||
		    !CFNumberGetValue(num, kCFNumberIntType, &defaultOrder)) {
			defaultOrder = DEFAULT_SEARCH_ORDER;
		}

		defaultDomainName    = CFDictionaryGetValue(defaultDomain, kSCPropNetDNSDomainName);
		defaultSearchDomains = CFDictionaryGetValue(defaultDomain, kSCPropNetDNSSearchDomains);
	}

	// validate the provided "search" domains or move/expand/promote the "domain" name
	if (isA_CFArray(defaultSearchDomains)) {
		CFIndex	n_search;

		n_search = CFArrayGetCount(defaultSearchDomains);
		for (i = 0; i < n_search; i++) {
			CFStringRef	search;

			search = CFArrayGetValueAtIndex(defaultSearchDomains, i);
			search = _SC_trimDomain(search);
			if (search != NULL) {
				CFArrayAppendValue(mySearchDomains, search);
				CFRelease(search);
			}
		}
	} else {
		defaultDomainName = _SC_trimDomain(defaultDomainName);
		if (defaultDomainName != NULL) {
			CFStringRef	defaultOptions;
			char		*domain;
			int		domain_parts	= 1;
			char		*dp;
			int		ndots		= 1;

#define	NDOTS_OPT	"ndots="
#define	NDOTS_OPT_LEN	(sizeof("ndots=") - 1)

			defaultOptions = CFDictionaryGetValue(defaultDomain, kSCPropNetDNSOptions);
			if (defaultOptions != NULL) {
				char	*cp;
				char	*options;

				options = _SC_cfstring_to_cstring(defaultOptions,
								 NULL,
								 0,
								 kCFStringEncodingUTF8);
				cp = strstr(options, NDOTS_OPT);
				if ((cp != NULL) &&
				    ((cp == options) || isspace(cp[-1])) &&
				    ((cp[NDOTS_OPT_LEN] != '\0') && isdigit(cp[NDOTS_OPT_LEN]))) {
					char    *end;
					long    val;

					cp +=  NDOTS_OPT_LEN;
					errno = 0;
					val = strtol(cp, &end, 10);
					if ((*cp != '\0') && (cp != end) && (errno == 0) &&
					    ((*end == '\0') || isspace(*end)) && (val > 0)) {
						ndots = val;
					}
				}
				CFAllocatorDeallocate(NULL, options);
			}

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

			// move "domain" to "search" list (and expand as needed)
			i = LOCALDOMAINPARTS;
			dp = domain;
			do {
				CFStringRef	search;
				CFStringRef	str;

				str = CFStringCreateWithCString(NULL,
								dp,
								kCFStringEncodingUTF8);
				search = _SC_trimDomain(str);
				CFRelease(str);
				if (search != NULL) {
					CFArrayAppendValue(mySearchDomains, search);
					CFRelease(search);
				}

				dp = strchr(dp, '.') + 1;
			} while (++i <= (domain_parts - ndots));
			CFAllocatorDeallocate(NULL, domain);
		}
	}

	// add any supplemental "domain" names to the search list
	n_supplemental = (supplemental != NULL) ? CFArrayGetCount(supplemental) : 0;
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
		supplementalDomain = _SC_trimDomain(supplementalDomain);
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
		} else {
			if (domainIndex == kCFNotFound) {
				// add to the (end of the) search list
				CFArrayAppendValue(mySearchDomains, supplementalDomain);
			}
		}

		CFRelease(supplementalDomain);
	}
	if (mySupplemental != NULL) CFRelease(mySupplemental);

	// update the "search" domains
	if (CFArrayGetCount(mySearchDomains) == 0) {
		CFRelease(mySearchDomains);
		mySearchDomains = NULL;
	}

	// remove the "domain" name and "search" list
	CFDictionaryRemoveValue(defaultDomain, kSCPropNetDNSDomainName);
	CFDictionaryRemoveValue(defaultDomain, kSCPropNetDNSSearchDomains);

	return mySearchDomains;
}


static void
add_scoped_resolvers(CFMutableArrayRef scoped, CFDictionaryRef services, CFArrayRef service_order)
{
	const void *		keys_q[N_QUICK];
	const void **		keys	= keys_q;
	CFIndex			i;
	CFIndex			n_order;
	CFIndex			n_services;
	CFMutableArrayRef	order;
	CFMutableSetRef		seen;

	n_services = isA_CFDictionary(services) ? CFDictionaryGetCount(services) : 0;
	if (n_services == 0) {
		return;		// if no services
	}

	// ensure that we process all services in order

	n_order = isA_CFArray(service_order) ? CFArrayGetCount(service_order) : 0;
	if (n_order > 0) {
		order = CFArrayCreateMutableCopy(NULL, 0, service_order);
	} else{
		order = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}

	if (n_services > (CFIndex)(sizeof(keys_q) / sizeof(CFTypeRef))) {
		keys = CFAllocatorAllocate(NULL, n_services * sizeof(CFTypeRef), 0);
	}
	CFDictionaryGetKeysAndValues(services, keys, NULL);
	for (i = 0; i < n_services; i++) {
		CFStringRef	serviceID = (CFStringRef)keys[i];

		if (!CFArrayContainsValue(order, CFRangeMake(0, n_order), serviceID)) {
			CFArrayAppendValue(order, serviceID);
			n_order++;
		}
	}
	if (keys != keys_q) {
		CFAllocatorDeallocate(NULL, keys);
	}

	// iterate over services

	seen = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
	for (i = 0; i < n_order; i++) {
		CFDictionaryRef		dns;
		uint32_t		flags;
		char			if_name[IF_NAMESIZE];
		CFStringRef		interface;
		CFMutableDictionaryRef	newDNS;
		CFNumberRef		num;
		CFArrayRef		searchDomains;
		CFDictionaryRef		service;
		CFStringRef		serviceID;

		serviceID = CFArrayGetValueAtIndex(order, i);
		service = CFDictionaryGetValue(services, serviceID);
		if (!isA_CFDictionary(service)) {
			// if no service
			continue;
		}

		dns = CFDictionaryGetValue(service, kSCEntNetDNS);
		if (!isA_CFDictionary(dns)) {
			// if no DNS
			continue;
		}

		interface = CFDictionaryGetValue(dns, kSCPropInterfaceName);
		if (interface == NULL) {
			// if no [scoped] interface
			continue;
		}
		if (CFSetContainsValue(seen, interface)) {
			// if we've already processed this [scoped] interface
			continue;
		}
		CFSetSetValue(seen, interface);

		if ((_SC_cfstring_to_cstring(interface,
					     if_name,
					     sizeof(if_name),
					     kCFStringEncodingASCII) == NULL) ||
		    (if_nametoindex(if_name) == 0)) {
			// if interface index not available
			continue;
		}

		// add [scoped] resolver entry
		newDNS = CFDictionaryCreateMutableCopy(NULL, 0, dns);

		// set search list
		searchDomains = extract_search_domains(newDNS, NULL);
		if (searchDomains != NULL) {
			CFDictionarySetValue(newDNS, kSCPropNetDNSSearchDomains, searchDomains);
			CFRelease(searchDomains);
		}

		// set "scoped" configuration flag(s)
		if (!CFDictionaryGetValueIfPresent(newDNS, DNS_CONFIGURATION_FLAGS_KEY, (const void **)&num) ||
		    !isA_CFNumber(num) ||
		    !CFNumberGetValue(num, kCFNumberSInt32Type, &flags)) {
			flags = 0;
		}
		flags |= DNS_RESOLVER_FLAGS_SCOPED;
		num = CFNumberCreate(NULL, kCFNumberSInt32Type, &flags);
		CFDictionarySetValue(newDNS, DNS_CONFIGURATION_FLAGS_KEY, num);
		CFRelease(num);

		// remove keys we don't want in a [scoped] resolver
		CFDictionaryRemoveValue(newDNS, kSCPropNetDNSSupplementalMatchDomains);
		CFDictionaryRemoveValue(newDNS, kSCPropNetDNSSupplementalMatchOrders);

		add_resolver(scoped, newDNS);
		CFRelease(newDNS);
	}

	CFRelease(seen);
	CFRelease(order);
	return;
}


static void
add_default_resolver(CFMutableArrayRef	resolvers,
		     CFDictionaryRef	defaultResolver,
		     Boolean		*orderAdded,
		     CFArrayRef		*searchDomains)
{
	CFMutableDictionaryRef	myDefault;
	uint32_t		myOrder	= DEFAULT_SEARCH_ORDER;
	CFNumberRef		order;

	if (defaultResolver == NULL) {
		myDefault = CFDictionaryCreateMutable(NULL,
						      0,
						      &kCFTypeDictionaryKeyCallBacks,
						      &kCFTypeDictionaryValueCallBacks);
	} else {
		myDefault = CFDictionaryCreateMutableCopy(NULL, 0, defaultResolver);
	}

	// ensure that the default resolver has a search order

	order = CFDictionaryGetValue(myDefault, kSCPropNetDNSSearchOrder);
	if (!isA_CFNumber(order) ||
	    !CFNumberGetValue(order, kCFNumberIntType, &myOrder)) {
		myOrder = DEFAULT_SEARCH_ORDER;
		order = CFNumberCreate(NULL, kCFNumberIntType, &myOrder);
		CFDictionarySetValue(myDefault, kSCPropNetDNSSearchOrder, order);
		CFRelease(order);
		*orderAdded = TRUE;
	}

	// extract the "search" domain list for the default resolver (and
	// any supplemental resolvers)

	*searchDomains = extract_search_domains(myDefault, resolvers);

	// add the default resolver

	add_resolver(resolvers, myDefault);
	CFRelease(myDefault);
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


static dns_create_resolver_t
create_resolver(CFDictionaryRef dns)
{
	CFArrayRef		list;
	CFNumberRef		num;
	dns_create_resolver_t	_resolver;
	CFStringRef		str;
	CFMutableArrayRef	serverAddresses		= NULL;
	CFStringRef		targetInterface		= NULL;
	unsigned int		targetInterfaceIndex	= 0;

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

	// process interface index
	num = CFDictionaryGetValue(dns, DNS_CONFIGURATION_IF_INDEX_KEY);
	if (isA_CFNumber(num)) {
		int	if_index;

		if (CFNumberGetValue(num, kCFNumberIntType, &if_index)) {
			char	if_name[IFNAMSIZ];

			_dns_resolver_set_if_index(&_resolver, if_index);

			if ((if_index != 0) &&
			    (if_indextoname(if_index, if_name) != NULL)) {
				targetInterface = CFStringCreateWithCString(NULL,
									    if_name,
									    kCFStringEncodingASCII);
				targetInterfaceIndex = if_index;
			}
		}
	}

	// process nameserver addresses
	list = CFDictionaryGetValue(dns, kSCPropNetDNSServerAddresses);
	if (isA_CFArray(list)) {
		CFIndex	i;
		CFIndex	n	= CFArrayGetCount(list);

		serverAddresses = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

		for (i = 0; i < n; i++) {
			union {
				struct sockaddr         sa;
				struct sockaddr_in      sin;
				struct sockaddr_in6     sin6;
			} addr;
			char				buf[64];
			CFDataRef			serverAddress;

			str = CFArrayGetValueAtIndex(list, i);
			if (!isA_CFString(str)) {
				continue;
			}

			if (_SC_cfstring_to_cstring(str, buf, sizeof(buf), kCFStringEncodingASCII) == NULL) {
				continue;
			}

			if (_SC_string_to_sockaddr(buf, AF_UNSPEC, (void *)&addr, sizeof(addr)) == NULL) {
				continue;
			}

			if ((addr.sa.sa_family == AF_INET6) &&
			    (IN6_IS_ADDR_LINKLOCAL(&addr.sin6.sin6_addr) ||
			     IN6_IS_ADDR_MC_LINKLOCAL(&addr.sin6.sin6_addr)) &&
			    (addr.sin6.sin6_scope_id == 0) &&
			    (targetInterfaceIndex != 0)) {
				// for link local [IPv6] addresses, if the scope id is not
				// set then we should use the interface associated with the
				// resolver configuration
				addr.sin6.sin6_scope_id = targetInterfaceIndex;
			}

			_dns_resolver_add_nameserver(&_resolver, &addr.sa);

			serverAddress = CFDataCreate(NULL, (const void *)&addr.sa, addr.sa.sa_len);
			CFArrayAppendValue(serverAddresses, serverAddress);
			CFRelease(serverAddress);
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

	// process flags
	num = CFDictionaryGetValue(dns, DNS_CONFIGURATION_FLAGS_KEY);
	if (isA_CFNumber(num)) {
		uint32_t	flags;

		if (CFNumberGetValue(num, kCFNumberSInt32Type, &flags)) {
			_dns_resolver_set_flags(&_resolver, flags);
		}
	}

	if (serverAddresses != NULL) {
		SCNetworkReachabilityFlags	flags		= kSCNetworkReachabilityFlagsReachable;
		CFIndex				i;
		CFIndex				n		= CFArrayGetCount(serverAddresses);
		CFMutableDictionaryRef		targetOptions;

		targetOptions = CFDictionaryCreateMutable(NULL,
							  0,
							  &kCFTypeDictionaryKeyCallBacks,
							  &kCFTypeDictionaryValueCallBacks);
		CFDictionarySetValue(targetOptions,
				     kSCNetworkReachabilityOptionServerBypass,
				     kCFBooleanTrue);
		if (targetInterface != NULL) {
			CFDictionarySetValue(targetOptions,
					     kSCNetworkReachabilityOptionInterface,
					     targetInterface);
		}

		for (i = 0; i < n; i++) {
			SCNetworkReachabilityFlags	ns_flags;
			Boolean				ok;
			CFDataRef			serverAddress;
			SCNetworkReachabilityRef	target;

			serverAddress = CFArrayGetValueAtIndex(serverAddresses, i);
			CFDictionarySetValue(targetOptions,
					     kSCNetworkReachabilityOptionRemoteAddress,
					     serverAddress);
			target = SCNetworkReachabilityCreateWithOptions(NULL, targetOptions);
			if (target == NULL) {
				CFDictionaryRemoveValue(targetOptions, kSCNetworkReachabilityOptionInterface);
				target = SCNetworkReachabilityCreateWithOptions(NULL, targetOptions);
				if (target != NULL) {
					// if interface name not (no longer) valid
					CFRelease(target);
					flags = 0;
					break;
				}

				// address not valid?
				SCLog(TRUE, LOG_ERR,
				      CFSTR("create_resolver SCNetworkReachabilityCreateWithOptions() failed:\n  options = %@"),
				      targetOptions);
				break;
			}

			ok = SCNetworkReachabilityGetFlags(target, &ns_flags);
			CFRelease(target);
			if (!ok) {
				break;
			}

			if ((i == 0) ||
			    (rankReachability(ns_flags) < rankReachability(flags))) {
				/* return the worst case result */
				flags = ns_flags;
			}
		}

		_dns_resolver_set_reach_flags(&_resolver, flags);

		CFRelease(targetOptions);
		CFRelease(serverAddresses);
	}

	if (targetInterface != NULL) {
		CFRelease(targetInterface);
	}

	return _resolver;
}


static __inline__ Boolean
isScopedConfiguration(CFDictionaryRef dns)
{
	uint32_t	flags;
	CFNumberRef	num;

	if ((dns != NULL) &&
	    CFDictionaryGetValueIfPresent(dns, DNS_CONFIGURATION_FLAGS_KEY, (const void **)&num) &&
	    (num != NULL) &&
	    CFNumberGetValue(num, kCFNumberSInt32Type, &flags) &&
	    ((flags & DNS_RESOLVER_FLAGS_SCOPED) != 0)) {
		return TRUE;
	}

	return FALSE;
}


static CFComparisonResult
compareDomain(const void *val1, const void *val2, void *context)
{
	CFDictionaryRef		dns1	= (CFDictionaryRef)val1;
	CFDictionaryRef		dns2	= (CFDictionaryRef)val2;
	CFStringRef		domain1;
	CFStringRef		domain2;
	CFArrayRef		labels1	= NULL;
	CFArrayRef		labels2	= NULL;
	CFIndex			n1;
	CFIndex			n2;
	CFComparisonResult	result;
	Boolean			rev1;
	Boolean			rev2;
	Boolean			scoped1;
	Boolean			scoped2;

	// "default" domains sort before "supplemental" domains
	domain1 = CFDictionaryGetValue(dns1, kSCPropNetDNSDomainName);
	domain2 = CFDictionaryGetValue(dns2, kSCPropNetDNSDomainName);
	if (domain1 == NULL) {
		return kCFCompareLessThan;
	} else if (domain2 == NULL) {
		return kCFCompareGreaterThan;
	}

	// sort non-scoped before scoped
	scoped1 = isScopedConfiguration(dns1);
	scoped2 = isScopedConfiguration(dns2);
	if (scoped1 != scoped2) {
		if (!scoped1) {
			return kCFCompareLessThan;
		} else {
			return kCFCompareGreaterThan;
		}
	}

	// must have domain names for any further comparisons
	if ((domain1 == NULL) || (domain2 == NULL)) {
		return kCFCompareEqualTo;
	}

	// forward (A, AAAA) domains sort before reverse (PTR) domains
	rev1 = CFStringHasSuffix(domain1, CFSTR(".arpa"));
	rev2 = CFStringHasSuffix(domain2, CFSTR(".arpa"));
	if (rev1 != rev2) {
		if (rev1) {
			return kCFCompareGreaterThan;
		} else {
			return kCFCompareLessThan;
		}
	}

	labels1 = CFStringCreateArrayBySeparatingStrings(NULL, domain1, CFSTR("."));
	n1 = CFArrayGetCount(labels1);

	labels2 = CFStringCreateArrayBySeparatingStrings(NULL, domain2, CFSTR("."));
	n2 = CFArrayGetCount(labels2);

	while ((n1 > 0) && (n2 > 0)) {
		CFStringRef	label1	= CFArrayGetValueAtIndex(labels1, --n1);
		CFStringRef	label2	= CFArrayGetValueAtIndex(labels2, --n2);

		// compare domain labels
		result = CFStringCompare(label1, label2, kCFCompareCaseInsensitive);
		if (result != kCFCompareEqualTo) {
			goto done;
		}
	}

	// longer labels (corp.apple.com) sort before shorter labels (apple.com)
	if (n1 > n2) {
		result = kCFCompareLessThan;
		goto done;
	} else if (n1 < n2) {
		result = kCFCompareGreaterThan;
		goto done;
	}

	// sort by search order
	result = compareBySearchOrder(val1, val2, context);

    done :

	if (labels1 != NULL) CFRelease(labels1);
	if (labels2 != NULL) CFRelease(labels2);
	return result;
}


__private_extern__
Boolean
dns_configuration_set(CFDictionaryRef   defaultResolver,
		      CFDictionaryRef   services,
		      CFArrayRef	serviceOrder,
		      CFArrayRef	multicastResolvers,
		      CFArrayRef	privateResolvers)
{
	dns_create_config_t	_config;
	Boolean			changed		= FALSE;
	CFIndex			i;
	CFMutableDictionaryRef	myDefault;
	Boolean			myOrderAdded	= FALSE;
	CFArrayRef		mySearchDomains	= NULL;
	CFIndex			n_resolvers;
	CFMutableArrayRef	resolvers;
	unsigned char		signature[CC_SHA1_DIGEST_LENGTH];
	static unsigned char	signature_last[CC_SHA1_DIGEST_LENGTH];

	// establish list of resolvers

	resolvers = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	// collect (and add) any "supplemental" resolver configurations

	add_supplemental_resolvers(resolvers, services, serviceOrder);

	// collect (and add) any "private" resolver configurations

	add_private_resolvers(resolvers, privateResolvers);

	// add the "default" resolver

	add_default_resolver(resolvers, defaultResolver, &myOrderAdded, &mySearchDomains);

	// collect (and add) any "multicast" resolver configurations

	add_multicast_resolvers(resolvers, multicastResolvers);

	// collect (and add) any "scoped" resolver configurations

	add_scoped_resolvers(resolvers, services, serviceOrder);

	// sort resolvers

	n_resolvers = CFArrayGetCount(resolvers);
	if (n_resolvers > 1) {
		CFArraySortValues(resolvers, CFRangeMake(0, n_resolvers), compareDomain, NULL);
	}

	// cleanup

	for (i = n_resolvers; --i > 0; ) {
		CFDictionaryRef	resolver;

		resolver = CFArrayGetValueAtIndex(resolvers, i);
		if (!CFDictionaryContainsKey(resolver, kSCPropNetDNSDomainName) &&
		    !CFDictionaryContainsKey(resolver, kSCPropNetDNSSearchDomains) &&
		    !CFDictionaryContainsKey(resolver, kSCPropNetDNSServerAddresses)) {
			// remove empty resolver
			CFArrayRemoveValueAtIndex(resolvers, i);
			n_resolvers--;
		}
	}

	// update the default resolver

	myDefault = CFDictionaryCreateMutableCopy(NULL,
						  0,
						  CFArrayGetValueAtIndex(resolvers, 0));
	if (mySearchDomains != NULL) {
		// add search domains to the default resolver
		CFDictionarySetValue(myDefault, kSCPropNetDNSSearchDomains, mySearchDomains);
		CFRelease(mySearchDomains);
	}
	if (myOrderAdded && (n_resolvers > 1)) {
		CFDictionaryRef	resolver;

		resolver = CFArrayGetValueAtIndex(resolvers, 1);
		if (CFDictionaryContainsKey(resolver, kSCPropNetDNSDomainName) ||
		    isScopedConfiguration(resolver)) {
			// if not a supplemental "default" resolver (a domain name is
			// present) or if it's a scoped configuration
			CFDictionaryRemoveValue(myDefault, kSCPropNetDNSSearchOrder);
		}
	}
	CFArraySetValueAtIndex(resolvers, 0, myDefault);
	CFRelease(myDefault);

	// establish resolver configuration

	if ((defaultResolver == NULL) && (n_resolvers <= 1)) {
		/*
		 * if no default and no supplemental/scoped resolvers
		 */
		_config = NULL;
	} else {
		/*
		 * if default and/or supplemental/scoped resolvers are defined
		 */
		_config = _dns_configuration_create();

		// add resolvers

		for (i = 0; i < n_resolvers; i++) {
			CFDictionaryRef		resolver;
			dns_create_resolver_t	_resolver;

			resolver = CFArrayGetValueAtIndex(resolvers, i);
			_resolver = create_resolver(resolver);
			_dns_configuration_add_resolver(&_config, _resolver);
			_dns_resolver_free(&_resolver);
		}

#if	!TARGET_OS_IPHONE
		// add flatfile resolvers

		_dnsinfo_flatfile_add_resolvers(&_config);
#endif	// !TARGET_OS_IPHONE
	}

	// check if the configuration changed
	_dns_configuration_signature(&_config, signature, sizeof(signature));
	if (bcmp(signature, signature_last, sizeof(signature)) != 0) {
		changed = TRUE;
	}
	bcopy(signature, signature_last, sizeof(signature));

	// save configuration
	if (!_dns_configuration_store(&_config)) {
		SCLog(TRUE, LOG_ERR, CFSTR("dns_configuration_set: could not store configuration"));
	}
	if (_config != NULL) _dns_configuration_free(&_config);

	CFRelease(resolvers);
	return changed;
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

	mp = _SC_CFMachPortCreateWithPort("IPMonitor/dns_configuration",
					  notify_port,
					  dns_configuration_changed,
					  NULL);

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


#pragma mark -
#pragma mark Standalone test code


#ifdef	MAIN

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
	state_dict = (CFMutableDictionaryRef)CFDictionaryGetValue(context, service_id);
	if (state_dict != NULL) {
		state_dict = CFDictionaryCreateMutableCopy(NULL, 0, state_dict);
	} else {
		state_dict = CFDictionaryCreateMutable(NULL,
						       0,
						       &kCFTypeDictionaryKeyCallBacks,
						       &kCFTypeDictionaryValueCallBacks);
	}

	if (CFEqual(entity_id, kSCEntNetIPv4) ||
	    CFEqual(entity_id, kSCEntNetIPv6)) {
		CFStringRef	interface;

		interface = CFDictionaryGetValue((CFDictionaryRef)value, kSCPropInterfaceName);
		if (interface != NULL) {
			CFDictionaryRef		dns;
			CFMutableDictionaryRef	new_dns;

			dns = CFDictionaryGetValue(state_dict, kSCEntNetDNS);
			if (dns != NULL) {
				new_dns = CFDictionaryCreateMutableCopy(NULL, 0, dns);
			} else {
				new_dns = CFDictionaryCreateMutable(NULL,
								0,
								&kCFTypeDictionaryKeyCallBacks,
								&kCFTypeDictionaryValueCallBacks);
			}
			CFDictionarySetValue(new_dns, kSCPropInterfaceName, interface);
			CFDictionarySetValue(state_dict, kSCEntNetDNS, new_dns);
			CFRelease(new_dns);
		}
	} else if (CFEqual(entity_id, kSCEntNetDNS)) {
		CFDictionaryRef	dns;

		dns = CFDictionaryGetValue(state_dict, kSCEntNetDNS);
		if (dns != NULL) {
			CFStringRef	interface;

			interface = CFDictionaryGetValue(dns, kSCPropInterfaceName);
			if (interface != NULL) {
				CFMutableDictionaryRef	new_dns;

				new_dns = CFDictionaryCreateMutableCopy(NULL, 0, (CFDictionaryRef)value);
				CFDictionarySetValue(new_dns, kSCPropInterfaceName, interface);
				CFDictionarySetValue(state_dict, kSCEntNetDNS, new_dns);
				CFRelease(new_dns);
			} else {
				CFDictionarySetValue(state_dict, kSCEntNetDNS, (CFDictionaryRef)value);
			}
		} else {
			CFDictionarySetValue(state_dict, kSCEntNetDNS, (CFDictionaryRef)value);
		}
	} else {
		CFDictionarySetValue(state_dict, entity_id, (CFDictionaryRef)value);
	}

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

	// get IPv4, IPv6, and DNS entities
	patterns = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							      kSCDynamicStoreDomainState,
							      kSCCompAnyRegex,
							      kSCEntNetIPv4);
	CFArrayAppendValue(patterns, pattern);
	CFRelease(pattern);
	pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							      kSCDynamicStoreDomainState,
							      kSCCompAnyRegex,
							      kSCEntNetIPv6);
	CFArrayAppendValue(patterns, pattern);
	CFRelease(pattern);
	pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(NULL,
							      kSCDynamicStoreDomainState,
							      kSCCompAnyRegex,
							      kSCEntNetDNS);
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
	(void)dns_configuration_set(primaryDNS,
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

