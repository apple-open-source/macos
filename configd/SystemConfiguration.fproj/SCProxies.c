/*
 * Copyright (c) 2000-2004, 2006-2011 Apple Inc. All rights reserved.
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
 * May 18, 2001			Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <TargetConditionals.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>

#include <netdb.h>




CFStringRef
SCDynamicStoreKeyCreateProxies(CFAllocatorRef allocator)
{
	return SCDynamicStoreKeyCreateNetworkGlobalEntity(allocator,
							  kSCDynamicStoreDomainState,
							  kSCEntNetProxies);
}


static void
validate_proxy_content(CFMutableDictionaryRef	proxies,
		       CFStringRef		proxy_enable,
		       CFStringRef		proxy_host,
		       CFStringRef		proxy_port,
		       const char *		proxy_service,
		       int			proxy_defaultport)
{
	int		enabled	= 0;
	CFNumberRef	num;

	num = CFDictionaryGetValue(proxies, proxy_enable);
	if (num != NULL) {
		if (!isA_CFNumber(num) ||
		    !CFNumberGetValue(num, kCFNumberIntType, &enabled)) {
			// if we don't like the enabled key/value
			goto disable;
		}
	}

	if (proxy_host != NULL) {
		CFStringRef	host;

		host = CFDictionaryGetValue(proxies, proxy_host);
		if (((enabled == 0) && (host != NULL)) ||
		    ((enabled != 0) && !isA_CFString(host))) {
			// pass only valid proxy hosts and only when enabled
			goto disable;
		}
	}

	if (proxy_port != NULL) {
		CFNumberRef	port;

		port = CFDictionaryGetValue(proxies, proxy_port);
		if (((enabled == 0) && (port != NULL)) ||
		    ((enabled != 0) && (port != NULL) && !isA_CFNumber(port))) {
			// pass only provided/valid proxy ports and only when enabled
			goto disable;
		}

		if ((enabled != 0) && (port == NULL)) {
			struct servent	*service;
			int		s_port;

			service = getservbyname(proxy_service, "tcp");
			if (service != NULL) {
				s_port = ntohs(service->s_port);
			} else {
				s_port = proxy_defaultport;
			}
			num = CFNumberCreate(NULL, kCFNumberIntType, &s_port);
			CFDictionarySetValue(proxies, proxy_port, num);
			CFRelease(num);
		}
	}

	return;

    disable :

	enabled = 0;
	num = CFNumberCreate(NULL, kCFNumberIntType, &enabled);
	CFDictionarySetValue(proxies, proxy_enable, num);
	CFRelease(num);
	if (proxy_host != NULL) {
		CFDictionaryRemoveValue(proxies, proxy_host);
	}
	if (proxy_port != NULL) {
		CFDictionaryRemoveValue(proxies, proxy_port);
	}

	return;
}


static void
normalize_scoped_proxy(const void *key, const void *value, void *context);


static void
normalize_supplemental_proxy(const void *value, void *context);


static CF_RETURNS_RETAINED CFDictionaryRef
__SCNetworkProxiesCopyNormalized(CFDictionaryRef proxy)
{
	CFArrayRef		array;
	CFMutableDictionaryRef	newProxy;
	CFNumberRef		num;
	CFDictionaryRef		scoped;
	CFArrayRef		supplemental;

	if (!isA_CFDictionary(proxy)) {
		proxy = CFDictionaryCreate(NULL,
					   NULL,
					   NULL,
					   0,
					   &kCFTypeDictionaryKeyCallBacks,
					   &kCFTypeDictionaryValueCallBacks);
		return proxy;
	}

	newProxy = CFDictionaryCreateMutableCopy(NULL, 0, proxy);

	validate_proxy_content(newProxy,
			       kSCPropNetProxiesFTPEnable,
			       kSCPropNetProxiesFTPProxy,
			       kSCPropNetProxiesFTPPort,
			       "ftp",
			       21);
	validate_proxy_content(newProxy,
			       kSCPropNetProxiesGopherEnable,
			       kSCPropNetProxiesGopherProxy,
			       kSCPropNetProxiesGopherPort,
			       "gopher",
			       70);
	validate_proxy_content(newProxy,
			       kSCPropNetProxiesHTTPEnable,
			       kSCPropNetProxiesHTTPProxy,
			       kSCPropNetProxiesHTTPPort,
			       "http",
			       80);
	validate_proxy_content(newProxy,
			       kSCPropNetProxiesHTTPSEnable,
			       kSCPropNetProxiesHTTPSProxy,
			       kSCPropNetProxiesHTTPSPort,
			       "https",
			       443);
	validate_proxy_content(newProxy,
			       kSCPropNetProxiesRTSPEnable,
			       kSCPropNetProxiesRTSPProxy,
			       kSCPropNetProxiesRTSPPort,
			       "rtsp",
			       554);
	validate_proxy_content(newProxy,
			       kSCPropNetProxiesSOCKSEnable,
			       kSCPropNetProxiesSOCKSProxy,
			       kSCPropNetProxiesSOCKSPort,
			       "socks",
			       1080);
	if (CFDictionaryContainsKey(newProxy, kSCPropNetProxiesProxyAutoConfigURLString)) {
		validate_proxy_content(newProxy,
				       kSCPropNetProxiesProxyAutoConfigEnable,
				       kSCPropNetProxiesProxyAutoConfigURLString,
				       NULL,
				       NULL,
				       0);

		// and we can't have both URLString and JavaScript keys
		CFDictionaryRemoveValue(newProxy, kSCPropNetProxiesProxyAutoConfigJavaScript);
	} else {
		validate_proxy_content(newProxy,
				       kSCPropNetProxiesProxyAutoConfigEnable,
				       kSCPropNetProxiesProxyAutoConfigJavaScript,
				       NULL,
				       NULL,
				       0);
	}
	validate_proxy_content(newProxy,
			       kSCPropNetProxiesProxyAutoDiscoveryEnable,
			       NULL,
			       NULL,
			       NULL,
			       0);

	// validate FTP passive setting
	num = CFDictionaryGetValue(newProxy, kSCPropNetProxiesFTPPassive);
	if (num != NULL) {
		int	enabled	= 0;

		if (!isA_CFNumber(num) ||
		    !CFNumberGetValue(num, kCFNumberIntType, &enabled)) {
			// if we don't like the enabled key/value
			enabled = 1;
			num = CFNumberCreate(NULL, kCFNumberIntType, &enabled);
			CFDictionarySetValue(newProxy,
					     kSCPropNetProxiesFTPPassive,
					     num);
			CFRelease(num);
		}
	}

	// validate proxy exception list
	array = CFDictionaryGetValue(newProxy, kSCPropNetProxiesExceptionsList);
	if (array != NULL) {
		CFIndex		i;
		CFIndex		n;

		n = isA_CFArray(array) ? CFArrayGetCount(array) : 0;
		for (i = 0; i < n; i++) {
			CFStringRef	str;

			str = CFArrayGetValueAtIndex(array, i);
			if (!isA_CFString(str)) {
				// if we don't like the array contents
				n = 0;
				break;
			}
		}

		if (n == 0) {
			CFDictionaryRemoveValue(newProxy, kSCPropNetProxiesExceptionsList);
		}
	}

	// validate exclude simple hostnames setting
	num = CFDictionaryGetValue(newProxy, kSCPropNetProxiesExcludeSimpleHostnames);
	if (num != NULL) {
		int	enabled;

		if (!isA_CFNumber(num) ||
		    !CFNumberGetValue(num, kCFNumberIntType, &enabled)) {
			// if we don't like the enabled key/value
			enabled = 0;
			num = CFNumberCreate(NULL, kCFNumberIntType, &enabled);
			CFDictionarySetValue(newProxy,
					     kSCPropNetProxiesExcludeSimpleHostnames,
					     num);
			CFRelease(num);
		}
	}

	// cleanup scoped proxies
	scoped = CFDictionaryGetValue(newProxy, kSCPropNetProxiesScoped);
	if (isA_CFDictionary(scoped)) {
		CFMutableDictionaryRef	newScoped;

		newScoped = CFDictionaryCreateMutable(NULL,
						      0,
						      &kCFTypeDictionaryKeyCallBacks,
						      &kCFTypeDictionaryValueCallBacks);
		CFDictionaryApplyFunction(scoped,
					  normalize_scoped_proxy,
					  newScoped);
		CFDictionarySetValue(newProxy, kSCPropNetProxiesScoped, newScoped);
		CFRelease(newScoped);
	}

	// cleanup split/supplemental proxies
	supplemental = CFDictionaryGetValue(newProxy, kSCPropNetProxiesSupplemental);
	if (isA_CFArray(supplemental)) {
		CFMutableArrayRef	newSupplemental;

		newSupplemental = CFArrayCreateMutable(NULL,
						       0,
						       &kCFTypeArrayCallBacks);
		CFArrayApplyFunction(supplemental,
				     CFRangeMake(0, CFArrayGetCount(supplemental)),
				     normalize_supplemental_proxy,
				     newSupplemental);
		CFDictionarySetValue(newProxy, kSCPropNetProxiesSupplemental, newSupplemental);
		CFRelease(newSupplemental);
	}

	proxy = CFDictionaryCreateCopy(NULL,newProxy);
	CFRelease(newProxy);

	return proxy;
}


static void
normalize_scoped_proxy(const void *key, const void *value, void *context)
{
	CFStringRef		interface	= (CFStringRef)key;
	CFDictionaryRef		proxy		= (CFDictionaryRef)value;
	CFMutableDictionaryRef	newScoped	= (CFMutableDictionaryRef)context;

	proxy = __SCNetworkProxiesCopyNormalized(proxy);
	CFDictionarySetValue(newScoped, interface, proxy);
	CFRelease(proxy);

	return;
}


static void
normalize_supplemental_proxy(const void *value, void *context)
{
	CFDictionaryRef		proxy		= (CFDictionaryRef)value;
	CFMutableArrayRef	newSupplemental	= (CFMutableArrayRef)context;

	proxy = __SCNetworkProxiesCopyNormalized(proxy);
	CFArrayAppendValue(newSupplemental, proxy);
	CFRelease(proxy);

	return;
}


CFDictionaryRef
SCDynamicStoreCopyProxies(SCDynamicStoreRef store)
{
	CFStringRef		key;
	CFDictionaryRef		proxies;


	/* copy proxy information from dynamic store */

	key = SCDynamicStoreKeyCreateProxies(NULL);
	proxies = SCDynamicStoreCopyValue(store, key);
	CFRelease(key);


	if (proxies != NULL) {
		CFDictionaryRef	base	= proxies;

		proxies = __SCNetworkProxiesCopyNormalized(base);
		CFRelease(base);
	} else {
		proxies = CFDictionaryCreate(NULL,
					     NULL,
					     NULL,
					     0,
					     &kCFTypeDictionaryKeyCallBacks,
					     &kCFTypeDictionaryValueCallBacks);
	}


	return proxies;
}


