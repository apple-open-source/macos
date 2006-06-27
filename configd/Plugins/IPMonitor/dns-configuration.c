/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>

#include <dnsinfo.h>
#include <dnsinfo_create.h>


/* pre-defined (supplemental) resolver configurations */
static  CFArrayRef      S_predefined  = NULL;


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
		CFIndex			j;
		CFStringRef		match_domain;
		CFNumberRef		match_order;
		uint32_t		match_order_val	= 0;
		CFMutableDictionaryRef	match_resolver;
		CFIndex			n_supplemental;

		match_domain = CFArrayGetValueAtIndex(domains, i);
		if (!isA_CFString(match_domain)) {
			continue;
		}

		match_order = (orders != NULL) ? CFArrayGetValueAtIndex(orders, i) : NULL;

		match_resolver = CFDictionaryCreateMutableCopy(NULL, 0, dns);
		CFDictionaryRemoveValue(match_resolver, kSCPropNetDNSSupplementalMatchDomains);
		CFDictionaryRemoveValue(match_resolver, kSCPropNetDNSSupplementalMatchOrders);
		CFDictionaryRemoveValue(match_resolver, kSCPropNetDNSSearchDomains);
		CFDictionarySetValue(match_resolver, kSCPropNetDNSDomainName, match_domain);
		if (isA_CFNumber(match_order)) {
			CFDictionarySetValue(match_resolver, kSCPropNetDNSSearchOrder, match_order);
		} else if (!CFDictionaryContainsKey(match_resolver, kSCPropNetDNSSearchOrder)) {
			CFNumberRef     num;

			num = CFNumberCreate(NULL, kCFNumberIntType, &defaultOrder);
			CFDictionarySetValue(match_resolver, kSCPropNetDNSSearchOrder, num);
			CFRelease(num);

			defaultOrder++;		// if multiple domains, maintain ordering
		}

		match_order = CFDictionaryGetValue(match_resolver, kSCPropNetDNSSearchOrder);
		if (!isA_CFNumber(match_order) ||
		    !CFNumberGetValue(match_order, kCFNumberIntType, &match_order_val)) {
			match_order     = NULL;
			match_order_val = 0;
		}

		n_supplemental = CFArrayGetCount(supplemental);
		for (j = 0; j < n_supplemental; j++) {
			CFMutableDictionaryRef	compare;
			Boolean			match;
			CFDictionaryRef		supplemental_resolver;

			supplemental_resolver = CFArrayGetValueAtIndex(supplemental, j);
			if (CFEqual(match_resolver, supplemental_resolver)) {
				// a real duplicate
				CFRelease(match_resolver);
				match_resolver = NULL;
				break;
			}

			compare = CFDictionaryCreateMutableCopy(NULL, 0, supplemental_resolver);
			if (match_order != NULL) {
				CFDictionarySetValue(compare, kSCPropNetDNSSearchOrder, match_order);
			}
			match = CFEqual(match_resolver, compare);
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

				if (match_order_val < supplemental_order_val ) {
					// if we should prefer this match resolver, else just skip it
					CFArraySetValueAtIndex(supplemental, j, match_resolver);
				}

				CFRelease(match_resolver);
				match_resolver = NULL;
				break;
			}
		}

		if (match_resolver != NULL) {
			CFArrayAppendValue(supplemental, match_resolver);
			CFRelease(match_resolver);
		}
	}

	return;
}