CFArrayRef
SCNetworkProxiesCopyMatching(CFDictionaryRef	globalConfiguration,
			     CFStringRef	server,
			     CFStringRef	interface)
{
	CFMutableDictionaryRef	newProxy;
	CFArrayRef		proxies		= NULL;
	CFDictionaryRef		proxy;
	int			sc_status	= kSCStatusOK;
	CFStringRef		trimmed		= NULL;

	if (!isA_CFDictionary(globalConfiguration)) {
		// if no proxy configuration
		_SCErrorSet(kSCStatusOK);
		return NULL;
	}

	if (interface != NULL) {
		CFDictionaryRef		scoped;

		if (!isA_CFString(interface) ||
		    (CFStringGetLength(interface) == 0)) {
			_SCErrorSet(kSCStatusInvalidArgument);
			return NULL;
		}

		scoped = CFDictionaryGetValue(globalConfiguration, kSCPropNetProxiesScoped);
		if (scoped == NULL) {
			// if no scoped proxy configurations
			_SCErrorSet(kSCStatusOK);
			return NULL;
		}

		if (!isA_CFDictionary(scoped)) {
			// if corrupt proxy configuration
			_SCErrorSet(kSCStatusFailed);
			return NULL;
		}

		proxy = CFDictionaryGetValue(scoped, interface);
		if (proxy == NULL) {
			// if no scoped proxy configuration for this interface
			_SCErrorSet(kSCStatusOK);
			return NULL;
		}

		if (!isA_CFDictionary(proxy)) {
			// if corrupt proxy configuration
			_SCErrorSet(kSCStatusFailed);
			return NULL;
		}

		// return per-interface proxy configuration
		proxies = CFArrayCreate(NULL, (const void **)&proxy, 1, &kCFTypeArrayCallBacks);
		return proxies;
	}

	if (server != NULL) {
		CFIndex			i;
		CFMutableArrayRef	matching	= NULL;
		CFIndex			n		= 0;
		CFIndex			server_len;
		CFArrayRef		supplemental;

		trimmed = _SC_trimDomain(server);
		if (trimmed == NULL) {
			_SCErrorSet(kSCStatusInvalidArgument);
			return NULL;
		}

		server = trimmed;
		server_len = CFStringGetLength(server);

		supplemental = CFDictionaryGetValue(globalConfiguration, kSCPropNetProxiesSupplemental);
		if (supplemental != NULL) {
			if (!isA_CFArray(supplemental)) {
				// if corrupt proxy configuration
				sc_status = kSCStatusFailed;
				goto done;
			}

			n = CFArrayGetCount(supplemental);
		}

		for (i = 0; i < n; i++) {
			CFStringRef	domain;
			CFIndex		domain_len;
			CFIndex		n_matching;

			proxy = CFArrayGetValueAtIndex(supplemental, i);
			if (!isA_CFDictionary(proxy)) {
				// if corrupt proxy configuration
				continue;
			}

			domain = CFDictionaryGetValue(proxy, kSCPropNetProxiesSupplementalMatchDomain);
			if (!isA_CFString(domain)) {
				// if corrupt proxy configuration
				continue;
			}

			domain_len = CFStringGetLength(domain);
			if (domain_len > 0) {
				if (!CFStringFindWithOptions(server,
							     domain,
							     CFRangeMake(0, server_len),
							     kCFCompareCaseInsensitive|kCFCompareAnchored|kCFCompareBackwards,
							     NULL)) {
					// if server does not match this proxy domain (or host)
					continue;
				}

				if ((server_len > domain_len) &&
				    !CFStringFindWithOptions(server,
							     CFSTR("."),
							     CFRangeMake(0, server_len - domain_len),
							     kCFCompareCaseInsensitive|kCFCompareAnchored|kCFCompareBackwards,
							     NULL)) {
					// if server does not match this proxy domain
					continue;
				}
//			} else {
//				// if this is a "default" (match all) proxy domain
			}

			if (matching == NULL) {
				matching = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			}
			n_matching = CFArrayGetCount(matching);

			newProxy = CFDictionaryCreateMutableCopy(NULL, 0, proxy);
			CFDictionaryRemoveValue(newProxy, kSCPropNetProxiesSupplementalMatchDomain);
			if ((n_matching == 0) ||
			    !CFArrayContainsValue(matching, CFRangeMake(0, n_matching), newProxy)) {
				// add this matching proxy
				CFArrayAppendValue(matching, newProxy);
			}
			CFRelease(newProxy);
		}

		if (matching != NULL) {
			// if we have any supplemental match domains
			proxies = CFArrayCreateCopy(NULL, matching);
			CFRelease(matching);
			goto done;
		}
	}

	// no matches, return "global" proxy configuration

	newProxy = CFDictionaryCreateMutableCopy(NULL, 0, globalConfiguration);
	CFDictionaryRemoveValue(newProxy, kSCPropNetProxiesScoped);
	CFDictionaryRemoveValue(newProxy, kSCPropNetProxiesSupplemental);
	proxies = CFArrayCreate(NULL, (const void **)&newProxy, 1, &kCFTypeArrayCallBacks);
	CFRelease(newProxy);

    done :

	if (sc_status != kSCStatusOK) {
		if (proxies != NULL) {
			CFRelease(proxies);
			proxies = NULL;
		}
		_SCErrorSet(sc_status);
	}
	if (trimmed != NULL) CFRelease(trimmed);

	return proxies;
}