static void
add_predefined_resolvers(CFMutableArrayRef supplemental)
{
	CFIndex i;
	CFIndex n;

	if (S_predefined == NULL) {
		return;
	}

	n = CFArrayGetCount(S_predefined);
	for (i = 0; i < n; i++) {
		uint32_t	defaultOrder;
		CFDictionaryRef	dns;

		dns = CFArrayGetValueAtIndex(S_predefined, i);
		if (!isA_CFDictionary(dns)) {
			continue;
		}

		defaultOrder = DEFAULT_SEARCH_ORDER +
			       (DEFAULT_SEARCH_ORDER / 2) +
			       ((DEFAULT_SEARCH_ORDER / 1000) * i);
		add_supplemental(supplemental, dns, defaultOrder);
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

		defaultOrder = DEFAULT_SEARCH_ORDER -
			       (DEFAULT_SEARCH_ORDER / 2) +
			       ((DEFAULT_SEARCH_ORDER / 1000) * i);
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


static CFComparisonResult
compareBySearchOrder(const void *val1, const void *val2, void *context)
{
	CFDictionaryRef	dns1	= (CFDictionaryRef)val1;
	CFDictionaryRef	dns2	= (CFDictionaryRef)val2;
	CFNumberRef	num;
	uint32_t	order1	= DEFAULT_SEARCH_ORDER;
	uint32_t	order2	= DEFAULT_SEARCH_ORDER;

	num = CFDictionaryGetValue(dns1, kSCPropNetDNSSearchOrder);
	if (!isA_CFNumber(num) ||
	    !CFNumberGetValue(num, kCFNumberIntType, &order1)) {
		order1 = DEFAULT_SEARCH_ORDER;
	}

	num = CFDictionaryGetValue(dns2, kSCPropNetDNSSearchOrder);
	if (!isA_CFNumber(num) ||
	    !CFNumberGetValue(num, kCFNumberIntType, &order2)) {
		order2 = DEFAULT_SEARCH_ORDER;
	}

	if (order1 == order2) {
		return kCFCompareEqualTo;
	}

	return (order1 < order2) ? kCFCompareLessThan : kCFCompareGreaterThan;
}


static CFStringRef
trimDomain(CFStringRef domain)
{
	CFIndex	length;
	CFRange	range;
	Boolean	trimmed	= FALSE;

	if (!isA_CFString(domain)) {
		return NULL;
	}

	// remove trailing dots
	length = CFStringGetLength(domain);
	while (CFStringFindWithOptions(domain,
				       CFSTR("."),
				       CFRangeMake(0, length),
				       kCFCompareAnchored|kCFCompareBackwards,
				       &range)) {
		trimmed = TRUE;
		length = range.location;
	}

	if (length == 0) {
		return NULL;
	}

	if (trimmed) {
		domain = CFStringCreateWithSubstring(NULL, domain, CFRangeMake(0, length));
	} else {
		CFRetain(domain);
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
	CFMutableArrayRef	mySupplemental		= (CFMutableArrayRef)supplemental;
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
	}

	for (i = 0; i < n_supplemental; i++) {
		CFDictionaryRef dns;
		CFIndex		domainIndex;
		CFNumberRef	num;
		CFStringRef	supplementalDomain;
		uint32_t	supplementalOrder;

		dns = CFArrayGetValueAtIndex(mySupplemental, i);

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
	if (mySupplemental != supplemental) CFRelease(mySupplemental);
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
	if (isA_CFString(str)) {
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
			if (isA_CFString(str)) {
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
		      CFArrayRef	serviceOrder)
{
	CFIndex			i;
	CFMutableDictionaryRef	myDefault;
	CFStringRef		myDomain	= NULL;
	uint32_t		myOrder		= DEFAULT_SEARCH_ORDER;
	Boolean			myOrderAdded	= FALSE;
	CFIndex			n_supplemental;
	CFNumberRef		order;
	dns_create_resolver_t	resolver;
	CFArrayRef		search;
	CFMutableArrayRef	supplemental;

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
		CFArrayAppendValue(supplemental, mySupplemental);
		CFRelease(mySupplemental);
	}

	// collect (and add) any supplemental resolver configurations

	add_supplemental_resolvers(supplemental, services, serviceOrder);

	// update the "search" list

	update_search_domains(&myDefault, supplemental);

	// add any pre-defined resolver configurations

	add_predefined_resolvers(supplemental);

	// check if the "match for default domain" (above) is really needed

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

	// establish resolver configuration

	n_supplemental = CFArrayGetCount(supplemental);
	if ((defaultResolver == NULL) && (n_supplemental == 0)) {
		/*
		 * if no default or supplemental resolvers
		 */
		if (!_dns_configuration_store(NULL)) {
			SCLog(TRUE, LOG_ERR, CFSTR("set_dns_configuration: could not store configuration"));
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

		// save configuration

		if (!_dns_configuration_store(&_config)) {
			SCLog(TRUE, LOG_ERR, CFSTR("set_dns_configuration() failed: could not store configuration"));
		}

		_dns_configuration_free(&_config);
	}

	CFRelease(myDefault);
	CFRelease(supplemental);

	return;
}


static void
load_predefined_resolvers(CFBundleRef bundle)
{
	Boolean			ok;
	CFURLRef		url;
	CFStringRef		xmlError	= NULL;
	CFDataRef		xmlResolvers	= NULL;

	url = CFBundleCopyResourceURL(bundle, CFSTR("Resolvers"), CFSTR("plist"), NULL);
	if (url == NULL) {
		return;
	}

	ok = CFURLCreateDataAndPropertiesFromResource(NULL, url, &xmlResolvers, NULL, NULL, NULL);
	CFRelease(url);
	if (!ok || (xmlResolvers == NULL)) {
		return;
	}

	/* convert the XML data into a property list */
	S_predefined = CFPropertyListCreateFromXMLData(NULL, xmlResolvers, kCFPropertyListImmutable, &xmlError);
	CFRelease(xmlResolvers);
	if (S_predefined == NULL) {
		if (xmlError != NULL) {
			SCLog(TRUE, LOG_DEBUG, CFSTR("add_predefined_resolvers: %@"), xmlError);
			CFRelease(xmlError);
		}
		return;
	}

	if (!isA_CFArray(S_predefined)) {
		CFRelease(S_predefined);
		S_predefined = NULL;
		return;
	}

	return;
}


__private_extern__
void
dns_configuration_init(CFBundleRef bundle)
{
	load_predefined_resolvers(bundle);
	return;
}

